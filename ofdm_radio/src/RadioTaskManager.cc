/* RadioTaskManager.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#include "RadioTaskManager.h"

//#define DEBUG_SUPPORTED 1

using namespace std;

RadioTaskManager::RadioTaskManager(
    FhSeqGenerator*         fsg_ptr,
    FreqTableGenerator*     ftg_ptr,
    RadioHardwareConfig*    rhc_ptr,
    RadioScheduler*         rs_ptr,
    Phy2Mac*                p2m_ptr,
    bool debug,
    bool u4
)
{
    this->fsg_ptr = fsg_ptr;
    this->ftg_ptr = ftg_ptr;
    this->rhc_ptr = rhc_ptr;
    this->rs_ptr = rs_ptr;
    this->p2m_ptr = p2m_ptr;
    this->debug = debug;
    this->u4 = u4;
    
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);

    // Task-specific structure default settings;
    //  many attributes, however, are set at task activation time 
    rx_manual_tune_task.rhc_ptr = rhc_ptr;
    tx_manual_tune_task.rhc_ptr = rhc_ptr;
    tune2normal_freq_task.rhc_ptr = rhc_ptr;

    rx_recommended_sample_size = rs_ptr->getRxRecommendedSampleSize();

    rx_frame_burst_task.rhc_ptr = rhc_ptr;
    rx_frame_burst_task.rx_recommended_sample_size = rx_recommended_sample_size;
    
    tx_frame_burst_task.rhc_ptr = rhc_ptr;
    tx_frame_burst_task.frame_header_ptr = p2m_ptr->getTxHeaderPtr();
    // The payload size may change over the duration of the application
    tx_frame_burst_task.frame_payload_size = p2m_ptr->getTxPayloadSize();
    tx_frame_burst_task.frame_payload_ptr =  p2m_ptr->getTxPayloadPtr();
    
    rx_heartbeat_burst_task.rhc_ptr = rhc_ptr;
    rx_heartbeat_burst_task.rx_recommended_sample_size = rx_recommended_sample_size;
    
    tx_heartbeat_burst_task.rhc_ptr = rhc_ptr;
    tx_heartbeat_burst_task.frame_header_ptr = p2m_ptr->getTxHeaderPtr();
    // The payload size may change over the duration of the application
    tx_heartbeat_burst_task.frame_payload_size = p2m_ptr->getTxPayloadSize();
    tx_heartbeat_burst_task.frame_payload_ptr =  p2m_ptr->getTxPayloadPtr();
    
    rx_snapshot_burst_task.rhc_ptr = rhc_ptr;
    rx_snapshot_burst_task.rx_recommended_sample_size = rx_recommended_sample_size;
    
    tx_noise_burst_task.rhc_ptr = rhc_ptr;
}
//////////////////////////////////////////////////////////////////////////


RadioTaskManager::~RadioTaskManager()
{   
}
//////////////////////////////////////////////////////////////////////////


int RadioTaskManager::doTask(
    bool waveform_is_normal,
    unsigned int task_id
    )
{
    rf_task_t* task_table;

    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);

    if(u4)
    {
        task_table = rs_ptr->getU4TaskSched();
    }
    else
    {
        if (waveform_is_normal) {
            // Frequency Division Duplex (FDD) mode is normal
            task_table = rs_ptr->getFddTaskSched();
        } else {   
            // Otherwise, in frequency hopping (FH) mode
            task_table = rs_ptr->getFhTaskSched();
        }
    }   
    bool rx_task_is_running = false;
    pthread_t rx_task_thread = (pthread_t) 0;    
    bool after_rx_task_check_for_frame = false;
    
    bool tx_task_is_running = false;
    pthread_t tx_task_thread = (pthread_t) 0;
    
    int thread_return_value;
    void* thread_status;
    
#if 0
    unsigned int rf_idx, dsp_idx;  
    double rf_freq, dsp_freq;      
    if (debug) {
        cout << "DEBUG: RadioTaskManager::doTask() "<<endl; 
        cout << "  started at radio hardware time: ";
        cout << rhc_ptr->getHardwareTimestamp() << endl;
        cout << "  task_id: "<< dec << task_id <<endl;
        cout << "  rx_time: "<< task_table[task_id].rx_time  <<endl;
        cout << "  tx_time: "<< task_table[task_id].tx_time  <<endl;
        cout << " "<<endl;
    }
#endif 


    if ( !waveform_is_normal ) {    // then in FH mode; check if retuning needed

        // Receive side retuning ----------------------------------------
        switch (task_table[task_id].rx_task) {
            case RF_TASK_RX_RETUNE_ONLY :
                // fall through
            case RF_TASK_RX_DATA :
                // fall through
            case RF_TASK_RX_SNAPSHOT :
                // Receive side needs retuning-->new rx side task
                rx_task_is_running = true;
                rx_manual_tune_task.task_id = task_id;
                rx_manual_tune_task.rf_freq = ftg_ptr->getRfTableFreqRx(
                        fsg_ptr->getFhSeqRfIdx(task_id) );
                rx_manual_tune_task.dsp_freq = ftg_ptr->getDspTableFreqRx(
                         fsg_ptr->getFhSeqDspIdx(task_id) );
                
                rx_manual_tune_task.thread_creation_status = pthread_create(
                        &rx_manual_tune_task.thread_id, &pthread_attr,
                        doRxManualTuneTask, (void *)&rx_manual_tune_task
                        );
                if (rx_manual_tune_task.thread_creation_status != 0) {
                    cerr << "ERROR: in RadioTaskManager::doTask ";
                    cerr << "cannot create rx_manual_tune_task" << endl;
                    exit(EXIT_FAILURE);
                }
                rx_task_thread = rx_manual_tune_task.thread_id;
#if 0
                if (debug) {
                    cout << "DEBUG: RadioTaskManager::doTask -- rx retune"<<endl;
                    dsp_idx = fsg_ptr->getFhSeqDspIdx(task_id);
                    dsp_freq = ftg_ptr->getDspTableFreqRx(dsp_idx);
                    rf_idx = fsg_ptr->getFhSeqRfIdx(task_id);
                    rf_freq = ftg_ptr->getRfTableFreqRx(rf_idx);
                    cout << "     dsp_idx: "<<dsp_idx <<"  dsp_freq: "<< dsp_freq <<endl;
                    cout << "     rf_idx:  "<<rf_idx <<"    rf_freq: "<< rf_freq<<endl;
                }
#endif
                break;    
             case RF_TASK_RX_HEARTBEAT :
                // Receive side _may_ need to retune to home frequency
                // Skip if current frequency is already at home frequency
                if (rhc_ptr->getRxNormalFreq() == rhc_ptr->getRxAbsoluteFreq()) {
                    // no retune needed
                    rx_task_is_running = false; //cout << "\nSKIP RX RETUNE" <<endl;
                    break;
                }
                
                rx_task_is_running = true;
                tune2normal_freq_task.task_id = task_id;
                tune2normal_freq_task.thread_creation_status = pthread_create(
                        &tune2normal_freq_task.thread_id, &pthread_attr,
                        doTune2NormalFreqTask, (void *)&tune2normal_freq_task
                        );
                if (tune2normal_freq_task.thread_creation_status != 0) {
                    cerr << "ERROR: in RadioTaskManager::doTask ";
                    cerr << "cannot create tune2normal_freq_task" << endl;
                    exit(EXIT_FAILURE);
                }
                rx_task_thread = tune2normal_freq_task.thread_id;
#if 0
                if (debug) {
                    cout << "DEBUG: RadioTaskManager::doTask :";
                    cout << " RF_TASK_RX_HEARTBEAT retune to normal frequency"<<endl;
                }
#endif
                break;
            case RF_TASK_RX_IDLE :
                // fall through
            default :
                // no retuning 
                rx_task_is_running = false;
                break;
        }
        
        // Transmit side retuning ----------------------------------------
        switch (task_table[task_id].tx_task) {
            case RF_TASK_TX_RETUNE_ONLY :
                // fall through
            case RF_TASK_TX_DATA :
                // fall through
            case RF_TASK_TX_NOISE :
                // Transmit side needs retuning-->new tx side task
                tx_task_is_running = true;
                tx_manual_tune_task.task_id = task_id;
                tx_manual_tune_task.rf_freq = ftg_ptr->getRfTableFreqTx(
                        fsg_ptr->getFhSeqRfIdx(task_id) );
                tx_manual_tune_task.dsp_freq = ftg_ptr->getDspTableFreqTx(
                         fsg_ptr->getFhSeqDspIdx(task_id) );
                tx_manual_tune_task.thread_creation_status = pthread_create(
                        &tx_manual_tune_task.thread_id, &pthread_attr,
                        doTxManualTuneTask, (void *)&tx_manual_tune_task
                        );
                if (tx_manual_tune_task.thread_creation_status != 0) {
                    cerr << "ERROR: in RadioTaskManager::doTask ";
                    cerr << "cannot create tx_manual_tune_task" << endl;
                    exit(EXIT_FAILURE);
                }
                tx_task_thread = tx_manual_tune_task.thread_id;
#if 0
                if (debug) {
                    cout << "DEBUG: RadioTaskManager::doTask -- tx retune"<<endl;
                    dsp_idx = fsg_ptr->getFhSeqDspIdx(task_id);
                    dsp_freq = ftg_ptr->getDspTableFreqTx(dsp_idx);
                    rf_idx = fsg_ptr->getFhSeqRfIdx(task_id);
                    rf_freq = ftg_ptr->getRfTableFreqTx(rf_idx);
                    cout << "     dsp_idx: "<<dsp_idx <<"  dsp_freq: "<< dsp_freq;
                    cout << "     rf_idx: "<<rf_idx <<"  rf_freq: "<< rf_freq;
                }
#endif
                break;
             case RF_TASK_TX_HEARTBEAT :
                // Transmit side _may_ need to retune to home frequency
                // Skip if current frequency is already at home frequency
                if (rhc_ptr->getTxNormalFreq() == rhc_ptr->getTxAbsoluteFreq()) {
                    // no retune needed
                    tx_task_is_running = false; //cout << "\nSKIP TX RETUNE" <<endl;
                    break;
                }
                
                tx_task_is_running = true;
                tune2normal_freq_task.task_id = task_id;
                tune2normal_freq_task.thread_creation_status = pthread_create(
                        &tune2normal_freq_task.thread_id, &pthread_attr,
                        doTune2NormalFreqTask, (void *)&tune2normal_freq_task
                        );
                if (tune2normal_freq_task.thread_creation_status != 0) {
                    cerr << "ERROR: in RadioTaskManager::doTask ";
                    cerr << "cannot create tune2normal_freq_task" << endl;
                    exit(EXIT_FAILURE);
                }
                rx_task_thread = tune2normal_freq_task.thread_id;
#if 0
                if (debug) {
                    cout << "DEBUG: RadioTaskManager::doTask :";
                    cout << " RF_TASK_TX_HEARTBEAT retune to normal frequency"<<endl;
                }
#endif
                break;
            case RF_TASK_TX_IDLE :
                // fall through
            default :
                // no retuning 
                break;
        }

    }

    
    // Receive side task execution ---------------------------------------
    
    // Wait for receive side retuning to finish
    if(rx_task_is_running) {
        if (rx_task_thread != (pthread_t) 0) {
            thread_return_value = pthread_join(rx_task_thread, &thread_status);
            if (thread_return_value) {
                cerr <<"ERROR: in RadioTaskManager::doTask on rx thread join"<<endl;
                cerr <<"       thread_return_value: " << dec<< thread_return_value;
                cerr <<" and thread_status: "<< dec<< thread_status <<endl;
                exit(EXIT_FAILURE);  
            }
        }
        rx_task_is_running = false;
        
        // Reset thread IDs in case more tasks required
        rx_task_thread = (pthread_t) 0;
    }
    switch (task_table[task_id].rx_task) {
        case RF_TASK_RX_DATA :
            rx_task_is_running = true;
            after_rx_task_check_for_frame = true;
            rx_frame_burst_task.task_id = task_id;
            rx_frame_burst_task.rhc_ptr = rhc_ptr;
            rx_frame_burst_task.start_time = task_table[task_id].rx_time;
            //rx_frame_burst_task.num_uhd_batches = rx_burst_num_uhd_batches;
            rx_frame_burst_task.thread_creation_status =  pthread_create( 
                    &rx_frame_burst_task.thread_id, &pthread_attr, 
                    doRxFrameBurstTask, (void *)&rx_frame_burst_task 
                    );
            if (rx_frame_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create rx_frame_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            rx_task_thread = rx_frame_burst_task.thread_id;
            break;
        case RF_TASK_RX_HEARTBEAT :
            rx_task_is_running = true;
            after_rx_task_check_for_frame = true;
            rx_heartbeat_burst_task.task_id = task_id;
            rx_heartbeat_burst_task.rhc_ptr = rhc_ptr;
            rx_heartbeat_burst_task.start_time = task_table[task_id].rx_time;
            //rx_heartbeat_burst_task.num_uhd_batches = rx_burst_num_uhd_batches;
            rx_heartbeat_burst_task.thread_creation_status =  pthread_create( 
                    &rx_heartbeat_burst_task.thread_id, &pthread_attr, 
                    doRxHeartbeatBurstTask, (void *)&rx_heartbeat_burst_task 
                    );
            if (rx_heartbeat_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create rx_heartbeat_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            rx_task_thread = rx_heartbeat_burst_task.thread_id;
            break;
        case RF_TASK_RX_SNAPSHOT :
            rx_task_is_running = true;
            rx_snapshot_burst_task.task_id = task_id;
            rx_snapshot_burst_task.rhc_ptr = rhc_ptr;
            rx_snapshot_burst_task.start_time = task_table[task_id].rx_time;
            //rx_snapshot_burst_task.num_uhd_batches = rx_burst_num_uhd_batches;
            rx_snapshot_burst_task.thread_creation_status =  pthread_create( 
                    &rx_snapshot_burst_task.thread_id, &pthread_attr, 
                    doRxSnapshotBurstTask, (void *)&rx_snapshot_burst_task 
                    );
            if (rx_snapshot_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create rx_snapshot_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            rx_task_thread = rx_snapshot_burst_task.thread_id;
            break;
        case RF_TASK_RX_IDLE :
            // fall through
        case RF_TASK_RX_RETUNE_ONLY :
            // fall through
        default :
            // no task executed
            rx_task_is_running = false;
        break;
    }

    // Transmit side task  execution -----------------------------------
    
    // Wait for transmit side retuning to finish
    if(tx_task_is_running) {
        if (tx_task_thread != (pthread_t) 0) {
            thread_return_value = pthread_join(tx_task_thread, &thread_status);
            if (thread_return_value) {
                cerr <<"ERROR: in RadioTaskManager::doTask on tx thread join"<<endl;
                cerr <<"       thread_return_value: " << dec<< thread_return_value;
                cerr <<" and thread_status: "<< dec<< thread_status <<endl;
                exit(EXIT_FAILURE);  
            }
        }
        tx_task_is_running = false;
        
        // Reset thread IDs in case more tasks required
        tx_task_thread = (pthread_t) 0;
    }
    switch (task_table[task_id].tx_task) {
        case RF_TASK_TX_DATA :
            tx_task_is_running = true;
            p2m_ptr->fetchTxFrame();
            tx_frame_burst_task.task_id = task_id;
            tx_frame_burst_task.rhc_ptr = rhc_ptr;
            tx_frame_burst_task.start_time = task_table[task_id].tx_time;
            tx_frame_burst_task.frame_header_ptr = p2m_ptr->getTxHeaderPtr();
            tx_frame_burst_task.frame_payload_size = p2m_ptr->getTxPayloadSize();
            tx_frame_burst_task.frame_payload_ptr = p2m_ptr->getTxPayloadPtr();
            tx_frame_burst_task.thread_creation_status = pthread_create( 
                    &tx_frame_burst_task.thread_id, &pthread_attr, 
                    doTxFrameBurstTask, (void *)&tx_frame_burst_task 
                    ); 
            if (tx_frame_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create tx_frame_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            tx_task_thread = tx_frame_burst_task.thread_id;
            break;
        case RF_TASK_TX_OFDMA_DATA:
            tx_task_is_running = true;
            //p2m_ptr->fetchTxFrame();
            tx_ofdma_frame_burst_task.task_id = task_id;
            tx_ofdma_frame_burst_task.rhc_ptr = rhc_ptr;
            tx_ofdma_frame_burst_task.frame_header_ptr = p2m_ptr->getTxHeaderPtr();
            tx_ofdma_frame_burst_task.frame_payload_size = p2m_ptr->getTxPayloadSize();
            tx_ofdma_frame_burst_task.frame_payload_ptr = p2m_ptr->getTxPayloadPtr();
            tx_ofdma_frame_burst_task.thread_creation_status = pthread_create( 
                    &tx_ofdma_frame_burst_task.thread_id, &pthread_attr, 
                    doTxOFDMAFrameBurstTask, (void *)&tx_ofdma_frame_burst_task 
                    ); 
            if (tx_ofdma_frame_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create tx_ofdma_frame_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            tx_task_thread = tx_ofdma_frame_burst_task.thread_id;
            break;
        case RF_TASK_TX_MC_DATA:
            tx_task_is_running = true;
            //p2m_ptr->fetchTxFrame();
            tx_mc_frame_burst_task.task_id = task_id;
            tx_mc_frame_burst_task.rhc_ptr = rhc_ptr;
            tx_mc_frame_burst_task.frame_header_ptr = p2m_ptr->getTxHeaderPtr();
            tx_mc_frame_burst_task.frame_payload_size = p2m_ptr->getTxPayloadSize();
            tx_mc_frame_burst_task.frame_payload_ptr = p2m_ptr->getTxPayloadPtr();
            tx_mc_frame_burst_task.thread_creation_status = pthread_create( 
                    &tx_mc_frame_burst_task.thread_id, &pthread_attr, 
                    doTxMCFrameBurstTask, (void *)&tx_mc_frame_burst_task 
                    ); 
            if (tx_mc_frame_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create tx_ofdma_frame_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            tx_task_thread = tx_mc_frame_burst_task.thread_id;
            break;
        case RF_TASK_TX_HEARTBEAT :
            tx_task_is_running = true;
            p2m_ptr->createHeartbeatTxFrame();
            tx_heartbeat_burst_task.task_id = task_id;
            tx_heartbeat_burst_task.rhc_ptr = rhc_ptr;
            tx_heartbeat_burst_task.start_time = task_table[task_id].tx_time;
            tx_heartbeat_burst_task.frame_header_ptr = p2m_ptr->getTxHeaderPtr();
            tx_heartbeat_burst_task.frame_payload_size = p2m_ptr->getTxPayloadSize();
            tx_heartbeat_burst_task.frame_payload_ptr = p2m_ptr->getTxPayloadPtr();
            tx_heartbeat_burst_task.thread_creation_status = pthread_create( 
                    &tx_heartbeat_burst_task.thread_id, &pthread_attr, 
                    doTxHeartbeatBurstTask, (void *)&tx_heartbeat_burst_task 
                    ); 
            if (tx_heartbeat_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create tx_heartbeat_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            tx_task_thread = tx_heartbeat_burst_task.thread_id;
            break;
        case RF_TASK_TX_NOISE :
            tx_task_is_running = true;
            tx_noise_burst_task.task_id = task_id;
            tx_noise_burst_task.rhc_ptr = rhc_ptr;
            tx_noise_burst_task.start_time = task_table[task_id].tx_time;
            // Currently, txNoiseBurst() only takes a start time as an arg
            tx_noise_burst_task.thread_creation_status = pthread_create( 
                    &tx_noise_burst_task.thread_id, &pthread_attr, 
                    doTxNoiseBurstTask, (void *)&tx_noise_burst_task 
                    ); 
            if (tx_noise_burst_task.thread_creation_status != 0) {
                cerr << "ERROR: in RadioTaskManager::doTask ";
                cerr << "cannot create tx_noise_burst_task" << endl;
                exit(EXIT_FAILURE);
            }
            tx_task_thread = tx_noise_burst_task.thread_id;
            break;
        case RF_TASK_TX_IDLE :
            // fall through
        case RF_TASK_TX_RETUNE_ONLY :
            // fall through
        default :
            // no task executed
            tx_task_is_running = false;
        break;
    }
    
    // Do thread join on any running rx or tx tasks
#if 0  
    if (debug) {
        cout << "DEBUG:  RadioTaskManager::doTask work task thread join start at:";
        cout << rhc_ptr->getHardwareTimestamp() <<endl;
    }
#endif
    
    
    if(rx_task_is_running) {
        if (rx_task_thread != (pthread_t) 0) {
            thread_return_value = pthread_join(rx_task_thread, &thread_status);
            if (thread_return_value) {
                cerr <<"ERROR: in RadioTaskManager::doTask on rx thread join"<<endl;
                cerr <<"       thread_return_value: " << dec<< thread_return_value;
                cerr <<" and thread_status: "<< dec<< thread_status <<endl;
                exit(EXIT_FAILURE);  
            }
        }
        rx_task_is_running = false;
        
        // Reset thread IDs in case more tasks required
        rx_task_thread = (pthread_t) 0;
    }
    
    if(tx_task_is_running) {
        if (tx_task_thread != (pthread_t) 0) {
            thread_return_value = pthread_join(tx_task_thread, &thread_status);
            if (thread_return_value) {
                cerr <<"ERROR: in RadioTaskManager::doTask on tx thread join"<<endl;
                cerr <<"       thread_return_value: " << dec<< thread_return_value;
                cerr <<" and thread_status: "<< dec<< thread_status <<endl;
                exit(EXIT_FAILURE);  
            }
        }
        tx_task_is_running = false;
        
        // Reset thread IDs in case more tasks required
        tx_task_thread = (pthread_t) 0;
    }
    
#if 0  
    if (debug) {
        cout << "DEBUG:  RadioTaskManager::doTask thread join done at:       ";
        cout << rhc_ptr->getHardwareTimestamp() <<endl;
    }
#endif

    // Post-task execution steps ----------------------------------------
    pthread_attr_destroy(&pthread_attr);

    if (after_rx_task_check_for_frame && rhc_ptr->wasValidFrameRx()) {
        // PLACEHOLDER for any additional task completion; note that
        // logging of received frames and sending a frame's payload to
        // upper protocol layers is handled by other classes
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioTaskManager::printTaskSched(   
        bool node_is_basestation,
        bool waveform_is_normal,
        unsigned int begin_task_id,
        unsigned int end_task_id
        )
{

    unsigned int num_tasks = rs_ptr->getActiveSchedSize(waveform_is_normal); 
    if (end_task_id >= num_tasks) {
        cerr << "ERROR in RadioTaskManager::printTaskFreq requested end task ID: ";
        cerr << dec << end_task_id << " is beyond size of active schedule: ";
        cerr << dec << num_tasks << endl;
        exit(EXIT_FAILURE);
    }
    
    rf_task_t* task_sched;
    unsigned int task_id;
    double rx_freq, tx_freq, tmp_rf, tmp_dsp;
    
    cout<<"\nScheduled Tasks are in ";
    if (waveform_is_normal) {
        cout << "FDD mode ";
        task_sched = rs_ptr->getFddTaskSched();
    } else {
        cout << "FH mode ";
        task_sched = rs_ptr->getFhTaskSched();
    }
    cout << " with a total of " << dec << num_tasks << " tasks." << endl;
    cout<< "-------------------------------------------------------------------------" << endl;
    

    if ( waveform_is_normal ) {
        // For FDD frequency fixed by role of base station or mobile
        if (node_is_basestation) {
            // Base has Rx offset
            rx_freq = rhc_ptr->getNormalFreq() +rhc_ptr->getTx2RxFreqSeparation();
            tx_freq = rhc_ptr->getNormalFreq();
        } else {
            // Mobile has Tx offset
            rx_freq = rhc_ptr->getNormalFreq();
            tx_freq = rhc_ptr->getNormalFreq() +rhc_ptr->getTx2RxFreqSeparation();
        }
        
        for (task_id = begin_task_id; task_id < end_task_id; task_id++) {
        
            // 1st line: receive parameters
            cout<< right << setw(5) << task_id <<" Rx: ";
            cout<< left << setw(8) << fixed<< setprecision(6) << task_sched[task_id].rx_time <<"  ";
            cout << scientific << rx_freq << "  ";
            switch( task_sched[task_id].rx_task ) {
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
            
            // 2nd line: transmit parameters
            cout << "\n      Tx: ";
            cout<< left << setw(8) << fixed<< setprecision(6) << task_sched[task_id].tx_time <<"  ";
            cout << scientific << tx_freq << "  ";
            switch( task_sched[task_id].tx_task ) {
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
            cout << "\n" << endl;
            
        } // task_id loop 
        
        return(EXIT_SUCCESS);
    }
        
    // Else in FH mode; task loop needs to check for retuning 
    for (task_id = begin_task_id; task_id < end_task_id; task_id++) {
        
        // 1st line: receive parameters
        cout<< right << setw(5) << task_id <<" Rx: ";
        cout<< left << setw(8) << fixed<< setprecision(6) << task_sched[task_id].rx_time <<"  ";
        
        // Match approach of doTask() calculation
        switch( task_sched[task_id].rx_task ) {
            case RF_TASK_RX_RETUNE_ONLY :
                // fall through
            case RF_TASK_RX_DATA :
                // fall through
            case RF_TASK_RX_SNAPSHOT :
                tmp_rf = ftg_ptr->getRfTableFreqRx( fsg_ptr->getFhSeqRfIdx(task_id) );
                tmp_dsp = ftg_ptr->getDspTableFreqRx( fsg_ptr->getFhSeqDspIdx(task_id) );
                rx_freq = tmp_rf + tmp_dsp;
                break;
                
            case RF_TASK_RX_HEARTBEAT :
                // Return to normal mode frequency
                if (node_is_basestation) {
                    // Base has Rx offset
                    tmp_rf = rhc_ptr->getNormalFreq() +rhc_ptr->getTx2RxFreqSeparation();
                } else {
                    tmp_rf = rhc_ptr->getNormalFreq();
                }
                tmp_dsp = 0;
                rx_freq = tmp_rf + tmp_dsp;
                break;
                
            default :
                rx_freq = 0;
                break;
        }
        cout << scientific << rx_freq << "  ";

        switch( task_sched[task_id].rx_task ) {
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
        
        // 2nd line: transmit parameters
        cout << "\n      Tx: ";
        cout<< left << setw(8) << fixed<< setprecision(6) << task_sched[task_id].tx_time <<"  ";
        
        // Match approach of doTask() calculation
        switch( task_sched[task_id].tx_task ) {
            case RF_TASK_TX_RETUNE_ONLY :
                // fall through
            case RF_TASK_TX_DATA :
                // fall through
            case RF_TASK_TX_NOISE :
                tmp_rf = ftg_ptr->getRfTableFreqTx( fsg_ptr->getFhSeqRfIdx(task_id) );
                tmp_dsp = ftg_ptr->getDspTableFreqTx( fsg_ptr->getFhSeqDspIdx(task_id) );
                tx_freq = tmp_rf + tmp_dsp;
                break;
                
            case RF_TASK_TX_HEARTBEAT :
                // Return to normal mode frequency
                if (node_is_basestation) {
                    // Base has no Tx offset
                    tmp_rf = rhc_ptr->getNormalFreq();
                } else {
                    tmp_rf = rhc_ptr->getNormalFreq() +rhc_ptr->getTx2RxFreqSeparation();
                }
                tmp_dsp = 0;
                tx_freq = tmp_rf + tmp_dsp;
                break;
            
            default :
                tx_freq = 0;
                break;
        }
        cout << scientific << tx_freq << "  ";
        
        switch( task_sched[task_id].tx_task ) {
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
        cout << "\n" << endl;
        
    }   // task_id loop
    cout << "\n" << endl;
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


// Helper functions ------------------------------------------------------
void* doRxManualTuneTask(void* thread_args) 
{
    rx_manual_tune_task_t* args;
    args = (rx_manual_tune_task_t*)thread_args;
    
    args->rhc_ptr->tuneRxManual(args->rf_freq, args->dsp_freq);

    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doTxManualTuneTask(void* thread_args) 
{
    tx_manual_tune_task_t* args;
    args = (tx_manual_tune_task_t*)thread_args;
    
    args->rhc_ptr->tuneTxManual(args->rf_freq, args->dsp_freq);

    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doTune2NormalFreqTask(void* thread_args) 
{
    tune2normal_freq_task_t* args;
    args = (tune2normal_freq_task_t*)thread_args;
    
    args->rhc_ptr->tune2NormalFreq();

    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doRxFrameBurstTask(void* thread_args)
{
    rx_frame_burst_task_t* args;
    args = (rx_frame_burst_task_t *)thread_args;

    args->rhc_ptr->rxFrameBurst(args->start_time, args->rx_recommended_sample_size);
    
    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doTxFrameBurstTask(void* thread_args) 
{
    tx_frame_burst_task_t* args;
    args = (tx_frame_burst_task_t *)thread_args;
    
    args->rhc_ptr->txFrameBurst(args->start_time, args->frame_header_ptr,
            args->frame_payload_size, args->frame_payload_ptr);
    
    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doTxOFDMAFrameBurstTask(void* thread_args)
{
    tx_ofdma_frame_burst_task_t* args;
    args = (tx_ofdma_frame_burst_task_t *)thread_args;

    args->rhc_ptr->txOFDMAFrameBurst();

    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doTxMCFrameBurstTask(void* thread_args)
{
    tx_mc_frame_burst_task_t* args;
    args = (tx_mc_frame_burst_task_t *)thread_args;

    args->rhc_ptr->txMCFrameBurst();

    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////

void* doRxHeartbeatBurstTask(void* thread_args)
{
    rx_heartbeat_burst_task_t* args;
    args = (rx_heartbeat_burst_task_t *)thread_args;

    args->rhc_ptr->rxHeartbeatBurst(args->start_time, args->rx_recommended_sample_size);
    
    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doTxHeartbeatBurstTask(void* thread_args) 
{
    tx_heartbeat_burst_task_t* args;
    args = (tx_heartbeat_burst_task_t *)thread_args;
    
    args->rhc_ptr->txHearbeatBurst(args->start_time, args->frame_header_ptr,
            args->frame_payload_size, args->frame_payload_ptr);
    
    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////


void* doRxSnapshotBurstTask(void* thread_args)
{
    rx_snapshot_burst_task_t* args;
    args = (rx_snapshot_burst_task_t *)thread_args;
    
    args->rhc_ptr->rxSnapshotBurst(args->start_time, args->rx_recommended_sample_size);
    
    pthread_exit(NULL);
}
/////////////////////////////////////////////////////////////////////////


void* doTxNoiseBurstTask(void* thread_args) 
{
    tx_noise_burst_task_t* args;
    args = (tx_noise_burst_task_t *)thread_args;
    
    args->rhc_ptr->txNoiseBurst(args->start_time);
    
    pthread_exit(NULL);
}
//////////////////////////////////////////////////////////////////////////

void* run_mc_rx(void* thread_args)
{
    //std::cout << "started mc_rx" << std::endl;
    rx_thread_args_t* args;
    args = (rx_thread_args_t*)thread_args;
    RadioHardwareConfig* rhc_ptr = args->rhc_ptr;
    const size_t max_samps_per_packet = rhc_ptr->usrp->get_device()->get_max_recv_samps_per_packet();
    std::vector<std::complex<float> > rx_usrp_buffer(max_samps_per_packet);

    uhd_error_stats_t uhd_error_stats;
    uhd::rx_metadata_t rx_md;
    unsigned int rx_uhd_recv_ctr = 0;
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = true;
    rhc_ptr->usrp->issue_stream_cmd(stream_cmd);
    int continue_running = 1;
    timer t0 = timer_create();
    timer_tic(t0);
    double num_seconds = args->run_time;
    while (continue_running) 
    {
        // grab data from device
        size_t uhd_num_delivered_samples = rhc_ptr->usrp->get_device()->recv(
                &rx_usrp_buffer.front(), rx_usrp_buffer.size(), rx_md,
                uhd::io_type_t::COMPLEX_FLOAT32,
                uhd::device::RECV_MODE_ONE_PACKET
                );
        //std::cout << "grabbed " << uhd_num_delivered_samples << " samples" << std::endl;

        // Check for UHD errors
        switch(rx_md.error_code) {
                // Keep running on these conditions
                case uhd::rx_metadata_t::ERROR_CODE_NONE:
                case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                    break;
                    // Otherwise, capture details of non-trivial error
                default:
                    uhd_error_stats.rx_error_timestamp = rhc_ptr->getHardwareTimestamp();
                    uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
                    uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;               
                    uhd_error_stats.rx_error_num_samples = uhd_num_delivered_samples;
                    rhc_ptr->reportUhdError();
                    uhd_error_stats.rx_fail_frameburst++;
                    return((void*)EXIT_FAILURE);
        }
        // Prefilter samples; this may be optional in a lab environment
        if(uhd_num_delivered_samples > 0)
        {
            /*
               for(int j = 0; j < uhd_num_delivered_samples; j++)
               {
               std::complex<float> usrp_sample = rx_usrp_buffer[j];
               firfilt_crcf_push(rhc_ptr->rx_prefilt, usrp_sample);
               firfilt_crcf_execute(rhc_ptr->rx_prefilt, &usrp_sample);

               unsigned int nw;
               msresamp_crcf_execute(rhc_ptr->rx_resamp, &usrp_sample, 1,
               rx_temp_resample_buf, &nw);

               ofdmflexframesync_execute(rhc_ptr->ofdma_fs, rx_temp_resample_buf, nw);
               }*/

            //Not sure if necessary to run samples through filter and resampler
            /*
               size_t ctr;
               for (ctr = 0; ctr < uhd_num_delivered_samples; ctr++) {
               firfilt_crcf_push(rhc_ptr->rx_prefilt, rx_usrp_buffer[ctr] );
               firfilt_crcf_execute(rhc_ptr->rx_prefilt, &rx_usrp_buffer[ctr] );
               }

            // Apply resampler to reduce UHD sample rate to modem's receive rate    
            unsigned int rx_num_resamples = 0;
            msresamp_crcf_execute(rhc_ptr->rx_resamp, &rx_usrp_buffer[0],
            (unsigned int)uhd_num_delivered_samples,
            &rx_temp_resample_buf[0],
            &rx_num_resamples);

             */
            for(unsigned int j = 0; j < uhd_num_delivered_samples; j++)
            {
                std::complex<float> usrp_sample = rx_usrp_buffer[j];
                // Input samples to modem
                rhc_ptr->mcrx->Execute(&usrp_sample, 1);
            }

        }
        if((num_seconds > 0 && timer_toc(t0) >= num_seconds) || rhc_ptr->join_rx_thread)
        {
            continue_running = 0;
        }
    }
    //std::cout << "shutting down receiver" << std::endl;
    pthread_exit(NULL);
}

