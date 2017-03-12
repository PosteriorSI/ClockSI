/*
 * data.c
 *
 *  Created on: Jan 29, 2016
 *      Author: Yu
 */

#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include "config.h"
#include "communicate.h"
#include "data.h"
#include "config.h"
#include "transactions.h"
#include "trans.h"
#include "thread_global.h"
#include "state.h"
#include "local_data_record.h"
#include "lock_record.h"
#include "socket.h"

static bool IsInsertDone(int table_id, int index);

static void PrimeBucketSize(void);

static void ReadPrimeTable(void);

/* initialize the record hash table and the record lock table, latch table. */

pthread_rwlock_t * RecordLock[TABLENUM];
pthread_spinlock_t * RecordLatch[TABLENUM];
Record* TableList[TABLENUM];

int BucketNum[TABLENUM];
int BucketSize[TABLENUM];
int RecordNum[TABLENUM];

int Prime[150000];
int PrimeNum;

static bool IsUpdateConflict(Record * r, int index);
static void waitIfNeeds(TimeStampTz starttime);

// if a transaction visit other node first time, and the start time is less than the current time
// it should wait until the current time.
void waitIfNeeds(TimeStampTz snapshottime)
{
    TimeStampTz now = GetCurrentTimestamp();
    if (snapshottime > now) {
        TimeStampTz wait = snapshottime - now;
        usleep(wait);
    }
}

/* to see whether the version is a deleted version. */
bool IsMVCCDeleted(Record * r, VersionId v)
{
   if(r->VersionList[v].deleted == true)
      return true;
   else
      return false;
}

void waitCommit(int index, int windex, TransactionId wtid)
{
    TransactionId tid;
    TimeStampTz snapshot_time;
    snapshot_time = getTransactionSnapshot(index);
    TransactionStateData* state = DatabaseState + windex;
    pthread_mutex_lock(&(state->wait_lock));
    struct timespec timer;
    //prevent wait too long
    timer.tv_sec=time(NULL);
    timer.tv_nsec=100000000;
    tid = state->tid;
    if (state->state == committed || tid != wtid || state->state == aborted) {
        pthread_mutex_unlock(&(state->wait_lock));
        return ;
    }
    else if ((state->state == committing)) {
        if (snapshot_time <= state->commit_time) {
            pthread_mutex_unlock(&(state->wait_lock));
            return ;
        }
        else {
            pthread_cond_timedwait(&(state->wait_commit), &(state->wait_lock), &timer);
            pthread_mutex_unlock(&(state->wait_lock));
            return ;
        }
    }
    else if (state->state == prepared) {
        if (snapshot_time <= state->prepare_time) {
            pthread_mutex_unlock(&(state->wait_lock));
            return ;
        }
        else {
            pthread_cond_timedwait(&(state->wait_committime), &(state->wait_lock), &timer);
            if (state->state == aborted) {
                pthread_mutex_unlock(&(state->wait_lock));
                return ;
            }
            else {
                if (snapshot_time <= state->commit_time) {
                    pthread_mutex_unlock(&(state->wait_lock));
                    return ;
                }
                else {
                    pthread_cond_timedwait(&(state->wait_commit), &(state->wait_lock), &timer);
                    pthread_mutex_unlock(&(state->wait_lock));
                    return ;
                }
            }
        }
    }
    else {
        pthread_mutex_unlock(&(state->wait_lock));
        return ;
    }
}

/*
 * @return:'true' for visible, 'false' for invisible.
 */
