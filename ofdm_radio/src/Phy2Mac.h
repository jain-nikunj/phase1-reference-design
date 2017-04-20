/* Phy2Mac.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef PHY2MAC_H_
#define PHY2MAC_H_

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>

#include "HeartbeatDefs.h"
#include "RadioTaskDefs.h"
#include "PacketStore.hh"

#define P2M_FRAME_HEADER_DEFAULT_SIZE               8
#define P2M_FRAME_HEADER_MAX_SIZE                   14
#define P2M_FRAME_PAYLOAD_DEFAULT_SIZE              2103
#define P2M_FRAME_PAYLOAD_MAX_SIZE                  10000

#define P2M_HEADER_FIELD_FRAME_ID                   0
#define P2M_HEADER_FIELD_SOURCE_ID                  2
#define P2M_HEADER_FIELD_DESTINATION_ID             3
#define P2M_HEADER_FIELD_FRAME_TYPE                 4
#define P2M_HEADER_FIELD_PADDED_FRAME		    5

#define P2M_DESTINATION_ID_NULL                     0
#define P2M_DESTINATION_ID_BROADCAST                255

#define P2M_FRAME_TYPE_IDLE_MAC                     0
#define P2M_FRAME_TYPE_DATA                         1
#define P2M_FRAME_TYPE_HEARTBEAT                    2
#define P2M_FRAME_TYPE_CONTROL                      3
#define P2M_FRAME_TYPE_NEW_ALLOC                    4

#define P2M_FRAME_TYPE_TEST                         255
//------------------------------------------------------------------------


class Phy2Mac
{
public:
    Phy2Mac(
        bool node_is_basestation,
        unsigned char node_id, 
        unsigned int num_nodes_in_net,
        unsigned char* nodes_in_net,
        HeartbeatActivityType heartbeat_activity,
        HeartbeatPolicyType heartbeat_policy,
        unsigned int frame_header_size,
        unsigned int frame_payload_size,
        std::string tap_name,
        bool debug,
        bool using_tun_tap,
        PacketStore* ps
    );
    ~Phy2Mac();
    
    // Data frame-related functions in main application loop
    int updateMacPreTask(
        RfTxTaskType next_tx_task
    );
    int fetchTxFrame();
    int updateMacPostTask(
        unsigned char* input_frame_header,
        unsigned int input_payload_size,
        unsigned char* input_frame_payload
    );
    int deliverRxFrame();
    
    // Frame generation methods
    int createDataTxFrame(
        unsigned char data_destination_id, 
        unsigned int data_payload_size,
        unsigned char* data_payload
    );
    int emulateDataTxFrame(
        unsigned char test_destination_id, 
        unsigned int test_payload_size
    );
    int createFrameFromPacketStore(
        unsigned char destination_id
    );
    int createHeartbeatTxFrame();
    int createIdleMacTxFrame();
    
    // Access frame attributes
    unsigned char* getRxHeaderPtr();
    unsigned int getRxPayloadSize();
    unsigned char* getRxPayloadPtr();

    unsigned char* getTxHeaderPtr();
    unsigned int getTxPayloadSize();
    unsigned char* getTxPayloadPtr();
    unsigned short getTxDataFrameID();
    
    // Frame and heartbeat statistics
    void printFrameStats();
    void resetFrameStats();
    
    // Heartbeat-related functions
    int updateHeartbeatPostTask(
        bool rx_frame_was_detected,
        bool rx_frame_was_valid,
        bool rx_frame_end_was_noisy,
        unsigned char* input_frame_header,
        unsigned int input_payload_size,
        unsigned char* input_frame_payload
    );
    int assessHeartbeatStats(
        bool waveform_is_normal
    );
    bool doesHeartbeatSelectNormalMode();
    void printHeartbeatStats();
    void resetHeartbeatWindowStats();
    void resetHeartbeatStats();
    
    //Network function
    void close_interface();
    
    unsigned int tx_num_all_frames;
    unsigned int tx_num_idle_mac_frames; 
    unsigned int tx_num_data_frames;
    unsigned int tx_num_heartbeat_frames;
    unsigned int tx_num_test_frames;
    unsigned int tx_num_fetched_net_frames;
    
private:
    // Working copy of constructor parameters
    bool node_is_basestation;
    unsigned char node_id;
    unsigned int num_nodes_in_net;
    unsigned char* nodes_in_net;
    HeartbeatActivityType heartbeat_activity;
    HeartbeatPolicyType heartbeat_policy;
    unsigned int header_buffer_size;
    unsigned int payload_buffer_size;
    bool debug;
    bool using_tun_tap;

    PacketStore* ps;
    // Buffers for pre-generated and on-demand frames
    // as well as frame statistics
    unsigned char* idle_mac_frame_payload;
    unsigned char* heartbeat_frame_payload;
    unsigned char* test_frame_payload;
    
    unsigned char* rx_header_buffer;
    unsigned int rx_payload_size;
    unsigned char* rx_payload_buffer;
    
    unsigned int rx_num_all_frames;
    unsigned int rx_num_idle_mac_frames; 
    unsigned int rx_num_data_any_node_frames;
    unsigned int rx_num_data_this_node_frames;
    unsigned int rx_num_heartbeat_frames;
    unsigned int rx_num_test_frames;
    unsigned int rx_num_delivered2net_frames;
    
    unsigned char* tx_header_buffer;
    unsigned int tx_payload_size;
    unsigned char* tx_payload_buffer;
    unsigned short tx_data_frame_id;
    
    
    unsigned int heartbeat_opportunity_ctr;
    unsigned int heartbeat_missing_ctr;
    unsigned int heartbeat_valid_ctr;
    unsigned int heartbeat_errored_ctr;
    unsigned int heartbeat_jammed_ctr;
    unsigned int heartbeat_sample_window;
    unsigned int heartbeat_sample_ctr;
    unsigned int session_heartbeat_opportunity_ctr;
    unsigned int session_heartbeat_missing_ctr;
    unsigned int session_heartbeat_valid_ctr;
    unsigned int session_heartbeat_errored_ctr;
    unsigned int session_heartbeat_jammed_ctr;
    bool heartbeat_policy_selects_normal_mode;
};


#endif // PHY2MAC_H_