void* run_ofdma_rx(void* thread_args)
{
    rx_thread_args_t* args;
    args = (rx_thread_args_t*)thread_args;
    RadioHardwareConfig* rhc_ptr = args->rhc_ptr;
    ofdmflexframesync sync;
    ofdmflexframesync_reset(rhc_ptr->ofdma_fs_inner);
    ofdmflexframesync_reset(rhc_ptr->ofdma_fs_outer);
    msresamp_crcf_reset(rhc_ptr->rx_resamp);
    firfilt_crcf_reset(rhc_ptr->rx_prefilt);
    //ofdmflexframesync_print(sync);
    const size_t max_samps_per_packet = rhc_ptr->usrp->get_device()->get_max_recv_samps_per_packet();
    std::vector<std::complex<float> > rx_usrp_buffer(20*max_samps_per_packet);
    std::complex<float> rx_temp_resample_buf[(int)(2.0f/rhc_ptr->rx_resamp_rate) + 64];

    uhd_error_stats_t uhd_error_stats;
    uhd::rx_metadata_t rx_md;
    unsigned int rx_uhd_recv_ctr = 0;
    rhc_ptr->usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    int continue_running = 1;
    timer t0 = timer_create();
    timer_tic(t0);
    double num_seconds = args->run_time;
    while (continue_running)
    {
        // grab data from device
        size_t uhd_num_delivered_samples = rhc_ptr->usrp->get_device()->recv(
                    &rx_usrp_buffer.front(),
                    rx_usrp_buffer.size(),
                    rx_md,
                    uhd::io_type_t::COMPLEX_FLOAT32,
                    uhd::device::RECV_MODE_FULL_BUFF
                    );

        // Check for UHD errors
            switch(rx_md.error_code)
            {
                //Keep running on these conditions
                    case uhd::rx_metadata_t::ERROR_CODE_NONE:
                    case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                        break;
                //Otherwise, capture details of non-trivial errori
                default:
                        uhd_error_stats.rx_error_timestamp = rhc_ptr->getHardwareTimestamp();
                        uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
                        uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;
                        uhd_error_stats.rx_error_num_samples = uhd_num_delivered_samples;
                rhc_ptr->reportUhdError();
                uhd_error_stats.rx_fail_frameburst++;
                return((void*)EXIT_FAILURE);
            }
            if(rhc_ptr->received_new_alloc)
                rhc_ptr->recreate_modem();
            //Get a lock on the sync before using it to make sure we don't try to 
            //execute samples while it is being recreated
            rhc_ptr->sync_mutex.lock();
            if(uhd_num_delivered_samples > 0)
            {
                if(rhc_ptr->allocation == INNER_ALLOCATION)
                    sync = rhc_ptr->ofdma_fs_inner;
                else if(rhc_ptr->allocation == OUTER_ALLOCATION)
                    sync = rhc_ptr->ofdma_fs_outer;
                else 
                    sync = rhc_ptr->ofdma_fs_default;
                for(unsigned int j = 0; j < uhd_num_delivered_samples; j++)
                {
                    // Prefilter samples; this may be optional in a lab environment
                    std::complex<float> usrp_sample = rx_usrp_buffer[j];
                    firfilt_crcf_push(rhc_ptr->rx_prefilt, usrp_sample);
                    firfilt_crcf_execute(rhc_ptr->rx_prefilt, &usrp_sample);

                    unsigned int nw;
                    msresamp_crcf_execute(rhc_ptr->rx_resamp, &usrp_sample, 1,
                            rx_temp_resample_buf, &nw);
                    ofdmflexframesync_execute(sync, rx_temp_resample_buf, nw);
                }

            }
            rhc_ptr->sync_mutex.unlock();
            if((num_seconds > 0 && timer_toc(t0) >= num_seconds) || rhc_ptr->join_rx_thread)
            {
                continue_running = 0;
            }
    }
    //std::cout << "shutting down receiver" << std::endl;
    pthread_exit(NULL);
}