bool ReadMVCCVisible(Record * r, VersionId v, int index)
{
    TimeStampTz snapshot_time;
    snapshot_time = getTransactionSnapshot(index);
    if (snapshot_time > r->VersionList[v].committime)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/* to see whether the transaction can update the data. return true to update, false to abort. */
bool IsUpdateConflict(Record * r, int index)
{
    TransactionId tid;
    TimeStampTz snapshottime;
    tid = getTransactionTid(index);
    snapshottime = getTransactionSnapshot(index);
    /* self already updated the data, note that rear is not the newest version. */
    VersionId newest;

    newest = (r->rear + VERSIONMAX -1) % VERSIONMAX;
    /*
    printf("tid = %u, lcommit_tid = %u, lcommit = %d, newest = %d, snapshottime = %ld, lcommittime = %ld, isdelete = %d", tid, r->VersionList[newest].tid,r->lcommit, \
           newest, snapshottime, r->VersionList[newest].committime, r->VersionList[newest].deleted );
    */
    if(r->lcommit != newest)
    {
        assert(r->VersionList[newest].tid == tid);
        /* self already  deleted. */
        if(IsMVCCDeleted(r, newest))
            return false;
        /* self already updated. */
        else
            return true;
    }
    /* self first update the data. */
    else
    {
        /* update permission only when the lcommit version is visible and is not a deleted version. */
        if((snapshottime > r->VersionList[newest].committime) && !IsMVCCDeleted(r, r->lcommit))
            return true;
        else
            return false;
    }
}

/* some functions used for manage the circular queue. */
void InitQueue(Record * r)
{
   int i;
   assert(r != NULL);
   r->tupleid = InvalidTupleId;
   r->rear = 0;
   r->front = 0;
   /* lcommit is means the last version id that commit, its initialized id should be -1 to represent the nothing position */
   r->lcommit = -1;
   for (i = 0; i < VERSIONMAX; i++)
   {
      r->VersionList[i].tid = 0;
      r->VersionList[i].committime = InvalidTimestamp;
      r->VersionList[i].deleted = false;
      r->VersionList[i].value=0;
   }
}

bool isFullQueue(Record * r)
{
   if ((r->rear + 1) % VERSIONMAX == r->front)
      return true;
   else
      return false;
}

bool isEmptyQueue(Record * r)
{
    if(r->lcommit == -1)
        return true;
    else
        return false;
}

void EnQueue(Record * r, TransactionId tid, TupleId value)
{
   if(isFullQueue(r))
   {
       printf("EnQueue failed, %d %d %d\n",r->front,r->rear,r->lcommit);
       exit(-1);
   }
   r->VersionList[r->rear].tid = tid;
   r->VersionList[r->rear].value=value;

   r->rear = (r->rear + 1) % VERSIONMAX;
}

void InitBucketNum_Size(void)
{
    int bucketNums;

    /*
    // bucket num. 
    BucketNum[Warehouse_ID]=1;
    BucketNum[Item_ID]=1;
    BucketNum[Stock_ID]=configWhseCount;
    BucketNum[District_ID]=configWhseCount;
    BucketNum[Customer_ID]=configWhseCount*configDistPerWhse;
    BucketNum[History_ID]=configWhseCount*configDistPerWhse;
    BucketNum[Order_ID]=configWhseCount*configDistPerWhse;
    BucketNum[NewOrder_ID]=configWhseCount*configDistPerWhse;
    BucketNum[OrderLine_ID]=configWhseCount*configDistPerWhse;
    // bucket size. 
    BucketSize[Warehouse_ID]=configWhseCount;
    BucketSize[Item_ID]=configUniqueItems;
    BucketSize[Stock_ID]=configUniqueItems;
    BucketSize[District_ID]=configDistPerWhse;
    BucketSize[Customer_ID]=configCustPerDist;
    BucketSize[History_ID]=configCustPerDist;
    BucketSize[Order_ID]=OrderMaxNum;
    BucketSize[NewOrder_ID]=OrderMaxNum;
    BucketSize[OrderLine_ID]=OrderMaxNum*10;
    */
    //smallbank
    bucketNums=configNumAccounts/configAccountsPerBucket + (((configNumAccounts%configAccountsPerBucket)==0)?0:1);
    BucketNum[Accounts_ID]=bucketNums;
    BucketNum[Savings_ID]=bucketNums;
    BucketNum[Checking_ID]=bucketNums;

    BucketSize[Accounts_ID]=configAccountsPerBucket;
    BucketSize[Savings_ID]=configAccountsPerBucket;
    BucketSize[Checking_ID]=configAccountsPerBucket;

    /* adapt the bucket-size to prime. */
    ReadPrimeTable();
    PrimeBucketSize();
}

void InitRecordNum(void)
{
    int i;

    for(i=0;i<TABLENUM;i++)
        RecordNum[i]=BucketNum[i]*BucketSize[i];
}

void InitRecordMem(void)
{
    int i;

    for(i=0;i<TABLENUM;i++)
    {
        TableList[i]=(Record*)malloc(sizeof(Record)*RecordNum[i]);
        if(TableList[i]==NULL)
        {
            printf("record memory allocation failed for table %d.\n",i);
            exit(-1);
        }
    }
}

void InitLatchMem(void)
{
    int i;

    for(i=0;i<TABLENUM;i++)
    {
        RecordLock[i]=(pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t)*RecordNum[i]);
        RecordLatch[i]=(pthread_spinlock_t*)malloc(sizeof(pthread_spinlock_t)*RecordNum[i]);
        if(RecordLock[i]==NULL || RecordLatch[i]==NULL)
        {
            printf("memory allocation failed for record-latch %d.\n",i);
            exit(-1);
        }
    }
}

