#ifndef __TCG_ARM_H__
#define __TCG_ARM_H__

#include <pthread.h>

extern pthread_mutex_t exec_lock;

int init_tcg_arm(void);

void exec(
	uint32_t *regs, uint64_t* fpregs,
    uint32_t *cpsr, uint32_t *fpscr,
    uint32_t *fpexc,
    int dump_reg);

#endif /* __TCG_ARM_H__ */
