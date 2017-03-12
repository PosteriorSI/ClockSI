/*
 * lock_record.h
 *
 *  Created on: Nov 23, 2015
 *      Author: xiaoxin
 */

#ifndef LOCK_RECORD_H_
#define LOCK_RECORD_H_

#include"type.h"

typedef enum LockMode
{
    LOCK_SHARED,
    LOCK_EXCLUSIVE
}LockMode;

struct DataLock
{
    uint32_t table_id;
    TupleId tuple_id;
    LockMode lockmode;

    uint64_t index;
};

typedef struct DataLock DataLock;

extern int DataLockInsert(DataLock* lock, int index);

extern void DataLockRelease(int index);

extern int IsWrLockHolding(uint32_t table_id, TupleId tuple_id, int index);

extern int IsRdLockHolding(uint32_t table_id, TupleId tuple_id, int index);

extern int IsDataLockExist(int table_id, TupleId tuple_id, LockMode mode, int index);
#endif /* LOCK_RECORD_H_ */