/* initialize the record hash table and the related lock*/
void InitRecord(void)
{
    InitBucketNum_Size();

    InitRecordNum();

    InitRecordMem();

    InitLatchMem();

    int i;
    uint64_t j;
    for (i = 0; i < TABLENUM; i++)
    {
       for (j = 0; j < RecordNum[i]; j++)
       {
          InitQueue(&TableList[i][j]);
       }
    }
    for (i = 0; i < TABLENUM; i++)
    {
       for (j = 0; j < RecordNum[i]; j++)
       {
             pthread_rwlock_init(&(RecordLock[i][j]), NULL);
          pthread_spin_init(&(RecordLatch[i][j]), PTHREAD_PROCESS_PRIVATE);
       }
    }
}

int Hash(int table_id, TupleId r, int k)
{
    uint64_t num;
    num=RecordNum[table_id];
    if(num-1 > 0)
        return (int)((TupleId)(r + (TupleId)k * (1 + (TupleId)(((r >> 5) +1) % (num - 1)))) % num);
    else
        return 0;
}

int LimitHash(int table_id, TupleId r, int k, int min_max)
{
    int num;
    num=RecordNum[table_id];
    if(min_max-1 > 0)
        return ((r%min_max + k * (1 + (((r>>5) +1) % (min_max - 1)))) % min_max);
    else
        return 0;
}

/* the function RecordFind is used to find a position of a particular tuple id in the HashTable. */
int BasicRecordFind(int tableid, TupleId r)
{
   int k = 0;
   int h = 0;
   uint64_t num=RecordNum[tableid];

   assert(TableList != NULL);
   THash HashTable = TableList[tableid];
   do
   {
       h = Hash(tableid, r, k);
       if (HashTable[h].tupleid == r)
          return h;
       else
          k++;
   } while (k < num);
   printf("Basic:can not find record id %ld in the table:%d! \n", r, tableid);
   return -1;
}

int LimitRecordFind(int table_id, TupleId r)
{
   int k = 0;
   int h = 0;
   int w_id, d_id, o_id, bucket_id, min, max, c_id;
   int offset=-1;

   int bucket_size=BucketSize[table_id];

   switch(table_id)
   {
   case Accounts_ID:
   case Savings_ID:
   case Checking_ID:
       bucket_id=(r-1)/configAccountsPerBucket;
       break;
   default:
       printf("table_ID error %d\n", table_id);

   /*
   case Order_ID:
   case NewOrder_ID:
        w_id=(int)((r/ORDER_ID)%WHSE_ID);
        d_id=(int)((r/(ORDER_ID*WHSE_ID))%DIST_ID);
        bucket_id=(w_id-1)*10+(d_id-1);

        offset=(int)(r%ORDER_ID);
        break;
   case OrderLine_ID:
        w_id=(int)((r/ORDER_ID)%WHSE_ID);
        d_id=(int)((r/(ORDER_ID*WHSE_ID))%DIST_ID);
        bucket_id=(w_id-1)*10+(d_id-1);
        break;
   case Customer_ID:
   case History_ID:
        w_id=(int)((r/CUST_ID)%WHSE_ID);
        d_id=(int)((r/(CUST_ID*WHSE_ID))%DIST_ID);
        bucket_id=(w_id-1)*10+(d_id-1);

        offset=(int)(r%CUST_ID);
           break;
   case District_ID:
        w_id=(int)(r%WHSE_ID);
        bucket_id=w_id-1;

        offset=(int)((r/WHSE_ID)%DIST_ID);
           break;
   case Stock_ID:
           w_id=(int)((r/ITEM_ID)%WHSE_ID);
           bucket_id=w_id-1;

        offset=(int)(r%ITEM_ID);
           break;
   case Item_ID:
           bucket_id=0;

           offset=(int)r;
           break;
   case Warehouse_ID:
           bucket_id=0;

           offset=(int)r;
           break;
   default:
           printf("table_ID error %d\n", table_id);
           */
   }

   min=bucket_size*bucket_id;
   max=min+bucket_size;
   assert(TableList != NULL);
   THash HashTable = TableList[table_id];
   do
   {
       h = min+LimitHash(table_id, r, k, bucket_size);
       if (HashTable[h].tupleid == r)
          return h;
       else
          k++;
   } while (k < bucket_size);
   printf("Limit:can not find record id %ld in the table:%d, bucketsize=%d! \n", r, table_id, bucket_size);
   return -1;
}

