/*
 * trans.c
 *
 *  Created on: Nov 10, 2015
 *      Author: xiaoxin
 */
/*
 * transaction actions are defined here.
 */
#include<malloc.h>
#include<sys/time.h>
#include<stdlib.h>
#include<assert.h>
#include<sys/socket.h>
#include"trans.h"
#include"thread_global.h"
#include"data_record.h"
#include"lock_record.h"
#include"mem.h"
#include"data_am.h"
#include"transactions.h"
#include"config.h"
#include"thread_main.h"
#include"socket.h"
#include"communicate.h"
#include"timestamp.h"

/* use for calculate the number of abort and commit transactions */
int commit_number;
int abort_number;

pthread_spinlock_t add_lock;

void add_commit(void)
{
    pthread_spin_lock(&add_lock);
    commit_number++;
    pthread_spin_unlock(&add_lock);
}

void add_abort(void)
{
    pthread_spin_lock(&add_lock);
    abort_number++;
    pthread_spin_unlock(&add_lock);
}

static IDMGR* CentIdMgr;

//the max number of transaction ID for per process.
int MaxTransId=100000;

TransactionId thread_0_tid;

void InitTransactionIdAssign(void)
{
    Size size;
    size=sizeof(IDMGR);

    CentIdMgr=(IDMGR*)malloc(size);

    if(CentIdMgr==NULL)
    {
        printf("malloc error for IdMgr.\n");
        return;
    }

    CentIdMgr->curid=nodeid*THREADNUM*MaxTransId + 1;

    pthread_mutex_init(&(CentIdMgr->IdLock), NULL);
}

void ProcTransactionIdAssign(THREAD* thread)
{
    int index;
    index=thread->index;

    thread->maxid=(index+1)*MaxTransId;
}

TransactionId AssignTransactionId(void)
{
    TransactionId tid;

    THREAD* threadinfo;

    threadinfo=pthread_getspecific(ThreadInfoKey);

    if(threadinfo->curid<=threadinfo->maxid)
        tid=threadinfo->curid++;
    else
        return 0;
    return tid;
}

void InitTransactionStructMemAlloc(void)
{
    TransactionData* td;
    THREAD* threadinfo;
    char* memstart;
    Size size;

    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    memstart=threadinfo->memstart;

    size=sizeof(TransactionData);

    td=(TransactionData*)MemAlloc((void*)memstart,size);

    if(td==NULL)
    {
        printf("memalloc error.\n");
        return;
    }

    pthread_setspecific(TransactionDataKey,td);

    // to set data memory.
    InitDataMemAlloc();

    // to set data-lock memory.
    //InitDataLockMemAlloc();
}

/*
 *start a transaction running environment, reset the
 *transaction's information for a new transaction.
 */
void StartTransaction(void)
{
    TransactionData* td;
    td = pthread_getspecific(TransactionDataKey);
    // to set data memory.
    InitDataMem();
    // to set data-lock memory.
    // InitDataLockMem();
    //assign transaction ID here.
    td->tid=AssignTransactionId();
    if(!TransactionIdIsValid(td->tid))
    {
        printf("transaction ID assign error.\n");
        return;
    }
    td->snapshottime = GetCurrentTimestamp();
    td->committime = InvalidTimestamp;
    //InitTransactionState(index, td->tid, td->snapshottime);
}

void CommitTransaction(void)
{
    CommitDataRecord();

    TransactionMemClean();
}

void AbortTransaction(void)
{
    AbortDataRecord();

    TransactionMemClean();
}

void LocalCommitTransaction(int index, TimeStampTz ctime)
{
    TransactionStateData* state;
    state = DatabaseState + index;
    LocalCommitDataRecord(index, ctime);
    // wake up the read transaction
    pthread_mutex_lock(&(state->wait_lock));
    state->state = committed;
    pthread_cond_broadcast(&(state->wait_commit));
    pthread_mutex_unlock(&(state->wait_lock));

    DataLockRelease(index);
    ResetServerdata(index);
}

