/* This file is some structure and manipulation about the state of the
 * current transaction
 * */
#ifndef STATE_H_
#define STATE_H_

#include <pthread.h>
#include "timestamp.h"
#include "local_data_record.h"
#include "lock_record.h"

typedef struct ServerData {
    int data_num;
    int lock_num;
    LocalDataRecord* datarecord;
    DataLock* lockrecord;
} ServerData;

typedef enum TransactionState {
    inactive,
    active,
    committing,
    prepared,
    committed,
    aborted
} TransactionState;

typedef struct TransactionStateData {
    TransactionState state;
    TransactionId tid;
    pthread_mutex_t wait_lock;
    pthread_spinlock_t state_lock;
    pthread_cond_t wait_committime;
    pthread_cond_t wait_commit;
    TimeStampTz prepare_time;
    TimeStampTz commit_time;
    TimeStampTz snapshot_time;
} TransactionStateData;

// record all the states of the current transactions, and store the states
// in the shared memory
extern TransactionStateData* DatabaseState;
extern ServerData* serverdata;
extern void InitDatabaseState(void);
extern void InitTransactionState(int index, TransactionId tid, TimeStampTz snapshot_time);
extern TransactionState getTransactionState(int index);
extern TimeStampTz getTransactionSnapshot(int index);
extern TransactionId getTransactionTid(int index);
extern TimeStampTz getTransactionCommitTime(int index);
extern void setTransactionState(int index, TransactionState wstate);
extern void setTransactionPreparedTime(int index, TimeStampTz prepared_time);
extern void setTransactionCommitTime(int index, TimeStampTz commit_time);

extern void freeState(void);

#endif