int RecordFind(int table_id, TupleId r)
{
    return LimitRecordFind(table_id, r);
    //return BasicRecordFind(table_id, r);
}

/*
 * the function RecordFind is used to find a position of a particular tuple id in the HashTable for insert.
 *@return:'h' for success, '-2' for already exists, '-1' for not success(already full)
 */
int BasicRecordFindHole(int tableid, TupleId r, int* flag)
{
   int k = 0;
   int h = 0;
   uint64_t num=RecordNum[tableid];

   assert(TableList != NULL);
   THash HashTable = TableList[tableid];
   do
   {
       h = Hash(tableid, r, k);
       /* find a empty record space. */
       if(__sync_bool_compare_and_swap(&HashTable[h].tupleid,InvalidTupleId,r))
       {
           /* to make sure that this place by 'h' is empty. */
           assert(isEmptyQueue(&HashTable[h]));
           *flag=0;
           return h;
       }
       /* to compare whether the two tuple_id are equal. */
       else if(HashTable[h].tupleid==r)
       {
             printf("the data by %ld is already exist.\n",r);
             *flag=1;
             return h;
       }
       /* to search the next record place. */
       else
           k++;
   } while (k < num);
   printf("can not find a space for insert record %ld %d!\n", r, num);
   *flag=-2;
   return -2;
}

int LimitRecordFindHole(int table_id, TupleId r, int *flag)
{
    int w_id, d_id, o_id, bucket_id, min, max;
    int bucket_size=BucketSize[table_id];
    int k = 0;
    int h = 0;

    int offset=-1;
    TransactionData *tdata;
    bool success;

    assert(TableList != NULL);
    THash HashTable = TableList[table_id];
    switch(table_id)
    {
    case Accounts_ID:
    case Savings_ID:
    case Checking_ID:
        bucket_id=(r-1)/configAccountsPerBucket;
        break;
    default:
        printf("table_ID error %d\n", table_id);
    /*
    case Order_ID:
    case NewOrder_ID:
        w_id=(int)((r/ORDER_ID)%WHSE_ID);
        d_id=(int)((r/(ORDER_ID*WHSE_ID))%DIST_ID);
        bucket_id=(w_id-1)*10+(d_id-1);

        offset=(int)(r%ORDER_ID);
        break;
    case OrderLine_ID:
        w_id=(int)((r/ORDER_ID)%WHSE_ID);
        d_id=(int)((r/(ORDER_ID*WHSE_ID))%DIST_ID);
        bucket_id=(w_id-1)*10+(d_id-1);
        break;
    case Customer_ID:
    case History_ID:
        w_id=(int)((r/CUST_ID)%WHSE_ID);
        d_id=(int)((r/(CUST_ID*WHSE_ID))%DIST_ID);
        bucket_id=(w_id-1)*10+(d_id-1);

        offset=(int)(r%CUST_ID);
        break;
    case District_ID:
        w_id=(int)(r%WHSE_ID);
        bucket_id=w_id-1;

        offset=(int)((r/WHSE_ID)%DIST_ID);
        break;
    case Stock_ID:
        w_id=(int)((r/ITEM_ID)%WHSE_ID);
        bucket_id=w_id-1;

        offset=(int)(r%ITEM_ID);
        break;
    case Item_ID:
        bucket_id=0;

        offset=(int)r;
        break;
    case Warehouse_ID:
        bucket_id=0;

        offset=(int)r;
        break;
    default:
        printf("table_ID error %d\n", table_id);
        */
    }

    min=bucket_size*bucket_id;
    max=min+bucket_size;

    do
    {
        h = min+LimitHash(table_id, r, k, bucket_size);

        pthread_spin_lock(&RecordLatch[table_id][h]);

        if(HashTable[h].tupleid == InvalidTupleId)
        {

            if(!isEmptyQueue(&HashTable[h]))
            {
                exit(-1);
            }
            if(r == InvalidTupleId)
            {
                printf("r is InvalidTupleId: table_id=%d, tuple_id=%ld\n",table_id, r);
                exit(-1);
            }

            HashTable[h].tupleid=r;
            pthread_spin_unlock(&RecordLatch[table_id][h]);
            success=true;
        }
        else
        {
            pthread_spin_unlock(&RecordLatch[table_id][h]);
            success=false;
        }

        if(success == true)
        {
            *flag=0;
            return h;
        }
        /* to compare whether the two tuple_id are equal. */
        else if(HashTable[h].tupleid==r)
        {
            *flag=1;
            return h;
        }
        /* to search the next record place. */
        else
           k++;
    } while (k < bucket_size);
    *flag=-2;
    return -2;
}

