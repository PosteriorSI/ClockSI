/*
 * data_record.h
 *
 *  Created on: Dec 7, 2015
 *      Author: xiaoxin
 */

#ifndef DATA_RECORD_H_
#define DATA_RECORD_H_

#include "type.h"
#include "timestamp.h"
#include "state.h"
#define DataNumSize sizeof(int)

struct DataRecord
{
    int node_id;
};
typedef struct DataRecord DataRecord;

extern void InitDataMem(void);

extern void InitDataMemAlloc(void);

extern void DataRecordInsert(DataRecord* datard);

extern Size DataMemSize(void);

extern void Prepare(int nid, bool* is_abort, TimeStampTz* prepare_time);

extern void CommitDataRecord();

extern void AbortDataRecord();

extern bool isFirstVisitNode(int node_id);

extern void DataRecordSort(DataRecord* dr, int num);

#endif /* DATA_RECORD_H_ */
