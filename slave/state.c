#include<stdlib.h>
#include "state.h"
#include "socket.h"
#include "trans.h"
#include "config.h"

TransactionStateData* DatabaseState;
ServerData* serverdata;

void InitDatabaseState(void)
{
    int size = (NODENUM*THREADNUM)*sizeof(TransactionStateData);

    DatabaseState = (TransactionStateData*)malloc(size);
    if (DatabaseState == NULL)
    {
        printf("atebase state malloc error.\n");
        return;
    }
    TransactionStateData* p;
    int i;
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr,PTHREAD_PROCESS_PRIVATE);
    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_PRIVATE);
    for (i = 0, p = DatabaseState; i < NODENUM*THREADNUM; i++, p++)
    {
        p->state = inactive;
        p->tid = InvalidTransactionId;
        p->prepare_time = InvalidTimestamp;
        p->commit_time = InvalidTimestamp;
        p->snapshot_time = InvalidTimestamp;
        pthread_mutex_init(&(p->wait_lock), &mutexattr);
        pthread_spin_init(&(p->state_lock), PTHREAD_PROCESS_PRIVATE);
        pthread_cond_init(&(p->wait_commit), &condattr);
        pthread_cond_init(&(p->wait_committime), &condattr);
    }
}

void InitTransactionState(int index, TransactionId tid, TimeStampTz snapshot_time)
{
    TransactionStateData* state = DatabaseState + index;
    pthread_spin_lock(&(state->state_lock));
    state->commit_time = InvalidTimestamp;
    state->prepare_time = InvalidTimestamp;
    state->snapshot_time = snapshot_time;
    pthread_spin_unlock(&(state->state_lock));

    pthread_mutex_lock(&(state->wait_lock));
    state->tid = tid;
    state->state = active;
    pthread_mutex_unlock(&(state->wait_lock));

}

TransactionState getTransactionState(int index)
{
    TransactionState result;
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_mutex_lock(&(state->wait_lock));
    result = state->state;
    pthread_mutex_unlock(&(state->wait_lock));
    return result;
}

TimeStampTz getTransactionSnapshot(int index)
{
    TimeStampTz result;
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_spin_lock(&(state->state_lock));
    result = state->snapshot_time;
    pthread_spin_unlock(&(state->state_lock));
    return result;
}

TransactionId getTransactionTid(int index)
{
    TransactionId result;
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_mutex_lock(&(state->wait_lock));
    result = state->tid;
    pthread_mutex_unlock(&(state->wait_lock));
    return result;
}

TimeStampTz getTransactionCommitTime(int index)
{
    TimeStampTz result;
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_spin_lock(&(state->state_lock));
    result = state->commit_time;
    pthread_spin_unlock(&(state->state_lock));
    return result;
}

void setTransactionState(int index, TransactionState wstate)
{
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_mutex_lock(&(state->wait_lock));
    state->state = wstate;
    pthread_mutex_unlock(&(state->wait_lock));
}

void setTransactionPreparedTime(int index, TimeStampTz prepared_time)
{
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_spin_lock(&(state->state_lock));
    state->prepare_time = prepared_time;
    pthread_spin_unlock(&(state->state_lock));
}

void setTransactionCommitTime(int index, TimeStampTz commit_time)
{
    TransactionStateData* state;
    state = DatabaseState + index;
    pthread_spin_lock(&(state->state_lock));
    state->commit_time = commit_time;
    pthread_spin_unlock(&(state->state_lock));
}

void freeState(void)
{
    free(DatabaseState);
}
