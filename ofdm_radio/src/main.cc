/* main.cc -- Demonstration program for the U1 waveform
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <csignal>

// Header files for U1 waveform application
#include "AppManager.h"
#include "FhSeqGenerator.h"
#include "FreqTableGenerator.h"
#include "HeartbeatDefs.h"
#include "Logger.hh"
#include "PacketStore.hh"
#include "Phy2Mac.h"
#include "RadioConfig.hh"
#include "RadioHardwareConfig.h"
#include "RadioScheduler.h"
#include "RadioTaskDefs.h"
#include "RadioTaskManager.h"
#include "RxPayload.hh"
#include "TxPayload.hh"
#include "TunTap.hh"
#include "timer.h"

long int bytes_received = 0;
long int last_bytes_received = 0;
long int last_bad_headers = 0;
long int last_bad_payloads = 0;
long int last_packets_received = 0;
long int last_packets_transmitted = 0;
int bad_counts[RHC_OFDMA_M] = {0};
using namespace std;
void openNullHole(unsigned char*, int, int);
volatile sig_atomic_t console_manual_termination_detected = false;
void consoleSignalHandler(int s) {
    console_manual_termination_detected = true;
}

double evaluate_throughput(timer t1, RadioHardwareConfig* rhc_ptr)
{
    long int total_bytes_received = rhc_ptr->valid_bytes_received;	
    long int difference = total_bytes_received - last_bytes_received;
    double throughput = (difference * 8 / 1024) / timer_toc(t1);
    last_bytes_received = total_bytes_received;
    timer_tic(t1);
    return throughput;
} 

void summarize_batch(unsigned int batch_count, float throughput, RadioHardwareConfig* rhc_ptr, bool mitigation_enabled,  bool jam_mitigation_running, int left_edge)
{
    long int new_bad_headers = rhc_ptr->invalid_headers_received - last_bad_headers;
    last_bad_headers = rhc_ptr->invalid_headers_received;
    long int new_bad_payloads = rhc_ptr->invalid_payloads_received - last_bad_payloads;
    last_bad_payloads = rhc_ptr->invalid_payloads_received;
    long int new_packets = rhc_ptr->valid_payloads_received - last_packets_received;
    last_packets_received = rhc_ptr->valid_payloads_received;
    long int new_transmits = rhc_ptr->total_packets_transmitted - last_packets_transmitted;
    last_packets_transmitted = rhc_ptr->total_packets_transmitted;
    if(!rhc_ptr->debug)
    {
        std::cout << "Batch ";
        if(rhc_ptr->rc->node_is_basestation || rhc_ptr->rc->uplink)
            std::cout << batch_count;
        std::cout << " summary:" << std::endl;
        std::cout << "Bad headers: " << new_bad_headers << std::endl;
        std::cout << "Bad payloads: " << new_bad_payloads << std::endl;
        if(rhc_ptr->rc->node_is_basestation)
        {
            std::cout << "Tx: " << new_transmits << " (" << rhc_ptr->total_packets_transmitted << " total, " << 
                rhc_ptr->total_packets_transmitted / (rhc_ptr->rc->num_nodes_in_net - 1) << " per mobile)" << std::endl;
        }
        else
        {
            std::cout << "Tx: " << new_transmits << " (" << rhc_ptr->total_packets_transmitted << " total)" << std::endl;
        }
        std::cout << "Rx: " << new_packets << " (" << rhc_ptr->valid_payloads_received << " total)" << std::endl;
        std::cout << "Batch throughput: " << throughput << std::endl;
        if(!rhc_ptr->rc->node_is_basestation)
        {
            std::cout << "Anti-jam mode: ";
            if(mitigation_enabled)
            {
                std::cout << "enabled, ";
                if(jam_mitigation_running)
                    std::cout << "hole at " << left_edge << std::endl;
                else
                    std::cout << "no hole" << std::endl;
            }
            else
                cout << "disabled" << std::endl;
        }


        std::cout << std::endl;
    }
}
int main(int argc, char **argv) {
    // This application was written for Ubuntu 12.04 LTS.
    // For porting signal() should be replaced with sigaction()
    signal(SIGINT, consoleSignalHandler);
    signal(SIGTERM, consoleSignalHandler);
    signal(SIGABRT, consoleSignalHandler);
    // Init Stage -----------------------------------------------
    //  Order of object initialization IS important 

    // Parse and validate settings in configuration file
    RadioConfig rc(argc, argv);
    rc.display_config();     //if (rc.debug) rc.display_config(); 

    // Overall application control
    AppManager app(rc.run_time, rc.debug);

    // Establish separate general purpose and RF event logging 
    Logger app_log(rc.app_log_file);
    app.doAppLogReport(&app_log, APP_LOG_REPORT_STARTED);
    Logger rf_log(rc.rf_log_file);

    //Just in case this hasn't already been done 
    std::system("sudo sysctl -w net.core.wmem_max=1048576");
    std::system("sudo sysctl -w net.core.rmem_max=50000000");

    timer rx_timer = timer_create();
    timer_tic(rx_timer);

    // Configure radio hardware based on user-specified preferences, and 
    // derive waveform parameters based on physical capabilities of the radio
    Logger rxf_event_log("rxf_event.log");
    Logger uhd_error_log("uhd_error.log");  
    PacketStore ps("tap0", rc.node_id, rc.num_nodes_in_net, rc.nodes_in_net, 
            rc.frame_size, rc.using_tun_tap);
    RadioHardwareConfig rhc(rc.radio_hardware, rc.usrp_address_name, 
            rc.radio_hardware_clock, rc.node_is_basestation, rc.node_id, rc.num_nodes_in_net, rc.frame_size,
            rc.normal_freq, rc.rf_gain_rx, rc.rf_gain_tx, rc.sample_rate, &app_log, &rf_log, 
            RXF_LOG_LEVEL_FILE_ONLY, &rxf_event_log, 
            UHD_ERROR_LOG_LEVEL_FILE_ONLY, &uhd_error_log,
            rc.debug, rc.u4, rc.using_tun_tap, &ps, &app, rx_timer, rc.slow, rc.ofdma_tx_window, rc.mc_tx_window,
            rc.anti_jam, &rc);

    // Precompute set of frequencies used in frequency hopping mode
    FreqTableGenerator ftg(rc.node_is_basestation, rc.normal_freq,
            rhc.getTx2RxFreqSeparation(), 
            rc.fh_freq_min, rc.fh_freq_max, rc.num_fh_prohibited_ranges, 
            rc.fh_prohibited_range_begin, rc.fh_prohibited_range_end,
            rhc.getFhWindowSmall(),  rhc.getFhWindowMedium(),
            rc.num_channels, rhc.isUhdRxTuningBugPresent(), rc.debug);

    // Calculate timing parameters of transmit & receive operations 
    RadioScheduler rs(rc.node_is_basestation, rc.node_id, 
            rc.num_nodes_in_net, rc.nodes_in_net,  HEARTBEAT_ACTIVITY_PER_SCHEDULE, 
            rc.num_channels, rhc.getRxRateMeasured(), rhc.getTxRateMeasured(),
            rhc.getTxBurstLength(), rhc.getUhdRetuneDelay(), rc.debug, rc.u4, rc.uplink);

    // This class handles interfacing between the physical and MAC (link layer)
    // as well as the overall MAC state variables
    Phy2Mac p2m(rc.node_is_basestation, rc.node_id, rc.num_nodes_in_net,
            rc.nodes_in_net, 
            HEARTBEAT_ACTIVITY_PER_SCHEDULE, HEARTBEAT_POLICY_A,
            P2M_FRAME_HEADER_DEFAULT_SIZE, P2M_FRAME_PAYLOAD_DEFAULT_SIZE,
            "tap0",
            rc.debug, rc.using_tun_tap, &ps);

    // When in frequency hopping mode follow a pre-computed sequence of frequencies;
    // this class does not work with the actual frequencies, only the indicies of
    // the frequency tables already generated by FreqTableGenerator
    FhSeqGenerator fsg(FH_SEQ_RESTART_ALG_A, rs.getFhTaskSchedSize(), 
            ftg.getRfTableSize(), ftg.getDspTableSize(), rc.num_channels, 
            rc.debug);
    fsg.makeSeq();


    // For the U1 waveform "normal" mode is frequency division duplex (FDD)
    // and otherwise it is in frequency hopping (FH) mode
    bool waveform_is_normal = true;
    RadioTaskManager rtm(&fsg, &ftg, &rhc, &rs, &p2m, rc.debug, rc.u4);

    rs.calcU4Schedule();
    // Initialization complete 
    // Log details of actual operating configuration
    app.doAppLogReport(&app_log, APP_LOG_REPORT_INIT_DONE); 
    app_log.write_log();

    // Radio communications stage ----------------------------------------
    //  This loop periodically generates a schedule of radio activity,
    //  communicates at the scheduled timeslots and evaluates the 
    //  "heartbeat" signal to determine if the operating mode should
    //  switch between normal, frequency division duplex (FDD), mode or
    //  frequency hopping (FH) mode


    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);
    pthread_t thread_id;
    pthread_t rx_task_thread = (pthread_t) 0;
    void* thread_status;
    int thread_return_value;
    int thread_creation_status;
    double run_time;
    double elapsed_time;
    double remaining = 0.0;

    timer rx_runtime = timer_create();
    timer throughput_timer = timer_create();
    timer good_throughput_timer = timer_create();
    timer mitigation_timer = timer_create();
    timer enable_mitigation_timer = timer_create();
    timer resend_alloc_timer = timer_create();
    timer_tic(resend_alloc_timer);

    bool sweeping = true;
    int left_edge = 0;
    int direction = 1;
    bool jam_mitigation_running = false;
    bool mitigation_enabled = true;
    bool resuming = false;
    bool check_full_band = true;

    if(rc.u4)
    {
        rx_thread_args_t rx_thread_args;
        run_time = rc.run_time;
        elapsed_time = app.getElapsedTime();
        remaining = run_time - elapsed_time;
        //rx_thread_args.run_time = rc.run_time - app.getElapsedTime() + 3.0;
        rx_thread_args.run_time = remaining + 1.0;
        rx_thread_args.rhc_ptr = &rhc;
        if(!rc.node_is_basestation)
        {
            thread_creation_status = pthread_create(&thread_id, &pthread_attr, run_ofdma_rx, (void*)&rx_thread_args);
            if(thread_creation_status != 0)
                std::cout << "error creating rx thread" << std::endl;
            rx_task_thread = thread_id;
        }
        else
        {
            thread_creation_status = pthread_create(&thread_id, &pthread_attr, run_mc_rx, (void*)&rx_thread_args);
            if(thread_creation_status != 0)
                std::cout << "error creating rx thread" << std::endl;
            rx_task_thread = thread_id;

        }
        timer_tic(rx_runtime);
        timer_tic(throughput_timer);
        //Wait 1 second to let other nodes start their receivers before transmitting
        timer t1 = timer_create();
        timer_tic(t1);
        while(timer_toc(t1) < 1.0);
        timer_destroy(t1);
    }
    unsigned int task_ctr;
    unsigned int num_scheduled_tasks;
    unsigned int batch_count = 0;
    double throughput = 0.0;
    timer second_timer = timer_create();
    timer_tic(second_timer);
    if(rc.manual_mode)
        std::cout << "Initialization complete. Press any key to send packets." << std::endl;
    while( app.isContinuing() ) {
        if ( !waveform_is_normal) {
            fsg.makeSeq();
            fsg.scrubSeq();
        }
        num_scheduled_tasks = rs.getActiveSchedSize(waveform_is_normal);
        for (task_ctr = 0; task_ctr < num_scheduled_tasks; task_ctr++) {
            // Complete transmit-side MAC activity prior to radio task and
            // receive-side MAC activity after radio task
            //p2m.updateMacPreTask( rs.getCurrentTxTask(waveform_is_normal, task_ctr) ); 
            if(rc.manual_mode)
                cin.ignore();
            rtm.doTask(waveform_is_normal, task_ctr);
        }

        app.doAppLogReport(&app_log, APP_LOG_REPORT_SCHEDULE_COUNT);
        rf_log.write_log();

        // Check run conditions; do misc tasks not handled elsewhere
        app.updateStatus();

        //Evaluate throughput and engage anti-jamming mode if necessary
        throughput = evaluate_throughput(throughput_timer, &rhc);
        summarize_batch(batch_count, throughput, &rhc, mitigation_enabled, jam_mitigation_running, left_edge);
        if(mitigation_enabled && rhc.valid_payloads_received > 0)
        {
            if(rc.anti_jam && !rc.node_is_basestation && batch_count > 3)
            {
                //Threshold to start or continue anti-jamming mode
                if(throughput < rc.jamming_threshold)
                {
                    if(jam_mitigation_running)
                    {
                        //If jam mitigation has been on for 10 seconds and the throughput is still below
                        //the threshold, turn it off since it's not working.
                        if(timer_toc(mitigation_timer) > rc.mitigation_timeout && false)
                        {
                            std::stringstream report;
                            report << scientific << app.getElapsedTime();
                            report << "    Main: ";
                            report << "jam mitigation ineffective, resuming normal mode";
                            std::cout << "jam mitigation ineffective, resuming normal mode" << std::endl;
                            app_log.log(report.str());
                            app_log.write_log();
                            unsigned char* alloc = new unsigned char[RHC_OFDMA_M];
                            ofdmframe_init_sctype(RHC_OFDMA_M, alloc, .05);

                            if(rc.node_id == 1)
                            {
                                rhc.txMCAllocBurst(alloc);
                            }
                            memcpy(rhc.new_alloc, alloc, RHC_OFDMA_M);

                            rhc.recreate_modem();

                            jam_mitigation_running = false;
                            //If the jammer has not been turned off, we'll want to reopen the hole
                            //in the same place. The resuming variablre is used below to keep it from moving when 
                            //the hole initially opens back up
                            resuming = true;
                            direction *= -1;
                            timer_tic(enable_mitigation_timer);
                            mitigation_enabled = false;
                            sweeping = false;
                            rs.setU4ScheduleSize(40);
                            continue;
                        }
                    }
                    //If not already sweeping, switch to anti-jam mode
                    if(!sweeping)
                    {
                        std::stringstream report;
                        report << scientific << app.getElapsedTime();
                        report << "    Main: ";
                        report << "switched to anti-jam mode";
                        app_log.log(report.str());
                        app_log.write_log();
                        timer_tic(mitigation_timer);
                        jam_mitigation_running = true;
                        std::cout << "activating anti-jam mode" << std::endl;
                    }

                    sweeping = true;
                    rs.setU4ScheduleSize(5);
                    timer_tic(good_throughput_timer);
                }
                //Throughput is above threshold
                //Jammer might be off or anti-jam mode has found jammer and is successfully mitigating
                else
                {

                    check_full_band = true;
                    timer_tic(mitigation_timer);
                    //Stop sweeping null nole
                    sweeping = false;
                    rs.setU4ScheduleSize(40);
                    //If a hole is currently open...
                    if(jam_mitigation_running)
                    {
                        //...periodically try closing hole in case jammer has been turned off
                        if(timer_toc(good_throughput_timer) > rc.close_hole_timeout)
                        {
                            std::stringstream report;
                            report << scientific << app.getElapsedTime();
                            report << "    Main: ";
                            report << "resuming normal mode";
                            std::cout << "resuming normal mode" << std::endl;
                            app_log.log(report.str());
                            app_log.write_log();
                            unsigned char* alloc = new unsigned char[RHC_OFDMA_M];
                            ofdmframe_init_sctype(RHC_OFDMA_M, alloc, .05);
                            memcpy(rhc.new_alloc, alloc, RHC_OFDMA_M);
                            rhc.recreate_modem();
                            if(rc.node_id == 1)
                            {
                                rhc.txMCAllocBurst(alloc);
                                rhc.txMCAllocBurst(alloc);
                                rhc.txMCAllocBurst(alloc);
                                rhc.txMCAllocBurst(alloc);
                                rhc.txMCAllocBurst(alloc);
                            }
                            jam_mitigation_running = false;
                            //If the jammer has not been turned off, we'll want to reopen the hole
                            //in the same place. The resuming variablre is used below to keep it from moving when 
                            //the hole initially opens back up
                            resuming = true;
                            direction *= -1;
                            rs.setU4ScheduleSize(40);
                            usleep(.05*1000000);
                        }
                    }
                }
                if(sweeping)
                {
                    if(check_full_band && rc.node_id > 1)
                    {
                        unsigned char* alloc = new unsigned char[RHC_OFDMA_M];
                        ofdmframe_init_sctype(RHC_OFDMA_M, alloc, .05);
                        memcpy(rhc.new_alloc, alloc, RHC_OFDMA_M);
                        rhc.recreate_modem();
                        jam_mitigation_running = false;
                        check_full_band = false;
                        rs.setU4ScheduleSize(10);
                    }
                    else
                    {
                        std::stringstream report;
                        report << scientific << app.getElapsedTime();
                        report << "    Main: ";
                        report << "Repositioning null hole, left edge: " << left_edge;
                        std::cout << "Repositioning null hole, left edge: " << left_edge << std::endl;;

                        app_log.log(report.str());
                        app_log.write_log();
                        unsigned char* alloc = new unsigned char[RHC_OFDMA_M];
                        ofdmframe_init_sctype(RHC_OFDMA_M, alloc, .05);
                        openNullHole(alloc, left_edge, left_edge + 150);
                        if(rc.node_id == 1)
                        {
                            rhc.txMCAllocBurst(alloc);
                            rhc.txMCAllocBurst(alloc);
                            rhc.txMCAllocBurst(alloc);
                            rhc.txMCAllocBurst(alloc);
                            rhc.txMCAllocBurst(alloc);
                        }
                        memcpy(rhc.new_alloc, alloc, RHC_OFDMA_M);
                        rhc.recreate_modem();
                        jam_mitigation_running = true;
                        if(left_edge <= 0)
                        {
                            left_edge = 0;
                            direction = 1;
                            check_full_band = true;
                        }
                        else if(left_edge >= 412)
                        {
                            direction = -1;
                            check_full_band = true;
                        }
                        //After a hole is opened and a jammer is found, the radio will periodically try to 
                        //fill the whole in case the jammer is gone. If it's not, we want the hole to open 
                        //again where it was before to stay right on the jammer
                        //The resuming boolean does this
                        if(!resuming)
                            left_edge += (direction * 25); 
                        else
                        {
                            resuming = false;
                            rs.setU4ScheduleSize(10);
                        }

                    }

                }
            }
        }
        else
        {
            if(!rc.node_is_basestation)
            {	
                if(timer_toc(resend_alloc_timer) > 1.0 && rc.node_id == 1)
                {
                    timer_tic(resend_alloc_timer);
                    unsigned char* alloc = new unsigned char[RHC_OFDMA_M];
                    ofdmframe_init_sctype(RHC_OFDMA_M, alloc, .05);
                    rhc.txMCAllocBurst(alloc);
                }
                if(timer_toc(enable_mitigation_timer) > rc.mitigation_reenable_timeout)
                {
                    std::cout << "re-enabling anti-jam mode" << std::endl;
                    mitigation_enabled = true;
                }
            }
        }
        if (console_manual_termination_detected) {
            rhc.exit_rx_thread();
            app.setManualTerminationState(true);
        }
        batch_count++;


    }
    app.doAppLogReport(&app_log, APP_LOG_REPORT_RUN_COMPLETE);
    app_log.write_log();

    thread_return_value = pthread_join(rx_task_thread, &thread_status);
    double runtime = timer_toc(rx_runtime);
    if(thread_return_value != 0)
        std::cout << "Error joining rx thread" << std::endl;
    pthread_attr_destroy(&pthread_attr);

    std::cout << std::endl;  
    std::cout << "On air for " << runtime << " seconds." << std::endl;
    std::cout << "Rx throughput: " << ((rhc.valid_bytes_received * 8) / 1024) / runtime << " kbps" <<
        std::endl;
    std::cout << "Received and wrote " << ps.get_written_packets() << " to network" << std::endl;

    // Finalize end of application --------------------------------------- 
    rhc.writeRfEventLog();
    app.doAppLogReport(&app_log, APP_LOG_REPORT_FINALIZATION_DONE);
    app_log.write_log();
    if(rc.using_tun_tap)
        p2m.close_interface();


#if 0
    //### testing
    cout << "\n### This end summary is only for development testing " << endl;
    rhc.printUhdErrorStats(); 
    p2m.printFrameStats();
#endif

    return(EXIT_SUCCESS);
}