void LocalAbortTransaction(int index, int trulynum)
{
    ServerData* data;
    data = serverdata + index;
    // in some status, transaction is failed before the truly manipulate the data record.
    if (data->lock_num == 0)
    {
        setTransactionState(index, aborted);
        ResetServerdata(index);
    }
    else
    {
        // need not broadcast the read transactions because now must be a transaction that not arrive the prepared state.
        LocalAbortDataRecord(index, trulynum);
        // wake up the read transaction
        setTransactionState(index, aborted);
        DataLockRelease(index);
        ResetServerdata(index);
    }

    /* test */
    // wake up the read transaction
    TransactionStateData* state;
    state = DatabaseState + index;

    pthread_mutex_lock(&(state->wait_lock));
    state->state = aborted;
    pthread_cond_broadcast(&(state->wait_committime));
    pthread_mutex_unlock(&(state->wait_lock));

    pthread_mutex_lock(&(state->wait_lock));
    state->state = aborted;
    pthread_cond_broadcast(&(state->wait_commit));
    pthread_mutex_unlock(&(state->wait_lock));
}

void finalLocalAbortTransaction(int index)
{
    TransactionStateData* state;
    state = DatabaseState + index;
    ServerData* data;
    data = serverdata + index;
    // in some status, transaction is failed before the truly manipulate the data record.
    if (data->lock_num == 0)
    {
        setTransactionState(index, aborted);
        ResetServerdata(index);
    }
    else
    {
        LocalAbortDataRecord(index, -1);
        // wake up the read transactions wait for the commit time because the read transaction have not seen the commit time now.
        pthread_mutex_lock(&(state->wait_lock));
        state->state = aborted;
        pthread_cond_broadcast(&(state->wait_committime));
        pthread_mutex_unlock(&(state->wait_lock));

        /* test */
        pthread_mutex_lock(&(state->wait_lock));
        state->state = aborted;
        pthread_cond_broadcast(&(state->wait_commit));
        pthread_mutex_unlock(&(state->wait_lock));

        DataLockRelease(index);
        ResetServerdata(index);
    }
}

void ReleaseConnect(void)
{
    int i;
    THREAD* threadinfo;
    int index2;

    uint64_t* sbuffer;
    int conn;

    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    index2=threadinfo->index;
    int lindex;
    lindex = GetLocalIndex(index2);

	sbuffer=send_buffer[lindex];

    for (i = 0; i < nodenum; i++)
    {
    	conn=connect_socket[i][lindex];
    	//send data-insert to node "nid".
        *(sbuffer) = cmd_release;
        int num = 1;
        Send(conn, sbuffer, num);
    }
}

void DataReleaseConnect(void)
{
    uint64_t* sbuffer;
    int conn;
	sbuffer=send_buffer[0];
	conn=connect_socket[nodeid][0];
    *(sbuffer) = cmd_release;
    int num = 1;
    Send(conn, sbuffer, num);
}

void TransactionRunSchedule(void* args)
{
    //to run transactions according to args.
    int type;
    int rv;
    terminalArgs* param=(terminalArgs*)args;
    type=param->type;

    THREAD* threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);

    if(type==0)
    {
        printf("begin LoadData......\n");
        //LoadData();

        //smallbank
        //LoadBankData();
        switch(benchmarkType)
        {
        case TPCC:
      	  LoadData();
      	  break;
        case SMALLBANK:
      	  LoadBankData();
      	  break;
        default:
      	  printf("benchmark not specified\n");
        }

        thread_0_tid=threadinfo->curid;
        DataReleaseConnect();
        ResetMem(0);
        ResetProc();
    }
    else
    {
        printf("ready to execute transactions...\n");

        rv=pthread_barrier_wait(&barrier);
        if(rv != 0 && rv != PTHREAD_BARRIER_SERIAL_THREAD)
        {
            printf("Couldn't wait on barrier\n");
            exit(-1);
        }


        printf("begin execute transactions...\n");
        //executeTransactions(transactionsPerTerminal, param->whse_id, param->dist_id, param->StateInfo);

        //smallbank
        //executeTransactionsBank(transactionsPerTerminal, param->StateInfo);
        switch(benchmarkType)
        {
        case TPCC:
      	  executeTransactions(transactionsPerTerminal, param->whse_id, param->dist_id, param->StateInfo);
      	  break;
        case SMALLBANK:
      	  executeTransactionsBank(transactionsPerTerminal, param->StateInfo);
      	  break;
        default:
      	  printf("benchmark not specified\n");
        }
        ReleaseConnect();
    }
}

