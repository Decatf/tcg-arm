#include "qemu/osdep.h"
// #include "qemu-version.h"
#include <sys/syscall.h>
#include <sys/resource.h>

#include "qapi/error.h"
#include "qemu.h"
#include "qemu/path.h"
#include "qemu/config-file.h"
#include "qemu/cutils.h"
#include "qemu/help_option.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg.h"
#include "qemu/timer.h"
#include "qemu/envlist.h"
#include "elf.h"
#include "exec/log.h"
#include "trace/control.h"
#include "glib-compat.h"

#include "exec/tb-hash.h"

#include "cpu-exec.h"
#include "tcg-arm.h"

#define TAG "tcg-arm"
#include "android_log_helper.h"

bool initialized = false;
const char *cpu_model = NULL;

#define N_CPUS 2
CPUState *local_cpus[N_CPUS];
pthread_mutex_t local_cpu_lock[2];

__attribute__((visibility("default")))
int init_tcg_arm(void)
{
    CPUArchState *env;
    char **target_environ, **wrk;
    int i;
    int ret;

    if (initialized) {
        return 0;
    }

    module_call_init(MODULE_INIT_QOM);

    cpu_model = NULL;

    if (cpu_model == NULL) {
#if defined(CONFIG_USER_ONLY)
        cpu_model = "any";
#else
        cpu_model = "cortex-a9";
#endif
    }

    tcg_exec_init(0);

    for (int i = 0; i < N_CPUS; i++)
    {
        CPUState *cpu;

        /* NOTE: we need to init the CPU at this stage to get
           qemu_host_page_size */
        cpu = cpu_init(cpu_model);
        if (!cpu) {
            fprintf(stderr, "Unable to find CPU definition\n");
            exit(EXIT_FAILURE);
        }
        env = cpu->env_ptr;
        cpu_reset(cpu);

        local_cpus[i] = cpu;
        pthread_mutex_init(&local_cpu_lock[i], NULL);
    }

#if !defined(CONFIG_SOFTMMU)
    /* Now that we've loaded the binary, GUEST_BASE is fixed.  Delay
       generating the prologue until now so that the prologue can take
       the real value of GUEST_BASE into account.  */
    tcg_prologue_init(&tcg_ctx);
#endif

    use_icount = 0;

    current_cpu = NULL;
    initialized = true;
    return initialized;
}

inline int acquire_cpu(void)
{
    int cpuid;
    do {
        for (int i = 0; i < N_CPUS; i++)
        {
            int res;
            res = pthread_mutex_trylock(&local_cpu_lock[i]);

            if (!res) {
                cpuid = i;
                break;
            }
        }

        if (cpuid < 0)
            sched_yield();
    } while (cpuid == -1);
    return cpuid;
}

inline void release_cpu(int cpuid)
{
    if (cpuid < 0 || cpuid >= N_CPUS)
        return;
    pthread_mutex_unlock(&local_cpu_lock[cpuid]);
    cpuid = -1;
}

__attribute__((visibility("default")))
void exec(
    uint32_t *regs, uint64_t* fpregs,
    uint32_t *cpsr, uint32_t *fpscr,
    uint32_t *fpexc,
    int dump_reg)
{
    CPUState *cpu = NULL;
    CPUArchState *env = NULL;
    TranslationBlock *tb = NULL, *last_tb = NULL;
    int tb_exit = 0;
    int cflags = 0;

    int cpuid = acquire_cpu();
    cpu = local_cpus[cpuid];

    current_cpu = cpu;
    env = cpu->env_ptr;

    if (!env) {
        fprintf(stderr, "CPUArchState not initialized.");
        return;
    }

    const uint32_t cpsr_mask = 0xFFFFFFFF;
    const uint32_t fpscr_mask = 0xF7F7009F;
    const uint32_t fpexc_mask = 0xE0000000;
    cpsr_write(env, *cpsr, cpsr_mask, CPSRWriteRaw);
    vfp_set_fpscr(env, *fpscr);

    uint32_t env_fpexc = env->vfp.xregs[ARM_VFP_FPEXC];
    env_fpexc = env_fpexc & ~fpexc_mask;
    env->vfp.xregs[ARM_VFP_FPEXC] |= ((*fpexc) & fpexc_mask);

    memcpy(env->regs, regs, 16*sizeof(uint32_t));
    memcpy(env->vfp.regs, fpregs, 32*sizeof(uint64_t));

    pre_cpu_exec(cpu);

    cflags = CF_COUNT_MASK & 1;
    tb = tb_find_fast(cpu,  &last_tb, tb_exit, cflags);
    
    do {
        uintptr_t ret;
        TranslationBlock *prev_tb = NULL;

        ret = cpu_tb_exec(cpu, tb);
        last_tb = (TranslationBlock *)(ret & ~TB_EXIT_MASK);
        tb_exit = ret & TB_EXIT_MASK;
        prev_tb = tb;

        switch (tb_exit) {
        case TB_EXIT_REQUESTED:
            LOGE("%s() TB_EXIT_REQUESTED\n", __func__);
            /* Something asked us to stop executing
             * chained TBs; just continue round the main
             * loop. Whatever requested the exit will also
             * have set something else (eg exit_request or
             * interrupt_request) which we will handle
             * next time around the loop.  But we need to
             * ensure the tcg_exit_req read in generated code
             * comes before the next read of cpu->exit_request
             * or cpu->interrupt_request.
             */
            smp_rmb();
            last_tb = NULL;
            break;
        case TB_EXIT_ICOUNT_EXPIRED:
        {
            LOGE("%s() TB_EXIT_ICOUNT_EXPIRED\n", __func__);
            /* Instruction counter expired.  */
            abort();
        }
        default:
            break;
        }

        tb = tb_from_cache(env);
        if (tb != NULL) {
            last_tb = prev_tb;
            tb = tb_find_fast(cpu,  &last_tb, tb_exit, cflags);
        }
    } while (tb);
    post_cpu_exec(cpu);

    memcpy(fpregs, env->vfp.regs, 32*sizeof(uint64_t));
    memcpy(regs, env->regs, 16*sizeof(uint32_t));

    *fpscr = (*fpscr) & ~fpscr_mask;
    *fpscr |= vfp_get_fpscr(env) & fpscr_mask;
    *cpsr = (*cpsr) & ~cpsr_mask;
    *cpsr |= cpsr_read(env) & cpsr_mask;
    *fpexc = (*fpexc) & ~fpexc_mask;
    *fpexc |= (env->vfp.xregs[ARM_VFP_FPEXC] & fpexc_mask);

    release_cpu(cpuid);
}
