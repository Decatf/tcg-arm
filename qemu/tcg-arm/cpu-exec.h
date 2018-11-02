#ifndef __CPU_EXEC_H__
#define __CPU_EXEC_H__

#include "qemu.h"

TranslationBlock *tb_find_fast(CPUState *cpu,
                                     TranslationBlock **last_tb,
                                     int tb_exit,
                                     int cflags);
TranslationBlock *tb_from_cache(CPUArchState *env);
void pre_cpu_exec(CPUState *cpu);
void post_cpu_exec(CPUState *cpu);
tcg_target_ulong cpu_tb_exec(CPUState *cpu, TranslationBlock *itb);

int guess_icount(CPUState *cpu);

#endif /* __CPU_EXEC_H__*/