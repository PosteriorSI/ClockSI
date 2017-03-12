/*
 * data_record.c
 *
 *  Created on: Dec 7, 2015
 *      Author: xiaoxin
 */
#include<assert.h>
#include"data_record.h"
#include"mem.h"
#include"thread_global.h"
#include"data_am.h"
#include"trans.h"
#include"communicate.h"


#define DataMemMaxSize 128*1024

/*
 * insert a data-update-record.
 */
void DataRecordInsert(DataRecord* datard)
{
    int num;
    char* start;
    char* DataMemStart;
    DataRecord* ptr;

    /* get the thread's pointer to data memory. */
    DataMemStart=(char*)pthread_getspecific(DataMemKey);

    if (!isFirstVisitNode(datard->node_id))
    {
        // the should be present just once for a node
        return;
    }
    else
    {
        start=(char*)((char*)DataMemStart+DataNumSize);
        num=*(int*)DataMemStart;

        if(DataNumSize+(num+1)*sizeof(DataRecord) > DataMemSize())
        {
            printf("data memory out of space. PID: %lu\n",pthread_self());
            return;
        }

        *(int*)DataMemStart=num+1;

        /* start address for record to insert. */
        start=start+num*sizeof(DataRecord);

        /* insert the data record here. */
        ptr=(DataRecord*)start;

        ptr->node_id = datard->node_id;
    }
}

Size DataMemSize(void)
{
    return DataMemMaxSize;
}

void InitDataMemAlloc(void)
{
    char* DataMemStart=NULL;
    char* memstart;
    THREAD* threadinfo;
    Size size;

    /* get start address of current thread's memory. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    memstart=threadinfo->memstart;

    size=DataMemSize();
    DataMemStart=(char*)MemAlloc((void*)memstart,size);

    if(DataMemStart==NULL)
    {
        printf("thread memory allocation error for data memory.PID:%lu\n",pthread_self());
        return;
    }

    /* set global variable. */
    pthread_setspecific(DataMemKey,DataMemStart);
}

void InitDataMem(void)
{
    char* DataMemStart=NULL;

    DataMemStart=(char*)pthread_getspecific(DataMemKey);

    memset(DataMemStart,0,DataMemSize());
}

void Prepare(int nid, bool* is_abort, TimeStampTz* prepare_time)
{
    int index;
    THREAD* threadinfo;
    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    index=threadinfo->index;
    bool is_local_transaction;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    int lindex;
    lindex = GetLocalIndex(index);

	sbuffer=send_buffer[lindex];
	rbuffer=recv_buffer[lindex];
	conn=connect_socket[nid][lindex];

    is_local_transaction = isLocalTransaction();

	//send data-insert to node "nid".
    *(sbuffer) = cmd_prepare;
    *(sbuffer+1) = index;
    *(sbuffer+2) = is_local_transaction;

    int num = 3;
    Send(conn, sbuffer, num);

    num = 2;
    Receive(conn, rbuffer, num);

    *is_abort = *(rbuffer);
    *prepare_time = *(rbuffer+1);
}
/*
 * write the commit time of current transaction to every updated tuple.
 */
void CommitDataRecord()
{
    int num;
    int i;
    char* start;
    DataRecord* ptr;
    int nid;
    int index;
    int local_index;
    bool is_local_transaction;
    TimeStampTz commit_time = InvalidTimestamp;
    char* DataMemStart=NULL;
    THREAD* threadinfo;
    TransactionData* td;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    DataMemStart=(char*)pthread_getspecific(DataMemKey);
    td=(TransactionData*)pthread_getspecific(TransactionDataKey);

    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    index=threadinfo->index;
    local_index = GetLocalIndex(index);

	sbuffer=send_buffer[local_index];
	rbuffer=recv_buffer[local_index];

    is_local_transaction = isLocalTransaction();
    start=DataMemStart+DataNumSize;
    num=*(int*)DataMemStart;

    /* local transaction just have only one visit node */
    if (!is_local_transaction)
    {
        /* this commit time is need just for cross transaction, because local transaction have
         * determined the commit time before commit.
         */
        commit_time = td->committime;
    }

    for(i=0;i<num;i++)
    {
        ptr=(DataRecord*)(start+i*sizeof(DataRecord));
        nid = ptr->node_id;

    	conn=connect_socket[nid][local_index];

    	//send data-insert to node "nid".
        *(sbuffer) = cmd_commit;
        *(sbuffer+1) = index;
        *(sbuffer+2) = is_local_transaction;
        *(sbuffer+4) = commit_time;

        int num = 4;
        Send(conn, sbuffer, num);

        num = 1;
        Receive(conn, rbuffer, num);
    }
}

/*
 * rollback all updated tuples by current transaction.
 */
void AbortDataRecord()
{
    int num;
    int i;
    char* start;
    DataRecord* ptr;
    int nid;
    int index;
    int local_index;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    char* DataMemStart=NULL;
    THREAD* threadinfo;
    DataMemStart=(char*)pthread_getspecific(DataMemKey);
    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    index=threadinfo->index;
    local_index = GetLocalIndex(index);

	sbuffer=send_buffer[local_index];
	rbuffer=recv_buffer[local_index];

    start=DataMemStart+DataNumSize;
    num=*(int*)DataMemStart;
    for(i=0;i<num;i++)
    {
        ptr=(DataRecord*)(start+i*sizeof(DataRecord));
        nid = ptr->node_id;

    	conn=connect_socket[nid][local_index];

    	//send data-insert to node "nid".
        *(sbuffer) = cmd_abort;
        *(sbuffer+1) = index;

        int num = 2;
        Send(conn, sbuffer, num);

        num = 1;
        Receive(conn, rbuffer, num);
    }
}

bool isFirstVisitNode(int node_id)
{
    int num,i;
    char* DataMemStart = NULL;
    DataMemStart = (char*)pthread_getspecific(DataMemKey);
    char* start=DataMemStart+DataNumSize;
    num=*(int*)DataMemStart;
    DataRecord* ptr;

    for(i=num-1;i>=0;i--)
    {
        ptr=(DataRecord*)(start+i*sizeof(DataRecord));
        if(ptr->node_id == node_id)
        {
           return false;
        }
    }
    // first access the tuple.
    return true;
}

/*
 * sort the transaction's data-record to avoid dead lock between different
 * update transactions.
 * @input:'dr':the start address of data-record, 'num': number of data-record.
 */
void DataRecordSort(DataRecord* dr, int num)
{
    /* sort according to the table_id and tuple_id and node_id */
    DataRecord* ptr1, *ptr2;
    DataRecord* startptr=dr;
    DataRecord temp;
    int i,j;

    for(i=0;i<num-1;i++)
        for(j=0;j<num-i-1;j++)
        {
            ptr1=startptr+j;
            ptr2=startptr+j+1;
            if(ptr1->node_id > ptr2->node_id)
            {
                temp.node_id = ptr1->node_id;

                ptr1->node_id=ptr2->node_id;

                ptr2->node_id=temp.node_id;
            }
        }
}
