#ifndef MASTER_H_
#define MASTER_H_

#define LINEMAX 100

#define LISTEN_QUEUE 500

#define NODENUM nodenum
#define THREADNUM threadnum

typedef enum BENCHMARK
{
    TPCC,
    SMALLBANK
} BENCHMARK;

extern void InitMessage(void);
extern void InitParam(void);
extern void InitRecord(void);
extern void InitNetworkParam(void);

extern int oneNodeWeight;
extern int twoNodeWeight;
extern int redo_limit;

//hotspot control
extern int HOTSPOT_PERCENTAGE;
extern int HOTSPOT_FIXED_SIZE;

//duration control
extern int extension_limit;

//random read control
extern int random_read_limit;

extern int time_skew_max;
extern int nodenum;
extern int threadnum;
extern int client_port;
extern int message_port;
extern int param_port;
extern int record_port;
extern char master_ip[20];

#endif
