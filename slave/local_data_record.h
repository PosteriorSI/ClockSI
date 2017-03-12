#ifndef LOCAL_DATA_RECORD_
#define LOCAL_DATA_RECORD_

#include "type.h"
#define MaxDataRecordNum 80
/*
 * type definitions for data update.
 */
typedef enum UpdateType
{
    //data insert
    DataInsert,
    //data update
    DataUpdate,
    //data delete
    DataDelete
} UpdateType;

typedef struct LocalDataRecord
{
    UpdateType type;

    int table_id;
    TupleId tuple_id;

    //other information attributes.
    TupleId value;

    //index in the table.
    uint64_t index;
} LocalDataRecord;

extern void InitServerData(void);

extern void LocalDataRecordInsert(LocalDataRecord* datard, int index);

extern TupleId IsDataRecordVisible(int index, int table_id, TupleId tuple_id);

extern void LocalCommitDataRecord(int index, TimeStampTz ctime);

extern void LocalAbortDataRecord(int index, int trulynum);

extern void LocalDataRecordSort(LocalDataRecord* dr, int num);

extern void ResetServerdata(int index);

extern void freeLocalDataRecord(void);

#endif