int RecordFindHole(int table_id, TupleId r, int *flag)
{
    return LimitRecordFindHole(table_id, r, flag);
    //return BasicRecordFindHole(table_id, r, flag);
}

void ProcessInsert(uint64_t * recv_buffer, int conn, int sindex)
{
    int h;
    int status = 1;
    int flag;
    int table_id;
    bool need_wait;
    bool first_visit;
    int index;
    TransactionId tid;
    uint64_t value;
    LocalDataRecord datard;
    TimeStampTz snapshot_time;
    uint64_t tuple_id;
    table_id = (uint32_t) recv_buffer[1];
    tuple_id = recv_buffer[2];
    need_wait = recv_buffer[3];
    snapshot_time = recv_buffer[4];
    value = recv_buffer[5];
    index = recv_buffer[6];
    tid = recv_buffer[7];
    first_visit = recv_buffer[8];
    h = RecordFindHole(table_id, tuple_id, &flag);

    if (need_wait) {
        waitIfNeeds(snapshot_time);
    }

    if (first_visit) {
        InitTransactionState(index, tid, snapshot_time);
    }

    if(flag==-2)
    {
        /* no space for new tuple to insert. */
        printf("Data_insert: flag==-1.\n");
        printf("no space for table_id:%d, tuple_id:%ld\n",table_id, tuple_id);
        exit(-1);
        status = 0;
    }

    else if(flag==1 && IsInsertDone(table_id, h))
    {
        status = 0;
    }

    else
    {
        datard.type=DataInsert;
        datard.table_id=table_id;
        datard.tuple_id=tuple_id;
        datard.value=value;
        datard.index=h;
        LocalDataRecordInsert(&datard, index);
    }

    if (status == 0)
    {
        LocalAbortTransaction(index, -1);
    }

    uint64_t* sbuffer = ssend_buffer[sindex];

    *(sbuffer) = status;
    int num = 1;
    SSend(conn, sbuffer, num);
}


void ProcessUpdate(uint64_t * recv_buffer, int conn, int sindex)
{
   int table_id;
   int index;
   LocalDataRecord datard;
   TransactionId tid;
   bool need_wait;
   bool first_visit;
   bool is_delete;
   uint64_t value;
   int h;
   int status = 1;
   uint64_t tuple_id;
   TimeStampTz snapshot_time;

   table_id = recv_buffer[1];
   tuple_id = recv_buffer[2];
   need_wait = recv_buffer[3];
   snapshot_time = recv_buffer[4];
   value = recv_buffer[5];
   index = recv_buffer[6];
   is_delete = recv_buffer[7];
   tid = recv_buffer[8];
   first_visit = recv_buffer[9];
   if (need_wait) {
      waitIfNeeds(snapshot_time);
   }

   if (first_visit) {
      InitTransactionState(index, tid, snapshot_time);
   }

   h = RecordFind(table_id, tuple_id);
   /* not found. */
   if (h < 0)
   {
      /* abort transaction outside the function. */
      status = 0;
   }
   else
   {
       datard.type=(is_delete ? DataDelete : DataUpdate);
       datard.table_id=table_id;
       datard.tuple_id=tuple_id;
       datard.value=value;
       datard.index=h;
       LocalDataRecordInsert(&datard, index);
   }
   if (status == 0)
   {
       LocalAbortTransaction(index, -1);
   }

   uint64_t* sbuffer = ssend_buffer[sindex];

   *(sbuffer) = status;
   int num = 1;
   SSend(conn, sbuffer, num);
}

