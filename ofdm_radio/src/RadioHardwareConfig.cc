/* RadioHardwareConfig.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 *
 */
#include "RadioHardwareConfig.h"
#include "Allocations.h"
bool ext_using_tun_tap = false;
bool ext_debug = false;

PacketStore* ext_ps_ptr;
RadioHardwareConfig* ext_rhc_ptr;
AppManager* ext_am_ptr;
Logger* ext_packet_log_ptr;
int evm_cutoff = 3;
double evm_sum = 0;
double evm_min = 1000;
double evm_max = -1000;
double evm_avg = 0;
double evm_stddev = 0;
using namespace std;

int ofdmaCallback(
        unsigned char *  _header,
        int              _header_valid,
        unsigned char *  _payload,
        unsigned int     _payload_len,
        int              _payload_valid,
        framesyncstats_s _stats,
        void *           _userdata
        )
{
    ext_rhc_ptr->total_packets_received++;
    timer rx_timer = (timer_s*)_userdata;
    timer_tic(rx_timer);
    if(ext_debug)printf("***** rssi=%7.2fdB evm=%7.2fdB, ", _stats.rssi, _stats.evm);
    std::stringstream report;
    report << "***** rssi:" << setw(10) << _stats.rssi << "db evm:" << setw(10) << _stats.evm << "db, ";
    if (_header_valid) 
    {
        ext_rhc_ptr->valid_headers_received++;
        if(_header[P2M_HEADER_FIELD_FRAME_TYPE] == P2M_FRAME_TYPE_DATA)
        {
            if (_payload_valid)
            {
                unsigned int key1 = _payload[0];
                unsigned int key2 = _payload[1];
                if(key1 == 42 && key2 == 37)
                {
                    long int* li_payload = (long int*)(_payload + 2);
                    unsigned long packet_id = li_payload[0];
                    unsigned int source_id = _header[P2M_HEADER_FIELD_SOURCE_ID];
                    if(ext_debug)printf("rx packet id: %6lu", packet_id);
                    report << "rx packet id: " << packet_id;
                    if(ext_debug)printf(" payload_len: %u", _payload_len);
                    report << " payload_len: " << _payload_len;

                    unsigned int total_packet_len = (_payload[2 + sizeof(long int)] << 8 | _payload[2 + sizeof(long int) + 1]);
                    if(total_packet_len == 0)	
                        return 1;
                    unsigned int frame_id = _payload[2 + sizeof(long int) + 2];
                    if(ext_using_tun_tap)
                    {
                        ext_ps_ptr->add_frame(packet_id, frame_id, _payload + PADDED_BYTES, total_packet_len);
                    }
                    ext_rhc_ptr->valid_bytes_received += _payload_len - PADDED_BYTES;
                    ext_rhc_ptr->network_packets_received++;

                }
                else
                {
                    if(ext_debug)printf(" payload_len: %u", _payload_len);
                    report << " payload_len: " << _payload_len;
                    ext_rhc_ptr->valid_bytes_received += _payload_len;
                    ext_rhc_ptr->dummy_packets_received++;
                }
                ext_rhc_ptr->valid_payloads_received++;
                if(ext_debug)printf("\n");
                // If valid frame received then generate a report
                rf_log_report_t rf_log_report;
                switch (ext_rhc_ptr->rf_log_level) {
                    case(RF_LOG_LEVEL_NONE) :
                        // no logging
                        break;
                    case(RF_LOG_LEVEL_NORMAL) :
                        rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
                        rf_log_report.rf_event = RF_LOG_EVENT_RX_OFDMA_DATA;
                        rf_log_report.frequency_nominal = ext_rhc_ptr->getRxAbsoluteFreq();
                        rf_log_report.bandwidth = ext_rhc_ptr->sample_rate;
                        ext_rhc_ptr->logRfEvent(rf_log_report);
                        break;
                    case(RF_LOG_LEVEL_DETAIL) :
                        // PLACEHOLDER for development version of logging
                        break;
                    default :
                        cerr << "\nERROR in RadioHardwareConfig::rxFrameBurst: ";
                        cerr << "Unknown rf_log_level\n" << endl;
                        exit(EXIT_FAILURE);
                        break;
                }
            }
            else
            {
                if(ext_debug)printf(" PAYLOAD INVALID\n");
                report << " PAYLOAD INVALID";
                ext_rhc_ptr->invalid_payloads_received++;
                //else printf("p");
            }
        }
        //FRAME TYPE was not data, control packet to switch pilot allocation
        else if(_header[P2M_HEADER_FIELD_FRAME_TYPE] == P2M_FRAME_TYPE_CONTROL)
        {
            std::cout << "received control packet" << std::endl;
            // If valid frame received then generate a report
            rf_log_report_t rf_log_report;
            switch (ext_rhc_ptr->rf_log_level) {
                case(RF_LOG_LEVEL_NONE) :
                    // no logging
                    break;
                case(RF_LOG_LEVEL_NORMAL) :
                    rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
                    rf_log_report.rf_event = RF_LOG_EVENT_RX_CONTROL_DATA;
                    rf_log_report.frequency_nominal = ext_rhc_ptr->getRxAbsoluteFreq();
                    rf_log_report.bandwidth = ext_rhc_ptr->sample_rate;
                    ext_rhc_ptr->logRfEvent(rf_log_report);
                    break;
                case(RF_LOG_LEVEL_DETAIL) :
                    // PLACEHOLDER for development version of logging
                    break;
                default :
                    cerr << "\nERROR in RadioHardwareConfig::rxFrameBurst: ";
                    cerr << "Unknown rf_log_level\n" << endl;
                    exit(EXIT_FAILURE);
                    break;
            }
        }
        else if(_header[P2M_HEADER_FIELD_FRAME_TYPE] == P2M_FRAME_TYPE_NEW_ALLOC)
        {
            if(_payload_valid)
            {
                ext_rhc_ptr->received_new_alloc = true;
                memcpy(ext_rhc_ptr->new_alloc, _payload, RHC_OFDMA_M);
            }
        }
    }
    //Packet detected but header invalid
    else
    {
        if(ext_debug)printf("HEADER INVALID\n");
        report << "HEADER INVALID";
        ext_rhc_ptr->invalid_headers_received++;
    }
    ext_packet_log_ptr->log(report.str());
    ext_packet_log_ptr->write_log();
    //ext_rhc_ptr->setHardwareTimestamp(0.0);
    return 0;
}
int mcCallback(
        unsigned char *  _header,
        int              _header_valid,
        unsigned char *  _payload,
        unsigned int     _payload_len,
        int              _payload_valid,
        framesyncstats_s _stats,
        void *           _userdata
        )
{
    ext_rhc_ptr->total_packets_received++;
    timer rx_timer = (timer_s*)_userdata;
    timer_tic(rx_timer);
    if(ext_debug)printf("***** rssi=%7.2fdB evm=%7.2fdB, ", _stats.rssi, _stats.evm);
    std::stringstream report;
    report << "***** rssi:" << setw(10) << _stats.rssi << "db evm:" << setw(10) << _stats.evm << "db, ";
    if (_header_valid) {
        if(_header[P2M_HEADER_FIELD_FRAME_TYPE] == P2M_FRAME_TYPE_DATA)
        {
            ext_rhc_ptr->valid_headers_received++;
            if (_payload_valid)
            {   
                unsigned int key1 = _payload[0];
                unsigned int key2 = _payload[1];
                if(key1 == 42 && key2 == 37)
                {
                    long int* li_payload = (long int*)(_payload + 2);
                    unsigned long packet_id = li_payload[0];
                    unsigned int source_id = _header[P2M_HEADER_FIELD_SOURCE_ID];
                    if(ext_debug)printf("rx packet id: %6lu", packet_id);
                    report << "rx packet id: " << packet_id << " from " << source_id;
                    if(ext_debug)printf(" payload_len: %u", _payload_len);
                    report << " payload_len: " << _payload_len;
                    unsigned int total_packet_len = (_payload[2 + sizeof(long int)] << 8 | _payload[2 + sizeof(long int) + 1]);
                    if(total_packet_len == 0)	
                        return 1;
                    unsigned int frame_id = _payload[2 + sizeof(long int) + 2];
                    if(ext_using_tun_tap)
                    {
                        ext_ps_ptr->add_frame(packet_id, frame_id, _payload + PADDED_BYTES, total_packet_len);
                    }
                    ext_rhc_ptr->valid_bytes_received += _payload_len - PADDED_BYTES;
                    ext_rhc_ptr->network_packets_received++;
                }
                else
                {
                    if(ext_debug)printf(" payload_len: %u", _payload_len);
                    report << " payload_len: " << _payload_len;
                    ext_rhc_ptr->valid_bytes_received += _payload_len;
                    ext_rhc_ptr->dummy_packets_received++;
                }
                ext_rhc_ptr->valid_payloads_received++;
                if(ext_debug)printf("\n");
                rf_log_report_t rf_log_report;
                switch (ext_rhc_ptr->rf_log_level) {
                    case(RF_LOG_LEVEL_NONE) :
                        // no logging
                        break;
                    case(RF_LOG_LEVEL_NORMAL) :
                        rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
                        rf_log_report.rf_event = RF_LOG_EVENT_RX_MC_DATA;
                        rf_log_report.frequency_nominal = ext_rhc_ptr->getRxAbsoluteFreq();
                        rf_log_report.bandwidth = ext_rhc_ptr->sample_rate;
                        ext_rhc_ptr->logRfEvent(rf_log_report);
                        break;
                    case(RF_LOG_LEVEL_DETAIL) :
                        // PLACEHOLDER for development version of logging
                        break;
                    default :
                        cerr << "\nERROR in RadioHardwareConfig::rxFrameBurst: ";
                        cerr << "Unknown rf_log_level\n" << endl;
                        exit(EXIT_FAILURE);
                        break;
                }
            }
            else
            {
                ext_rhc_ptr->invalid_payloads_received++;
                if(ext_debug)printf(" PAYLOAD INVALID\n");
                report << " PAYLOAD_INVALID";
            }
        }
        else if(_header[P2M_HEADER_FIELD_FRAME_TYPE] == P2M_FRAME_TYPE_CONTROL)
        {
            std::cout << "control packet received" << std::endl;
            // If valid frame received then generate a report
            rf_log_report_t rf_log_report;
            switch (ext_rhc_ptr->rf_log_level) {
                case(RF_LOG_LEVEL_NONE) :
                    // no logging
                    break;
                case(RF_LOG_LEVEL_NORMAL) :
                    rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
                    rf_log_report.rf_event = RF_LOG_EVENT_RX_CONTROL_DATA;
                    rf_log_report.frequency_nominal = ext_rhc_ptr->getRxAbsoluteFreq();
                    rf_log_report.bandwidth = ext_rhc_ptr->sample_rate;
                    ext_rhc_ptr->logRfEvent(rf_log_report);
                    break;
                case(RF_LOG_LEVEL_DETAIL) :
                    // PLACEHOLDER for development version of logging
                    break;
                default :
                    cerr << "\nERROR in RadioHardwareConfig::rxFrameBurst: ";
                    cerr << "Unknown rf_log_level\n" << endl;
                    exit(EXIT_FAILURE);
                    break;
            }
            ext_rhc_ptr->switch_allocation();
        }
        else if(_header[P2M_HEADER_FIELD_FRAME_TYPE] == P2M_FRAME_TYPE_NEW_ALLOC)
        {
            if(_payload_valid)
            {
                memcpy(ext_rhc_ptr->new_alloc, _payload, RHC_OFDMA_M);
                ext_rhc_ptr->recreate_modem();
            }
        }
    }
    else
    {
        ext_rhc_ptr->invalid_headers_received++;
        report << "HEADER INVALID";
        if(ext_debug)printf("HEADER INVALID\n");
    }
    fflush(stdout);
    ext_packet_log_ptr->log(report.str());
    ext_packet_log_ptr->write_log();
    return 0;
}

