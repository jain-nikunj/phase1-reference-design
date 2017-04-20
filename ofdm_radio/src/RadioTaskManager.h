/* RadioTaskManager.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef RADIOTASKMANAGER_H_
#define RADIOTASKMANAGER_H_

#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "FhSeqGenerator.h"
#include "FreqTableGenerator.h"
#include "Phy2Mac.h"
#include "RadioHardwareConfig.h"
#include "RadioScheduler.h"
#include "RadioTaskDefs.h"
#include "timer.h"


// Task-specific structures ----------------------------------------------
typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  rf_freq;
    double                  dsp_freq;
} rx_manual_tune_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  rf_freq;
    double                  dsp_freq;
} tx_manual_tune_task_t; 

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
} tune2normal_freq_task_t; 

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  start_time;
    size_t                  rx_recommended_sample_size;
} rx_frame_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  start_time;
    size_t                  rx_recommended_sample_size;
} rx_heartbeat_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  start_time;
    unsigned char*          frame_header_ptr;
    unsigned int            frame_payload_size;
    unsigned char*          frame_payload_ptr;
} tx_frame_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    unsigned char*          frame_header_ptr;
    unsigned int            frame_payload_size;
    unsigned char*          frame_payload_ptr;
} tx_ofdma_frame_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    unsigned char*          frame_header_ptr;
    unsigned int            frame_payload_size;
    unsigned char*          frame_payload_ptr;
} tx_mc_frame_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  start_time;
    unsigned char*          frame_header_ptr;
    unsigned int            frame_payload_size;
    unsigned char*          frame_payload_ptr;
} tx_heartbeat_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  start_time;
    size_t                  rx_recommended_sample_size;
} rx_snapshot_burst_task_t;

typedef struct {
    unsigned int            task_id;
    pthread_t               thread_id;
    int                     thread_creation_status;
    void*                   thread_status;
    int                     thread_return_value;
    RadioHardwareConfig*    rhc_ptr;
    double                  start_time;
} tx_noise_burst_task_t;


typedef struct {
    //Fields needed to call rx runner threads for U4
    double                  run_time;
    RadioHardwareConfig*    rhc_ptr;
    timer_s*		    timer;
} rx_thread_args_t;

/*typedef struct {
    //Fields needed by callback to log data and write back to network
    RadioHardwareConfig*    rhc_ptr;
    PacketStore*            ps_ptr;
} callback_userdata_t;*/
//------------------------------------------------------------------------


// Helper functions ------------------------------------------------------
// Task-specific functions for threaded execution 
void* doRxManualTuneTask(void* thread_args);
void* doTxManualTuneTask(void* thread_args);
void* doTune2NormalFreqTask(void* thread_args);
void* doRxFrameBurstTask(void* thread_args);
void* doTxFrameBurstTask(void* thread_args);
void* doTxOFDMAFrameBurstTask(void* thread_args);
void* doTxMCFrameBurstTask(void* thread_args);
void* doRxHeartbeatBurstTask(void* thread_args); 
void* doTxHeartbeatBurstTask(void* thread_args); 
void* doRxSnapshotBurstTask(void* thread_args);
void* doTxNoiseBurstTask(void* thread_args);
void* run_ofdma_rx(void* thread_args);
void* run_mc_rx(void* thread_args);
//////////////////////////////////////////////////////////////////////////


class RadioTaskManager
{
public:
    RadioTaskManager(
        FhSeqGenerator*         fsg_ptr,
        FreqTableGenerator*     ftg_ptr,
        RadioHardwareConfig*    rhc_ptr,
        RadioScheduler*         rs_ptr,
        Phy2Mac*                p2m_ptr,
        bool debug, bool u4
        );
    ~RadioTaskManager();
    int doTask(
        bool waveform_is_normal,
        unsigned int task_id
        );
    int printTaskSched(
        bool node_is_basestation,
        bool waveform_is_normal,
        unsigned int begin_task_id,
        unsigned int end_task_id
        );
    
private:   
    // Copy of constructor parameters 
    FhSeqGenerator*             fsg_ptr;
    FreqTableGenerator*         ftg_ptr;
    RadioHardwareConfig*        rhc_ptr;
    RadioScheduler*             rs_ptr;
    Phy2Mac*                    p2m_ptr;
    bool                        debug;
    bool                        u4;
    
    // Parameters that typically are stable for the session
    size_t                      rx_recommended_sample_size;

    // Task-specific structures
    rx_manual_tune_task_t       rx_manual_tune_task;
    tx_manual_tune_task_t       tx_manual_tune_task;
    tune2normal_freq_task_t     tune2normal_freq_task;
    rx_frame_burst_task_t       rx_frame_burst_task;
    tx_frame_burst_task_t       tx_frame_burst_task;
    rx_heartbeat_burst_task_t   rx_heartbeat_burst_task;
    tx_heartbeat_burst_task_t   tx_heartbeat_burst_task;
    rx_snapshot_burst_task_t    rx_snapshot_burst_task;
    tx_noise_burst_task_t       tx_noise_burst_task;
    tx_ofdma_frame_burst_task_t tx_ofdma_frame_burst_task;
    tx_mc_frame_burst_task_t    tx_mc_frame_burst_task;
    pthread_attr_t pthread_attr;
};


#endif // RADIOTASKMANAGER_H_