void ProcessRead(uint64_t * recv_buffer, int conn, int sindex)
{
    int table_id;
    int h;
    int flag = 1;
    uint64_t value;
    uint64_t visible;
    TransactionId tid;
    TransactionId wtid;
    int index;
    int windex;
    bool need_wait;
    bool first_visit;
    TimeStampTz snapshot_time;
    uint64_t tuple_id;
    table_id = recv_buffer[1];
    tuple_id = recv_buffer[2];
    need_wait = recv_buffer[3];
    snapshot_time = recv_buffer[4];
    index = recv_buffer[5];
    tid = recv_buffer[6];
    first_visit = recv_buffer[7];

    if (need_wait) {
       waitIfNeeds(snapshot_time);
    }

    if (first_visit) {
       InitTransactionState(index, tid, snapshot_time);
    }
    /* Test if the tuple has updated or deleted by the transaction itself */
    visible=IsDataRecordVisible(index, table_id, tuple_id);

    if(visible == -1)
    {
        /* current transaction has deleted the tuple to read, so return to rollback. */
        flag=-1;
    }

    else if(visible > 0)
    {
        /* see own transaction's update. */
        value = visible;
    }

    else
    {
        h = RecordFind(table_id, tuple_id);
        /* not found. */
        if (h < 0) {
        /* abort transaction outside the function. */
           flag = 0;
        }

        int i;
        THash HashTable = TableList[table_id];
        pthread_spin_lock(&RecordLatch[table_id][h]);
        VersionId newest;
        newest = (HashTable[h].rear + VERSIONMAX -1) % VERSIONMAX;
        /* the read version should be a committed version */
        if (newest == HashTable[h].lcommit)
        {
            pthread_spin_unlock(&RecordLatch[table_id][h]);
        }
        else
        {
            /* the read transaction may be wait for the transaction until it commit, get the tid if the write transaction */
            wtid = HashTable[h].VersionList[newest].tid;
            pthread_spin_unlock(&RecordLatch[table_id][h]);
            windex = GetGlobalIndexByTid(wtid);
            waitCommit(index, windex, wtid);
        }
        pthread_spin_lock(&RecordLatch[table_id][h]);

        if(HashTable[h].lcommit >= 0)
        {
            for (i = HashTable[h].lcommit; i != (HashTable[h].front + VERSIONMAX - 1) % VERSIONMAX; i = (i + VERSIONMAX - 1) % VERSIONMAX)
            {
                if (ReadMVCCVisible(&(HashTable[h]), i, index))
                {
                    if(IsMVCCDeleted(&HashTable[h],i))
                    {
                        pthread_spin_unlock(&RecordLatch[table_id][h]);
                        flag = -2;
                        break;
                    }
                    else
                    {
                        pthread_spin_unlock(&RecordLatch[table_id][h]);
                        value = HashTable[h].VersionList[i].value;
                        break;
                    }
                }
            }
        }

        if (i == (HashTable[h].front + VERSIONMAX - 1) % VERSIONMAX)
        {
            flag = -3;
            pthread_spin_unlock(&RecordLatch[table_id][h]);
        }
    }

    if (flag != 1)
    {
        LocalAbortTransaction(index, -1);
    }

    uint64_t* sbuffer = ssend_buffer[sindex];

    *(sbuffer) = flag;
    *(sbuffer+1) = value;
    int num = 2;
    SSend(conn, sbuffer, num);
}

void ProcessPrepare(uint64_t * recv_buffer, int conn, int sindex)
{
    int index;
    int number;
    int ret;
    TimeStampTz prepare_time = InvalidTimestamp;
    bool is_local_transaction;
    bool is_abort = false;
    index = recv_buffer[1];
    is_local_transaction = recv_buffer[2];
    ret = LocalPreCommit(&number, &prepare_time, index, is_local_transaction);
    if (ret == -1)
    {
        LocalAbortTransaction(index, number);
        is_abort = true;
    }

    uint64_t* sbuffer = ssend_buffer[sindex];

    *(sbuffer) = is_abort;
    *(sbuffer+1) = prepare_time;

    int num = 2;
    SSend(conn, sbuffer, num);
}