void testLoadData(void)
{
    pthread_spin_init(&add_lock,0);
    abort_number = 0;
    commit_number = 0;
    int i;
    for (i = 0; i < 600; i++)
    {
        StartTransaction();
        Data_Insert(8, i+1, i*i, nodeid);
        int status = PreCommit();
        if (status == -1)
        {
            AbortTransaction();
        }
        else
        {
            CommitTransaction();
        }
    }
}

void test_Transaction(void* args)
{
    int i;
    run_args* param=(run_args*)args;
    if (param->i == 1)
    {
        for (i = 0; i < 500; i++)
        {
            StartTransaction();
            int result;
            int flag;
            result = Data_Read(8, i+4, (nodeid+1)%nodenum, &flag);
            Data_Update(8, 2, i*i, nodeid);
            Data_Update(8, 3, i*i+8, (nodeid+1)%nodenum);
            printf("read 1 result = %d, flag = %d\n", result, flag);
            int status = PreCommit();
            if (status == -1)
            {
               AbortTransaction();
               add_abort();
            }
            else
            {
               CommitTransaction();
               add_commit();
            }
        }
    }

    if (param->i == 2)
    {
        for (i = 0; i < 500; i++)
        {
            StartTransaction();
            int result;
            int flag;
            result = Data_Read(8, i+4, (nodeid+1)%nodenum, &flag);
            Data_Update(8, 2, i*i, nodeid);
            Data_Update(8, 3, i*i+8, (nodeid+1)%nodenum);
            printf("read 2 result = %d, flag = %d\n", result, flag);
            int status = PreCommit();
            if (status == -1)
            {
               AbortTransaction();
               add_abort();
            }
            else
            {
               CommitTransaction();
               add_commit();
            }
        }
    }

    if (param->i == 3)
    {
        for (i = 0; i < 500; i++)
        {
            StartTransaction();
            int result;
            int flag;
            result = Data_Read(8, i+4, (nodeid+1)%nodenum, &flag);
            Data_Update(8, 2, i*i, nodeid);
            Data_Update(8, 3, i*i+8, (nodeid+1)%nodenum);
            printf("read 3 result = %d, flag = %d\n", result, flag);
            int status = PreCommit();
            if (status == -1)
            {
               AbortTransaction();
               add_abort();
            }
            else
            {
               CommitTransaction();
               add_commit();
            }
        }
    }

    if (param->i == 4)
    {
        for (i = 0; i < 500; i++)
        {
            StartTransaction();
            int result;
            int flag;
            result = Data_Read(8, i+4, (nodeid+1)%nodenum, &flag);
            Data_Update(8, 2, i*i, nodeid);
            Data_Update(8, 3, i*i+8, (nodeid+1)%nodenum);
            printf("read 4 result = %d, flag = %d\n", result, flag);
            int status = PreCommit();
            if (status == -1)
            {
               AbortTransaction();
               add_abort();
            }
            else
            {
               CommitTransaction();
               add_commit();
            }
        }
    }
}

void testTransactionRunSchedule(void* args)
{
    //to run transactions according to args.
    int type;
    int rv;
    run_args* param=(run_args*)args;
    type=param->type;
    THREAD* threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);

    if(type==0)
    {
        printf("begin LoadData......\n");
        testLoadData();
        thread_0_tid=threadinfo->curid;
        DataReleaseConnect();
        ResetMem(0);
        ResetProc();
    }
    else
    {
        printf("ready to execute transactions...\n");

        rv=pthread_barrier_wait(&barrier);
        if(rv != 0 && rv != PTHREAD_BARRIER_SERIAL_THREAD)
        {
            printf("Couldn't wait on barrier\n");
            exit(-1);
        }

        printf("begin execute transactions...\n");
        test_Transaction(args);
        ReleaseConnect();
    }
}

void TransactionContextCommit(TransactionId tid, TimeStampTz ctime, int index)
{
    CommitDataRecord(tid,ctime);

    TransactionMemClean();
}

void TransactionContextAbort(TransactionId tid, int index)
{
    AbortDataRecord(tid, -1);

    TransactionMemClean();
}

