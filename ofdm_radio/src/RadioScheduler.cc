/* RadioScheduler.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#include "RadioScheduler.h"

using namespace std;

RadioScheduler::RadioScheduler(
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
    )
{   
    this->node_is_basestation = node_is_basestation;
    this->node_id = node_id;
    this->num_nodes_in_net = num_nodes_in_net;
    this->nodes_in_net = nodes_in_net;
    this->heartbeat_activity = heartbeat_activity;
    this->num_channels = num_channels;
    this->rx_rate_measured = rx_rate_measured;
    this->tx_rate_measured = tx_rate_measured;
    this->tx_burst_length = tx_burst_length;
    this->uhd_retune_delay = uhd_retune_delay;
    this->debug = debug;
    this->u4 = u4;
    this->uplink = uplink;
    
    // Placeholder for future development:
    if (num_channels > 1) {
        cerr << "ERROR: RadioScheduler multichannel support under development." << endl;
        exit(EXIT_FAILURE);
    }
    
    // Establish timing relationships
    //  FDD:  frequency division duplex between base and mobiles
    //  FH:   frequency hopping of FDD 

    // Establish intraslot timing relationships
    tau_tune = uhd_retune_delay;    // Placeholder for any validation checks
    tau_tx_guard_begin = RS_TAU_GUARD_BEGIN_DEFAULT;
    tau_tx_guard_end = RS_TAU_GUARD_END_DEFAULT;    // Adjusted later by calculation
    tau_tx_burst = ((double) this->tx_burst_length) / tx_rate_measured;

    rx_recommended_sample_size = ceil( rx_rate_measured *
            (tau_tx_guard_begin + tau_tx_burst + tau_tx_guard_end) ); 

    tau_rx_dwell = (double)rx_recommended_sample_size / (double)rx_rate_measured;
    tau_tx_guard_end = tau_rx_dwell - tau_tx_guard_begin - tau_tx_burst; 

    tau_fdd_endslot_margin = RS_TAU_FDD_ENDSLOT_MARGIN_DEFAULT;
    tau_fh_endslot_margin = RS_TAU_FH_ENDSLOT_MARGIN_DEFAULT;
    
    // Establish interslot timing relationships
    tau_1pps_guard = RS_TAU_1PPS_GUARD_DEFAULT;
    
    tau_fdd_slot = tau_rx_dwell + tau_fdd_endslot_margin;
    tau_fdd_net = tau_fdd_slot * (double)(num_nodes_in_net - 1);
    if ((1.0 - tau_fdd_net) < tau_1pps_guard) {
        cerr << "ERROR: RadioScheduler cannot support FDD net." << endl;
        cerr << "  tau_fdd_net = " << scientific << tau_fdd_net << endl;
        exit(EXIT_FAILURE);
    }
    num_fdd_nets_persec = floor( (1.0 - tau_1pps_guard) / tau_fdd_net);
    tau_fdd_margin_persec = 1.0 - ((double)num_fdd_nets_persec * tau_fdd_net);
    fdd_task_sched_size = (num_nodes_in_net - 1)*num_fdd_nets_persec;
    fdd_task_sched = new rf_task_t[fdd_task_sched_size];
    for (unsigned int fdd_id=0; fdd_id < fdd_task_sched_size; fdd_id++) {
        fdd_task_sched[fdd_id].rx_task = RF_TASK_RX_IDLE;
        fdd_task_sched[fdd_id].rx_time = 0.0;
        fdd_task_sched[fdd_id].tx_task = RF_TASK_TX_IDLE;
        fdd_task_sched[fdd_id].tx_time = 0.0;
    }
    
    tau_fh_slot = tau_tune + tau_rx_dwell + tau_fh_endslot_margin;
    tau_fh_net = tau_fh_slot * (double)(num_nodes_in_net - 1);
    if ((1.0 - tau_fdd_net) < tau_1pps_guard) {
        cerr << "ERROR: RadioScheduler cannot support FH net." << endl;
        cerr << "  tau_fh_net = " << scientific << tau_fdd_net << endl;
        exit(EXIT_FAILURE);
    }
    num_fh_nets_persec = floor( (1.0 - tau_1pps_guard) / tau_fh_net);
    tau_fh_margin_persec = 1.0 - ((double)num_fh_nets_persec * tau_fh_net);
    fh_task_sched_size = (num_nodes_in_net - 1)*num_fh_nets_persec;
    fh_task_sched = new rf_task_t[fh_task_sched_size];
    for (unsigned int fh_id=0; fh_id < fh_task_sched_size; fh_id++) {
        fh_task_sched[fh_id].rx_task = RF_TASK_RX_IDLE;
        fh_task_sched[fh_id].rx_time = 0.0;
        fh_task_sched[fh_id].tx_task = RF_TASK_TX_IDLE;
        fh_task_sched[fh_id].tx_time = 0.0;
    }  

   u4_task_sched_size = 40;
   u4_task_sched = new rf_task_t[u4_task_sched_size];
   for(unsigned int u4_id = 0; u4_id < u4_task_sched_size; u4_id++)
   {
       u4_task_sched[u4_id].rx_task = RF_TASK_RX_IDLE;
       u4_task_sched[u4_id].tx_task = RF_TASK_TX_IDLE;
   }

}
//////////////////////////////////////////////////////////////////////////


RadioScheduler::~RadioScheduler() 
{
    try {delete[] fdd_task_sched;} catch (...) { }
    try {delete[] fh_task_sched; } catch (...) { }
}
//////////////////////////////////////////////////////////////////////////


size_t RadioScheduler::getRxRecommendedSampleSize()
{
    return((size_t)rx_recommended_sample_size);
}
//////////////////////////////////////////////////////////////////////////


int RadioScheduler::calcCalibrationSchedule(
    double radio_clock_time
    )
{
    // For now, no modification of radio_clock_time for setting start time
    double start_time = radio_clock_time;
    unsigned int ctr;
    
    // Load both FDD and FH task sequences with one tx-rx pair of tasks
    fdd_task_sched[0].rx_task = RF_TASK_RX_SNAPSHOT;
    fdd_task_sched[0].rx_time = start_time;
    fdd_task_sched[0].tx_task = RF_TASK_TX_NOISE;
    fdd_task_sched[0].tx_time = start_time + tau_tx_guard_begin;
    for (ctr=1; ctr < fdd_task_sched_size; ctr++) {
        fdd_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
        fdd_task_sched[ctr].rx_time = start_time +(ctr * tau_fdd_slot);
        fdd_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
        fdd_task_sched[ctr].tx_time = start_time + (ctr * tau_fdd_slot) +
                    tau_tx_guard_begin;
    }
    
    fh_task_sched[0].rx_task = RF_TASK_RX_SNAPSHOT;
    fh_task_sched[0].rx_time = start_time +tau_tune;
    fh_task_sched[0].tx_task = RF_TASK_TX_NOISE;
    fh_task_sched[0].tx_time = start_time +tau_tune + tau_tx_guard_begin;
    for (ctr=1; ctr < fh_task_sched_size; ctr++) {
        fh_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
        fh_task_sched[ctr].rx_time = start_time +(ctr * tau_fh_slot) +tau_tune;
        fh_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
        fh_task_sched[ctr].tx_time =  start_time + (ctr * tau_fh_slot) +
                    tau_tune + tau_tx_guard_begin;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioScheduler::calcFddSchedule(
    double radio_clock_time
    )
{
    // For now, no modification of radio_clock_time for setting start time
    double start_time = radio_clock_time;
    unsigned int ctr;
    unsigned int active_slot;
    unsigned int active_task_idx;
    
    // Receive task timing: by default all nodes receive during all slots
    for (ctr=0; ctr < fdd_task_sched_size; ctr++) {
        fdd_task_sched[ctr].rx_task = RF_TASK_RX_DATA;
        fdd_task_sched[ctr].rx_time = start_time +(ctr * tau_fdd_slot);
    }
    
    // Transmit task timing: activity depends on role as basestation or mobile  
    if (node_is_basestation) {
        // Basestation transmits at every opportunity
        for (ctr=0; ctr < fdd_task_sched_size; ctr++) {
            fdd_task_sched[ctr].tx_task = RF_TASK_TX_DATA;
            fdd_task_sched[ctr].tx_time = start_time + (ctr * tau_fdd_slot) +
                    tau_tx_guard_begin;
        }
    } else {
        // Mobiles transmit only at the node-specific slot in each net period;
        // set all to default idle then search for node-specific active slots
        for (ctr=0; ctr < fdd_task_sched_size; ctr++) {
            fdd_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
            fdd_task_sched[ctr].tx_time = start_time + (ctr * tau_fdd_slot) +
                    tau_tx_guard_begin;
        }
        
        // Find active slot for this node
        active_slot = num_nodes_in_net;
        for (ctr = 0; ctr < num_nodes_in_net; ctr++) {
            if (node_id == nodes_in_net[ctr]) {
                active_slot = ctr;
                break;
            }
        }
        if (active_slot >= num_nodes_in_net) {
            cerr<<"ERROR: calcFhSchedule unable to find active_slot"<<endl;
            exit(EXIT_FAILURE);
        }
        
        // Schedule activity once per net period 
        for (ctr=0; ctr < num_fdd_nets_persec; ctr++) {
            active_task_idx = (ctr * num_nodes_in_net) + active_slot;
            fdd_task_sched[active_task_idx].tx_task = RF_TASK_TX_DATA;
        }
    } 
    
    // Heartbeat insertion
    if (heartbeat_activity != HEARTBEAT_ACTIVITY_NONE) {
        // Add some heartbeats at start of schedule by overwriting
        // slots otherwise used for data communications 
        for (ctr=0; ctr < num_nodes_in_net; ctr++) {
            fdd_task_sched[ctr].rx_task = RF_TASK_RX_HEARTBEAT;
            if (fdd_task_sched[ctr].tx_task == RF_TASK_TX_DATA) {
                fdd_task_sched[ctr].tx_task = RF_TASK_TX_HEARTBEAT;
            }
        }
        
        // Add some heartbeats near middle of schedule by overwriting
        // slots otherwise used for data communications 
        unsigned int mid_pt = (unsigned int)ceil(fdd_task_sched_size/2);
        for (ctr = mid_pt; ctr < (mid_pt +num_nodes_in_net); ctr++) {
            fdd_task_sched[ctr].rx_task = RF_TASK_RX_HEARTBEAT;
            if (fdd_task_sched[ctr].tx_task == RF_TASK_TX_DATA) {
                fdd_task_sched[ctr].tx_task = RF_TASK_TX_HEARTBEAT;
            }
        }     
    }
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
void RadioScheduler::setU4ScheduleSize(int new_size)
{
    u4_task_sched_size = new_size;
    if(u4_task_sched_size > 40)
        u4_task_sched_size = 40;
}
int RadioScheduler::calcU4Schedule()
{
    unsigned int ctr;
    // Transmit task timing: activity depends on role as basestation or mobile  
    if (node_is_basestation) 
    {
        // Basestation transmits at every opportunity
        for (ctr=0; ctr < u4_task_sched_size; ctr++) 
        {
            u4_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
            u4_task_sched[ctr].tx_task = RF_TASK_TX_OFDMA_DATA;
            u4_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
        }
    } 
    else 
    {
        for (ctr=0; ctr < u4_task_sched_size; ctr++) 
        {
            u4_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
           // if(uplink)
           // {
                u4_task_sched[ctr].tx_task = RF_TASK_TX_MC_DATA;
           // }
            u4_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
        }
        
    } 
    /* 
    // Heartbeat insertion
    if (heartbeat_activity != HEARTBEAT_ACTIVITY_NONE) {
        // Add some heartbeats at start of schedule by overwriting
        // slots otherwise used for data communications 
        for (ctr=0; ctr < num_nodes_in_net; ctr++) {
            u4_task_sched[ctr].rx_task = RF_TASK_RX_HEARTBEAT;
            if (u4_task_sched[ctr].tx_task == RF_TASK_TX_DATA) {
                u4_task_sched[ctr].tx_task = RF_TASK_TX_HEARTBEAT;
            }
        }
        
        // Add some heartbeats near middle of schedule by overwriting
        // slots otherwise used for data communications 
        unsigned int mid_pt = (unsigned int)ceil(fdd_task_sched_size/2);
        for (ctr = mid_pt; ctr < (mid_pt +num_nodes_in_net); ctr++) {
            u4_task_sched[ctr].rx_task = RF_TASK_RX_HEARTBEAT;
            if (u4_task_sched[ctr].tx_task == RF_TASK_TX_DATA) {
                u4_task_sched[ctr].tx_task = RF_TASK_TX_HEARTBEAT;
            }
        }     
    }
    */
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

