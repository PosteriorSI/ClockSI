#include <malloc.h>
#include "state.h"
#include "timestamp.h"
#include "local_data_record.h"
#include "data.h"
#include "trans.h"
#include "config.h"
#include "socket.h"
/*
 * insert a data-update-record.
 */

void InitServerData(void)
{
   serverdata = (ServerData*)malloc(NODENUM*THREADNUM*sizeof(ServerData));
   if (serverdata == NULL)
   {
       printf("server data malloc error\n");
   }
   ServerData* p;
   int i;
   for (i = 0, p = serverdata; i < NODENUM*THREADNUM; i++, p++)
   {
       p->datarecord = (LocalDataRecord*)malloc(MaxDataRecordNum*sizeof(LocalDataRecord));
       p->lockrecord = (DataLock*)malloc(MaxDataLockNum*sizeof(DataLock));
       if (p->datarecord == NULL || p->lockrecord == NULL)
       {
           printf("local data record or local data lock malloc error\n");
       }
   }
}

void CommitInsertData(int table_id, int index, TimeStampTz ctime)
{
    THash HashTable=TableList[table_id];
    Record * r = &(HashTable[index]);
    pthread_spin_lock(&RecordLatch[table_id][index]);
    r->lcommit = (r->lcommit + 1) % VERSIONMAX;
    r->VersionList[r->lcommit].committime = ctime;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
}

void CommitUpdateData(int table_id, int index, TimeStampTz ctime)
{
    THash HashTable=TableList[table_id];
    Record *r = &HashTable[index];

    pthread_spin_lock(&RecordLatch[table_id][index]);
    r->lcommit = (r->lcommit + 1) % VERSIONMAX;
    r->VersionList[r->lcommit].committime = ctime;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
}

void CommitDeleteData(int table_id, int index, TimeStampTz ctime)
{
    THash HashTable=TableList[table_id];
    Record *r = &HashTable[index];

    pthread_spin_lock(&RecordLatch[table_id][index]);
    r->lcommit = (r->lcommit + 1) % VERSIONMAX;
    r->VersionList[r->lcommit].committime = ctime;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
}

void AbortInsertData(int table_id, int index)
{
    VersionId newest;
    THash HashTable=TableList[table_id];
    Record *r = &HashTable[index];

    pthread_spin_lock(&RecordLatch[table_id][index]);
    newest = (r->rear + VERSIONMAX -1) % VERSIONMAX;
    r->tupleid=InvalidTupleId;
    r->rear=0;
    r->front=0;
    r->lcommit=-1;
    r->VersionList[newest].tid = InvalidTransactionId;
    r->VersionList[newest].value = 0;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
}

void AbortUpdateData(int table_id, int index)
{
    VersionId newest;
    THash HashTable=TableList[table_id];
    Record *r = &HashTable[index];
    pthread_spin_lock(&RecordLatch[table_id][index]);
    newest = (r->rear + VERSIONMAX -1) % VERSIONMAX;
    r->VersionList[newest].tid = InvalidTransactionId;
    r->VersionList[newest].value = 0;
    r->VersionList[newest].preparetime = InvalidTimestamp;
    r->rear = newest;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
}

void AbortDeleteData(int table_id, int index)
{
    VersionId newest;
    THash HashTable=TableList[table_id];
    Record *r = &HashTable[index];
    pthread_spin_lock(&RecordLatch[table_id][index]);
    newest = (r->rear + VERSIONMAX -1) % VERSIONMAX;
    r->VersionList[newest].tid = InvalidTransactionId;
    r->VersionList[newest].deleted = false;
    r->VersionList[newest].preparetime = InvalidTimestamp;
    r->rear = newest;
    pthread_spin_unlock(&RecordLatch[table_id][index]);
}

/*
 * write the commit time of current transaction to every updated tuple.
 */
void LocalCommitDataRecord(int index, TimeStampTz ctime)
{
    int num;
    int i;

    LocalDataRecord* start;
    LocalDataRecord* ptr;
    int table_id;
    uint64_t h;

    start=(serverdata+index)->datarecord;
    num=(serverdata+index)->data_num;

    /* deal with data-update record one by one. */
    for(i=0;i<num;i++)
    {
        ptr=start+i;
        table_id=ptr->table_id;
        h=ptr->index;
        switch(ptr->type)
        {
            case DataInsert:
                CommitInsertData(table_id, h, ctime);
                break;
            case DataUpdate:
                CommitUpdateData(table_id, h, ctime);
                break;
            case DataDelete:
                CommitDeleteData(table_id, h, ctime);
                break;
            default:
                printf("shouldn't arrive here.\n");
        }
    }
}

/*
 * rollback all updated tuples by current transaction.
 */
