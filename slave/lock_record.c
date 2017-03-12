/*
 * lock_record.c
 *
 *  Created on: Nov 23, 2015
 *      Author: xiaoxin
 */

/*
 * interface to manage locks during transaction running which can be unlocked
 * only once transaction committing, such as data-update-lock .
 */
#include<stdbool.h>
#include<stdint.h>
#include"config.h"
#include"trans.h"
#include"communicate.h"
#include"lock_record.h"
#include"mem.h"
#include"thread_global.h"
#include"state.h"
#include"data.h"

int LockHash(int table_id, TupleId tuple_id);

int DataLockInsert(DataLock* lock, int index)
{
    DataLock* lockptr;
    DataLock* start;
    int step;
    int table_id;
    int tuple_id;
    int flag=0;
    int search=0;

    start = (serverdata+index)->lockrecord;

    table_id=lock->table_id;
    tuple_id=lock->tuple_id;

    step=LockHash(table_id,tuple_id);
    lockptr=start+step;
    search+=1;

    while(lockptr->tuple_id > 0)
    {
        if(search > MaxDataLockNum)
        {
            /* there is no free space. */
            flag=2;
            break;
        }
        if(lockptr->table_id==lock->table_id && lockptr->tuple_id==lock->tuple_id)
        {
            /* the lock already exists. */
            flag=1;
            break;
        }
        step=(step+1)%MaxDataLockNum;
        lockptr=start+step;
        search++;
    }

    if(flag==0)
    {
        /* succeed in finding free space, so insert it. */
        lockptr->table_id=lock->table_id;
        lockptr->tuple_id=lock->tuple_id;
        lockptr->lockmode=lock->lockmode;
        lockptr->index=lock->index;
        ((serverdata+index)->lock_num)++;
        return 1;
    }

    else if(flag==1)
    {
        /* already exists. */
        return -1;
    }
    else
    {
        /* no more free space. */
        printf("no more free space for lock.\n");
        return 0;
    }
}

int LockHash(int table_id, TupleId tuple_id)
{
    return (((table_id*10)%MaxDataLockNum+tuple_id%10)%MaxDataLockNum);
}

void DataLockRelease(int index)
{
    int step;
    DataLock* lockptr;
    DataLock* start;

    start = (serverdata+index)->lockrecord;

    lockptr=start;

    /* release all locks that current transaction holds. */
    for(step=0;step<MaxDataLockNum;step++)
    {
        lockptr=start+step;
        /* wait to change. */
        if(lockptr->tuple_id > 0)
        {
            pthread_rwlock_unlock(&(RecordLock[lockptr->table_id][lockptr->index]));
        }
    }
}

/*
 * Is the lock on data (table_id,tuple_id) already exist.
 * @return:'0' for false, '1' for true.
 */
int IsDataLockExist(int table_id, TupleId tuple_id, LockMode mode, int index)
{
    int step,count,flag;
    DataLock* lockptr;
    DataLock* start;

    start = (serverdata+index)->lockrecord;
    step=LockHash(table_id,tuple_id);
    lockptr=start+step;

    count=0;
    flag=0;
    while(lockptr->tuple_id > 0 && count<MaxDataLockNum)
    {
        if(lockptr->table_id==table_id && lockptr->tuple_id==tuple_id && lockptr->lockmode==mode)
        {
            flag=1;
            break;
        }
        step=(index+1)%MaxDataLockNum;
        lockptr=start+step;
        count++;
    }

    return flag;
}
/*
 * Is the write-lock on data (table_id,tuple_id) being hold by current transaction.
 * @return:'1' for true,'0' for false.
 */
int IsWrLockHolding(uint32_t table_id, TupleId tuple_id, int index)
{
    if(IsDataLockExist(table_id,tuple_id,LOCK_EXCLUSIVE,index))
        return 1;
    return 0;
}

/*
 * Is the read-lock on data (table_id,tuple_id) being hold by current transaction.
 * @return:'1' for true,'0' for false.
 */
int IsRdLockHolding(uint32_t table_id, TupleId tuple_id, int index)
{
    if(IsDataLockExist(table_id,tuple_id,LOCK_SHARED,index))
        return 1;
    return 0;
}
