/*
 * data_am.h
 *
 *  Created on: Dec 7, 2015
 *      Author: xiaoxin
 */

#ifndef DATA_AM_H_
#define DATA_AM_H_

#include<stdbool.h>
#include"type.h"
#include"timestamp.h"
#include"data.h"

extern int Data_Insert(int table_id, TupleId tuple_id, TupleId value, int nid);

extern int Data_Update(int table_id, TupleId tuple_id, TupleId value, int nid);

extern int Data_Delete(int table_id, TupleId tuple_id, int nid);

extern TupleId Data_Read(int table_id, TupleId tuple_id, int nid, int *flag);

extern void InitRecord(void);

extern void PrintTable(int table_id);

extern void validation(int table_id);

#endif /* DATA_AM_H_ */
