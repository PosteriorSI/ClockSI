/*
 * data.h
 *
 *  Created on: Jan 29, 2016
 *      Author: Yu
 */

#ifndef DATA_H_
#define DATA_H_
#include<stdbool.h>
#include"type.h"
#include"timestamp.h"

//#define TABLENUM  9

//smallbank
//#define TABLENUM 3
#define VERSIONMAX 20

#define InvalidTupleId (TupleId)(0)

/* add by yu the data structure for the tuple */

/* Version is used for store a version of a record */
typedef struct {
    TransactionId tid;
    TimeStampTz preparetime;
    TimeStampTz committime;
    bool deleted;
    /* to stock other information of each version. */
    TupleId value;
} Version;

/*  Record is a multi-version tuple structure */
typedef struct {
    TupleId tupleid;
    int rear;
    int front;
    int lcommit;
    Version VersionList[VERSIONMAX];
} Record;

/* THash is pointer to a hash table for every table */
typedef Record * THash;

typedef int VersionId;

/* the lock in the tuple is used to verify the atomic operation of transaction */
extern pthread_rwlock_t** RecordLock;

/* just use to verify the atomic operation of a short-time */
extern pthread_spinlock_t** RecordLatch;

/* every table will have a separated HashTable */
extern Record** TableList;

extern int* BucketNum;
extern int* BucketSize;

extern uint64_t* RecordNum;

extern int TABLENUM;

extern bool MVCCVisible(Record * r, VersionId v);

extern void ProcessInsert(uint64_t * recv_buffer, int conn, int sindex);

extern void ProcessTrulyInsert(uint64_t * recv_buffer, int conn, int sindex);

extern void ProcessUpdate(uint64_t * recv_buffer, int conn, int sindex);

extern void ProcessRead(uint64_t * recv_buffer, int conn, int sindex);

extern void ProcessPrepare(uint64_t * recv_buffer, int conn, int sindex);

extern void ProcessCommit(uint64_t * recv_buffer, int conn, int sindex);

extern void ProcessAbort(uint64_t * recv_buffer, int conn, int sindex);

extern int TrulyDataInsert(int table_id, int h, TupleId tuple_id, TupleId value, int index);

extern int TrulyDataUpdate(int table_id, int h, TupleId tuple_id, TupleId value, int index);

extern int TrulyDataDelete(int table_id, int h, TupleId tuple_id, int index);

#endif
