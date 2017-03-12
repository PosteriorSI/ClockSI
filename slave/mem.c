/*
 * mem.c
 *
 *  Created on: Nov 10, 2015
 *      Author: xiaoxin
 */
#include<malloc.h>
#include<stdlib.h>
#include"config.h"
#include"mem.h"
#include"thread_global.h"
#include"trans.h"
#include"lock_record.h"
#include"data_record.h"

uint32_t PROC_START_OFFSET=sizeof(PMHEAD);

uint32_t ThreadReuseMemStart;

char* MemStart=NULL;


uint32_t ThreadReuseMemStartCompute(void)
{
    uint32_t size=0;

    size+=sizeof(PMHEAD);

    size+=sizeof(THREAD);

    size+=sizeof(TransactionData);

    size+=DataMemSize();

    //size+=MaxDataLockNum*sizeof(DataLock);

    return size;
}
/*
 * malloc the memory needed for all processes ahead, avoid to malloc
 * dynamically during process running.
 */
void InitMem(void)
{
    Size size=MEM_TOTAL_SIZE;

    ThreadReuseMemStart=ThreadReuseMemStartCompute();

    printf("ThreadReuseMemStart=%d\n",ThreadReuseMemStart);

    char* start=NULL;
    MemStart=(char*)malloc(size);
    if(MemStart==NULL)
    {
        printf("memory malloc failed.\n");
        exit(-1);
    }
    int procnum;
    PMHEAD* pmhead=NULL;
    for (procnum=0;procnum<THREADNUM+1;procnum++)
    {
        start=MemStart+procnum*MEM_PROC_SIZE;
        pmhead=(PMHEAD*)start;
        pmhead->total_size=MEM_PROC_SIZE;
        pmhead->freeoffset=PROC_START_OFFSET;
    }
}

void ResetMem(int i)
{
    char* start=NULL;
    PMHEAD* pmhead=NULL;
    start=MemStart+i*MEM_PROC_SIZE;
    memset((char*)start,0,MEM_PROC_SIZE);
    pmhead=(PMHEAD*)start;
    pmhead->total_size=MEM_PROC_SIZE;
    pmhead->freeoffset=PROC_START_OFFSET;
}

/*
 * new interface for memory allocation in thread running.
 */
void* MemAlloc(void* memstart,Size size)
{
    PMHEAD* pmhead=NULL;
    Size newStart;
    Size newFree;
    void* newSpace;

    pmhead=(PMHEAD*)memstart;

    newStart=pmhead->freeoffset;
    newFree=newStart+size;

    if(newFree>pmhead->total_size)
        newSpace=NULL;
    else
    {
        newSpace=(void*)((char*)memstart+newStart);
        pmhead->freeoffset=newFree;
    }

    if(!newSpace)
    {
        printf("out of memory for process %ld\n",pthread_self());
        exit(-1);
    }
    return newSpace;
}

/*
 * new interface for memory clean in process ending.
 */
void MemClean(void *memstart)
{
    PMHEAD* pmhead=NULL;

    /* reset process memory. */
    memset((char*)memstart,0,MEM_PROC_SIZE);

    pmhead=(PMHEAD*)memstart;

    pmhead->freeoffset=PROC_START_OFFSET;
    pmhead->total_size=MEM_PROC_SIZE;
}

/*
 * clean transaction memory context.
 * @memstart:start address of current thread's private memory.
 */
void TransactionMemClean(void)
{
    PMHEAD* pmhead=NULL;
    char* reusemem;
    void* memstart;
    THREAD* threadinfo;
    Size size;

    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    memstart=(void*)threadinfo->memstart;
    reusemem=(char*)memstart+ThreadReuseMemStart;
    size=MEM_PROC_SIZE-ThreadReuseMemStart;
    memset(reusemem,0,size);

    pmhead=(PMHEAD*)memstart;
    pmhead->freeoffset=ThreadReuseMemStart;
}

void FreeMem(void)
{
    free(MemStart);
}