/*
 *@return:'1' for success, '-1' for rollback.
 *@input:'index':record the truly done data-record's number when PreCommit failed.
 */
int LocalPreCommit(int* number, TimeStampTz* prepare_time, int index, bool is_local_transaction)
{
    LocalDataRecord*start;
    int num,i,result = 1;
    LocalDataRecord* ptr;

    TransactionStateData* state;
    state = DatabaseState + index;

    start=(serverdata+index)->datarecord;

    num=(serverdata+index)->data_num;

    // sort the data-operation records.
    LocalDataRecordSort(start, num);

    for(i=0;i<num;i++)
    {
        ptr=start+i;

        switch(ptr->type)
        {
            case DataInsert:
                result=TrulyDataInsert(ptr->table_id, ptr->index, ptr->tuple_id, ptr->value, index);
                break;
            case DataUpdate:
                result=TrulyDataUpdate(ptr->table_id, ptr->index, ptr->tuple_id, ptr->value, index);
                break;
            case DataDelete:
                result=TrulyDataDelete(ptr->table_id, ptr->index, ptr->tuple_id, index);
                break;
            default:
                printf("PreCommit:shouldn't arrive here.\n");
        }
        if(result == -1)
        {
            //return to roll back.
            *number=i;
            return -1;
        }
    }
    *prepare_time = GetCurrentTimestamp();
    if (is_local_transaction)
    {
        // must set the commit time first, because if other transactions
        // visit the state which is committing, it will get the commit time
        // first.

        // wake up the read transaction wait for the transaction time.
        setTransactionCommitTime(index, *prepare_time);
        pthread_mutex_lock(&(state->wait_lock));
        state->state = committing;
        pthread_cond_broadcast(&(state->wait_committime));
        pthread_mutex_unlock(&(state->wait_lock));
    }
    else
    {
        setTransactionPreparedTime(index, *prepare_time);
        setTransactionState(index, prepared);
    }
    return 1;
}

/*
 *@return:'1' for success, '-1' for rollback.
 *@input:'index':record the truly done data-record's number when PreCommit failed.
 */
int PreCommit(void)
{
    char* DataMemStart, *start;
    int num,i;
    DataRecord* ptr;
    TransactionData* td;
    DataMemStart=(char*)pthread_getspecific(DataMemKey);
    td=(TransactionData*)pthread_getspecific(TransactionDataKey);
    start=DataMemStart+DataNumSize;

    num=*(int*)DataMemStart;

    bool t_is_abort = false;
    TimeStampTz max_prepare_time = InvalidTimestamp;
    bool is_abort;
    TimeStampTz prepare_time;

    DataRecordSort((DataRecord*)start, num);

    for(i=0;i<num;i++)
    {
        ptr=(DataRecord*)(start+i*sizeof(DataRecord));
        Prepare(ptr->node_id, &is_abort, &prepare_time);
        if (is_abort == true)
        {
            t_is_abort = true;
        }
        if (prepare_time > max_prepare_time)
        {
            max_prepare_time = prepare_time;
        }
    }

    if (t_is_abort)
    {
        return -1;
    }
    else
    {
        td->committime = max_prepare_time;
        return 1;
    }
}

bool isLocalTransaction(void)
{
    char* start;
    char* DataMemStart;
    DataRecord* ptr;

    /* get the thread's pointer to data memory. */
    DataMemStart=(char*)pthread_getspecific(DataMemKey);

    int num;
    start=DataMemStart+DataNumSize;
    num=*(int*)DataMemStart;
    ptr=(DataRecord*)start;

    if (num == 1 && ptr->node_id == nodeid)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool isNeedWait(int node_id)
{
   if (node_id == nodeid)
       return false;
   else if (isFirstVisitNode(node_id))
       return true;
   else
       return false;
}

int GetGlobalIndexByTid(TransactionId tid)
{
    int index;
    index = (tid-1) / MaxTransId;
    return index;
}

int GetNodeId(int index)
{
   return (index/threadnum);
}

int GetLocalIndex(int index)
{
    return (index%threadnum);
}

void test_report(void)
{
    printf("transaction commit = %d\n", commit_number);
    printf("transaction abort = %d\n", abort_number);
}