void ProcessCommit(uint64_t * recv_buffer, int conn, int sindex)
{
    bool is_local_transaction;
    TimeStampTz commit_time;
    int index;
    index = recv_buffer[1];
    is_local_transaction = recv_buffer[2];
    commit_time = recv_buffer[3];

    if (is_local_transaction)
    {
        commit_time = getTransactionCommitTime(index);
        LocalCommitTransaction(index, commit_time);
    }
    else
    {
        TransactionStateData* state;
        state = DatabaseState + index;
        // wake up the read transaction wait for the transaction time.
        setTransactionCommitTime(index, commit_time);
        pthread_mutex_lock(&(state->wait_lock));
        state->state = committing;
        pthread_cond_broadcast(&(state->wait_committime));
        pthread_mutex_unlock(&(state->wait_lock));
        LocalCommitTransaction(index, commit_time);
    }

    uint64_t* sbuffer = ssend_buffer[sindex];
    *(sbuffer) = 1;
    int num = 1;
    SSend(conn, sbuffer, num);
}

void ProcessAbort(uint64_t * recv_buffer, int conn, int sindex)
{
    int index;
    index = recv_buffer[1];
    TransactionState t_state = getTransactionState(index);
    if (t_state == aborted)
    {
        ;
    }
    else
    {
        finalLocalAbortTransaction(index);
    }

    uint64_t* sbuffer = ssend_buffer[sindex];
    *(sbuffer) = 1;
    int num = 1;
    SSend(conn, sbuffer, num);
}

/*
 * @return:'1' for success, '-1' for rollback.
 */
int TrulyDataInsert(int table_id, int h, TupleId tuple_id, TupleId value, int index)
{
    TransactionId tid;
    DataLock lockrd;

    tid = getTransactionTid(index);
    pthread_rwlock_wrlock(&(RecordLock[table_id][h]));

    if(IsInsertDone(table_id, h))
    {
        /* other transaction has inserted the tuple. */
        pthread_rwlock_unlock(&(RecordLock[table_id][h]));
        return -1;
    }
    else
    {
        THash HashTable=TableList[table_id];
        pthread_spin_lock(&RecordLatch[table_id][h]);
        HashTable[h].tupleid=tuple_id;
        EnQueue(&HashTable[h],tid, value);
        pthread_spin_unlock(&RecordLatch[table_id][h]);
    }

    /* record the lock. */
    lockrd.table_id=table_id;
    lockrd.tuple_id=tuple_id;
    lockrd.index = h;
    lockrd.lockmode=LOCK_EXCLUSIVE;
    DataLockInsert(&lockrd, index);

    return 1;
}

/*
 * @return:'1' for success, '-1' for rollback.
 */
int TrulyDataUpdate(int table_id, int h, TupleId tuple_id, TupleId value, int index)
{
    TransactionId tid;
    tid = getTransactionTid(index);
    int old;
    int i;
    bool firstadd=false;
    DataLock lockrd;
    /* to void repeatedly add lock. */
    if(!IsWrLockHolding(table_id,tuple_id,index))
    {
        /* the first time to hold the wr-lock on data (table_id,tuple_id). */
        firstadd=true;
    }

    THash HashTable=TableList[table_id];
    if (firstadd)
    {
        /* the first time to hold the wr-lock on data (table_id,tuple_id). */
        pthread_rwlock_wrlock(&(RecordLock[table_id][h]));
    }

    if(!IsUpdateConflict(&(HashTable[h]), index))
    {
        /* release the write-lock and return to roll back. */
        if(firstadd)
            pthread_rwlock_unlock(&(RecordLock[table_id][h]));
        return -1;
    }

    /* record the lock. */
    lockrd.table_id=table_id;
    lockrd.tuple_id=tuple_id;
    lockrd.index = h;
    lockrd.lockmode=LOCK_EXCLUSIVE;
    DataLockInsert(&lockrd, index);

    pthread_spin_lock(&RecordLatch[table_id][h]);
    assert(!isEmptyQueue(&HashTable[h]));
    EnQueue(&HashTable[h], tid, value);
    if (isFullQueue(&(HashTable[h])))
    {
       old = (HashTable[h].front +  VERSIONMAX/3) % VERSIONMAX;
       for (i = HashTable[h].front; i != old; i = (i+1) % VERSIONMAX)
       {
           HashTable[h].VersionList[i].committime = InvalidTimestamp;
           HashTable[h].VersionList[i].tid = InvalidTransactionId;
           HashTable[h].VersionList[i].deleted = false;
           HashTable[h].VersionList[i].value= 0;
       }
         HashTable[h].front = old;
    }
    pthread_spin_unlock(&RecordLatch[table_id][h]);
    return 1;
}

