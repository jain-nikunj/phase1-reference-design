/* RadioHardwareConfig.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef RADIOHARDWARECONFIG_H_
#define RADIOHARDWARECONFIG_H_


#include <iostream>
#include <fstream>
#include <complex>
#include <cstdlib>
#include <string>
#include <sstream>
#include <mutex>

#include <time.h>
#include <unistd.h>
#include <math.h>

// Ettus UHD header
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/msg.hpp>

// Liquid-dsp, usrp lib headers
#include <liquid/liquid.h>
#include <liquid/multichannelrx.h>
#include <liquid/multichanneltx.h>

// U1 application headers
#include "Phy2Mac.h"
#include "Logger.hh"
#include "timer.h"
#include "StructDefs.h"
#include "RadioConfig.hh"
// USRP hardware-specific constants
// Not clear at this point if USRP X-Series better or worse than N210
#define RHC_USRP_N210_TX2RX_SEPARATION              100.0E6
#define RHC_USRP_X300_TX2RX_SEPARATION              100.0E6
#define RHC_USRP_X310_TX2RX_SEPARATION              100.0E6

#define RHC_USRP_N210_RETUNE_DELAY                  1.4E-3
#define RHC_USRP_X300_RETUNE_DELAY                  1.4E-3
#define RHC_USRP_X310_RETUNE_DELAY                  1.4E-3

#define RHC_USRP_N210_FH_WINDOW_MEDIUM              20.0E6
#define RHC_USRP_X300_FH_WINDOW_MEDIUM              20.0E6

// Resampler ratio applies to both rx (decimate ratio) and to
// transmit (interpolate ratio)
#define RHC_NOMINAL_RESAMPLER_RATIO                 2
#define RHC_RX_RECOMMENDED_SAMPLE_SIZE_DEFAULT      8786
#define RHC_TX_UHD_TRANSPORT_SIZE                   300
#define RHC_TX_BURST_LENGTH                         8100

#define RHC_NUM_CHANNELS                            1
#define RHC_M                                       48
#define RHC_OFDMA_M				                    512
#define RHC_cp_len                                  6
#define RHC_OFDM_SYMBOL_LENGTH                      54
#define RHC_OFDMA_SYMBOL_LENGTH                     518
#define RHC_OFDM_SYMBOLS_PER_FRAME                  75
#define RHC_taper_len                               4
#define RHC_ms                                      LIQUID_MODEM_QPSK
#define RHC_fec0                                    LIQUID_FEC_CONV_V27
#define RHC_fec1                                    LIQUID_FEC_NONE
#define RHC_check                                   LIQUID_CRC_32

#define RHC_FRAME_HEADER_DEFAULT_SIZE               8
#define RHC_FRAME_HEADER_MAX_SIZE                   14
#define RHC_FRAME_PAYLOAD_DEFAULT_SIZE              2103
#define RHC_FRAME_PAYLOAD_MAX_SIZE                  10000

#define RHC_CALIBRATE_RX_NOISE_RATIO                2.0
#define RHC_CALIBRATE_RX_NOISE_THRESHOLD_DEFAULT    1.0E-2

#define PADDED_BYTES				                13
#define RHC_THROUGHPUT_THRESHOLD		            25

//Types of USRP hardware supported by this application
enum UsrpHardwareType{
    USRP_MODEL_N210,
    USRP_MODEL_X300_PCIE,
    USRP_MODEL_X300_GBE
};

// Types of reference clock available for the USRP hardware 
enum ClockReferenceType{
    CLOCK_REF_GPSDO,
    CLOCK_REF_LAB,
    CLOCK_REF_NONE
};


// RF event logging is configured and controlled exclusively
// within this class, unlike the overall application logging
enum RfLogLevelType {
    RF_LOG_LEVEL_NONE,
    RF_LOG_LEVEL_NORMAL,
    RF_LOG_LEVEL_DETAIL
};

enum RfLogEventType {
    // These are for the RF_LOG_LEVEL_NORMAL 
    RF_LOG_EVENT_TX_HEARTBEAT           = 0,
    RF_LOG_EVENT_TX_DATA                = 1,
    RF_LOG_EVENT_RX_HEARTBEAT           = 2,
    RF_LOG_EVENT_RX_DATA                = 3,
    RF_LOG_EVENT_TX_MC_DATA             = 4,
    RF_LOG_EVENT_TX_OFDMA_DATA          = 5,
    RF_LOG_EVENT_RX_MC_DATA             = 6,
    RF_LOG_EVENT_RX_OFDMA_DATA          = 7,
    RF_LOG_EVENT_TX_CONTROL_DATA        = 8,
    RF_LOG_EVENT_RX_CONTROL_DATA        = 9
    
    // RF_LOG_LEVEL_DETAIL may include other codes
};

enum SubcarrierAllocation {
    OUTER_ALLOCATION,
    INNER_ALLOCATION,
    DEFAULT_ALLOCATION
};

enum OFDMATransmissionType {
    DATA        = 201,
    CONTROL     = 202
};

typedef struct {
    // Fields needed for RF_LOG_LEVEL_NORMAL listed in order of report
    double               hardware_timestamp_nominal;
    RfLogEventType       rf_event;
    double               frequency_nominal;
    double               bandwidth;
} rf_log_report_t;


// Structure for capturing received frame and its metadata 
typedef struct {
    bool                frame_was_detected;
    bool                header_is_valid;
    unsigned char       frame_header[RHC_FRAME_HEADER_MAX_SIZE];
    bool                payload_is_valid;
    unsigned int        payload_size;
    unsigned char       frame_payload[RHC_FRAME_PAYLOAD_MAX_SIZE];
    bool                frame_is_valid;
    
    double              samplerate;
    float               stats_cfo;
    float               stats_evm;
    float               stats_rssi;

    double              frame_end_noise_level;
    bool                frame_end_is_noisy;

    double              rx_complete_timestamp;

    bool                callback_debug;                
} received_frame_t;


enum RxfEventLogLevelType {
    RXF_LOG_LEVEL_NONE,
    RXF_LOG_LEVEL_FILE_ONLY,
    RXF_LOG_LEVEL_CONSOLE_ONLY,
    RXF_LOG_LEVEL_ALL
};

// rxf event logging recycles the received_frame_t structure 

enum UhdErrorLogLevelType {
    UHD_ERROR_LOG_LEVEL_NONE,
    UHD_ERROR_LOG_LEVEL_FILE_ONLY,
    UHD_ERROR_LOG_LEVEL_CONSOLE_ONLY,
    UHD_ERROR_LOG_LEVEL_ALL
};

typedef struct {
    // Receive side details of a specific error
    double rx_error_timestamp;
    unsigned int rx_error_code;
    unsigned int rx_uhd_recv_ctr;
    size_t rx_error_num_samples;    

    // Receive side summary statistics
    unsigned int rx_attempts; 
    unsigned int rx_completions;
    unsigned int rx_fail_frameburst;
    unsigned int rx_fail_hearbeatburst;
    unsigned int rx_fail_snapshotburst;

    // Transmit side currently untracked 

} uhd_error_stats_t;



// Helper functions ------------------------------------------------------
int rxCallback(
        unsigned char *  _header,
        int              _header_valid,
        unsigned char *  _payload,
        unsigned int     _payload_len,
        int              _payload_valid,
        framesyncstats_s _stats,
        void *           _userdata
    );

    
void handleUhdMessage (
        uhd::msg::type_t type, 
        const std::string &msg
     );
//////////////////////////////////////////////////////////////////////////

void* run_ofdma_rx(void* thread_args);
void* run_mc_rx(void* thread_args);

class RadioHardwareConfig
{
public:
    RadioHardwareConfig(
        std::string radio_hardware,
        //UsrpHardwareType usrp_hardware,
        std::string usrp_address_name,
        //ClockReferenceType clock_ref_type,
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
    );
    ~RadioHardwareConfig();  
    
    double getHardwareTimestamp();
    double getHardwareNextSecond();
    int setHardwareTimestamp(uhd::time_spec_t time);

    double getFhWindowMedium();
    double getFhWindowSmall();
    double getNormalFreq();
    double getRxNormalFreq();
    double getTxNormalFreq();
    double getTx2RxFreqSeparation();
    double getRxAbsoluteFreq();
    double getTxAbsoluteFreq();
    double getRxRateMeasured();
    unsigned int getTxBurstLength();
    double getTxRateMeasured();
    double getUhdRetuneDelay();
    void exit_rx_thread();
 
    int tuneRxManual(
        double rf_freq, 
        double dsp_freq
        );
    int tuneTxManual(
        double rf_freq, 
        double dsp_freq
        );
    int tune2NormalFreq();
    int rxFrameBurst(
        double rx_start_time,
        size_t rx_total_requested_samples
        );
    int rxHeartbeatBurst(
        double rx_start_time,
        size_t rx_total_requested_samples
        );
    int rxHeartbeatCalibration(
        double rx_start_time,
        size_t rx_total_requested_samples
        );
    int rxSnapshotBurst(
        double rx_start_time,
        size_t rx_total_requested_samples
        );
    int writeSnapshotBurst();
    int txFrameBurst(
        double tx_start_time,
        unsigned char* header_buf, 
        unsigned int payload_size,
        unsigned char* payload_buf
        );
    int txOFDMAFrameBurst(
        OFDMATransmissionType tx_type = DATA
        );
    int txOFDMAAllocBurst(
        unsigned char* new_alloc
        );
    int txMCFrameBurst(
        OFDMATransmissionType tx_type = DATA
        );
    int txMCAllocBurst(
        unsigned char* new_alloc
        );
    int txHearbeatBurst(
        double tx_start_time,
        unsigned char* header_buf, 
        unsigned int payload_size,
        unsigned char* payload_buf
        );
    int txNoiseBurst(
        double tx_start_time
        );
    
    bool wasFrameDetected();
    bool wasValidFrameRx();
    bool wasFrameEndNoisy();

    bool wasFrameTx();
    
    unsigned char* getRxFrameHeaderPtr();
    unsigned int getRxFramePayloadSize();
    unsigned char* getRxFramePayloadPtr();
    
    int logRfEvent(
        rf_log_report_t rf_log_report
        );
    int writeRfEventLog();
    
    bool isUhdRxTuningBugPresent();
  
    void switch_allocation();
    void recreate_modem(); 
    // Working copy of constructor parameters
    double normal_freq;
    double rf_gain_rx;
    double rf_gain_tx;
    double sample_rate;
    UsrpHardwareType usrp_hardware;
    std::string usrp_address_name;
    ClockReferenceType clock_ref_type;
    bool node_is_basestation;
    unsigned int node_id;
    unsigned int num_nodes_in_net;
    unsigned int frame_len;
    Logger* app_log_ptr;
    Logger* rf_log_ptr;
    RxfEventLogLevelType rxf_event_log_level;
    Logger* rxf_event_log_ptr;
    UhdErrorLogLevelType uhd_error_log_level;
    Logger* uhd_error_log_ptr;
    Logger* alloc_log_ptr;
    bool debug;
    bool u4;
    timer_s* rx_timer;
    bool slow;
    float ofdma_tx_window;
    float mc_tx_window;
    bool anti_jam;
    
    // Fine control of RF event logging behavior
    RfLogLevelType rf_log_level;
    
    //Timing variables for slowing down tx since U4 isn't scheduled
    timer transmit_timer;
    double time_between_ofdma_tx;
    double time_between_mc_tx;

    //Bool to turn off rx_thread prematurely in case of sigint
    bool join_rx_thread;
    
    // RadioHardwareConfig debug tools
    int initRxfEventLog(RxfEventLogLevelType init_log_level);
    int reportRxfEvent();
    int finalizeRxfEventLog();
    int initUhdErrorLog(UhdErrorLogLevelType init_log_level);
    int resetUhdErrorStats();
    int reportUhdError();
    void printUhdErrorStats();
    int finalizeUhdErrorLog();
    
    // Receive side USRP variables/objects
    size_t initial_rx_recommended_sample_size;
    size_t rx_uhd_transport_size;
    size_t rx_uhd_max_buffer_size;
    double rx_absolute_freq;
    uhd::tune_request_t rx_tune_req;
    double usrp_rx_rate;
    double rx_resamp_rate;
    msresamp_crcf rx_resamp;
    uhd::rx_streamer::sptr rx_stream;
    uhd::rx_metadata_t rx_md;
    std::mutex sync_mutex;
    std::mutex gen_mutex;
    // General USRP variables
    uhd::usrp::multi_usrp::sptr usrp;
    unsigned int uhd_transport_size;
    double tx2rx_freq_separation;
    double fh_window_small;
    double fh_window_medium;
    // fh_window_large computed when frequency table generated
    double uhd_retune_delay;
    bool  uhd_rx_tuning_bug_is_present;
    uhd_error_stats_t uhd_error_stats;

    bool received_new_alloc;
    unsigned char new_alloc[RHC_OFDMA_M];

    bool hardened;

    RadioConfig* rc; 
    //Stats
    unsigned int valid_bytes_received;
    unsigned int total_packets_transmitted;
    unsigned int total_packets_received;
    unsigned int valid_headers_received;
    unsigned int invalid_headers_received;
    unsigned int valid_payloads_received;
    unsigned int invalid_payloads_received;
    unsigned int network_packets_transmitted;
    unsigned int network_packets_received;
    unsigned int dummy_packets_transmitted;
    unsigned int dummy_packets_received; 
    unsigned int high_evm_counts[RHC_OFDMA_M];
    SubcarrierAllocation allocation;

    // Receive side modem variables/objects
    firfilt_crcf rx_prefilt;
    //For receiving heartbeats/snapshots at either basestation or mobile
    ofdmflexframesync fs;
    //For receiving ofdma data at mobiles
    ofdmflexframesync ofdma_fs_inner;
    ofdmflexframesync ofdma_fs_outer;
    ofdmflexframesync ofdma_fs_default;
    //For receiving multichannel data at basestation
    multichannelrx* mcrx;
    //For transmitting multichannel data from mobiles
    multichanneltx* mctx;
    // rxf includes buffers for frame itself and metadata
    received_frame_t rxf;

private:
    
    // Receive side sensing for heartbeat integrity
    double rx_noise_linear_threshold;
    
    // Receive side sensing for testing
    bool rx_snapshot_was_triggered;
    std::complex<float>* rx_snapshot_samples;
    unsigned int rx_snapshot_total_size;
    unsigned int rx_snapshot_sample_idx;
    std::string rx_snapshot_filename;
    
    // Transmit side USRP variables/objects
    size_t tx_uhd_transport_size;
    size_t tx_uhd_max_buffer_size;
    double tx_absolute_freq;
    uhd::tune_request_t tx_tune_req;
    double usrp_tx_rate;
    double tx_resamp_rate;
    msresamp_crcf tx_resamp;
    uhd::tx_streamer::sptr tx_stream;
    uhd::async_metadata_t tx_async_md;
    bool tx_uhd_ack_received; 
    uhd::tx_metadata_t  tx_md;
    
    // Transmit side modem variables/objects
    ofdmflexframegenprops_s fgprops;
    //For transmitting heartbeats/snapshots from either basestation or mobile
    ofdmflexframegen fg;
    //For transmitting ofdma data from basestation
    ofdmflexframegen ofdma_fg_inner;
    ofdmflexframegen ofdma_fg_outer;
    ofdmflexframegen ofdma_fg_default;
    
    bool frame_was_transmitted;
    unsigned char tx_frame_header[RHC_FRAME_HEADER_MAX_SIZE];
    unsigned char tx_frame_payload[RHC_FRAME_PAYLOAD_MAX_SIZE];
};



#endif // RADIOHARDWARECONFIG_H_
