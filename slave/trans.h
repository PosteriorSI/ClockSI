/*
 * trans.h
 *
 *  Created on: Nov 10, 2015
 *      Author: xiaoxin
 */

#ifndef TRANS_H_
#define TRANS_H_

#include "type.h"
#include "proc.h"
#include "timestamp.h"

#define InvalidTransactionId ((TransactionId)0)

struct TransIdMgr
{
    TransactionId curid;
    TransactionId maxid;
    pthread_mutex_t IdLock;
};

typedef struct TransIdMgr IDMGR;

extern int commit_number;
extern int abort_number;

struct TransactionData
{
    TransactionId tid;

    TimeStampTz snapshottime;

    TimeStampTz committime;
};

typedef struct TransactionData TransactionData;

extern TransactionId thread_0_tid;

extern int MaxTransId;

#define TransactionIdIsValid(tid) (tid != InvalidTransactionId)

extern void InitTransactionStructMemAlloc(void);

extern void ProcTransactionIdAssign(THREAD* thread);

extern void TransactionLoadData(int i);

extern void TransactionRunSchedule(void* args);

extern void testTransactionRunSchedule(void* args);

extern void TransactionContextCommit(TransactionId tid, TimeStampTz ctime, int index);

extern void TransactionContextAbort(TransactionId tid, int index);

extern void StartTransaction(void);

extern void CommitTransaction(void);

extern void AbortTransaction(void);

extern void LocalCommitTransaction(int index, TimeStampTz ctime);

extern void LocalAbortTransaction(int index, int trulynum);

extern void finalLocalAbortTransaction(int index);

extern int LocalPreCommit(int* number, TimeStampTz* prepare_time, int index, bool is_local_transaction);

extern int PreCommit(void);

extern int GetNodeId(int index);

extern int GetGlobalIndexByTid(TransactionId tid);

extern bool isLocalTransaction(void);

extern bool isNeedWait(int node_id);

extern int GetLocalIndex(int index);

extern void test_report(void);

#endif /* TRANS_H_ */
