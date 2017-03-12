/*
 * data_am.c
 *
 *  Created on: Dec 7, 2015
 *      Author: xiaoxin
 */

/*
 * interface for data access method.
 */

#include<pthread.h>
#include<assert.h>
#include<sys/socket.h>
#include<assert.h>
#include<stdlib.h>
#include"config.h"
#include"data_am.h"
#include"data_record.h"
#include"lock_record.h"
#include"thread_global.h"
#include"proc.h"
#include"trans.h"
#include"transactions.h"
#include"socket.h"
#include"communicate.h"

/*
 * @return: '0' to rollback, '1' to go head.
 */
int Data_Insert(int table_id, TupleId tuple_id, TupleId value, int nid)
{
    int index;
    int status;
    bool need_wait;
    bool first_visit;
    TransactionId tid;
    TimeStampTz snapshot_time;
    TransactionData* td;
    DataRecord datard;
    THREAD* threadinfo;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    td=(TransactionData*)pthread_getspecific(TransactionDataKey);
    index=threadinfo->index;

    /*
     * the node transaction process must to get the data from the storage process in the
     * node itself or in the other nodes, both use the socket to communicate.
     */
    need_wait = isNeedWait(nid);
    first_visit = isFirstVisitNode(nid);
    snapshot_time = td->snapshottime;
    tid = td->tid;
    int lindex;
    lindex = GetLocalIndex(index);

    sbuffer=send_buffer[lindex];
    rbuffer=recv_buffer[lindex];
    conn=connect_socket[nid][lindex];

    //send data-insert to node "nid".
    *(sbuffer) = cmd_insert;
    *(sbuffer+1) = table_id;
    *(sbuffer+2) = tuple_id;
    *(sbuffer+3) = need_wait;
    *(sbuffer+4) = snapshot_time;
    *(sbuffer+5) = value;
    *(sbuffer+6) = index;
    *(sbuffer+7) = tid;
    *(sbuffer+8) = first_visit;

    int num = 9;
    Send(conn, sbuffer, num);

    // response from "nid".
    num = 1;
    Receive(conn, rbuffer, num);

    status = *(rbuffer);

    if (status == 0)
        return 0;

    datard.node_id = nid;
    DataRecordInsert(&datard);

    return 1;
}

/*
 * @return:'0' for not found, '1' for success, '-1' for update-conflict-rollback.
 */
int Data_Update(int table_id, TupleId tuple_id, TupleId value, int nid)
{
    int index=0;
    DataRecord datard;
    TransactionId tid;
    bool need_wait;
    bool first_visit;
    bool is_delete = false;
    TimeStampTz snapshottime;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    int status;
    THREAD* threadinfo;
    TransactionData* td;
    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    td=(TransactionData*)pthread_getspecific(TransactionDataKey);
    index=threadinfo->index;

    snapshottime = td->snapshottime;
    tid = td->tid;
    need_wait = isNeedWait(nid);
    first_visit = isFirstVisitNode(nid);

    int lindex;
    lindex = GetLocalIndex(index);

    sbuffer=send_buffer[lindex];
    rbuffer=recv_buffer[lindex];
    conn=connect_socket[nid][lindex];

    //send data-insert to node "nid".
    *(sbuffer) = cmd_update;
    *(sbuffer+1) = table_id;
    *(sbuffer+2) = tuple_id;
    *(sbuffer+3) = need_wait;
    *(sbuffer+4) = snapshottime;
    *(sbuffer+5) = value;
    *(sbuffer+6) = index;
    *(sbuffer+7) = is_delete;
    *(sbuffer+8) = tid;
    *(sbuffer+9) = first_visit;

    int num = 10;
    Send(conn, sbuffer, num);

    // response from "nid".
    num = 1;
    Receive(conn, rbuffer, num);

    status = *(rbuffer);
    if (status == 0)
        return 0;

    /* record the updated data. */
    datard.node_id = nid;
    DataRecordInsert(&datard);
    return 1;
}

/*
 * @return:'0' for not found, '1' for success, '-1' for update-conflict-rollback.
 */

