/* RadioScheduler.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef RADIOSCHEDULER_H_
#define RADIOSCHEDULER_H_

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>

#include "HeartbeatDefs.h"
#include "RadioTaskDefs.h" 

// Defaults for intraslot, interslot, and radio net timing parameters
#define RS_TAU_GUARD_BEGIN_DEFAULT                      1.7150e-04
#define RS_TAU_GUARD_END_DEFAULT                        1.7150e-04
#define RS_TAU_FDD_ENDSLOT_MARGIN_DEFAULT               1.2e-3
#define RS_TAU_FH_ENDSLOT_MARGIN_DEFAULT                1.8e-3
#define RS_TAU_1PPS_GUARD_DEFAULT                       10.0e-3

class RadioScheduler
{
public:
    RadioScheduler(
        bool node_is_basestation,
        unsigned char node_id,
        unsigned int num_nodes_in_net,
        unsigned char* nodes_in_net,
        HeartbeatActivityType heartbeat_activity,
        unsigned int num_channels,
        double rx_rate_measured,
        double tx_rate_measured,
        unsigned int tx_burst_length,
        double uhd_retune_delay,
        bool debug, bool u4, bool uplink
    );
    ~RadioScheduler();

    size_t getRxRecommendedSampleSize();

    int calcCalibrationSchedule(
        double radio_clock_time
        );
    int calcFddSchedule(
        double radio_clock_time
        );
    int calcFhSchedule(
        double radio_clock_time
        );
    void setU4ScheduleSize(int new_size);
    int calcU4Schedule(
        );
    int calcTestSchedule(
        double radio_clock_time
        ); 
    RfTxTaskType getCurrentTxTask(
        bool use_normal_mode,
        unsigned int task_id
        );
    bool isHeartbeatExpected(
        bool use_normal_mode,
        unsigned int task_id
        );
    unsigned int getActiveSchedSize(
        bool use_normal_mode
        );
    bool isActiveSched4Basestation();
    rf_task_t* getFddTaskSched();
    unsigned int getFddTaskSchedSize();
    rf_task_t* getFhTaskSched();
    unsigned int getFhTaskSchedSize();
    rf_task_t* getU4TaskSched();
    unsigned int getU4TaskSchedSize();
    int printScheduleEntries(
            rf_task_t* task_sched,
            unsigned int begin_idx,
            unsigned int end_idx
         );
    
    double getFhInterdwellSpacing();
    int printTimingValues();
        
private:   
    // Copy of constructor parameters 
    bool node_is_basestation;
    unsigned char node_id;
    unsigned int num_nodes_in_net;
    unsigned char* nodes_in_net;
    HeartbeatActivityType heartbeat_activity;
    unsigned int num_channels;
    double rx_rate_measured;
    double tx_rate_measured;
    unsigned int tx_burst_length;
    double uhd_retune_delay;
    bool debug;
    bool u4;
    bool uplink;

    // Timing parameters 
    double tau_tune;
    double tau_tx_guard_begin;
    double tau_tx_burst;
    unsigned int num_rx_uhd_batches;
    double tau_rx_dwell;
    double tau_tx_guard_end;
    double tau_fdd_endslot_margin;
    double tau_fh_endslot_margin;
    double tau_1pps_guard;
    double tau_fdd_slot;
    double tau_fdd_net;
    unsigned int num_fdd_nets_persec;
    double tau_fdd_margin_persec;
    double tau_fh_slot;
    double tau_fh_net;
    unsigned int num_fh_nets_persec;
    double tau_fh_margin_persec;

    // Derived sample acquistion setting
    unsigned int rx_recommended_sample_size;
    
    // Task sequence parameters
    rf_task_t* fdd_task_sched;
    unsigned int fdd_task_sched_size;
    rf_task_t* fh_task_sched;
    unsigned int fh_task_sched_size;

    rf_task_t* u4_task_sched;
    unsigned int u4_task_sched_size;
}; 

#endif // RADIOSCHEDULER_H_
