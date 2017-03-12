/*
 * mem.h
 *
 *  Created on: Nov 10, 2015
 *      Author: xiaoxin
 */

#ifndef MEM_H_
#define MEM_H_

#include"proc.h"
#include"socket.h"
#define MEM_PROC_SIZE (uint64_t)1*1024*1024
#define MEM_TOTAL_SIZE (uint64_t)(THREADNUM+1)*MEM_PROC_SIZE

struct PROC_MEM_HEAD
{
    Size total_size;
    Size freeoffset;
};

typedef struct PROC_MEM_HEAD PMHEAD;

extern char* MemStart;

extern void InitMem(void);

extern void ResetMem(int i);

extern void FreeMem(void);

extern void* MemAlloc(void* memstart,Size size);

extern void MemClean(void *memstart);

extern void TransactionMemClean(void);

#endif /* MEM_H_ */
