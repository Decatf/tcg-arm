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

#define TAG "cpu-exec"
#include "android_log_helper.h"

struct tb_desc {
    target_ulong pc;
    target_ulong cs_base;
    CPUArchState *env;
    tb_page_addr_t phys_page1;
    uint32_t flags;
};

static bool tb_cmp(const void *p, const void *d)
{
    const TranslationBlock *tb = p;
    const struct tb_desc *desc = d;

    if (tb->pc == desc->pc &&
        tb->page_addr[0] == desc->phys_page1 &&
        tb->cs_base == desc->cs_base &&
        tb->flags == desc->flags) {
        /* check next page if needed */
        if (tb->page_addr[1] == -1) {
            return true;
        } else {
            tb_page_addr_t phys_page2;
            target_ulong virt_page2;

            virt_page2 = (desc->pc & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
            phys_page2 = get_page_addr_code(desc->env, virt_page2);
            if (tb->page_addr[1] == phys_page2) {
                return true;
            }
        }
    }
    return false;
}

static TranslationBlock *tb_find_physical(CPUState *cpu,
                                          target_ulong pc,
                                          target_ulong cs_base,
                                          uint32_t flags)
{
    tb_page_addr_t phys_pc;
    struct tb_desc desc;
    uint32_t h;

    desc.env = (CPUArchState *)cpu->env_ptr;
    desc.cs_base = cs_base;
    desc.flags = flags;
    desc.pc = pc;
    phys_pc = get_page_addr_code(desc.env, pc);
    desc.phys_page1 = phys_pc & TARGET_PAGE_MASK;
    h = tb_hash_func(phys_pc, pc, flags);
    return qht_lookup(&tcg_ctx.tb_ctx.htable, tb_cmp, &desc, h);
}

static TranslationBlock *tb_find_slow(CPUState *cpu,
                                      target_ulong pc,
                                      target_ulong cs_base,
                                      uint32_t flags,
                                      int cflags)
{
    TranslationBlock *tb;

    tb = tb_find_physical(cpu, pc, cs_base, flags);
    if (tb) {
        goto found;
    }

#ifdef CONFIG_USER_ONLY
    /* mmap_lock is needed by tb_gen_code, and mmap_lock must be
     * taken outside tb_lock.  Since we're momentarily dropping
     * tb_lock, there's a chance that our desired tb has been
     * translated.
     */
    tb_unlock();
    mmap_lock();
    tb_lock();
    tb = tb_find_physical(cpu, pc, cs_base, flags);
    if (tb) {
        mmap_unlock();
        goto found;
    }
#endif

    /* if no translated code available, then translate it now */
    tb = tb_gen_code(cpu, pc, cs_base, flags, cflags);

#ifdef CONFIG_USER_ONLY
    mmap_unlock();
#endif

found:
    /* we add the TB in the virtual pc hash table */
    cpu->tb_jmp_cache[tb_jmp_cache_hash_func(pc)] = tb;
    return tb;
}

TranslationBlock *tb_find_fast(CPUState *cpu,
                                             TranslationBlock **last_tb,
                                             int tb_exit,
                                             int cflags)
{
    CPUArchState *env = (CPUArchState *)cpu->env_ptr;
    TranslationBlock *tb;
    target_ulong cs_base, pc;
    uint32_t flags;

    /* we record a subset of the CPU state. It will
       always be the same before a given translated block
       is executed. */
    cpu_get_tb_cpu_state(env, &pc, &cs_base, (uint32_t*)&flags);
    tb_lock();
    tb = cpu->tb_jmp_cache[tb_jmp_cache_hash_func(pc)];
    if (unlikely(!tb || tb->pc != pc || tb->cs_base != cs_base ||
                 tb->flags != flags)) {
        tb = tb_find_slow(cpu, pc, cs_base, flags, cflags);
    }
    if (cpu->tb_flushed) {
        /* Ensure that no TB jump will be modified as the
         * translation buffer has been flushed.
         */
        *last_tb = NULL;
        cpu->tb_flushed = false;
    }
#ifndef CONFIG_USER_ONLY
    /* We don't take care of direct jumps when address mapping changes in
     * system emulation. So it's not safe to make a direct jump to a TB
     * spanning two pages because the mapping for the second page can change.
     */
    if (tb->page_addr[1] != -1) {
        *last_tb = NULL;
    }
#endif
    /* See if we can patch the calling TB. */
    if (*last_tb && !qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
        tb_add_jump(*last_tb, tb_exit, tb);
    }
    tb_unlock();
    return tb;
}


TranslationBlock *tb_from_cache(CPUArchState *env)
{
    CPUState *cpu = ENV_GET_CPU(env);
    TranslationBlock *tb;
    target_ulong cs_base, pc;
    int flags;

    /* we record a subset of the CPU state. It will
       always be the same before a given translated block
       is executed. */
    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
    tb_lock();

    tb = tb_find_physical(cpu, pc, cs_base, flags);
    if (tb) {
        goto found;
    }

    tb = cpu->tb_jmp_cache[tb_jmp_cache_hash_func(pc)];
    if (unlikely(!tb || tb->pc != pc || tb->cs_base != cs_base ||
                 tb->flags != flags)) {
        tb = NULL;
    }

found:
    tb_unlock();
    return tb;
}

void pre_cpu_exec(CPUState *cpu)
{
    CPUClass *cc = CPU_GET_CLASS(cpu);
    int ret;

    // atomic_mb_set(&tcg_current_cpu, cpu);
    rcu_read_lock();
    cc->cpu_exec_enter(cpu);

    cpu->tb_flushed = false; /* reset before first TB lookup */
}

void post_cpu_exec(CPUState *cpu)
{
    CPUClass *cc = CPU_GET_CLASS(cpu);

    cc->cpu_exec_exit(cpu);
    rcu_read_unlock();
}

tcg_target_ulong cpu_tb_exec(CPUState *cpu, TranslationBlock *itb)
{
    CPUArchState *env = cpu->env_ptr;
    uintptr_t ret;
    TranslationBlock *last_tb;
    int tb_exit;
    uint8_t *tb_ptr = itb->tc_ptr;

    cpu->can_do_io = !use_icount;
    ret = tcg_qemu_tb_exec(env, tb_ptr);
    cpu->can_do_io = 1;
    tb_exit = ret & TB_EXIT_MASK;
    trace_exec_tb_exit(last_tb, tb_exit);

    if (tb_exit > TB_EXIT_IDX1) {
        /* We didn't start executing this TB (eg because the instruction
         * counter hit zero); we must restore the guest PC to the address
         * of the start of the TB.
         */
        CPUClass *cc = CPU_GET_CLASS(cpu);
        qemu_log_mask_and_addr(CPU_LOG_EXEC, last_tb->pc,
                               "Stopped execution of TB chain before %p ["
                               TARGET_FMT_lx "] %s\n",
                               last_tb->tc_ptr, last_tb->pc,
                               lookup_symbol(last_tb->pc));
        if (cc->synchronize_from_tb) {
            cc->synchronize_from_tb(cpu, last_tb);
        } else {
            assert(cc->set_pc);
            cc->set_pc(cpu, last_tb->pc);
        }
    }
    if (tb_exit == TB_EXIT_REQUESTED) {
        /* We were asked to stop executing TBs (probably a pending
         * interrupt. We've now stopped, so clear the flag.
         */
        cpu->tcg_exit_req = 0;
        LOGE("UNHANDLED cpu->tcg_exit_req = 0\n");
    }
    return ret;
}

/* Load an instruction and return it in the standard little-endian order */
static inline uint32_t arm_ldl_code(CPUARMState *env, target_ulong addr,
                                    bool sctlr_b)
{
    uint32_t insn = cpu_ldl_code(env, addr);
    if (bswap_code(sctlr_b)) {
        return bswap32(insn);
    }
    return insn;
}

/* Ditto, for a halfword (Thumb) instruction */
static inline uint16_t arm_lduw_code(CPUARMState *env, target_ulong addr,
                                     bool sctlr_b)
{
    uint16_t insn = cpu_lduw_code(env, addr);
    if (bswap_code(sctlr_b)) {
        return bswap16(insn);
    }
    return insn;
}

/* extracted from disas_arm_insn() */
static inline int is_arm_neon(uint32_t insn) {
    int cond;
    int handle_insn = 0;

    cond = insn >> 28;

    if (cond == 0xf) {
        if (((insn >> 25) & 7) == 1) {
            // if (disas_neon_data_insn(s, insn)) { }
            handle_insn = 1;
        }
        if ((insn & 0x0f100000) == 0x04000000) {
            // if (disas_neon_ls_insn(s, insn)) { }
            handle_insn = 1;
        }
        if ((insn & 0x0f000e10) == 0x0e000a00) {
            // if (disas_vfp_insn(s, insn)) { }
            handle_insn = 1;
        }
    }

    if ((insn & 0x0f900000) == 0x03000000) {
    } else if ((insn & 0x0f900000) == 0x01000000
               && (insn & 0x00000090) != 0x00000090) {
    } else if (((insn & 0x0e000000) == 0 &&
               (insn & 0x00000090) != 0x90) ||
               ((insn & 0x0e000000) == (1 << 25))) {
    } else {
        unsigned int op1 = (insn >> 24) & 0xf;
        switch(op1) {
        case 0xc:
        case 0xd:
        case 0xe:
            if (((insn >> 8) & 0xe) == 10) {
                // if (disas_vfp_insn(s, insn)) { }
                handle_insn = 1;
            }
        }
    }

    return handle_insn;
}

/* extracted from disas_thumb2_insn() */
static inline int is_thumb2_neon(CPUARMState *env, target_ulong pc, int sctlr_b, uint16_t insn_hw1)
{
    uint32_t insn;

    // // if (!(arm_dc_feature(s, ARM_FEATURE_THUMB2)
    // //       || arm_dc_feature(s, ARM_FEATURE_M))) {
    // if (1) {
    //     insn = insn_hw1;
    //     if ((insn & (1 << 12)) == 0) {
    //         return 0;
    //     }
    //     if (insn & (1 << 11)) {
    //         return 0;
    //     }
    //     if ((pc & ~TARGET_PAGE_MASK) == 0) {
    //         return 0;
    //     }
    //     /* Fall through */
    // }

    insn = arm_lduw_code(env, pc, sctlr_b);
    insn |= (uint32_t)insn_hw1 << 16;

    switch ((insn >> 25) & 0xf) {
    case 6: case 7: case 14: case 15:
        /* Coprocessor.  */
        if (((insn >> 24) & 3) == 3) {
            // if (disas_neon_data_insn(s, insn)) { }
            return 1;
        } else if (((insn >> 8) & 0xe) == 10) {
            // if (disas_vfp_insn(s, insn)) { }
            return 1;
        } 

    case 12: /* Load/store single data item.  */
        if ((insn & 0x01100000) == 0x01000000) {
            // if (disas_neon_ls_insn(s, insn)) { }
            return 1;
        }
    }

    return 0;
}

/* extracted from disas_thumb2_insn() */
static inline int is_thumb_neon(CPUARMState *env, target_ulong pc, int sctlr_b, uint16_t insn)
{
    int handle_insn = 0;

    switch (insn >> 12) {
    case 14:
        if (insn & (1 << 11)) {
            // if (disas_thumb2_insn(env, s, insn))
            handle_insn = is_thumb2_neon(env, pc, sctlr_b, insn);
        }
    case 15:
        // if (disas_thumb2_insn(env, s, insn))
        handle_insn = is_thumb2_neon(env, pc, sctlr_b, insn);
    }

    return handle_insn;
}

/* count the number of consecutive NEON instructions */
int neon_icount(CPUState *cpu, target_ulong pc, uint32_t flags)
{
    CPUArchState *env = (CPUArchState *)cpu->env_ptr;
    uint32_t cpsr;
    int thumb;
    int sctlr_b;
    int icount = 0;

    cpsr = cpsr_read(env) & 0xFFFFFFFF;
    thumb = !!(cpsr & CPSR_T);
    sctlr_b = ARM_TBFLAG_SCTLR_B(flags);

    if (!thumb) {
        do {
            int handle_insn;
            uint32_t insn;

            insn = arm_ldl_code(env, pc, sctlr_b);
            handle_insn = is_arm_neon(insn);

            if (handle_insn) {
                pc += 4;
                icount++;
                continue;
            } else {
                break;
            }
        } while (1);
    } else {

        // do {
        //     int handle_insn = 0;
        //     uint16_t insn;

        //     insn = arm_lduw_code(env, pc, sctlr_b);
        //     pc += 2;

        //     LOGE("%s: insn = 0x%X", __func__, insn & 0xFFFF);

        //     handle_insn = is_thumb_neon(env, pc, sctlr_b, insn);

        //     if (handle_insn) {
        //         pc += 2;
        //         icount++;
        //         continue;
        //     } else {
        //         break;
        //     }
        // } while (1);
        // LOGE("neon_icount() thumb: icount = %d\n", icount);

        icount = 1;
    }

    if (icount == 0) {
      LOGE("neon_icount() oops icount = %d\n", icount); 
      icount = 1;
    }

    return icount;
}

int guess_icount(CPUState *cpu)
{
    CPUArchState *env = (CPUArchState *)cpu->env_ptr;
    TranslationBlock *tb;
    target_ulong cs_base, pc;
    uint32_t flags;
    int icount = 1;

    cpu_get_tb_cpu_state(env, &pc, &cs_base, (uint32_t*)&flags);
    tb_lock();
    tb = cpu->tb_jmp_cache[tb_jmp_cache_hash_func(pc)];
    if (unlikely(!tb || tb->pc != pc || tb->cs_base != cs_base ||
                 tb->flags != flags)) {
        icount = neon_icount(cpu, pc, flags);
    }

    tb_unlock();
    return icount;
}