void LocalAbortDataRecord(int index, int trulynum)
{
    int num;
    int i;
    LocalDataRecord* start;
    LocalDataRecord* ptr;
    int table_id;
    uint64_t h;

    start=(serverdata+index)->datarecord;

    /* transaction abort in function CommitTransaction. */
    if(trulynum==-1)
        num=(serverdata+index)->data_num;
    /* transaction abort in function AbortTransaction. */
    else
        num=trulynum;

    for(i=0;i<num;i++)
    {
        ptr=start+i;
        table_id=ptr->table_id;
        h=ptr->index;
        switch(ptr->type)
        {
        case DataInsert:
            AbortInsertData(table_id, h);
            break;
        case DataUpdate:
            AbortUpdateData(table_id, h);
            break;
        case DataDelete:
            AbortDeleteData(table_id, h);
            break;
        default:
            printf("shouldn't get here.\n");
        }
    }
}

void LocalDataRecordInsert(LocalDataRecord* datard, int index)
{
    int num;
    LocalDataRecord* start;
    LocalDataRecord* ptr;

    /* get the thread's pointer to data memory. */
    start=(serverdata+index)->datarecord;

    num=(serverdata+index)->data_num;

    if(num+1 > MaxDataRecordNum)
    {
        printf("local data record memory out of space. PID: %lu\n",pthread_self());
        return;
    }

    (serverdata+index)->data_num = num+1;

    /* start address for record to insert. */
    start=start+num;

    /* insert the data record here. */
    ptr=start;
    ptr->type=datard->type;

    ptr->table_id=datard->table_id;
    ptr->tuple_id=datard->tuple_id;

    ptr->value=datard->value;

    ptr->index=datard->index;
}

/*
 * sort the transaction's data-record to avoid dead lock between different
 * update transactions.
 * @input:'dr':the start address of data-record, 'num': number of data-record.
 */
void LocalDataRecordSort(LocalDataRecord* dr, int num)
{
    /* sort according to the table_id and tuple_id and node_id */
    LocalDataRecord* ptr1, *ptr2;
    LocalDataRecord* startptr=dr;
    LocalDataRecord temp;
    int i,j;

    for(i=0;i<num-1;i++)
        for(j=0;j<num-i-1;j++)
        {
            ptr1=startptr+j;
            ptr2=startptr+j+1;
            if((ptr1->table_id > ptr2->table_id) || ((ptr1->table_id == ptr2->table_id) && (ptr1->tuple_id > ptr2->tuple_id)))
            {
                temp.table_id=ptr1->table_id;
                temp.tuple_id=ptr1->tuple_id;
                temp.type=ptr1->type;
                temp.value=ptr1->value;
                temp.index=ptr1->index;

                ptr1->table_id=ptr2->table_id;
                ptr1->tuple_id=ptr2->tuple_id;
                ptr1->type=ptr2->type;
                ptr1->value=ptr2->value;
                ptr1->index=ptr2->index;

                ptr2->table_id=temp.table_id;
                ptr2->tuple_id=temp.tuple_id;
                ptr2->type=temp.type;
                ptr2->value=temp.value;
                ptr2->index=temp.index;
            }
        }
}

/*
 * @return:'1' for visible inner own transaction, '-1' for invisible inner own transaction,
 * '0' for first access the tuple inner own transaction.
 */
TupleId IsDataRecordVisible(int index, int table_id, TupleId tuple_id)
{
    int num,i;
    LocalDataRecord* start=(serverdata+index)->datarecord;
    num=(serverdata+index)->data_num;
    LocalDataRecord* ptr;

    for(i=num-1;i>=0;i--)
    {
        ptr=start + i;
        if(ptr->tuple_id == tuple_id && ptr->table_id == table_id)
        {
            if(ptr->type == DataDelete)
            {
                /* the tuple has been deleted by current transaction. */
                return -1;
            }
            else
            {
                return ptr->value;
            }
        }
    }

    /* first access the tuple. */
    return 0;
}

void ResetServerdata(int index)
{
    ServerData* data;
    data = serverdata + index;
    int size_data;
    int size_lock;
    data->data_num = 0;
    data->lock_num = 0;
    size_data = MaxDataRecordNum*sizeof(LocalDataRecord);
    size_lock = MaxDataLockNum*sizeof(DataLock);
    memset(data->datarecord, 0, size_data);
    memset(data->lockrecord, 0, size_lock);
}

void freeLocalDataRecord(void)
{
    ServerData* p;
    int i;
    for (i = 0, p = serverdata; i < NODENUM*THREADNUM; i++, p++)
    {
        free(p->datarecord);
        free(p->lockrecord);
    }

    free(serverdata);
}