int Data_Delete(int table_id, TupleId tuple_id, int nid)
{
    int index=0;
    DataRecord datard;
    TransactionId tid;
    bool need_wait;
    bool first_visit;
    bool is_delete = true;
    uint64_t value = InvalidTupleId;
    TimeStampTz snapshottime;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    int status;
    THREAD* threadinfo;
    TransactionData* td;
    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    td=(TransactionData*)pthread_getspecific(TransactionDataKey);
    index=threadinfo->index;

    snapshottime = td->snapshottime;
    tid = td->tid;
    need_wait = isNeedWait(nid);
    first_visit = isFirstVisitNode(nid);

    int lindex;
    lindex = GetLocalIndex(index);

    sbuffer=send_buffer[lindex];
    rbuffer=recv_buffer[lindex];
    conn=connect_socket[nid][lindex];

    //send data-insert to node "nid".
    *(sbuffer) = cmd_update;
    *(sbuffer+1) = table_id;
    *(sbuffer+2) = tuple_id;
    *(sbuffer+3) = need_wait;
    *(sbuffer+4) = snapshottime;
    *(sbuffer+5) = value;
    *(sbuffer+6) = index;
    *(sbuffer+7) = is_delete;
    *(sbuffer+8) = tid;
    *(sbuffer+9) = first_visit;


    int num = 10;
    Send(conn, sbuffer, num);

    // response from "nid".
    num = 1;
    Receive(conn, rbuffer, num);

    status = *(rbuffer);
    if (status == 0)
        return 0;

    /* record the updated data. */
    datard.node_id = nid;
    DataRecordInsert(&datard);
    return 1;
}

/*
 * @input:'isupdate':true for reading before updating, false for commonly reading.
 * @return:NULL for read nothing, to rollback or just let it go.
 */
TupleId Data_Read(int table_id, TupleId tuple_id, int nid, int* flag)
{
    int index;
    bool need_wait = false;
    bool first_visit;
    TransactionId tid;
    TimeStampTz snapshot_time;
    uint64_t value;
    THREAD* threadinfo;
    TransactionData* td;

    uint64_t* sbuffer;
    uint64_t* rbuffer;
    int conn;

    *flag=1;

    /* get the pointer to current thread information. */
    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    td = (TransactionData*)pthread_getspecific(TransactionDataKey);
    index=threadinfo->index;
    int lindex;
    lindex = GetLocalIndex(index);

    need_wait = isNeedWait(nid);
    first_visit = isFirstVisitNode(nid);
    snapshot_time = td->snapshottime;
    tid = td->tid;

    sbuffer=send_buffer[lindex];
    rbuffer=recv_buffer[lindex];
    conn=connect_socket[nid][lindex];

    //send data-insert to node "nid".
    *(sbuffer) = cmd_read;
    *(sbuffer+1) = table_id;
    *(sbuffer+2) = tuple_id;
    *(sbuffer+3) = need_wait;
    *(sbuffer+4) = snapshot_time;
    *(sbuffer+5) = index;
    *(sbuffer+6) = tid;
    *(sbuffer+7) = first_visit;


    int num = 8;
    Send(conn, sbuffer, num);

    // response from "nid".
    num = 2;
    Receive(conn, rbuffer, num);

    *flag = *(rbuffer);
    value = *(rbuffer+1);


    if (*flag != 1)
    {
        return 0;
    }

    else
    {
        return value;
    }

    return 0;
}

bool IsCurrentTransaction(TransactionId tid, TransactionId cur_tid)
{
    return tid == cur_tid;
}

void PrintTable(int table_id)
{
    int i = 3,j,k;
    THash HashTable;
    Record* rd;
    char filename[10];

    FILE* fp;

    memset(filename,'\0',sizeof(filename));

    filename[0]=(char)(table_id+'0');
    filename[1]=(char)('+');
    filename[2]=(char)(nodeid+'0');
    strcat(filename, ".txt");

    if((fp=fopen(filename,"w"))==NULL)
    {
        printf("file open error\n");
        exit(-1);
    }

    HashTable=TableList[i];
    for(j=0;j<RecordNum[i];j++)
    {
        rd=&HashTable[j];
        fprintf(fp,"%d: %ld",j,rd->tupleid);
        for(k=0;k<VERSIONMAX;k++)
            fprintf(fp,"(%2d %ld %ld %2d)",rd->VersionList[k].tid,rd->VersionList[k].committime,rd->VersionList[k].value,rd->VersionList[k].deleted);
        fprintf(fp,"\n");
    }
    printf("\n");
}