int RadioScheduler::calcFhSchedule(
    double radio_clock_time
    )
{
    // For now, no modification of radio_clock_time for setting start time
    double start_time = radio_clock_time;
    unsigned int ctr;
    unsigned int active_slot;
    unsigned int active_task_idx;
    
    // Receive task timing: by default all nodes receive during all slots
    for (ctr=0; ctr < fh_task_sched_size; ctr++) {
        fh_task_sched[ctr].rx_task = RF_TASK_RX_DATA;
        fh_task_sched[ctr].rx_time = start_time +(ctr * tau_fh_slot) +tau_tune;
    }
    
    // Transmit task timing: activity depends on role as basestation or mobile  
    if (node_is_basestation) {
        // Basestation transmits at every opportunity
        for (ctr=0; ctr < fh_task_sched_size; ctr++) {
            fh_task_sched[ctr].tx_task = RF_TASK_TX_DATA;
            fh_task_sched[ctr].tx_time = start_time + (ctr * tau_fh_slot) +
                    tau_tune + tau_tx_guard_begin;
        }
    } else {
        // Mobiles transmit only at the node-specific slot in each net period;
        // set all to default idle then search for node-specific active slots
        for (ctr=0; ctr < fh_task_sched_size; ctr++) {
            fh_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
            fh_task_sched[ctr].tx_time = start_time + (ctr * tau_fh_slot) +
                    tau_tune + tau_tx_guard_begin;
        }
        
        // Find active slot for this node
        active_slot = num_nodes_in_net;
        for (ctr = 0; ctr < num_nodes_in_net; ctr++) {
            if (node_id == nodes_in_net[ctr]) {
                active_slot = ctr;
                break;
            }
        }
        if (active_slot >= num_nodes_in_net) {
            cerr<<"ERROR: calcFhSchedule unable to find active_slot"<<endl;
            exit(EXIT_FAILURE);
        } 
        
        // Schedule activity once per net period 
        for (ctr=0; ctr < num_fh_nets_persec; ctr++) {
            active_task_idx = (ctr * num_nodes_in_net) + active_slot;
            fh_task_sched[active_task_idx].tx_task = RF_TASK_TX_DATA;
        }
    } 
    
    // Heartbeat insertion
    if (heartbeat_activity != HEARTBEAT_ACTIVITY_NONE) {    
        // Add some heartbeats at start of schedule by overwriting
        // slots otherwise used for data communications 
        for (ctr=0; ctr < num_nodes_in_net; ctr++) {
            fh_task_sched[ctr].rx_task = RF_TASK_RX_HEARTBEAT;
            if (fh_task_sched[ctr].tx_task == RF_TASK_TX_DATA) {
                fh_task_sched[ctr].tx_task = RF_TASK_TX_HEARTBEAT;
                //fh_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
            }
        }
        
        // Add some heartbeats near middle of schedule by overwriting
        // slots otherwise used for data communications 
        unsigned int mid_pt = (unsigned int)ceil(fh_task_sched_size/2);
        for (ctr = mid_pt; ctr < (mid_pt +num_nodes_in_net); ctr++) {
            fh_task_sched[ctr].rx_task = RF_TASK_RX_HEARTBEAT;
            if (fh_task_sched[ctr].tx_task == RF_TASK_TX_DATA) {
                fh_task_sched[ctr].tx_task = RF_TASK_TX_HEARTBEAT;
                //fh_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
            }
        }
    }  

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioScheduler::calcTestSchedule(
    double radio_clock_time
    )
{
    // This method is for any special test code
    
    // For now, no modification of radio_clock_time for setting start time
    double start_time = radio_clock_time;
    unsigned int ctr;
    
    cout << "INFO: RadioScheduler::calcTestSchedule generating empty schedule" << endl;
    
    // FDD mode schedule
    for (ctr=0; ctr < fdd_task_sched_size; ctr++) {
        fdd_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
        fdd_task_sched[ctr].rx_time = start_time +(ctr * tau_fdd_slot);
        fdd_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
        fdd_task_sched[ctr].tx_time = start_time + (ctr * tau_fdd_slot) +
                    tau_tx_guard_begin;
    }
    
    // FH mode schedule
    for (ctr=0; ctr < fh_task_sched_size; ctr++) {
        fh_task_sched[ctr].rx_task = RF_TASK_RX_IDLE;
        fh_task_sched[ctr].rx_time = start_time +(ctr * tau_fh_slot) +tau_tune;
        fh_task_sched[ctr].tx_task = RF_TASK_TX_IDLE;
        fh_task_sched[ctr].tx_time =  start_time + (ctr * tau_fh_slot) +
                    tau_tune + tau_tx_guard_begin;
    }

    cout << "INFO: RadioScheduler::calcTestSchedule adding snapshot to 1st slot" << endl;
    fdd_task_sched[0].rx_task = RF_TASK_RX_SNAPSHOT;
    fdd_task_sched[0].rx_time = start_time;
    fh_task_sched[0].rx_task = RF_TASK_RX_SNAPSHOT;
    fh_task_sched[0].rx_time = start_time;

   return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


RfTxTaskType RadioScheduler::getCurrentTxTask(
        bool use_normal_mode,
        unsigned int task_id
        )
{
    if(u4) return( u4_task_sched[task_id].tx_task);
    if (use_normal_mode) {
        return( fdd_task_sched[task_id].tx_task );
    } else {
        return( fh_task_sched[task_id].tx_task );
    }
}
//////////////////////////////////////////////////////////////////////////


bool RadioScheduler::isHeartbeatExpected(
        bool use_normal_mode,
        unsigned int task_id
        )
{
    RfRxTaskType completed_rx_task;

    if (use_normal_mode) {
         completed_rx_task = fdd_task_sched[task_id].rx_task;
    } else {
         completed_rx_task = fh_task_sched[task_id].rx_task;
    }
    
    if (completed_rx_task == RF_TASK_RX_HEARTBEAT) {
        return(true);
    } else {
        return(false);
    }
    
    return(false);
}
//////////////////////////////////////////////////////////////////////////


unsigned int RadioScheduler::getActiveSchedSize(
        bool use_normal_mode
        )
{
    if(u4) return u4_task_sched_size;
    if (use_normal_mode) {
        return(fdd_task_sched_size);
    } else {
        return(fh_task_sched_size);
    }
}
//////////////////////////////////////////////////////////////////////////


bool RadioScheduler::isActiveSched4Basestation()
{
    if (node_is_basestation) {
        return(true);
    } else {
        return(false);
    }
}
//////////////////////////////////////////////////////////////////////////


rf_task_t* RadioScheduler::getFddTaskSched()
{
    return(fdd_task_sched);
}
//////////////////////////////////////////////////////////////////////////


unsigned int RadioScheduler::getFddTaskSchedSize()
{
    return(fdd_task_sched_size);
}
//////////////////////////////////////////////////////////////////////////


rf_task_t* RadioScheduler::getFhTaskSched()
{
    return(fh_task_sched);
}
//////////////////////////////////////////////////////////////////////////


unsigned int RadioScheduler::getFhTaskSchedSize()
{
    return(fh_task_sched_size);
}
//////////////////////////////////////////////////////////////////////////

rf_task_t* RadioScheduler::getU4TaskSched()
{
    return(u4_task_sched);
}
//////////////////////////////////////////////////////////////////////////


unsigned int RadioScheduler::getU4TaskSchedSize()
{
    return(u4_task_sched_size);
}
//////////////////////////////////////////////////////////////////////////

int RadioScheduler::printScheduleEntries(
    rf_task_t* task_sched,
    unsigned int begin_idx,
    unsigned int end_idx
    )
{
    cout<< "\nEntries from RadioScheduler object at "<< hex << task_sched << ":"<< endl;
    cout<< "Index   Rx Time  Rx Task                  Tx Time  Tx Task" << endl;
    cout<< "-------------------------------------------------------------------------" << endl;
    for (unsigned int idx = begin_idx; idx < (end_idx +1); idx++) {
        cout<< right << setw(5) << idx <<"  ";
        cout<< left << setw(8) << fixed<< setprecision(6) << task_sched[idx].rx_time <<"  ";
        switch( task_sched[idx].rx_task ) {
            case RF_TASK_RX_IDLE :
                cout<<"RF_TASK_RX_IDLE         ";
                break;
            case RF_TASK_RX_RETUNE_ONLY :
                cout<<"RF_TASK_RX_RETUNE_ONLY  ";
                break;
            case RF_TASK_RX_DATA :
                cout<<"RF_TASK_RX_DATA         ";
                break;
            case RF_TASK_RX_HEARTBEAT :
                cout<<"RF_TASK_RX_HEARTBEAT    ";
                break;
            case RF_TASK_RX_SNAPSHOT :
                cout<<"RF_TASK_RX_SNAPSHOT     ";
                break;
            default :
                cerr<<"\nERROR: Unrecognized rx_task"<<endl;
                exit(EXIT_FAILURE);
                break;
        }
        cout<< left << setw(8) << fixed<< setprecision(6) << task_sched[idx].tx_time <<"  ";
        switch( task_sched[idx].tx_task ) {
            case RF_TASK_TX_IDLE :
                cout<<"RF_TASK_TX_IDLE         ";
                break;
            case RF_TASK_TX_RETUNE_ONLY :
                cout<<"RF_TASK_TX_RETUNE_ONLY  ";
                break;
            case RF_TASK_TX_DATA :
                cout<<"RF_TASK_TX_DATA         ";
                break;
            case RF_TASK_TX_HEARTBEAT :
                cout<<"RF_TASK_TX_HEARTBEAT     ";
                break;     
            case RF_TASK_TX_NOISE :
                cout<<"RF_TASK_TX_NOISE        ";
                break;
            default :
                cerr<<"\nERROR: Unrecognized tx_task"<<endl;
                exit(EXIT_FAILURE);
                break;
        }

        cout<<" "<<endl;
    }
    
    return(EXIT_SUCCESS);
} 
//////////////////////////////////////////////////////////////////////////


double RadioScheduler::getFhInterdwellSpacing()
{
    return(tau_tune +tau_tx_guard_begin +tau_tx_guard_end +tau_fh_endslot_margin);
}
//////////////////////////////////////////////////////////////////////////


int RadioScheduler::printTimingValues()
{
    cout << "\nRadioScheduler Timing Values" << endl;
    cout <<   "----------------------------" << endl;
    cout << "  Intraslot values:" << endl;
    cout << "   tau_tune                  : " << scientific << tau_tune << endl;
    cout << "   tau_tx_guard_begin        : " << scientific << tau_tx_guard_begin << endl;
    cout << "   tau_tx_burst              : " << scientific << tau_tx_burst << endl;
    cout << "   tau_tx_guard_end          : " << scientific << tau_tx_guard_end << endl;
    cout << "   tau_rx_dwell              : " << scientific << tau_rx_dwell << endl;
    cout << "   rx_recommended_sample_size: " << dec <<        rx_recommended_sample_size << endl;
    cout << " " << endl;
    cout << "   tau_fdd_endslot_margin    : " << scientific << tau_fdd_endslot_margin << endl;
    cout << "   tau_fh_endslot_margin     : " << scientific << tau_fh_endslot_margin << endl;
    cout << " " << endl;
    cout << "  Interslot values:" << endl;
    cout << "   tau_1pps_guard            : " << scientific << tau_1pps_guard << endl;
    cout << " " << endl;
    cout << "   tau_fdd_slot              : " << scientific << tau_fdd_slot << endl;
    cout << "   tau_fdd_net               : " << scientific << tau_fdd_net << endl;
    cout << "   num_fdd_nets_persec       : " << dec <<        num_fdd_nets_persec << endl;
    cout << "   tau_fdd_margin_persec     : " << scientific << tau_fdd_margin_persec << endl;
    cout << " " << endl;
    cout << "   tau_fh_slot               : " << scientific << tau_fh_slot << endl;
    cout << "   tau_fh_net                : " << scientific << tau_fh_net << endl;
    cout << "   num_fh_nets_persec        : " << dec <<        num_fh_nets_persec << endl;
    cout << "   tau_fh_margin_persec      : " << scientific << tau_fh_margin_persec << endl;
    cout << " " << endl;
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
