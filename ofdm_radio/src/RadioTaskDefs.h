/* RadioTaskDefs.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef RADIOTASKDEFS_H_
#define RADIOTASKDEFS_H_

#ifdef __cplusplus
extern "C" {
#endif


enum RfRxTaskType {
    RF_TASK_RX_IDLE,
    RF_TASK_RX_RETUNE_ONLY,
    RF_TASK_RX_DATA,
    RF_TASK_RX_OFDMA_DATA,
    RF_TASK_RX_MC_DATA,
    RF_TASK_RX_HEARTBEAT,
    RF_TASK_RX_SNAPSHOT
};

enum RfTxTaskType {
    RF_TASK_TX_IDLE,
    RF_TASK_TX_RETUNE_ONLY,
    RF_TASK_TX_DATA,
    RF_TASK_TX_OFDMA_DATA,
    RF_TASK_TX_MC_DATA,
    RF_TASK_TX_HEARTBEAT,
    RF_TASK_TX_NOISE
};

typedef struct {
    RfRxTaskType    rx_task;
    double          rx_time;
    RfTxTaskType    tx_task;
    double          tx_time;
} rf_task_t;


#ifdef __cplusplus
}
#endif  //  extern "C"


#endif  /* RADIOTASKDEFS_H_ */ 