int rxCallback(
        unsigned char *  _header,
        int              _header_valid,
        unsigned char *  _payload,
        unsigned int     _payload_len,
        int              _payload_valid,
        framesyncstats_s _stats,
        void *           _userdata
        )
{
    received_frame_t* rxfp;
    rxfp = (received_frame_t *) _userdata;

    rxfp->frame_was_detected = true;
    rxfp->stats_cfo = _stats.cfo * rxfp->samplerate / (2*M_PI);
    rxfp->stats_evm = _stats.evm;
    rxfp->stats_rssi = _stats.rssi;
    rxfp->header_is_valid = (bool) _header_valid;
    rxfp->payload_is_valid = (bool) _payload_valid;

    // The following are NOT calculated here in order to keep the CPU 
    // burden of this callback function low:
    //  rxfp->frame_end_noise_level
    //  rxfp->frame_end_is_noisy
    //  rxfp->rx_complete_timestamp

    // Any special debugging can be triggered with if(rxfp->callback_debug), but
    // less intrusive logging code can work with the rxfp structure elsewhere

    if ( !rxfp->header_is_valid ) {
        rxfp->frame_is_valid = false;
        return(EXIT_SUCCESS);
    }

    if ( !rxfp->payload_is_valid ) {
        rxfp->frame_is_valid = false;
        return(EXIT_SUCCESS);
    } 

    // Protect against overrun of frame buffer
    rxfp->payload_size = _payload_len;
    if (rxfp->payload_size > RHC_FRAME_PAYLOAD_DEFAULT_SIZE) {
        rxfp->frame_is_valid = false;
        return(EXIT_SUCCESS);
    } 

    rxfp->frame_is_valid = true;
    memcpy(rxfp->frame_header, _header, RHC_FRAME_HEADER_DEFAULT_SIZE);
    memcpy(rxfp->frame_payload, _payload, rxfp->payload_size);

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

RadioHardwareConfig::RadioHardwareConfig(
        std::string radio_hardware,
        std::string usrp_address_name,
        std::string radio_hardware_clock,
        bool node_is_basestation,
        unsigned int node_id,
        unsigned int num_nodes_in_net,
        unsigned int frame_size,
        double normal_freq,
        double rf_gain_rx,
        double rf_gain_tx,
        double sample_rate,
        Logger* app_log_ptr,
        Logger* rf_log_ptr,
        RxfEventLogLevelType rxf_event_log_level,
        Logger* rxf_event_log_ptr,
        UhdErrorLogLevelType uhd_error_log_level,
        Logger* uhd_error_log_ptr,
        bool debug, bool u4, bool using_tun_tap,
        PacketStore* ps, AppManager* app,
        timer_s* rx_timer, bool slow, float ofdma_tx_window, float mc_tx_window, bool anti_jam,
        RadioConfig* rc
)
{
    ext_using_tun_tap = using_tun_tap;
    ext_ps_ptr = ps;
    ext_am_ptr = app;
    ext_debug = debug;
    if (radio_hardware.compare("USRP_MODEL_N210") == 0) {
        usrp_hardware = USRP_MODEL_N210;

    } else {
        if (radio_hardware.compare("USRP_MODEL_X300_PCIE") == 0) {
            usrp_hardware = USRP_MODEL_X300_PCIE;

        } else {
            if (radio_hardware.compare("USRP_MODEL_X300_GBE") == 0) {
                usrp_hardware = USRP_MODEL_X300_GBE;

            } else {
                cerr << "ERROR: in RadioHardwareConfig::RadioHardwareConfig"<< endl;
                cerr << "       " << radio_hardware << " is unsupported" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    this->usrp_address_name = usrp_address_name;

    if (radio_hardware_clock.compare("CLOCK_REF_GPSDO") == 0) {
        clock_ref_type = CLOCK_REF_GPSDO;

    } else {
        if (radio_hardware_clock.compare("CLOCK_REF_LAB") == 0) {
            clock_ref_type = CLOCK_REF_LAB;

        } else {
            if (radio_hardware_clock.compare("CLOCK_REF_NONE") == 0) {
                clock_ref_type = CLOCK_REF_NONE;

            } else {
                cerr << "ERROR: in RadioHardwareConfig::RadioHardwareConfig"<<endl;
                cerr << "       "<< radio_hardware_clock <<" is unsupported" <<endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    this->node_is_basestation = node_is_basestation;
    this->node_id = node_id;
    this->num_nodes_in_net = num_nodes_in_net;
    this->frame_len = frame_size;
    this->normal_freq = normal_freq;
    this->rf_gain_rx = rf_gain_rx;
    this->rf_gain_tx = rf_gain_tx;
    this->sample_rate = sample_rate;
    this->app_log_ptr = app_log_ptr;
    this->rf_log_ptr = rf_log_ptr;
    this->rxf_event_log_level = rxf_event_log_level;
    this->rxf_event_log_ptr = rxf_event_log_ptr;
    this->uhd_error_log_level = uhd_error_log_level;
    this->uhd_error_log_ptr = uhd_error_log_ptr;
    this->debug = debug;
    this->u4 = u4;
    this->rx_timer = rx_timer;
    this->slow = slow;
    this->ofdma_tx_window = ofdma_tx_window;
    this->mc_tx_window = mc_tx_window;
    this->anti_jam = anti_jam;
    this->rc = rc;
    this->alloc_log_ptr = new Logger(this->rc->alloc_log_file);
    ext_packet_log_ptr = new Logger(this->rc->packet_log_file);
    hardened = this->rc->hardened;
    
    //Initialize stats
    valid_bytes_received = 0;
    total_packets_transmitted = 0;
    total_packets_received = 0;
    valid_headers_received = 0;
    invalid_headers_received = 0;
    valid_payloads_received = 0;
    invalid_payloads_received = 0;
    network_packets_transmitted = 0;
    network_packets_received = 0;
    dummy_packets_transmitted = 0;
    dummy_packets_received = 0;

    //Initialize subcarrier allocation mode for U4
    allocation = DEFAULT_ALLOCATION;
    received_new_alloc = false;

    //We'll use tx_frame_payload to send dummy packets when no data is available from rapr
    //Place zeros in the first 2 slots. When packets are received, we look for 42 and 37
    //in those slots to indicate the packet is from rapr. The zeros makes sure the 
    //(extremely unlikely) case of getting 42 and 37 from rand() doesn't happen
    tx_frame_payload[0] = 0;
    tx_frame_payload[1] = 0;
    for(int i = 2; i < P2M_FRAME_PAYLOAD_MAX_SIZE; i++)
        tx_frame_payload[i] = (unsigned char)(rand() & 0x00);

    // RF event logging is configured and controlled exclusively
    // within this class, unlike the overall application logging
    rf_log_level = RF_LOG_LEVEL_NORMAL;
    switch(rf_log_level) {
        case RF_LOG_LEVEL_NONE :
            break;
        case RF_LOG_LEVEL_NORMAL :
            rf_log_ptr->log("% RF Event Log");
            rf_log_ptr->log("% ");
            rf_log_ptr->log("% The column fields from left to right are:");
            rf_log_ptr->log("%   1: radio hardware's timestamp of event");
            rf_log_ptr->log("%   2: integer code for type of RF event:");
            rf_log_ptr->log("%      0: TX_HEARTBEAT, 1: TX_DATA, 2: RX_HEARTBEAT, 3: RX_DATA");
            rf_log_ptr->log("%      4: TX_MC_DATA, 5: TX_OFDMA_DATA, 6: RX_MC_DATA, 7: RX_OFDMA_DATA");
            rf_log_ptr->log("%      8: TX_CONTROL_DATA, 9: RX_CONTROL_DATA");
            rf_log_ptr->log("%   3: center frequency");
            rf_log_ptr->log("%   4: bandwidth");
            rf_log_ptr->log("%  ");
            rf_log_ptr->write_log();
            break;
        case RF_LOG_LEVEL_DETAIL :    
            rf_log_ptr->log("% RF Event Log (RF_LOG_LEVEL_DETAIL)");
            rf_log_ptr->log("% RF_LOG_LEVEL_DETAIL is currently unimplemented");
            rf_log_ptr->write_log();
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig constructor: ";
            cerr << "unknown rf_log_level" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    // General initialization of the USRP --------------------------------
    switch (usrp_hardware) {
        case USRP_MODEL_N210 :
            tx2rx_freq_separation = rc->fdd_separation;
            fh_window_small = sample_rate;
            fh_window_medium = RHC_USRP_N210_FH_WINDOW_MEDIUM;
            uhd_retune_delay = RHC_USRP_N210_RETUNE_DELAY;
            break;
        case USRP_MODEL_X300_PCIE :
        case USRP_MODEL_X300_GBE :
            tx2rx_freq_separation = rc->fdd_separation;
            fh_window_small = sample_rate;
            fh_window_medium = RHC_USRP_X300_FH_WINDOW_MEDIUM;
            uhd_retune_delay = RHC_USRP_X300_RETUNE_DELAY;
            break;
        default :
            cerr << "ERROR: In RadioHardwareConfig unsupported type of USRP.\n" <<endl;
            exit(EXIT_FAILURE);
            break;
    }

    // USRP manual state thats the message handler should be the first call
    uhd::msg::register_handler(&handleUhdMessage);

    uhd::device_addr_t dev_addr;     
    if (usrp_hardware == USRP_MODEL_X300_PCIE) {
        dev_addr["resource"] = "RIO0";
    } else {
        if(usrp_address_name != "")
        dev_addr["addr0"] = usrp_address_name;
    }
    try {
        usrp = uhd::usrp::multi_usrp::make(dev_addr);
    } catch (...) {
        cerr << "ERROR: In RadioHardwareConfig initialization: ";
        cerr << "Unable to access the specified USRP" << endl;
        if (usrp_hardware == USRP_MODEL_X300_PCIE) {
            cerr << "\nNOTE: When USRP_MODEL_X300_PCIE is specified " << endl;
            cerr << "ensure that the X300 has the appropriate FPGA bit file" << endl;
            cerr << "Also, confirm that the NI real time I/O module is loaded:" << endl;
            cerr << "  sudo /usr/local/bin/niusrprio_pcie start \n" << endl;
        }
        exit(EXIT_FAILURE);
    }

    resetUhdErrorStats();

    // Receive side USRP configuration -----------------------------------
    usrp->set_rx_antenna("RX2");
    usrp->set_rx_gain(rf_gain_rx);

    // Configure receiver for manual frequency tuning
    //  The actual frequency will change depending on operating mode, but
    //  this at least starts the node in the correct radio band
    rx_absolute_freq = 0;   tx_absolute_freq = 0; 
    tune2NormalFreq();

    usrp->set_rx_rate(RHC_NOMINAL_RESAMPLER_RATIO * sample_rate);
    usrp_rx_rate = usrp->get_rx_rate();
    rx_resamp_rate = sample_rate / usrp_rx_rate; 

    rx_resamp = msresamp_crcf_create(rx_resamp_rate, 60.0f);
    // Resampler buffers and UHD sample buffers now within the class 
    // methods that actually perform burst tx & rx

    // Create rx streamer object for burst receiption
    uhd::stream_args_t rx_stream_args("fc32"); //complex floats
    rx_stream = usrp->get_rx_stream(rx_stream_args); 
    rx_uhd_transport_size = rx_stream->get_max_num_samps();
    rx_uhd_max_buffer_size = rx_uhd_transport_size +64;

    // Set number of rx samples to a default to permit other stages of
    // initialization (e.g., heartbeat noise calibration) to work without
    // having first performed the normal radio task scheduling calculations
    initial_rx_recommended_sample_size = RHC_RX_RECOMMENDED_SAMPLE_SIZE_DEFAULT;

    // Receive side modem configuration ----------------------------------
    rx_prefilt = firfilt_crcf_create_kaiser(31, 0.24f, 60.0f, 0.0f);
    rxf.samplerate = usrp_rx_rate;
    rxf.callback_debug = false;

    void * userdata[num_nodes_in_net];
    framesync_callback callbacks[num_nodes_in_net];
    for(unsigned int i = 0; i < num_nodes_in_net; i++)
    {
        userdata[i] = (void*)rx_timer;
        callbacks[i] = mcCallback;
    }
    if(u4)
    {
        //Initialize subcarriers with 5% guard bands on both sides = 10% guard
        //5Mhz - 10% guard = 4.5Mhz we're looking for to look like LTE
        unsigned char* inner_subcarrier_allocation = new unsigned char[RHC_OFDMA_M];
        unsigned char* outer_subcarrier_allocation = new unsigned char[RHC_OFDMA_M];
        unsigned char* default_subcarrier_allocation = new unsigned char[RHC_OFDMA_M];
        createInnerPilotAllocation(inner_subcarrier_allocation);
        createOuterPilotAllocation(outer_subcarrier_allocation);
        ofdmframe_init_sctype(RHC_OFDMA_M, default_subcarrier_allocation, .05);
        //openNullHole(default_subcarrier_allocation, 150, 350);
       // ofdmframe_print_sctype(default_subcarrier_allocation, 512);

        if(!node_is_basestation)
        {
            ofdma_fs_inner = ofdmflexframesync_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, inner_subcarrier_allocation, ofdmaCallback, (void *)rx_timer, node_id - 1, num_nodes_in_net - 1);
            ofdma_fs_outer = ofdmflexframesync_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, outer_subcarrier_allocation, ofdmaCallback, (void *)rx_timer, node_id - 1, num_nodes_in_net - 1);
            ofdma_fs_default = ofdmflexframesync_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, default_subcarrier_allocation, ofdmaCallback, (void *)rx_timer, node_id - 1, num_nodes_in_net - 1);
            unsigned int nulls, pilots, data;
            ofdmframe_validate_sctype(default_subcarrier_allocation, RHC_OFDMA_M, &nulls, &pilots, &data);
            std::cout << "Uplink Subcarrier Summary:" << std::endl;
            std::cout << "Total: " << nulls + pilots + data << std::endl;
            std::cout << "Null: " << nulls << std::endl;
            std::cout << "Pilot: " << pilots << std::endl;
            std::cout << "Data: " << data << std::endl << std::endl;
        }
        else
        {
            unsigned int subcarriers_per_channel = RHC_OFDMA_M / (num_nodes_in_net - 1);
            unsigned char* p = new unsigned char[subcarriers_per_channel];
            ofdmframe_init_sctype(subcarriers_per_channel, p, .05);
            ///ofdmframe_print_sctype(p, 512);
            mcrx = new multichannelrx(num_nodes_in_net - 1, subcarriers_per_channel, RHC_cp_len, RHC_taper_len,
                    p, userdata, callbacks);
        }
    }
    fs = ofdmflexframesync_create(RHC_M, RHC_cp_len, RHC_taper_len, NULL,
            rxCallback, (void *) &rxf);

    // Receive side sensing configuration --------------------------------
    rx_snapshot_was_triggered = false;
    rx_snapshot_samples = 0;
    rx_snapshot_total_size = 0;
    rx_snapshot_sample_idx = 0;

    // Transmit side USRP configuration ----------------------------------
    usrp->set_tx_antenna("TX/RX");
    usrp->set_tx_gain(rf_gain_tx);

    // Transmit frequency already set in receive stage tune2NormalFreq()
    if(u4 && !node_is_basestation)
    {
        usrp->set_tx_rate(RHC_NOMINAL_RESAMPLER_RATIO * sample_rate);
        usrp_tx_rate = usrp->get_tx_rate();
        tx_resamp_rate = usrp_tx_rate / sample_rate;
    }
    else
    {
        usrp->set_tx_rate(RHC_NOMINAL_RESAMPLER_RATIO * sample_rate);
        usrp_tx_rate = usrp->get_tx_rate();
        tx_resamp_rate = usrp_tx_rate / sample_rate;
    }

    tx_resamp = msresamp_crcf_create(tx_resamp_rate, rc->sba);
    // Resampler buffers and UHD sample buffers now within the class 
    // methods that actually perform burst tx & rx

    // Create tx streamer object for burst transmissions
    uhd::stream_args_t tx_stream_args("fc32");
    tx_stream = usrp->get_tx_stream(tx_stream_args);

    // Using this constant in transport size makes a whole number of transfers;
    // this constant is compatible with both Gigabit Ethernet and PCIe interfaces
    tx_uhd_transport_size =  RHC_TX_UHD_TRANSPORT_SIZE; 
    tx_uhd_max_buffer_size = tx_stream->get_max_num_samps() +64;

    // The following is for the check of tx_async_md that _seems_ to need
    // to be fetched after a burst
    tx_uhd_ack_received = false;

    // Transmit side modem configuration ---------------------------------
    ofdmflexframegenprops_init_default(&fgprops);
    fgprops.check           = RHC_check;  
    fgprops.fec0            = LIQUID_FEC_NONE;
    fgprops.fec1            = LIQUID_FEC_NONE;
    fgprops.mod_scheme      = RHC_ms;
    if(hardened)
    {
        fgprops.fec0            = RHC_fec0;
        fgprops.fec1            = LIQUID_FEC_RS_M8;
    } 

    if(u4)
    {
        unsigned int subcarriers_per_channel = RHC_OFDMA_M / (num_nodes_in_net - 1);
        //Initialize subcarriers with 5% guard bands on both sides = 10% guard
        //5Mhz - 10% guard = 4.5Mhz we're looking for to look like LTE
        unsigned char* inner_subcarrier_allocation = new unsigned char[RHC_OFDMA_M];
        unsigned char* outer_subcarrier_allocation = new unsigned char[RHC_OFDMA_M];
        unsigned char* default_subcarrier_allocation = new unsigned char[RHC_OFDMA_M];
        createInnerPilotAllocation(inner_subcarrier_allocation);
        createOuterPilotAllocation(outer_subcarrier_allocation);
        ofdmframe_init_sctype(RHC_OFDMA_M, default_subcarrier_allocation, .05);
        //openNullHole(default_subcarrier_allocation, 150, 350);
        //ofdmframe_print_sctype(default_subcarrier_allocation, 512);

        if(node_is_basestation)
        {
            ofdma_fg_inner = ofdmflexframegen_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, inner_subcarrier_allocation, &fgprops, num_nodes_in_net - 1);
            ofdma_fg_outer = ofdmflexframegen_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, outer_subcarrier_allocation, &fgprops, num_nodes_in_net - 1);
            ofdma_fg_default = ofdmflexframegen_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, default_subcarrier_allocation, &fgprops, num_nodes_in_net - 1);
            std::stringstream report;
            report << scientific << ext_am_ptr->getElapsedTime();
            report << "    RadioHardwareConfig: ";
            report << "DL Subcarrier Allocation" << std::endl;
            
            unsigned char* map = ofdmflexframegen_get_subcarrier_map(ofdma_fg_default);
            unsigned char* alloc = ofdmflexframegen_get_subcarrier_allocation(ofdma_fg_default);
            
            for(int i = 0; i < RHC_OFDMA_M; i++)
            {
                report << i << ": ";
                if(alloc[i] == OFDMFRAME_SCTYPE_DATA)
                    report << (int)map[i] + 1 << std::endl;
                else if(alloc[i] == OFDMFRAME_SCTYPE_NULL)
                    report << "NULL" << std::endl;
                else if(alloc[i] == OFDMFRAME_SCTYPE_PILOT)
                    report << "PILOT" << std::endl;
            }

            alloc_log_ptr->log(report.str());
            alloc_log_ptr->write_log();
            unsigned int nulls, pilots, data;
            ofdmframe_validate_sctype(default_subcarrier_allocation, RHC_OFDMA_M, &nulls, &pilots, &data);
            std::cout << "Downlink Subcarrier Summary:" << std::endl;
            std::cout << "Total: " << nulls + pilots + data << std::endl;
            std::cout << "Null: " << nulls << std::endl;
            std::cout << "Pilot: " << pilots << std::endl;
            std::cout << "Data: " << data << std::endl << std::endl;;
        }
        else
        {
            unsigned char* p = new unsigned char[subcarriers_per_channel];
            ofdmframe_init_sctype(subcarriers_per_channel, p, .05);
            mctx = new multichanneltx(num_nodes_in_net - 1, subcarriers_per_channel, RHC_cp_len, RHC_taper_len,
                    p); 
        }

        fg  = ofdmflexframegen_create(subcarriers_per_channel, RHC_cp_len, RHC_taper_len, inner_subcarrier_allocation, &fgprops);
    }
    else
    {
        fg  = ofdmflexframegen_create(RHC_M, RHC_cp_len, RHC_taper_len, NULL, &fgprops);
    }

    if(u4)
    {
        if(slow)
        {
            ofdma_tx_window = 1.0;
            mc_tx_window = 1.0;
        }
        transmit_timer = timer_create();
        timer_tic(transmit_timer);
    }


    // Establish choice of USRP hardware clock
    switch (clock_ref_type) {
        case CLOCK_REF_GPSDO :
            try {
                usrp->set_time_source("gpsdo");
                usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
            } catch (...) {
                usrp->set_time_source("gpsdo");
                usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
            }
            break;

        case CLOCK_REF_LAB :
            try {
                usrp->set_time_source("external");
                usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
            } catch (...) {
                usrp->set_time_source("external");
                usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
            }
            break;

        case CLOCK_REF_NONE :
            if(!u4)
            {
                cerr << "WARNING: Option CLOCK_REF_NONE selected."<< endl;
                cerr << "         This application needs a hardware-based clock reference to "<<endl;
                cerr << "         communicate reliably with other radios in the network."<<endl;
                cerr << "         Running without a reference only suitable for standalone testing."<<endl;
            }
            usrp->set_time_now(uhd::time_spec_t(0.0));

            break;

        default :
            cerr << "ERROR: in ClockReference unknown type of clock reference" <<endl;
            exit(EXIT_FAILURE);
            break;
    }

    uhd::sensor_value_t ref_locked = usrp->get_mboard_sensor("ref_locked", 0);
    if ( ref_locked.to_bool() ) {
        // Silent if everything working
    } else {
        cerr << "WARNING: Could not lock to a clock reference." <<endl;
        cerr << "         Running without a reference only suitable for standalone testing."<<endl;
    }

    // Calibration of receiver noise on normal frequency for idle channel
    rxHeartbeatCalibration( (getHardwareTimestamp() +0.01), initial_rx_recommended_sample_size);

    // Check for presence of a bug in UHD's manual tuning mode within the receive side
    // DSP stage, an error in the sign of the DSP stage's frequency 
#ifdef DEBUG_SUPPORTED
    if (debug) {
        cout << "\nDEBUG: Probe of host computer's UHD for bug in manual tuning mode" << endl;
        cout << "       of receive side DSP stage tuning (sign inversion) " << endl;
        cout << "       Probe trying to tune to 2.405e9 +1.0e6 = 2.406 GHz . . . "<<endl;
    }
#endif
    rx_tune_req.rf_freq = 2.405e9;  
    rx_tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    rx_tune_req.dsp_freq = 1.0e6;   
    rx_tune_req.dsp_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    usrp->set_rx_freq(rx_tune_req);
    double bug_check_freq = usrp->get_rx_freq();
    if (bug_check_freq != 2.406e9) {
        uhd_rx_tuning_bug_is_present = true;
    } else {
        uhd_rx_tuning_bug_is_present = false;
    }
#ifdef DEBUG_SUPPORTED
    if (debug) {
        cout << "       Probe complete.  Bug is ";
        if (uhd_rx_tuning_bug_is_present) {
            cout << "DETECTED.  Actual frequency is: " << scientific << 
                bug_check_freq << "\n" << endl; 
        } else {
            cout << "NOT detected.\n" << endl;
        }
    }
#endif

    tune2NormalFreq();

    // Initialize RadioHardwareConfig debug tools
    initRxfEventLog(rxf_event_log_level);
    initUhdErrorLog(uhd_error_log_level);

    ext_rhc_ptr = this;

}
//////////////////////////////////////////////////////////////////////////


RadioHardwareConfig::~RadioHardwareConfig()
{   
    finalizeRxfEventLog();
    finalizeUhdErrorLog();

    if (rx_snapshot_was_triggered) {
        cout << "\nINFO: Receive snapshot was triggered" << endl;
        cout << "    INFO: rx_snapshot_total_size: " << rx_snapshot_total_size <<endl;
        cout << "    INFO: rx_snapshot_sample_idx: " << rx_snapshot_sample_idx << endl;
    } 
    std::cout << std::endl;  
    std::cout << "Transmitted " << total_packets_transmitted << " packets. ";
    if(rc->node_is_basestation)
    {
        std::cout << total_packets_transmitted / (rc->num_nodes_in_net - 1) << " per mobile.";
    }
    std::cout << endl;
    std::cout << "Network: " << network_packets_transmitted << std::endl;
    std::cout << "Dummy: " << dummy_packets_transmitted << std::endl << std::endl;
    std::cout << "Detected: " << total_packets_received << " packets." << std::endl;
    std::cout << "Network: " << network_packets_received << std::endl;
    std::cout << "Dummy: " << dummy_packets_received << std::endl << std::endl;
    std::cout << valid_headers_received << " valid headers (" << 100 * (float)valid_headers_received / total_packets_received
        << "%)" << std::endl;
    std::cout << valid_payloads_received << " valid payloads (" << 100 * (float)valid_payloads_received / total_packets_received
        << "%)" << std::endl;

    rf_log_ptr->write_log();
    alloc_log_ptr->write_log();
    delete alloc_log_ptr;
    //Delete receiver side objects
    firfilt_crcf_destroy(rx_prefilt);
    msresamp_crcf_destroy(rx_resamp);
    ofdmflexframesync_destroy(fs);

    // Delete transmitter side objects
    ofdmflexframegen_destroy(fg);
    msresamp_crcf_destroy(tx_resamp);
    
    //Delete OFDMA objects
    if(u4)
    {
        delete mcrx;
        delete mctx;
        timer_destroy(transmit_timer);
        ofdmflexframesync_destroy(ofdma_fs_inner);
        ofdmflexframegen_destroy_multi_user(ofdma_fg_inner);
    }
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getNormalFreq()
{
    return(normal_freq);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getRxNormalFreq()
{
    if (node_is_basestation) {
        return(normal_freq + tx2rx_freq_separation);
    } else {
        return(normal_freq);
    }
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getTxNormalFreq()
{
    if (node_is_basestation) {
        return(normal_freq);
    } else {
        return(normal_freq + tx2rx_freq_separation);
    }
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getTx2RxFreqSeparation()
{
    return(tx2rx_freq_separation);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getFhWindowSmall()
{
    return(fh_window_small);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getFhWindowMedium()
{
    return(fh_window_medium);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getHardwareTimestamp()
{
    uhd::time_spec_t uhd_system_time_now = usrp->get_time_now(0);

    // The following approach may do better at keeping full precision of 
    // underlying time_spec_t data than the .get_real_secs() method 
    return( (double)uhd_system_time_now.get_full_secs() +  
            (double)uhd_system_time_now.get_frac_secs() ); 
}
//////////////////////////////////////////////////////////////////////////
int RadioHardwareConfig::setHardwareTimestamp(uhd::time_spec_t time)
{
    usrp->set_time_now(time);
    return(1);
}

//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getHardwareNextSecond()
{
    // Add 10 msec to current time as a safeguard against planning
    // a starting schedule too close to current time
    const double END_OF_SECOND_MARGIN = 0.01;

    uhd::time_spec_t uhd_system_time_now = usrp->get_time_now(0);

    return(1.0 + floor( END_OF_SECOND_MARGIN +
                (double)uhd_system_time_now.get_full_secs() +  
                (double)uhd_system_time_now.get_frac_secs()  )  ); 
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getRxAbsoluteFreq()
{
    return(rx_absolute_freq);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getTxAbsoluteFreq()
{
    return(tx_absolute_freq);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getRxRateMeasured()
{
    return(usrp_rx_rate);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getTxRateMeasured()
{
    return(usrp_tx_rate);
}
//////////////////////////////////////////////////////////////////////////


unsigned int RadioHardwareConfig::getTxBurstLength()
{
    //TODO replace with parameter or calculation
    return(RHC_TX_BURST_LENGTH);
}
//////////////////////////////////////////////////////////////////////////


double RadioHardwareConfig::getUhdRetuneDelay()
{
    return(uhd_retune_delay);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::tuneRxManual(double rf_freq, double dsp_freq)
{
    //NOTE on "mode_n=integer"
    //  This mode appears to interfere with proper operation of POLICY_MANUAL
    //  even though it should be preferable in terms of reduced RF spurs
    //rx_tune_req.args = uhd::device_addr_t("mode_n=integer"); // or fractional
    //  default mode "fractional" being used instead
    rx_tune_req.rf_freq = rf_freq;
    rx_tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    rx_tune_req.dsp_freq = dsp_freq;
    rx_tune_req.dsp_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    usrp->set_rx_freq(rx_tune_req);

    if (uhd_rx_tuning_bug_is_present) {
        rx_absolute_freq = rx_tune_req.rf_freq - rx_tune_req.dsp_freq;
    } else {
        rx_absolute_freq = rx_tune_req.rf_freq + rx_tune_req.dsp_freq;
    }

    while( !(usrp->get_rx_sensor("lo_locked",0).to_bool()) ) {
        usleep(20);
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::tuneTxManual(double rf_freq, double dsp_freq)
{
    //NOTE on "mode_n=integer"
    //  This mode appears to interfere with proper operation of POLICY_MANUAL
    //  even though it should be preferable in terms of reduced RF spurs
    //tx_tune_req.args = uhd::device_addr_t("mode_n=integer"); // or fractional
    //  default mode "fractional" being used instead
    tx_tune_req.rf_freq = rf_freq;
    tx_tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    tx_tune_req.dsp_freq = dsp_freq;
    tx_tune_req.dsp_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    usrp->set_tx_freq(tx_tune_req);
    tx_absolute_freq = tx_tune_req.rf_freq + tx_tune_req.dsp_freq;

    while( !(usrp->get_tx_sensor("lo_locked",0).to_bool()) ) {
        usleep(20);
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::tune2NormalFreq()
{
    //NOTE on "mode_n=integer"
    //  This mode appears to interfere with proper operation of POLICY_MANUAL
    //  even though it should be preferable in terms of reduced RF spurs
    //rx_tune_req.args = uhd::device_addr_t("mode_n=integer"); // or fractional
    //  default mode "fractional" being used instead
    rx_tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    rx_tune_req.dsp_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    rx_tune_req.dsp_freq = 0.0;

    //tx_tune_req.args = uhd::device_addr_t("mode_n=integer"); // or fractional
    tx_tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    tx_tune_req.dsp_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    tx_tune_req.dsp_freq = 0.0;

    if (node_is_basestation) {
        rx_tune_req.rf_freq = normal_freq + tx2rx_freq_separation;
        tx_tune_req.rf_freq = normal_freq;
    } else {
        if(u4)
        {
            rx_tune_req.rf_freq = normal_freq;
            //double channel_width = sample_rate / (num_nodes_in_net - 1);
            //double left_edge = normal_freq + tx2rx_freq_separation - (sample_rate / 2.0);
            //double mobile_channel_freq = left_edge + ((node_id - 1) * channel_width) + (channel_width / 2.0);
            //tx_tune_req.rf_freq = mobile_channel_freq;
            tx_tune_req.rf_freq = normal_freq + tx2rx_freq_separation ;
        }
        else
        {
            rx_tune_req.rf_freq = normal_freq;
            tx_tune_req.rf_freq = normal_freq + tx2rx_freq_separation ;
        }
    }
    usrp->set_rx_freq(rx_tune_req);
    rx_absolute_freq = rx_tune_req.rf_freq + rx_tune_req.dsp_freq;

    usrp->set_tx_freq(tx_tune_req);
    tx_absolute_freq = tx_tune_req.rf_freq + tx_tune_req.dsp_freq;

    while( !(usrp->get_rx_sensor("lo_locked",0).to_bool()) ) {
        usleep(200+(rand()%100));
    }
    while( !(usrp->get_tx_sensor("lo_locked",0).to_bool()) ) {
        usleep(200+(rand()%100));
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////    
void RadioHardwareConfig::recreate_modem()
{
    std::stringstream report;
    report << scientific << ext_am_ptr->getElapsedTime();
    report << "    RadioHardwareConfig: ";
    if(node_is_basestation)
    {
        if(!hardened)
        {
            fgprops.check           = RHC_check;  
            fgprops.fec0            = RHC_fec0;
            fgprops.fec1            = LIQUID_FEC_RS_M8;
            fgprops.mod_scheme      = RHC_ms;
            hardened = true;
        }
        gen_mutex.lock();
        ofdmflexframegen_destroy(ofdma_fg_default);
        ofdma_fg_default = ofdmflexframegen_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, new_alloc,
        &fgprops, num_nodes_in_net - 1);
        gen_mutex.unlock();
        report << "New DL Subcarrier Allocation" << std::endl;
        unsigned char* map = ofdmflexframegen_get_subcarrier_map(ofdma_fg_default);
        unsigned char* alloc = ofdmflexframegen_get_subcarrier_allocation(ofdma_fg_default);
        for(int i = 0; i < RHC_OFDMA_M; i++)
        {
            report << i << ": ";
            if(alloc[i] == OFDMFRAME_SCTYPE_DATA)
                report << (int)map[i] + 1 << std::endl;
            else if(alloc[i] == OFDMFRAME_SCTYPE_NULL)
                report << "NULL" << std::endl;
            else if(alloc[i] == OFDMFRAME_SCTYPE_PILOT)
                report << "PILOT" << std::endl;
        }
        alloc_log_ptr->log(report.str());
        alloc_log_ptr->write_log();
    }
    else
    {
        //get a lock on the sync so we dont try to destroy it while it is
        //executing symbols
        sync_mutex.lock();
        ofdmflexframesync_destroy(ofdma_fs_default);
        ofdma_fs_default = ofdmflexframesync_create_multi_user(RHC_OFDMA_M, RHC_cp_len, RHC_taper_len, new_alloc, ofdmaCallback, (void *)rx_timer, node_id - 1, num_nodes_in_net - 1);
        received_new_alloc = false;
        sync_mutex.unlock();
    }
}
//////////////////////////////////////////////////////////////////////////    
void RadioHardwareConfig::switch_allocation()
{
    std::stringstream report;
    report << scientific << ext_am_ptr->getElapsedTime();
    report << "    RadioHardwareConfig: ";
    if(allocation == INNER_ALLOCATION)
    {
        allocation = DEFAULT_ALLOCATION;
        std::cout << "switched to anti-jam allocation" << std::endl;
        if(!node_is_basestation)
        {
            txMCFrameBurst(CONTROL);
        }
    }
    if(node_is_basestation)
    {
        report << "New DL Subcarrier Allocation" << std::endl;
        unsigned char* map = ofdmflexframegen_get_subcarrier_map(ofdma_fg_default);
        unsigned char* alloc = ofdmflexframegen_get_subcarrier_allocation(ofdma_fg_default);
        for(int i = 0; i < RHC_OFDMA_M; i++)
        {
            report << i << ": ";
            if(alloc[i] == OFDMFRAME_SCTYPE_DATA)
                report << (int)map[i] + 1 << std::endl;
            else if(alloc[i] == OFDMFRAME_SCTYPE_NULL)
                report << "NULL" << std::endl;
            else if(alloc[i] == OFDMFRAME_SCTYPE_PILOT)
                report << "PILOT" << std::endl;
        }
    }
    app_log_ptr->log(report.str());
    app_log_ptr->write_log();
}

int RadioHardwareConfig::rxFrameBurst(
        double rx_start_time,
        size_t rx_total_requested_samples
        )
{   
    // Buffers for original UHD samples and resampler (decimate by ~2)
    std::vector<std::complex<float> > rx_usrp_buffer(rx_uhd_max_buffer_size); 
    std::vector<std::complex<float> > rx_temp_resample_buf(rx_uhd_max_buffer_size);      

    // Prepare for new attempt at receiving a frame
    rxf.frame_was_detected = false; 
    rxf.frame_is_valid = false;    
    rxf.rx_complete_timestamp = 0.0;
    ofdmflexframesync_reset(fs);
    firfilt_crcf_reset(rx_prefilt);
    unsigned int rx_num_resamples;
    size_t ctr;
    unsigned int n;

    // Reset noise calculation variables
    rxf.frame_end_noise_level = 0.0;
    rxf.frame_end_is_noisy = false;
    double noise_power_sum = 0.0;
    double tmp_re, tmp_im;

    // Prepare for timed data acquisition
    size_t uhd_num_delivered_samples = 0;
    size_t uhd_total_delivered_samples = 0;
    double rx_timeout = rx_start_time +0.01;
    uhd::stream_cmd_t rx_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    rx_stream_cmd.num_samps = rx_total_requested_samples;
    rx_stream_cmd.stream_now = false; 
    rx_stream_cmd.time_spec = uhd::time_spec_t(rx_start_time);
    rx_stream->issue_stream_cmd(rx_stream_cmd);

    // Keep fetching samples until requested amount delivered or UHD error
    uhd_error_stats.rx_attempts++;
    unsigned int rx_uhd_recv_ctr = 0;
    while (uhd_total_delivered_samples < rx_total_requested_samples) {

        uhd_num_delivered_samples = rx_stream->recv(&rx_usrp_buffer.front(),
                rx_uhd_transport_size, rx_md, rx_timeout, true);

        // Check for UHD errors
        switch(rx_md.error_code) {
            // Keep running on these conditions
            case uhd::rx_metadata_t::ERROR_CODE_NONE:
            case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                break;
                // Otherwise, capture details of non-trivial error
            default :
                uhd_error_stats.rx_error_timestamp = getHardwareTimestamp();
                uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
                uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;                
                uhd_error_stats.rx_error_num_samples =  uhd_num_delivered_samples;
                reportUhdError();
                uhd_error_stats.rx_fail_frameburst++;
                return(EXIT_FAILURE);
        }

        // Prefilter samples; this may be optional in a lab environment
        for (ctr = 0; ctr < uhd_num_delivered_samples; ctr++) {
            firfilt_crcf_push(rx_prefilt, rx_usrp_buffer[ctr] );
            firfilt_crcf_execute(rx_prefilt, &rx_usrp_buffer[ctr] );
        }

        // Apply resampler to reduce UHD sample rate to modem's receive rate
        rx_num_resamples = 0;
        msresamp_crcf_execute(rx_resamp, &rx_usrp_buffer[0], 
                (unsigned int)uhd_num_delivered_samples, 
                &rx_temp_resample_buf[0], &rx_num_resamples);  

        // Input samples to modem
        ofdmflexframesync_execute(fs, &rx_temp_resample_buf[0], rx_num_resamples);

        // Prep for next set of samples
        rx_uhd_recv_ctr++;
        uhd_total_delivered_samples += uhd_num_delivered_samples;
        rx_timeout += 0.01;   // future timeouts have a small increment
    }

    // If this point reached the overall sample acquistion assumed successful
    uhd_error_stats.rx_completions++;

    // Check for noise level at end of frame's time window
    if (rx_num_resamples > 0) {
        for (n = 0; n < rx_num_resamples; n++) {
            tmp_re = real(rx_temp_resample_buf[n]);
            tmp_im = imag(rx_temp_resample_buf[n]);
            noise_power_sum += sqrt(tmp_re * tmp_re + tmp_im * tmp_im);
        }
        rxf.frame_end_noise_level = noise_power_sum /(double)rx_num_resamples;
        if (rxf.frame_end_noise_level > rx_noise_linear_threshold) {
            rxf.frame_end_is_noisy = true;
        }
    }   // Else, leave as rxf.frame_end_is_noisy = false


    // If rxf logging active then report attributes of any detected frame
    if (rxf.frame_was_detected) {
        reportRxfEvent();
    } 

    // If valid frame received then generate a report
    rf_log_report_t rf_log_report;
    if (rxf.frame_is_valid) {   // Condition for RF_LOG_EVENT_RX_DATA
        switch (rf_log_level) {
            case(RF_LOG_LEVEL_NONE) :
                // no logging
                break;
            case(RF_LOG_LEVEL_NORMAL) :
                rf_log_report.hardware_timestamp_nominal = rx_start_time;
                rf_log_report.rf_event = RF_LOG_EVENT_RX_DATA;
                rf_log_report.frequency_nominal = getRxAbsoluteFreq();
                rf_log_report.bandwidth = sample_rate;
                logRfEvent(rf_log_report);
                break;
            case(RF_LOG_LEVEL_DETAIL) :
                // PLACEHOLDER for development version of logging
                break;
            default :
                cerr << "\nERROR in RadioHardwareConfig::rxFrameBurst: ";
                cerr << "Unknown rf_log_level\n" << endl;
                exit(EXIT_FAILURE);
                break;
        }
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

int RadioHardwareConfig::rxHeartbeatBurst(
        double rx_start_time,
        size_t rx_total_requested_samples
        )
{   
    // Buffers for original UHD samples and resampler (decimate by ~2)
    std::vector<std::complex<float> > rx_usrp_buffer(rx_uhd_max_buffer_size); 
    std::vector<std::complex<float> > rx_temp_resample_buf(rx_uhd_max_buffer_size);    

    // Prepare for new attempt at receiving a frame
    rxf.frame_was_detected = false; 
    rxf.frame_is_valid = false;    
    rxf.rx_complete_timestamp = 0.0;
    if(!u4)ofdmflexframesync_reset(fs);
    firfilt_crcf_reset(rx_prefilt);
    unsigned int rx_num_resamples;
    size_t ctr;
    unsigned int n;

    // Reset noise calculation variables
    rxf.frame_end_noise_level = 0.0;
    rxf.frame_end_is_noisy = false;
    double noise_power_sum = 0.0;
    double tmp_re, tmp_im;

    // Prepare for timed data acquisition
    size_t uhd_num_delivered_samples = 0;
    size_t uhd_total_delivered_samples = 0;
    double rx_timeout = rx_start_time +0.01;
    uhd::stream_cmd_t rx_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    rx_stream_cmd.num_samps = rx_total_requested_samples;
    rx_stream_cmd.stream_now = false; 
    rx_stream_cmd.time_spec = uhd::time_spec_t(rx_start_time);
    rx_stream->issue_stream_cmd(rx_stream_cmd);

    // Keep fetching samples until requested amount delivered or UHD error
    uhd_error_stats.rx_attempts++;
    unsigned int rx_uhd_recv_ctr = 0;
    while (uhd_total_delivered_samples < rx_total_requested_samples) {

        uhd_num_delivered_samples = rx_stream->recv(&rx_usrp_buffer.front(),
                rx_uhd_transport_size, rx_md, rx_timeout, true);

        // Check for UHD errors
        switch(rx_md.error_code) {
            // Keep running on these conditions
            case uhd::rx_metadata_t::ERROR_CODE_NONE:
            case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                break;
                // Otherwise, capture details of non-trivial error
            default :
                uhd_error_stats.rx_error_timestamp = getHardwareTimestamp();
                uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
                uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;                
                uhd_error_stats.rx_error_num_samples =  uhd_num_delivered_samples;
                reportUhdError();
                uhd_error_stats.rx_fail_hearbeatburst++;
                return(EXIT_FAILURE);
        }

        // Prefilter samples; this may be optional in a lab environment
        for (ctr = 0; ctr < uhd_num_delivered_samples; ctr++) {
            firfilt_crcf_push(rx_prefilt, rx_usrp_buffer[ctr] );
            firfilt_crcf_execute(rx_prefilt, &rx_usrp_buffer[ctr] );
        }

        // Apply resampler to reduce UHD sample rate to modem's receive rate
        rx_num_resamples = 0;
        msresamp_crcf_execute(rx_resamp, &rx_usrp_buffer[0], 
                (unsigned int)uhd_num_delivered_samples, 
                &rx_temp_resample_buf[0], &rx_num_resamples);  

        // Input samples to modem
        ofdmflexframesync_execute(fs, &rx_temp_resample_buf[0], rx_num_resamples);

        // Prep for next set of samples
        rx_uhd_recv_ctr++;
        uhd_total_delivered_samples += uhd_num_delivered_samples;
        rx_timeout += 0.01;   // future timeouts have a small increment
    }

    // If this point reached the overall sample acquistion assumed successful
    uhd_error_stats.rx_completions++;

    // Check for noise level at end of frame's time window
    if (rx_num_resamples > 0) {
        for (n = 0; n < rx_num_resamples; n++) {
            tmp_re = real(rx_temp_resample_buf[n]);
            tmp_im = imag(rx_temp_resample_buf[n]);
            noise_power_sum += sqrt(tmp_re * tmp_re + tmp_im * tmp_im);
        }
        rxf.frame_end_noise_level = noise_power_sum /(double)rx_num_resamples;
        if (rxf.frame_end_noise_level > rx_noise_linear_threshold) {
            rxf.frame_end_is_noisy = true;
        }
    }   // Else, leave as rxf.frame_end_is_noisy = false


    // If rxf logging active then report attributes of any detected frame
    if (rxf.frame_was_detected) {
        reportRxfEvent();
    } 

    // If valid frame received then generate a report
    rf_log_report_t rf_log_report;
    if (rxf.frame_is_valid) {   // Condition for RF_LOG_EVENT_RX_DATA
        switch (rf_log_level) {
            case(RF_LOG_LEVEL_NONE) :
                // no logging
                break;
            case(RF_LOG_LEVEL_NORMAL) :
                rf_log_report.hardware_timestamp_nominal = rx_start_time;
                rf_log_report.rf_event = RF_LOG_EVENT_RX_HEARTBEAT;
                rf_log_report.frequency_nominal = getRxAbsoluteFreq();
                rf_log_report.bandwidth = sample_rate;
                logRfEvent(rf_log_report);
                break;
            case(RF_LOG_LEVEL_DETAIL) :
                // PLACEHOLDER for development version of logging
                break;
            default :
                cerr << "\nERROR in RadioHardwareConfig::rxFrameBurst: ";
                cerr << "Unknown rf_log_level\n" << endl;
                exit(EXIT_FAILURE);
                break;
        }
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::rxHeartbeatCalibration(
        double rx_start_time,
        size_t rx_total_requested_samples
        )
{
    // Buffers for original UHD samples and resampler (decimate by ~2)
    std::vector<std::complex<float> > rx_usrp_buffer(rx_uhd_max_buffer_size); 
    std::vector<std::complex<float> > rx_temp_resample_buf(rx_uhd_max_buffer_size);    

    // For now no RF event logging of calibration

    firfilt_crcf_reset(rx_prefilt);
    unsigned int rx_num_resamples;
    size_t ctr;
    unsigned int n;

    // Reset noise calculation variables
    rxf.frame_end_noise_level = 0.0;
    rxf.frame_end_is_noisy = false;
    double noise_power_sum = 0.0;
    double tmp_re, tmp_im;

    // Provide a default value for noise threshold in case the UHD sample
    // acquistion suffers an error
    rx_noise_linear_threshold = RHC_CALIBRATE_RX_NOISE_THRESHOLD_DEFAULT;
    double rx_noise_linear = 0;

    // Prepare for timed data acquisition
    size_t uhd_num_delivered_samples = 0;
    size_t uhd_total_delivered_samples = 0;
    double rx_timeout = rx_start_time +0.01;
    uhd::stream_cmd_t rx_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    rx_stream_cmd.num_samps = rx_total_requested_samples;
    rx_stream_cmd.stream_now = false; 
    rx_stream_cmd.time_spec = uhd::time_spec_t(rx_start_time);
    rx_stream->issue_stream_cmd(rx_stream_cmd);

    // This rxHeartbeatCalibration process matches all the sample processing
    // steps of rxHeartbeatBurst except for passing samples to the modem
    uhd_error_stats.rx_attempts++;
    unsigned int rx_uhd_recv_ctr = 0;
    while (uhd_total_delivered_samples < rx_total_requested_samples) {

        uhd_num_delivered_samples = rx_stream->recv(&rx_usrp_buffer.front(),
                rx_uhd_transport_size, rx_md, rx_timeout, true);

        // Check for UHD errors
        switch(rx_md.error_code) {
            // Keep running on these conditions
            case uhd::rx_metadata_t::ERROR_CODE_NONE:
            case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                break;
                // Otherwise, capture details of non-trivial error
            default :
                uhd_error_stats.rx_error_timestamp = getHardwareTimestamp();
                uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
                uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;                
                uhd_error_stats.rx_error_num_samples =  uhd_num_delivered_samples;
                reportUhdError();
                uhd_error_stats.rx_fail_hearbeatburst++;
                return(EXIT_FAILURE);
        }

        // Prefilter samples; this may be optional in a lab environment
        for (ctr = 0; ctr < uhd_num_delivered_samples; ctr++) {
            firfilt_crcf_push(rx_prefilt, rx_usrp_buffer[ctr] );
            firfilt_crcf_execute(rx_prefilt, &rx_usrp_buffer[ctr] );
        }

        // Apply resampler to reduce UHD sample rate to modem's receive rate
        rx_num_resamples = 0;
        msresamp_crcf_execute(rx_resamp, &rx_usrp_buffer[0], 
                (unsigned int)uhd_num_delivered_samples, 
                &rx_temp_resample_buf[0], &rx_num_resamples);  

        // Prep for next set of samples
        rx_uhd_recv_ctr++;
        uhd_total_delivered_samples += uhd_num_delivered_samples;
        rx_timeout += 0.01;   // future timeouts have a small increment
    }

    // If this point reached the overall sample acquistion assumed successful
    uhd_error_stats.rx_completions++;

    // Samples at the end of a frame's time window are the basis for 
    // determining the noise level
    if (rx_num_resamples > 0) {
        for (n = 0; n < rx_num_resamples; n++) {
            tmp_re = real(rx_temp_resample_buf[n]);
            tmp_im = imag(rx_temp_resample_buf[n]);
            noise_power_sum += sqrt(tmp_re * tmp_re + tmp_im * tmp_im);
        }

        rx_noise_linear = noise_power_sum /(double)rx_num_resamples;
        rx_noise_linear_threshold = rx_noise_linear *RHC_CALIBRATE_RX_NOISE_RATIO;

    }   // Else, leave the threshold at its default

#ifdef DEBUG_SUPPORTED 
    if (debug) {
        cout<<"DEBUG: RadioHardwareConfig::rxNoiseCalibrate executed at ";
        cout<<"hardware clock: "<< getHardwareTimestamp() <<endl;
        cout<<"    rx_noise_linear:           "<<scientific << rx_noise_linear <<endl;
        cout<<"    rx_noise_linear_threshold: "<<scientific << rx_noise_linear_threshold <<endl;
    }
#endif

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::rxSnapshotBurst(
        double rx_start_time,
        size_t rx_total_requested_samples
        )
{
    rx_snapshot_was_triggered = true;
    rx_snapshot_total_size = rx_total_requested_samples;
    rx_snapshot_samples = new std::complex<float>[rx_snapshot_total_size];
    rx_snapshot_sample_idx = 0;

    // Buffers for original UHD samples and resampler (decimate by ~2)
    std::vector<std::complex<float> > rx_usrp_buffer(rx_uhd_max_buffer_size); 
    std::vector<std::complex<float> > rx_temp_resample_buf(rx_uhd_max_buffer_size);        

    firfilt_crcf_reset(rx_prefilt);
    unsigned int rx_num_resamples;
    size_t ctr;

    // Prepare for timed data acquisition
    size_t uhd_num_delivered_samples = 0;
    size_t uhd_total_delivered_samples = 0;
    double rx_timeout = rx_start_time +0.01;
    uhd::stream_cmd_t rx_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    rx_stream_cmd.num_samps = rx_total_requested_samples;
    rx_stream_cmd.stream_now = false; 
    rx_stream_cmd.time_spec = uhd::time_spec_t(rx_start_time);
    rx_stream->issue_stream_cmd(rx_stream_cmd);

    // This rxSnapshotBurst process applies the same sample pre-processing
    // steps as in the functions for receiving a frame
    uhd_error_stats.rx_attempts++;
    unsigned int rx_uhd_recv_ctr = 0;
    while (uhd_total_delivered_samples < rx_total_requested_samples) {

        uhd_num_delivered_samples = rx_stream->recv(&rx_usrp_buffer.front(),
                rx_uhd_transport_size, rx_md, rx_timeout, true);

        // Check for UHD errors
        switch(rx_md.error_code) {
            // Keep running on these conditions
            case uhd::rx_metadata_t::ERROR_CODE_NONE:
            case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                break;
                // Otherwise, capture details of non-trivial error
            default :
                uhd_error_stats.rx_error_timestamp = getHardwareTimestamp();
                uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
                uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;                
                uhd_error_stats.rx_error_num_samples =  uhd_num_delivered_samples;
                reportUhdError();
                uhd_error_stats.rx_fail_snapshotburst++;
                return(EXIT_FAILURE);
        }

        // Prefilter samples; this may be optional in a lab environment
        for (ctr = 0; ctr < uhd_num_delivered_samples; ctr++) {
            firfilt_crcf_push(rx_prefilt, rx_usrp_buffer[ctr] );
            firfilt_crcf_execute(rx_prefilt, &rx_usrp_buffer[ctr] );
        }

        // Apply resampler to reduce UHD sample rate to modem's receive rate
        rx_num_resamples = 0;
        msresamp_crcf_execute(rx_resamp, &rx_usrp_buffer[0], 
                (unsigned int)uhd_num_delivered_samples, 
                &rx_temp_resample_buf[0], &rx_num_resamples);  

        // Add processed samples to snapshot buffer
        if (uhd_num_delivered_samples > 0) {
            std::copy(&rx_temp_resample_buf[0], 
                    &rx_temp_resample_buf[uhd_num_delivered_samples],
                    &rx_snapshot_samples[rx_snapshot_sample_idx] );

            rx_snapshot_sample_idx += uhd_num_delivered_samples;
        }        

        // Prep for next set of samples
        rx_uhd_recv_ctr++;
        uhd_total_delivered_samples += uhd_num_delivered_samples;
        rx_timeout += 0.01;   // future timeouts have a small increment
    }

#ifdef DEBUG_SUPPORTED    
    if (1) {    
        cout << "INFO: Exiting from RadioHardwareConfig::rxSnapshotBurst"<<endl;
    }
#endif

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::writeSnapshotBurst() 
{
    FILE *snapshot_file;
    float tmp_r, tmp_i;
    unsigned int n;

    if (rx_snapshot_was_triggered) {
        cout << "INFO: RadioHardwareConfig::writeSnapshotBurst(): snapshot triggered" << endl;
        snapshot_file = fopen("rx_snapshot.dat", "w");
        if (snapshot_file == NULL) {
            cerr << "ERROR: unable to access file" << endl;
            delete[] rx_snapshot_samples;
            exit(EXIT_FAILURE);
        }
        for (n = 0; n < (rx_snapshot_total_size -1); n++) {
            tmp_r = (float) real(rx_snapshot_samples[n]);
            tmp_i = (float) imag(rx_snapshot_samples[n]);
            fprintf(snapshot_file, "%f  %f\n",tmp_r, tmp_i);
        }
        fclose(snapshot_file);
        delete[] rx_snapshot_samples;
        rx_snapshot_was_triggered = false;
        cout << "INFO: in RadioHardwareConfig::writeSnapshotBurst()" << endl;
        cout << "      Snapshot triggered --> file rx_snapshot.dat written" << endl;

    } else {
        cout << "INFO: in RadioHardwareConfig::writeSnapshotBurst()" << endl;
        cout << "      No snapshot triggered --> no file written" << endl;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


//NOTE: 
//  For now txHeartbeatBurst is almost identical to txFrameBurst, but
//  this may change in the future; hence, separate methods 
int RadioHardwareConfig::txHearbeatBurst(
        double tx_start_time,
        unsigned char* header_buf, 
        unsigned int payload_size,
        unsigned char* payload_buf
        )
{
    // USRP and resampler buffers
    std::vector<std::complex<float> > tx_usrp_buffer(tx_uhd_max_buffer_size);
    unsigned int tx_usrp_sample_counter = 0;
    std::complex<float> tx_frame_resample_buf[2*RHC_OFDM_SYMBOL_LENGTH +64];
    std::complex<float> ofdm_symbol[RHC_OFDM_SYMBOL_LENGTH];

    // Prepare frame for modulation
    frame_was_transmitted = false;
    ofdmflexframegen_reset(fg);
    ofdmflexframegen_assemble(fg, header_buf, payload_buf, payload_size);

    // Configure rest of metadata for first set of samples in a burst
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = false;
    tx_md.time_spec = uhd::time_spec_t(tx_start_time);
    tx_md.has_time_spec = true;   
    double tx_timeout = tx_start_time +0.1;

    unsigned int ctr;
    int last_symbol=0;
    unsigned int zero_pad=1;
    while (!last_symbol || zero_pad > 0) {

        if (!last_symbol) {
            last_symbol = ofdmflexframegen_writesymbol(fg, ofdm_symbol);
        } else {
            zero_pad--;
            for (ctr=0; ctr < RHC_OFDM_SYMBOL_LENGTH;  ctr++)
                ofdm_symbol[ctr] = 0.0f;
        }

        for (ctr=0; ctr < RHC_OFDM_SYMBOL_LENGTH; ctr++) {
            unsigned int tx_nw = 0;
            msresamp_crcf_execute(tx_resamp, &ofdm_symbol[ctr], 1, tx_frame_resample_buf, &tx_nw);

            // put resampled symbols into USRP buffer after scaling to avoid saturation
            unsigned int tx_n;
            for (tx_n=0; tx_n < tx_nw; tx_n++) {
                tx_usrp_buffer[tx_usrp_sample_counter++] = 0.1f * tx_frame_resample_buf[tx_n];

                if (tx_usrp_sample_counter == tx_uhd_transport_size) {    
                    tx_usrp_sample_counter = 0;
                    // Could check (size_t)actual_uhd_transport_size = tx_stream->send(...)
                    tx_stream->send(
                            &tx_usrp_buffer.front(), tx_uhd_transport_size, tx_md, tx_timeout );

                    // prep metadata for next set of samples
                    tx_md.start_of_burst = false;
                    tx_md.has_time_spec = false;
                }
            }
        }
    } 

    // End burst
    tx_md.start_of_burst = false;
    tx_md.has_time_spec = false;
    tx_md.end_of_burst = true;
    tx_stream->send(&tx_usrp_buffer.front(), 0, tx_md, tx_timeout);

    // Fetching of UHD async messages seems to be required 
    tx_uhd_ack_received = false;
    while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md, tx_timeout) ) {
        tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
    }
    frame_was_transmitted = true;

    // Prepare RF event log entry 
    rf_log_report_t rf_log_report;
    switch (rf_log_level) {
        case(RF_LOG_LEVEL_NONE) :
            // no logging
            break;
        case(RF_LOG_LEVEL_NORMAL) :
            rf_log_report.hardware_timestamp_nominal = tx_start_time;
            rf_log_report.rf_event = RF_LOG_EVENT_TX_HEARTBEAT;
            rf_log_report.frequency_nominal = getTxAbsoluteFreq();
            rf_log_report.bandwidth = sample_rate;
            logRfEvent(rf_log_report);
            break;
        case(RF_LOG_LEVEL_DETAIL) :
            // PLACEHOLDER for development version of logging
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::txHearbeatBurst: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::txFrameBurst(
        double tx_start_time,
        unsigned char* header_buf, 
        unsigned int payload_size,
        unsigned char* payload_buf
        )
{
    // USRP and resampler buffers
    std::vector<std::complex<float> > tx_usrp_buffer(tx_uhd_max_buffer_size);
    unsigned int tx_usrp_sample_counter = 0;
    std::complex<float> tx_frame_resample_buf[2*RHC_OFDM_SYMBOL_LENGTH +64];
    std::complex<float> ofdm_symbol[RHC_OFDM_SYMBOL_LENGTH];

    // Prepare frame for modulation
    frame_was_transmitted = false;
    ofdmflexframegen_reset(fg);
    ofdmflexframegen_assemble(fg, header_buf, payload_buf, payload_size);

    // configure rest of metadata for first set of samples in a burst
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = false;
    tx_md.time_spec = uhd::time_spec_t(tx_start_time);
    tx_md.has_time_spec = true;   
    double tx_timeout = tx_start_time +0.1;

    unsigned int ctr;
    int last_symbol=0;
    unsigned int zero_pad=1;
    while (!last_symbol || zero_pad > 0) {

        if (!last_symbol) {
            last_symbol = ofdmflexframegen_writesymbol(fg, ofdm_symbol);
        } else {
            zero_pad--;
            for (ctr=0; ctr < RHC_OFDM_SYMBOL_LENGTH;  ctr++)
                ofdm_symbol[ctr] = 0.0f;
        }

        for (ctr=0; ctr < RHC_OFDM_SYMBOL_LENGTH; ctr++) {
            unsigned int tx_nw = 0;
            msresamp_crcf_execute(tx_resamp, &ofdm_symbol[ctr], 1, tx_frame_resample_buf, &tx_nw);

            // put resampled symbols into USRP buffer after scaling to avoid saturation
            unsigned int tx_n;
            for (tx_n=0; tx_n < tx_nw; tx_n++) {
                tx_usrp_buffer[tx_usrp_sample_counter++] = 0.1f * tx_frame_resample_buf[tx_n];

                if (tx_usrp_sample_counter == tx_uhd_transport_size) {    
                    tx_usrp_sample_counter = 0;
                    // Could check (size_t)actual_uhd_transport_size = tx_stream->send(...)
                    tx_stream->send(
                            &tx_usrp_buffer.front(), tx_uhd_transport_size, tx_md, tx_timeout );

                    // prep metadata for next set of samples
                    tx_md.start_of_burst = false;
                    tx_md.has_time_spec = false;
                }
            }
        }
    } 

    // End burst
    tx_md.start_of_burst = false;
    tx_md.has_time_spec = false;
    tx_md.end_of_burst = true;
    tx_stream->send(&tx_usrp_buffer.front(), 0, tx_md, tx_timeout);

    // Fetching of UHD async messages seems to be required 
    tx_uhd_ack_received = false;
    while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md, tx_timeout) ) {
        tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
    }
    frame_was_transmitted = true;

    // Prepare RF event log entry 
    rf_log_report_t rf_log_report;
    switch (rf_log_level) {
        case(RF_LOG_LEVEL_NONE) :
            // no logging
            break;
        case(RF_LOG_LEVEL_NORMAL) :
            rf_log_report.hardware_timestamp_nominal = tx_start_time;
            rf_log_report.rf_event = RF_LOG_EVENT_TX_DATA;
            rf_log_report.frequency_nominal = getTxAbsoluteFreq();
            rf_log_report.bandwidth = sample_rate;
            logRfEvent(rf_log_report);
            break;
        case(RF_LOG_LEVEL_DETAIL) :
            // PLACEHOLDER for development version of logging
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::txFrameBurst: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

int RadioHardwareConfig::txOFDMAFrameBurst(
        OFDMATransmissionType tx_type
        )
{
    timer_tic(transmit_timer);

    uhd::time_spec_t time_of_burst;
    //Wait for transmission timer to end before transmitting next packet
    /*double time_since_tic = timer_toc(transmit_timer);
    double time_to_sleep = ofdma_tx_window - time_since_tic;
    if(time_to_sleep > 0)
        usleep(time_to_sleep*1000000);
    //restart timer to begin tx window 
    */
    // USRP and resampler buffers
    std::vector<std::complex<float> > tx_usrp_buffer(tx_uhd_max_buffer_size);
    unsigned int tx_usrp_sample_counter = 0;
    //std::complex<float> tx_frame_resample_buf[(int)(2*tx_resamp_rate) + 64];
    std::complex<float> tx_frame_resample_buf[(int)(2*tx_resamp_rate) * RHC_OFDMA_SYMBOL_LENGTH];
    std::complex<float> ofdm_symbol[RHC_OFDMA_SYMBOL_LENGTH];
    unsigned char* header_buf = new unsigned char[P2M_FRAME_HEADER_DEFAULT_SIZE];
    header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_DATA;
    gen_mutex.lock();
    ofdmflexframegen gen;
    if(allocation == INNER_ALLOCATION)
        gen = ofdma_fg_inner;
    else if(allocation == OUTER_ALLOCATION)
        gen = ofdma_fg_outer;
    else
        gen = ofdma_fg_default;
    // Prepare frame for modulation
    frame_was_transmitted = false;

    setHardwareTimestamp(0.0);
    time_of_burst = ofdma_tx_window/4;
    if(tx_type == DATA)
    {
        unsigned char* payload_data;
        unsigned int payload_len = 0;
        unsigned int frame_id;
        long int packet_id;
        unsigned int total_packet_len;
        for(unsigned int i = 0; i < num_nodes_in_net - 1; i++)
        {
            payload_len = 0;
            payload_data = ext_ps_ptr->get_next_frame_for_destination(i + 1, &packet_id, &frame_id, &payload_len, &total_packet_len);
            //std::cout << "dest: " << i + 1 << ", packet id: " << packet_id << ", size: " << total_packet_len << std::endl;
            if(payload_len > 0)
            {
                unsigned char* padded_data = new unsigned char[payload_len + PADDED_BYTES];
                memmove(padded_data + PADDED_BYTES, payload_data, payload_len);
                //Set 2 "keys" so we can check in the callback to see if we received one
                //of these packets with extra control data in the front of the payload
                padded_data[0] = 42;
                padded_data[1] = 37;
                long int* li_padded_data = (long int*)(padded_data + 2);
                li_padded_data[0] = packet_id;
                padded_data[2 + sizeof(long int)] = (total_packet_len >> 8) & 0xff;
                padded_data[2 + sizeof(long int) + 1] = (total_packet_len) & 0xff;
                padded_data[2 + sizeof(long int) + 2] = frame_id;
                //std::cout << "loading " << packet_id << std::endl;
                ofdmflexframegen_multi_user_update_data(gen, padded_data, payload_len + PADDED_BYTES, i);

                network_packets_transmitted++;
                total_packets_transmitted++;
            }
            else
            {
                dummy_packets_transmitted++;
                total_packets_transmitted++;
                ofdmflexframegen_multi_user_update_data(gen, tx_frame_payload, frame_len, i);

            }
        }
    }
    else
    {
    //std::cout << "transmitting control packet" << std::endl;
        for(unsigned int i = 0; i < num_nodes_in_net - 1; i++)
        {
            ofdmflexframegen_multi_user_update_data(gen, tx_frame_payload, 0, i);
        }
        header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_CONTROL;
    }
    header_buf[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
    header_buf[P2M_HEADER_FIELD_DESTINATION_ID] = P2M_DESTINATION_ID_BROADCAST;
    ofdmflexframegen_assemble_multi_user(gen, header_buf);


    // configure rest of metadata for first set of samples in a burst
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = false;
    tx_md.time_spec = time_of_burst;
    tx_md.has_time_spec = true;
    //tx_stream->send("", 0, tx_md);

    unsigned int ctr;
    int last_symbol=0;
    unsigned int zero_pad=1;
    while (!last_symbol || zero_pad > 0) {
        if (!last_symbol) {
            last_symbol = ofdmflexframegen_writesymbol(gen, ofdm_symbol);
        } else {
            zero_pad--;
            for (ctr=0; ctr < RHC_OFDMA_SYMBOL_LENGTH;  ctr++)
                ofdm_symbol[ctr] = 0.0f;
        }
        //for (ctr=0; ctr < RHC_OFDMA_SYMBOL_LENGTH; ctr++) {
            unsigned int tx_nw = 0;
            msresamp_crcf_execute(tx_resamp, &ofdm_symbol[0], RHC_OFDMA_SYMBOL_LENGTH, tx_frame_resample_buf, &tx_nw);

            // put resampled symbols into USRP buffer after scaling to avoid saturation
            unsigned int tx_n;
            for (tx_n=0; tx_n < tx_nw; tx_n++) {
                tx_usrp_buffer[tx_usrp_sample_counter++] = rc->software_backoff * tx_frame_resample_buf[tx_n];

                if (tx_usrp_sample_counter == tx_uhd_transport_size) {    
                    tx_usrp_sample_counter = 0;
                    // Could check (size_t)actual_uhd_transport_size = tx_stream->send(...)
                    tx_stream->send(
                            &tx_usrp_buffer.front(), tx_uhd_transport_size, tx_md);

                    // prep metadata for next set of samples
                    tx_md.start_of_burst = false;
                    tx_md.has_time_spec = false;
                }
            }
       // }
    } 
    // End burst
    
       tx_md.start_of_burst = false;
       tx_md.has_time_spec = false;
       tx_md.end_of_burst = true;
       tx_stream->send("", 0, tx_md, 0.0);
     

    // Fetching of UHD async messages seems to be required 
    /*tx_uhd_ack_received = false;
      while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md) ) {
      tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
      }*/
    frame_was_transmitted = true;
    gen_mutex.unlock();

    // Prepare RF event log entry 
    rf_log_report_t rf_log_report;
    switch (rf_log_level) {
        case(RF_LOG_LEVEL_NONE) :
            // no logging
            break;
        case(RF_LOG_LEVEL_NORMAL) :
            rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
            if(tx_type == DATA)
                rf_log_report.rf_event = RF_LOG_EVENT_TX_OFDMA_DATA;
            else
                rf_log_report.rf_event = RF_LOG_EVENT_TX_CONTROL_DATA;
            rf_log_report.frequency_nominal = getTxAbsoluteFreq();
            rf_log_report.bandwidth = sample_rate;
            logRfEvent(rf_log_report);
            break;
        case(RF_LOG_LEVEL_DETAIL) :
            // PLACEHOLDER for development version of logging
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::txFrameBurst: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }
    while(getHardwareTimestamp() < ofdma_tx_window)
    {
        usleep(100);
    }
    return(EXIT_SUCCESS);
}
////////////////////////////////////////////////////////////////////////

int RadioHardwareConfig::txMCFrameBurst(
        OFDMATransmissionType tx_type
        )
{
    //Wait for transmission timer to end before transmitting next packet
    /*double time_since_tic = timer_toc(transmit_timer);
    double time_to_sleep = mc_tx_window - time_since_tic;
    if(time_to_sleep > 0)
        usleep(time_to_sleep*1000000);
*/

    if(!rc->uplink)
    {
        usleep(1000000*mc_tx_window);
        return 0;
    }
    timer_tic(transmit_timer);
    // USRP and resampler buffers
    std::vector<std::complex<float> > tx_usrp_buffer(256);

    unsigned int mctx_buffer_len = 2 * (num_nodes_in_net - 1);
    std::complex<float> mctx_buffer[mctx_buffer_len];
    
    unsigned char* header_buf = new unsigned char[P2M_FRAME_HEADER_DEFAULT_SIZE];
    header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_DATA;
    if(tx_type == DATA)
    {
        unsigned char* payload_data;
        unsigned int payload_len = 0;
        unsigned int frame_id;
        long int packet_id;
        unsigned int total_packet_len;
        //Only mobiles send multichannel data, so the destination will always be
        //the basestation, whose id is equal to the number of nodes in the network
        //ie nodes_in_net = [1,2,3], then 3 is the basestation


        //grab next frame from packetstore
        payload_data = ext_ps_ptr->get_next_frame_for_destination(num_nodes_in_net, &packet_id, &frame_id,
                &payload_len, &total_packet_len);

        if(payload_len > 0)
        {
            unsigned char* padded_data = new unsigned char[payload_len + PADDED_BYTES];
            memmove(padded_data + PADDED_BYTES, payload_data, payload_len);
            //Set 2 "keys" so we can check in the callback to see if we received one
            //of these packets with extra control data in the front of the payload
            padded_data[0] = 42;
            padded_data[1] = 37;
            long int* li_padded_data = (long int*)(padded_data + 2);
            li_padded_data[0] = packet_id;
            padded_data[2 + sizeof(long int)] = (total_packet_len >> 8) & 0xff;
            padded_data[2 + sizeof(long int) + 1] = (total_packet_len) & 0xff;
            padded_data[2 + sizeof(long int) + 2] = frame_id;
            header_buf[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
            header_buf[P2M_HEADER_FIELD_DESTINATION_ID] = num_nodes_in_net;
            header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_DATA;
            // Prepare frame for modulation
            frame_was_transmitted = false;
            network_packets_transmitted++;
            mctx->UpdateData(node_id - 1, header_buf, padded_data, payload_len + PADDED_BYTES, RHC_ms,
                    RHC_fec0, LIQUID_FEC_RS_M8);

        }
        else
        {
            dummy_packets_transmitted++;
            mctx->UpdateData(node_id - 1, header_buf, tx_frame_payload, frame_len, RHC_ms, RHC_fec0,
                    LIQUID_FEC_RS_M8);

        }
    }
    else if(tx_type == CONTROL)
    {
        header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_CONTROL;
        mctx->UpdateData(node_id - 1, header_buf, tx_frame_payload, 0, RHC_ms, RHC_fec0, LIQUID_FEC_RS_M8);
    }
    //while(getHardwareTimestamp() > 0.0005 && getHardwareTimestamp() < 2.0)
   // {
     //   usleep(10);
   // }

    setHardwareTimestamp(0.0);
    // configure rest of metadata for first set of samples in a burst
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = false;
    tx_md.time_spec = uhd::time_spec_t(0.005 + 0.007*(float)(node_id-1));
    tx_md.has_time_spec = true;
    unsigned int usrp_sample_counter = 0; 
    while(!mctx->IsChannelReadyForData(node_id - 1))
    {
        mctx->GenerateSamples(mctx_buffer);

        // push resulting samples to USRP
        for (unsigned int i=0; i<mctx_buffer_len; i++) {

            // append to USRP buffer, scaling by software
            tx_usrp_buffer[usrp_sample_counter++] = 0.1f * mctx_buffer[i];

            // once USRP buffer is full, reset counter and send to device
            if (usrp_sample_counter==256) {
                // reset counter
                usrp_sample_counter=0;

                // send the result to the USRP
                /*
                usrp->get_device()->send(
                        &tx_usrp_buffer.front(), tx_usrp_buffer.size(), tx_md,
                        uhd::io_type_t::COMPLEX_FLOAT32,
                        uhd::device::SEND_MODE_FULL_BUFF
                        );
                */
                tx_stream->send(&tx_usrp_buffer.front(), tx_usrp_buffer.size(), tx_md);
                tx_md.start_of_burst = false;
                tx_md.has_time_spec = false;
            }
        }
    }
    // End burst
    tx_md.start_of_burst = false;
    tx_md.has_time_spec = false;
    tx_md.end_of_burst = true;
    tx_stream->send("", 0, tx_md, 0.0);
    /*
    // Fetching of UHD async messages seems to be required 
    tx_uhd_ack_received = false;
    while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md) ) {
    tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
    }*/
    frame_was_transmitted = true;    
    total_packets_transmitted++;
    // Prepare RF event log entry 
    rf_log_report_t rf_log_report;
    switch (rf_log_level) {
        case(RF_LOG_LEVEL_NONE) :
            // no logging
            break;
        case(RF_LOG_LEVEL_NORMAL) :
            rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
            if(tx_type == DATA)
                rf_log_report.rf_event = RF_LOG_EVENT_TX_MC_DATA;
            else
                rf_log_report.rf_event = RF_LOG_EVENT_TX_CONTROL_DATA;
            rf_log_report.frequency_nominal = getTxAbsoluteFreq();
            rf_log_report.bandwidth = sample_rate;
            logRfEvent(rf_log_report);
            break;
        case(RF_LOG_LEVEL_DETAIL) :
            // PLACEHOLDER for development version of logging
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::txFrameBurst: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }
    while(getHardwareTimestamp() < mc_tx_window)
    {
        usleep(100);
    }
    return(EXIT_SUCCESS);
}


int RadioHardwareConfig::txOFDMAAllocBurst(
        unsigned char* new_alloc
        )
{
    //Wait for transmission timer to end before transmitting next packet
    double time_since_tic = timer_toc(transmit_timer);
    double time_to_sleep = ofdma_tx_window - time_since_tic;
    if(time_to_sleep > 0)
        usleep(time_to_sleep*1000000);
    //restart timer to begin tx window 
    timer_tic(transmit_timer);
    // USRP and resampler buffers
    std::vector<std::complex<float> > tx_usrp_buffer(tx_uhd_max_buffer_size);
    unsigned int tx_usrp_sample_counter = 0;
    std::complex<float> tx_frame_resample_buf[(int)(2*tx_resamp_rate) + 64];
    std::complex<float> ofdm_symbol[RHC_OFDMA_SYMBOL_LENGTH];
    unsigned char* header_buf = new unsigned char[P2M_FRAME_HEADER_DEFAULT_SIZE];
    header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_NEW_ALLOC;
    ofdmflexframegen gen;
    if(allocation == INNER_ALLOCATION)
        gen = ofdma_fg_inner;
    else if(allocation == OUTER_ALLOCATION)
        gen = ofdma_fg_outer;
    else
        gen = ofdma_fg_default;
    // Prepare frame for modulation
    frame_was_transmitted = false;
    for(unsigned int i = 0; i < num_nodes_in_net - 1; i++)
    {
        ofdmflexframegen_multi_user_update_data(gen, new_alloc, RHC_OFDMA_M, i);
    }
    header_buf[P2M_HEADER_FIELD_DESTINATION_ID] = P2M_DESTINATION_ID_BROADCAST;
    ofdmflexframegen_assemble_multi_user(gen, header_buf);


    // configure rest of metadata for first set of samples in a burst
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = false;

    unsigned int ctr;
    int last_symbol=0;
    unsigned int zero_pad=1;
    while (!last_symbol || zero_pad > 0) {

        if (!last_symbol) {
            last_symbol = ofdmflexframegen_writesymbol(gen, ofdm_symbol);
        } else {
            zero_pad--;
            for (ctr=0; ctr < RHC_OFDMA_SYMBOL_LENGTH;  ctr++)
                ofdm_symbol[ctr] = 0.0f;
        }
        for (ctr=0; ctr < RHC_OFDMA_SYMBOL_LENGTH; ctr++) {
            unsigned int tx_nw = 0;
            msresamp_crcf_execute(tx_resamp, &ofdm_symbol[ctr], 1, tx_frame_resample_buf, &tx_nw);

            // put resampled symbols into USRP buffer after scaling to avoid saturation
            unsigned int tx_n;
            for (tx_n=0; tx_n < tx_nw; tx_n++) {
                tx_usrp_buffer[tx_usrp_sample_counter++] = 0.1f * tx_frame_resample_buf[tx_n];

                if (tx_usrp_sample_counter == tx_uhd_transport_size) {    
                    tx_usrp_sample_counter = 0;
                    // Could check (size_t)actual_uhd_transport_size = tx_stream->send(...)
                    tx_stream->send(
                            &tx_usrp_buffer.front(), tx_uhd_transport_size, tx_md);

                    // prep metadata for next set of samples
                    tx_md.start_of_burst = false;
                    tx_md.has_time_spec = false;
                }
            }
        }
    } 
    // End burst
    /*
       tx_md.start_of_burst = false;
       tx_md.has_time_spec = false;
       tx_md.end_of_burst = true;
       tx_stream->send(&tx_usrp_buffer.front(), 0, tx_md);
     */

    // Fetching of UHD async messages seems to be required 
    /*tx_uhd_ack_received = false;
      while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md) ) {
      tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
      }*/
    frame_was_transmitted = true;


    // Prepare RF event log entry 
    rf_log_report_t rf_log_report;
    switch (rf_log_level) {
        case(RF_LOG_LEVEL_NONE) :
            // no logging
            break;
        case(RF_LOG_LEVEL_NORMAL) :
            rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
            rf_log_report.rf_event = RF_LOG_EVENT_TX_CONTROL_DATA;
            rf_log_report.frequency_nominal = getTxAbsoluteFreq();
            rf_log_report.bandwidth = sample_rate;
            logRfEvent(rf_log_report);
            break;
        case(RF_LOG_LEVEL_DETAIL) :
            // PLACEHOLDER for development version of logging
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::txFrameBurst: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
int RadioHardwareConfig::txMCAllocBurst(
       unsigned char* new_alloc 
        )
{
    //Wait for transmission timer to end before transmitting next packet
    double time_since_tic = timer_toc(transmit_timer);
    double time_to_sleep = mc_tx_window - time_since_tic;
    if(time_to_sleep > 0)
        usleep(time_to_sleep*1000000);

    timer_tic(transmit_timer);
    // USRP and resampler buffers
    std::vector<std::complex<float> > tx_usrp_buffer(256);

    unsigned int mctx_buffer_len = 2 * (num_nodes_in_net - 1);
    std::complex<float> mctx_buffer[mctx_buffer_len];
    
    unsigned char* header_buf = new unsigned char[P2M_FRAME_HEADER_DEFAULT_SIZE];
    header_buf[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_NEW_ALLOC;
    mctx->UpdateData(node_id - 1, header_buf, new_alloc, RHC_OFDMA_M, RHC_ms, RHC_fec0, RHC_fec1);

    // configure rest of metadata for first set of samples in a burst
    tx_md.start_of_burst = false;
    tx_md.end_of_burst = false;
    tx_md.has_time_spec = false;
    unsigned int usrp_sample_counter = 0; 
    while(!mctx->IsChannelReadyForData(node_id - 1))
    {
        mctx->GenerateSamples(mctx_buffer);

        // push resulting samples to USRP
        for (unsigned int i=0; i<mctx_buffer_len; i++) {

            // append to USRP buffer, scaling by software
            tx_usrp_buffer[usrp_sample_counter++] = 0.1f * mctx_buffer[i];

            // once USRP buffer is full, reset counter and send to device
            if (usrp_sample_counter==256) {
                // reset counter
                usrp_sample_counter=0;

                // send the result to the USRP
                usrp->get_device()->send(
                        &tx_usrp_buffer.front(), tx_usrp_buffer.size(), tx_md,
                        uhd::io_type_t::COMPLEX_FLOAT32,
                        uhd::device::SEND_MODE_FULL_BUFF
                        );
            }
        }
    }
    // End burst
    /* tx_md.start_of_burst = false;
       tx_md.has_time_spec = false;
       tx_md.end_of_burst = true;
       tx_stream->send(&tx_usrp_buffer.front(), 0, tx_md);

    // Fetching of UHD async messages seems to be required 
    tx_uhd_ack_received = false;
    while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md) ) {
    tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
    }*/
    frame_was_transmitted = true;    
    total_packets_transmitted++;
    // Prepare RF event log entry 
    rf_log_report_t rf_log_report;
    switch (rf_log_level) {
        case(RF_LOG_LEVEL_NONE) :
            // no logging
            break;
        case(RF_LOG_LEVEL_NORMAL) :
            rf_log_report.hardware_timestamp_nominal = ext_am_ptr->getElapsedTime();
            rf_log_report.rf_event = RF_LOG_EVENT_TX_CONTROL_DATA;
            rf_log_report.frequency_nominal = getTxAbsoluteFreq();
            rf_log_report.bandwidth = sample_rate;
            logRfEvent(rf_log_report);
            break;
        case(RF_LOG_LEVEL_DETAIL) :
            // PLACEHOLDER for development version of logging
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::txFrameBurst: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
int RadioHardwareConfig::txNoiseBurst(
        double tx_start_time
        )
{
    size_t noise_sample_size = floor(tx_uhd_transport_size / 
            RHC_NOMINAL_RESAMPLER_RATIO) -2;
    std::vector<std::complex<float> > noise_samples(noise_sample_size);
    std::vector<std::complex<float> > tx_usrp_buffer(tx_uhd_max_buffer_size);

    // configure rest of metadata for first set of samples in a noise only burst
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = false;
    tx_md.time_spec = uhd::time_spec_t(tx_start_time);
    tx_md.has_time_spec = true;   
    double tx_timeout = tx_start_time +0.1;

    size_t ctr;
    float itmp, qtmp;
    srand(time(NULL));
    for (ctr = 0; ctr < noise_sample_size; ctr++) {
        itmp =  0.7071f * ( (float)(rand() % 16383) )/16383.0f;
        qtmp =  0.7071f * ( (float)(rand() % 16383) )/16383.0f;
        noise_samples[ctr] = std::complex<float>(itmp, qtmp);
    }

    unsigned int tx_nw = 0;
    msresamp_crcf_execute(tx_resamp, &noise_samples[0], noise_sample_size, 
            &tx_usrp_buffer[0], &tx_nw);

    // Rescaling to avoid saturation at DAC
    unsigned int tx_n;
    for (tx_n=0; tx_n < tx_nw; tx_n++) {
        tx_usrp_buffer[tx_n] *= 0.1f;
    }

    // Send in a single burst
    tx_stream->send(&tx_usrp_buffer.front(), tx_nw, tx_md, tx_timeout);

    // End burst
    tx_md.start_of_burst = false;
    tx_md.has_time_spec = false;
    tx_md.end_of_burst = true;
    tx_stream->send(&tx_usrp_buffer.front(), 0, tx_md, tx_timeout);

    // Fetching of UHD async messages seems to be required 
    tx_uhd_ack_received = false;
    while ( !tx_uhd_ack_received && tx_stream->recv_async_msg(tx_async_md, tx_timeout) ) {
        tx_uhd_ack_received = (tx_async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


bool RadioHardwareConfig::wasFrameDetected()
{
    if (rxf.frame_was_detected) {
        return(true);
    } else {
        return(false);
    }
}
//////////////////////////////////////////////////////////////////////////


bool RadioHardwareConfig::wasFrameEndNoisy()
{
    if (rxf.frame_end_is_noisy) {
        return(true);
    } else {
        return(false);
    }
}
//////////////////////////////////////////////////////////////////////////


bool RadioHardwareConfig::wasValidFrameRx()
{
    if (rxf.frame_was_detected  && rxf.frame_is_valid) {
        return(true);
    } else {
        return(false);
    }
}
//////////////////////////////////////////////////////////////////////////


bool RadioHardwareConfig::wasFrameTx()
{
    if (frame_was_transmitted) {
        return(true);
    } else {
        return(false);
    }
}
//////////////////////////////////////////////////////////////////////////


unsigned char*  RadioHardwareConfig::getRxFrameHeaderPtr()
{
    return(rxf.frame_header);
}
//////////////////////////////////////////////////////////////////////////


unsigned int RadioHardwareConfig::getRxFramePayloadSize()
{
    return(rxf.payload_size);
}
//////////////////////////////////////////////////////////////////////////


unsigned char*  RadioHardwareConfig::getRxFramePayloadPtr()
{
    return(rxf.frame_payload);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::logRfEvent( 
        rf_log_report_t rf_log_report
        ) 
{   
    std::stringstream report;

    switch (rf_log_level) {
        case RF_LOG_LEVEL_NONE :
            return(EXIT_SUCCESS);
            break;

        case RF_LOG_LEVEL_NORMAL :
            report << std::fixed << setw(12) << setprecision(6) << rf_log_report.hardware_timestamp_nominal;
            report << "  ";
            report << dec << (int)rf_log_report.rf_event;
            report << "  ";
            report << std::fixed << setw(14) << scientific << rf_log_report.frequency_nominal;
            report << "  "; 
            report << std::fixed << setw(14) << scientific << rf_log_report.bandwidth;
            break;

        case RF_LOG_LEVEL_DETAIL :
            // PLACEHOLDER for development level logging
            break;

        default :
            cerr << "\nERROR: in RadioHardwareConfig::logRfEvent: ";
            cerr << "Unknown rf_log_level\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    rf_log_ptr->log(report.str());

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::writeRfEventLog() 
{
    rf_log_ptr->write_log();

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


bool RadioHardwareConfig::isUhdRxTuningBugPresent() 
{
    if (uhd_rx_tuning_bug_is_present) {
        return(true);
    } else {
        return(false);
    }
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::initRxfEventLog(RxfEventLogLevelType init_log_level) 
{
    // This init method needs the log object and its pointer created in the 
    // RadioHardwareConfig constructor
    rxf_event_log_level = init_log_level;
    switch (rxf_event_log_level) {
        case RXF_LOG_LEVEL_NONE :
        case RXF_LOG_LEVEL_CONSOLE_ONLY :
            // silent at startup
            break;
        case RXF_LOG_LEVEL_FILE_ONLY :
        case RXF_LOG_LEVEL_ALL :
            rxf_event_log_ptr->log("%% Start of rxf event log");
            rxf_event_log_ptr->write_log();
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::initRxEventLog" << endl;
            cerr << "    Unrecognized log level \n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::reportRxfEvent() 
{
    std::stringstream report;

    if ((rxf_event_log_level == RXF_LOG_LEVEL_NONE) ||
            (!rxf.frame_was_detected) )  {
        return(EXIT_SUCCESS);
    }

    // For all other cases capture timestamp here instead of calling
    // function in order to minimize overhead when logging inactive
    rxf.rx_complete_timestamp = getHardwareTimestamp();

    if ( (rxf_event_log_level == RXF_LOG_LEVEL_FILE_ONLY) || 
            (rxf_event_log_level == RXF_LOG_LEVEL_ALL) ) {

        report << std::fixed << setw(12) << setprecision(6) << rxf.rx_complete_timestamp;
        if  (rxf.header_is_valid) {
            report << "  1  ";
        } else {
            report << "  0  ";
        } 
        if  (rxf.payload_is_valid) {
            report << dec << rxf.payload_size << "  ";
        } else {
            report << "  0  ";
        } 
        report << rxf.stats_rssi << "  " << rxf.stats_evm << "  ";
        report << rxf.stats_cfo << "  ";
        if (rxf.frame_end_is_noisy) {
            report << "1  ";
        } else {
            report << "0  ";
        }
        report << scientific << rxf.frame_end_noise_level;
        rxf_event_log_ptr->log( report.str() );
        // Defer writing to disk due to overhead
    }

    if ( (rxf_event_log_level == RXF_LOG_LEVEL_CONSOLE_ONLY) || 
            (rxf_event_log_level == RXF_LOG_LEVEL_ALL) ) {

        cout << "\n%% Frame detected --> Header: ";
        if  (rxf.header_is_valid) {
            cout << "  VALID"; 
        } else {
            cout << "INVALID";
        }
        cout << "  Payload: ";
        if (rxf.payload_is_valid) {
            cout << dec << rxf.payload_size << " bytes" << endl;
        } else {
            cout << "INVALID" << endl;
        }
        cout << "%% " << rxf.rx_complete_timestamp;
        cout << "  RSSI: " << rxf.stats_rssi << "  EVM: " << rxf.stats_evm;
        cout << "  CFO [kHz]: " << rxf.stats_cfo << endl;
        cout << "%% End frame conditions: ";
        if (rxf.frame_end_is_noisy) {
            cout << "1  ";
        } else {
            cout << "0  ";
        }
        cout << scientific << rxf.frame_end_noise_level;
        cerr << " " << endl;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::finalizeRxfEventLog() 
{
    switch (rxf_event_log_level) {
        case RXF_LOG_LEVEL_NONE :
        case RXF_LOG_LEVEL_CONSOLE_ONLY :
            // silent
            break;
        case RXF_LOG_LEVEL_FILE_ONLY :
        case RXF_LOG_LEVEL_ALL :
            rxf_event_log_ptr->log("%% End of rxf event log");
            rxf_event_log_ptr->write_log();
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::finalizeRxEventLog" << endl;
            cerr << "    Unrecognized log level \n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::initUhdErrorLog(UhdErrorLogLevelType init_log_level)
{
    // This init method needs the log object and its pointer created in the 
    // RadioHardwareConfig constructor
    uhd_error_log_level = init_log_level;
    switch (uhd_error_log_level) {
        case UHD_ERROR_LOG_LEVEL_NONE :
        case UHD_ERROR_LOG_LEVEL_CONSOLE_ONLY :
            // silent at startup
            break;
        case UHD_ERROR_LOG_LEVEL_FILE_ONLY :
        case UHD_ERROR_LOG_LEVEL_ALL :
            uhd_error_log_ptr->log("%% Start of UHD error log");
            uhd_error_log_ptr->write_log();
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::initUhdErrorLog" << endl;
            cerr << "    Unrecognized log level \n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    resetUhdErrorStats();

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::resetUhdErrorStats()
{
    uhd_error_stats.rx_attempts = 0; 
    uhd_error_stats.rx_completions = 0;
    uhd_error_stats.rx_fail_frameburst = 0;
    uhd_error_stats.rx_fail_hearbeatburst = 0;
    uhd_error_stats.rx_fail_snapshotburst = 0;
    uhd_error_stats.rx_error_timestamp = 0.0;
    uhd_error_stats.rx_error_code = 0;
    uhd_error_stats.rx_uhd_recv_ctr = 0;
    uhd_error_stats.rx_error_num_samples = 0;    

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::reportUhdError()
{
    std::stringstream report;

    if (uhd_error_log_level == UHD_ERROR_LOG_LEVEL_NONE)  {
        return(EXIT_SUCCESS);
    }

    // Otherwise, for all other log levels assumes the caller has set:
    //  uhd_error_stats.rx_error_timestamp = getHardwareTimestamp();
    //  uhd_error_stats.rx_error_code = (unsigned int) rx_md.error_code;
    //  uhd_error_stats.rx_uhd_recv_ctr = rx_uhd_recv_ctr;
    //  uhd_error_stats.rx_error_num_samples = uhd_num_delivered_samples;

    if ( (uhd_error_log_level == UHD_ERROR_LOG_LEVEL_FILE_ONLY) || 
            (uhd_error_log_level == UHD_ERROR_LOG_LEVEL_ALL) ) {

        report << std::fixed << setw(12) << setprecision(6) << uhd_error_stats.rx_error_timestamp;
        report << dec << setw(6) << uhd_error_stats.rx_error_code;
        report << dec << setw(6) << uhd_error_stats.rx_uhd_recv_ctr;
        report << dec << setw(6) << uhd_error_stats.rx_error_num_samples;
        uhd_error_log_ptr->log( report.str() );
        // UHD errors are important enough and (hopefully) rare enough to
        // justify the overhead of immediately writing to disk
        uhd_error_log_ptr->write_log();
    }

    if ( (uhd_error_log_level == UHD_ERROR_LOG_LEVEL_CONSOLE_ONLY) || 
            (uhd_error_log_level == UHD_ERROR_LOG_LEVEL_ALL) ) {

        cerr << "\n%% UHD error" << endl;
        cerr << "    rx_error_timestamp: " << scientific <<  uhd_error_stats.rx_error_timestamp;
        cerr << "  error code: " << dec << uhd_error_stats.rx_error_code;
        cerr << "  rx_uhd_recv_ctr: " << dec << uhd_error_stats.rx_uhd_recv_ctr;
        cerr << "  rx_error_num_samples: " << dec << uhd_error_stats.rx_error_num_samples;
        cerr << " " << endl;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


void RadioHardwareConfig::printUhdErrorStats() 
{
    cout<<"\nSummary of UHD Error Stats"<<endl;
    cout << "----------------------------------------"<<endl;
    cout << "rx_attempts:             " << dec << uhd_error_stats.rx_attempts << endl; 
    cout << "rx_completions:          " << dec << uhd_error_stats.rx_completions << endl;
    cout << "rx_fail_frameburst:      " << dec << uhd_error_stats.rx_fail_frameburst << endl;
    cout << "rx_fail_hearbeatburst:   " << dec << uhd_error_stats.rx_fail_hearbeatburst << endl;
    cout << "rx_fail_snapshotburst:   " << dec << uhd_error_stats.rx_fail_snapshotburst << endl;
    cout<<" "<<endl;
}
//////////////////////////////////////////////////////////////////////////


int RadioHardwareConfig::finalizeUhdErrorLog() 
{
    switch (uhd_error_log_level) {
        case UHD_ERROR_LOG_LEVEL_NONE :
        case UHD_ERROR_LOG_LEVEL_CONSOLE_ONLY :
            // silent
            break;
        case UHD_ERROR_LOG_LEVEL_FILE_ONLY :
        case UHD_ERROR_LOG_LEVEL_ALL :
            uhd_error_log_ptr->log("%% End of UHD error log");
            uhd_error_log_ptr->write_log();
            break;
        default :
            cerr << "\nERROR in RadioHardwareConfig::finalizeUhdErrorLog" << endl;
            cerr << "    Unrecognized log level \n" << endl;
            exit(EXIT_FAILURE);
            break;
    }

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
void RadioHardwareConfig::exit_rx_thread()
{
    join_rx_thread = true;
}

// helper functions------------------------------------------------------


void handleUhdMessage (
        uhd::msg::type_t type, 
        const std::string &msg
        )
{
    if(0)
        cout <<"UHD_message: "<< msg <<endl;
    //cout << ".";
    //cout.flush();
}
//////////////////////////////////////////////////////////////////////////
