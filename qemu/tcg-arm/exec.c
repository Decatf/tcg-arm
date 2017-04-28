#include "qemu/osdep.h"
#include "qapi/error.h"
#ifndef _WIN32
#endif

#include "qemu/cutils.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg.h"
#include "hw/qdev-core.h"
#if !defined(CONFIG_USER_ONLY)
#include "hw/boards.h"
#include "hw/xen/xen.h"
#endif
#include "sysemu/kvm.h"
#include "sysemu/sysemu.h"
#include "qemu/timer.h"
#include "qemu/config-file.h"
#include "qemu/error-report.h"
#if defined(CONFIG_USER_ONLY)
#include "qemu.h"
#else /* !CONFIG_USER_ONLY */
#include "hw/hw.h"
#include "exec/memory.h"
#include "exec/ioport.h"
#include "sysemu/dma.h"
#include "exec/address-spaces.h"
#include "sysemu/xen-mapcache.h"
#include "trace.h"
#endif
#include "exec/cpu-all.h"
#include "qemu/rcu_queue.h"
#include "qemu/main-loop.h"
#include "translate-all.h"
#include "sysemu/replay.h"

#include "exec/memory-internal.h"
#include "exec/ram_addr.h"
#include "exec/log.h"

#include "migration/vmstate.h"

#include "qemu/range.h"
#ifndef _WIN32
#include "qemu/mmap-alloc.h"
#endif

struct CPUTailQ cpus = QTAILQ_HEAD_INITIALIZER(cpus);
/* current CPU in the current thread. It is only valid inside
   cpu_exec() */
__thread CPUState *current_cpu;

/* 0 = Do not count executed instructions.
   1 = Precise instruction counting.
   2 = Adaptive rate instruction counting.  */
int use_icount;

static int cpu_get_free_index(void)
{
    CPUState *some_cpu;
    int cpu_index = 0;

    CPU_FOREACH(some_cpu) {
        cpu_index++;
    }
    return cpu_index;
}

void cpu_exec_exit(CPUState *cpu)
{
    CPUClass *cc = CPU_GET_CLASS(cpu);

    cpu_list_lock();
    if (cpu->node.tqe_prev == NULL) {
        /* there is nothing to undo since cpu_exec_init() hasn't been called */
        cpu_list_unlock();
        return;
    }

    QTAILQ_REMOVE(&cpus, cpu, node);
    cpu->node.tqe_prev = NULL;
    cpu->cpu_index = UNASSIGNED_CPU_INDEX;
    cpu_list_unlock();

    if (cc->vmsd != NULL) {
        vmstate_unregister(NULL, cc->vmsd, cpu);
    }
    if (qdev_get_vmsd(DEVICE(cpu)) == NULL) {
        vmstate_unregister(NULL, &vmstate_cpu_common, cpu);
    }
}

void cpu_exec_init(CPUState *cpu, Error **errp)
{
    CPUClass *cc ATTRIBUTE_UNUSED = CPU_GET_CLASS(cpu);
    Error *local_err ATTRIBUTE_UNUSED = NULL;

    cpu->as = NULL;
    cpu->num_ases = 0;

#ifndef CONFIG_USER_ONLY
    cpu->thread_id = qemu_get_thread_id();

    /* This is a softmmu CPU object, so create a property for it
     * so users can wire up its memory. (This can't go in qom/cpu.c
     * because that file is compiled only once for both user-mode
     * and system builds.) The default if no link is set up is to use
     * the system address space.
     */
    object_property_add_link(OBJECT(cpu), "memory", TYPE_MEMORY_REGION,
                             (Object **)&cpu->memory,
                             qdev_prop_allow_set_link_before_realize,
                             OBJ_PROP_LINK_UNREF_ON_RELEASE,
                             &error_abort);
    cpu->memory = system_memory;
    object_ref(OBJECT(cpu->memory));
#endif

    cpu_list_lock();
    if (cpu->cpu_index == UNASSIGNED_CPU_INDEX) {
        cpu->cpu_index = cpu_get_free_index();
        assert(cpu->cpu_index != UNASSIGNED_CPU_INDEX);
    }
    QTAILQ_INSERT_TAIL(&cpus, cpu, node);
    cpu_list_unlock();

#ifndef CONFIG_USER_ONLY
    if (qdev_get_vmsd(DEVICE(cpu)) == NULL) {
        vmstate_register(NULL, cpu->cpu_index, &vmstate_cpu_common, cpu);
    }
    if (cc->vmsd != NULL) {
        vmstate_register(NULL, cpu->cpu_index, cc->vmsd, cpu);
    }
#endif
}

/*
 * A helper function for the _utterly broken_ virtio device model to find out if
 * it's running on a big endian machine. Don't do this at home kids!
 */
bool target_words_bigendian(void);
bool target_words_bigendian(void)
{
#if defined(TARGET_WORDS_BIGENDIAN)
    return true;
#else
    return false;
#endif
}

#if defined(CONFIG_USER_ONLY)
void cpu_watchpoint_remove_all(CPUState *cpu, int mask)

{
}

void cpu_watchpoint_remove_by_ref(CPUState *cpu, CPUWatchpoint *watchpoint)
{
}

int cpu_watchpoint_insert(CPUState *cpu, vaddr addr, vaddr len,
                          int flags, CPUWatchpoint **watchpoint)
{
    return -ENOSYS;
}
#endif

/* Add a breakpoint.  */
int cpu_breakpoint_insert(CPUState *cpu, vaddr pc, int flags,
                          CPUBreakpoint **breakpoint)
{
    printf("STUB: cpu_breakpoint_insert\n");
    return -ENOSYS;
}

/* Remove a specific breakpoint.  */
int cpu_breakpoint_remove(CPUState *cpu, vaddr pc, int flags)
{
    printf("STUB: cpu_breakpoint_remove\n");
    return -ENOSYS;
}

/* Remove a specific breakpoint by reference.  */
void cpu_breakpoint_remove_by_ref(CPUState *cpu, CPUBreakpoint *breakpoint)
{
    printf("STUB: cpu_breakpoint_remove_by_ref\n");
}

/* Remove all matching breakpoints. */
void cpu_breakpoint_remove_all(CPUState *cpu, int mask)
{
    // printf("STUB: cpu_breakpoint_remove_all\n");
}

/* enable or disable single step mode. EXCP_DEBUG is returned by the
   CPU loop after each instruction */
void cpu_single_step(CPUState *cpu, int enabled)
{
    printf("STUB: cpu_single_step\n");
}

void cpu_abort(CPUState *cpu, const char *fmt, ...)
{
    printf("STUB: cpu_abort\n");
    abort();
}

int cpu_memory_rw_debug(CPUState *cpu, target_ulong addr,
                        uint8_t *buf, int len, int is_write)
{
    printf("STUB: cpu_memory_rw_debug\n");
    return -ENOSYS;
}