/*
 * @return:'1' for success, '-1' for rollback.
 */
int TrulyDataDelete(int table_id, int h, TupleId tuple_id, int index)
{
    TransactionId tid;
    tid = getTransactionTid(index);
    int old;
    int i;
    bool firstadd=false;
    DataLock lockrd;
    /* to void repeatedly add lock. */
    if(!IsWrLockHolding(table_id,tuple_id,index))
    {
        /* the first time to hold the wr-lock on data (table_id,tuple_id). */
        firstadd=true;
    }

    THash HashTable=TableList[table_id];
    if (firstadd)
    {
        /* the first time to hold the wr-lock on data (table_id,tuple_id). */
        pthread_rwlock_wrlock(&(RecordLock[table_id][h]));
    }

    if(!IsUpdateConflict(&(HashTable[h]), index))
    {
        /* release the write-lock and return to roll back. */
        if(firstadd)
            pthread_rwlock_unlock(&(RecordLock[table_id][h]));
        return -1;
    }

    /* record the lock. */
    lockrd.table_id=table_id;
    lockrd.tuple_id=tuple_id;
    lockrd.index = h;
    lockrd.lockmode=LOCK_EXCLUSIVE;
    DataLockInsert(&lockrd, index);

    pthread_spin_lock(&RecordLatch[table_id][h]);
    assert(!isEmptyQueue(&HashTable[h]));
    EnQueue(&HashTable[h], tid, 0);
    VersionId newest;
    newest = (HashTable[h].rear + VERSIONMAX -1) % VERSIONMAX;
    HashTable[h].VersionList[newest].deleted = true;
    if (isFullQueue(&(HashTable[h])))
    {
       old = (HashTable[h].front +  VERSIONMAX/3) % VERSIONMAX;
       for (i = HashTable[h].front; i != old; i = (i+1) % VERSIONMAX)
       {
           HashTable[h].VersionList[i].committime = InvalidTimestamp;
           HashTable[h].VersionList[i].tid = InvalidTransactionId;
           HashTable[h].VersionList[i].deleted = false;
           HashTable[h].VersionList[i].value= 0;
       }
         HashTable[h].front = old;
    }
    pthread_spin_unlock(&RecordLatch[table_id][h]);
    return 1;
}

void ReadPrimeTable(void)
{
    printf("begin read prime table\n");
    FILE* fp;
    int i, num;
    if((fp=fopen("prime.txt","r"))==NULL)
    {
        printf("file open error.\n");
        exit(-1);
    }
    i=0;
    while(fscanf(fp,"%d",&num) > 0)
    {
        Prime[i++]=num;
    }
    PrimeNum=i;
    fclose(fp);
}

void validation(int table_id)
{
    THash HashTable;
    uint64_t i;
    int count=0;

    HashTable=TableList[table_id];

    for(i=0;i<RecordNum[table_id];i++)
    {
        if(HashTable[i].tupleid == InvalidTupleId)
            count++;
    }
    printf("table: %d of %d rows are available.\n",count, RecordNum[table_id]);
}

/*
 * @return:'true' means the tuple in 'index' has been inserted, 'false' for else.
 */
bool IsInsertDone(int table_id, int index)
{
    THash HashTable = TableList[table_id];
    bool done;

    pthread_spin_lock(&RecordLatch[table_id][index]);
    if(HashTable[index].lcommit >= 0) done=true;
    else done=false;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
    return done;
}

void PrimeBucketSize(void)
{
    int i, j;
    i=0, j=0;
    for(i=0;i<TABLENUM;i++)
    {
        j=0;
        while(BucketSize[i] > Prime[j] && j < PrimeNum)
        {
            j++;
        }
        if(j < PrimeNum)
            BucketSize[i]=Prime[j];
        printf("BucketSize:%d , %d\n",i, BucketSize[i]);
    }
}
