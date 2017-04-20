/* Phy2Mac.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 *
 */
#include "Phy2Mac.h"

using namespace std;

Phy2Mac::Phy2Mac(
        bool node_is_basestation,
        unsigned char node_id, 
        unsigned int num_nodes_in_net,
        unsigned char* nodes_in_net,
        HeartbeatActivityType heartbeat_activity,
        HeartbeatPolicyType heartbeat_policy,
        unsigned int frame_header_size,
        unsigned int frame_payload_size,
        string tap_name,
        bool debug, 
        bool using_tun_tap,
        PacketStore* ps
    )
{
    this->node_is_basestation = node_is_basestation;
    this->node_id = node_id;
    this->num_nodes_in_net = num_nodes_in_net;
    this->nodes_in_net = nodes_in_net;
    this->heartbeat_activity = heartbeat_activity;
    this->heartbeat_policy = heartbeat_policy;
    this->header_buffer_size = frame_header_size;
    this->payload_buffer_size = frame_payload_size;
    this->debug = debug;
    this->using_tun_tap = using_tun_tap;
    this->ps = ps;

    idle_mac_frame_payload = new unsigned char[payload_buffer_size];
    heartbeat_frame_payload = new unsigned char[payload_buffer_size];
    test_frame_payload = new unsigned char[payload_buffer_size];
    
    unsigned int n;
    for (n = 0; n < payload_buffer_size; n++) {
        idle_mac_frame_payload[n] = (unsigned char)(n  & 0x00);
        heartbeat_frame_payload[n] = (unsigned char)(n  & 0x00);
        test_frame_payload[n] = (unsigned char)(rand() & 0x00);
    }
    
    if (header_buffer_size > P2M_FRAME_HEADER_MAX_SIZE) {
        cerr<<"ERROR: In Phy2Mac::Phy2Mac header_buffer_size is > ";
        cerr<<"P2M_FRAME_HEADER_MAX_SIZE  " <<endl;
        exit(EXIT_FAILURE);
    }
    if (payload_buffer_size > P2M_FRAME_PAYLOAD_MAX_SIZE) {
        cerr<<"ERROR: In Phy2Mac::Phy2Mac payload_buffer_size is > ";
        cerr<<"P2M_FRAME_PAYLOAD_MAX_SIZE  " <<endl;
        exit(EXIT_FAILURE);
    }
    
    rx_header_buffer = new unsigned char[header_buffer_size];
    rx_payload_size = 0;
    rx_payload_buffer = new unsigned char[payload_buffer_size];
    
    tx_header_buffer = new unsigned char[header_buffer_size];
    tx_payload_size = 0;
    tx_payload_buffer = new unsigned char[payload_buffer_size];
    tx_data_frame_id = 0;

    
    resetFrameStats();
    resetHeartbeatStats();
    // Set initial choice of normal mode or FH mode
    switch (heartbeat_policy) {
        case HEARTBEAT_POLICY_LOCKED_TO_FDD :
            heartbeat_policy_selects_normal_mode = true;
            break;
        case HEARTBEAT_POLICY_LOCKED_TO_FH :
            heartbeat_policy_selects_normal_mode = false;
            break;
        case HEARTBEAT_POLICY_A :
            // fall through
        case HEARTBEAT_POLICY_B :
            heartbeat_policy_selects_normal_mode = true;
            break;
        default :
            cerr << "ERROR: In Phy2Mac::Phy2Mac unknown policy"<<endl;
            exit(EXIT_FAILURE);
    }
}
//////////////////////////////////////////////////////////////////////////


Phy2Mac::~Phy2Mac()
{   
    try{ delete[] idle_mac_frame_payload; } catch (...) {}
    try{ delete[] heartbeat_frame_payload; } catch (...) {}
    try{ delete[] test_frame_payload; } catch (...) {}
    
    try{ delete[] rx_header_buffer; } catch (...) {}
    try{ delete[] rx_payload_buffer; } catch (...) {}

    try{ delete[] tx_header_buffer; } catch (...) {}
    try{ delete[] tx_payload_buffer; } catch (...) {}
}
//////////////////////////////////////////////////////////////////////////


int Phy2Mac::updateMacPreTask(
        RfTxTaskType next_tx_task
    )
{
    // Transmit side MAC operations
    
    /* Currently, this transmit side preparation code is now in
     *  RadioTaskManager::doTask()  to take advantage of its threading
     * 
     * The logic is as follows:
     *       switch (next_tx_task) {
     *           case RF_TASK_TX_DATA :
     *               // if (1) PLACEHOLDER if network has data to send
     *               if (1) {
     *                   fetchTxFrame();
     *               } else {
     *                   createIdleMacTxFrame();
     *               }
     *               break;
     *           case RF_TASK_TX_HEARTBEAT :
     *
     *               createHeartbeatTxFrame();
     *               break;
     *           default :
     *               // No change to transmit side MAC for other kinds of tasks
     *               break;
     *       }
     * 
     */ 
    
    return(EXIT_SUCCESS);
}
/////////////////////////////////////////////////////////////////////////


// This function assumes it is only called if a heartbeat message is expected
// this can be tested with RadioScheduler::isHeartbeatExpected
int Phy2Mac::updateHeartbeatPostTask(
        bool rx_frame_was_detected,
        bool rx_frame_was_valid,
        bool rx_frame_end_was_noisy,
        unsigned char* input_frame_header,
        unsigned int input_payload_size,
        unsigned char* input_frame_payload
        )
{
    heartbeat_opportunity_ctr++;
    
    // The current approach to AWGN jamming detection is to check if the loss 
    // of an expected heartbeat is accompanied by a high noise level; otherwise,
    // it is assumed to be "missing" due to natural causes (e.g., shadowing)
    if ( !rx_frame_was_detected ) {
        if (rx_frame_end_was_noisy) {
           heartbeat_jammed_ctr++;
        } else {
            heartbeat_missing_ctr++;
        }

        return(EXIT_SUCCESS);
    }
    
    // Frames with errors accompanied by a high noise level are also 
    // assumed to be the product of AWGN jamming
    if (rx_frame_was_detected && !rx_frame_was_valid) {
        if (rx_frame_end_was_noisy) {
           heartbeat_jammed_ctr++;
        } else {
            heartbeat_errored_ctr++;
        }

        return(EXIT_SUCCESS);
    }
    
    if (rx_frame_was_valid) {
        heartbeat_valid_ctr++;
        
        rx_num_heartbeat_frames++; 
        // PLACEHOLDER
        // updateHeartbeatPostTask responsible for all heartbeat-related
        // link layer processing; if needed any special payload could
        // be extracted here
    }
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


// This function assumes that a valid frame was received by the radio
//  can test for this with RadioHardwareConfig::wasValidFrameRx()
int Phy2Mac::updateMacPostTask(
    unsigned char* input_frame_header,
    unsigned int input_payload_size,
    unsigned char* input_frame_payload
    )
{
    bool need_to_copy_frame = false;
    bool need_to_deliver_data_frame = false;
    unsigned char frame_type = input_frame_header[P2M_HEADER_FIELD_FRAME_TYPE];
    unsigned char destination_id = input_frame_header[P2M_HEADER_FIELD_DESTINATION_ID];
    
    // Assume this function only called when valid frame detected
    rx_num_all_frames++; 
    
    switch (frame_type) {
        case P2M_FRAME_TYPE_IDLE_MAC :
            rx_num_idle_mac_frames++;
            break;
        case P2M_FRAME_TYPE_DATA :
            // Update counter for data frame addressed to any node then
            // check frame's destination address and/or role of this 
            // node to determine if the data frame needs to be processed
            rx_num_data_any_node_frames++;
            
            if (node_is_basestation) {
                // Then every data frame needs to be processed
                rx_num_data_this_node_frames++;
                need_to_copy_frame = true;
                need_to_deliver_data_frame = true;
            } else {
                // Else a mobile only needs to process frames addressed to
                // it or to the broadcast address 
                if ((destination_id == node_id) || 
                        (destination_id == P2M_DESTINATION_ID_BROADCAST)) {
                    rx_num_data_this_node_frames++;
                    need_to_copy_frame = true;
                    need_to_deliver_data_frame = true;
                }
            }
            break;
            
        //NOTE: Heartbeat frames and associated statistics are handled in
        //      Phy2Mac::updateHeartbeatPostTask

        case P2M_FRAME_TYPE_TEST :
            rx_num_test_frames++;
            break;
        default :
            // Ignore all other kinds of frames
            break;
    }
    
    if (need_to_copy_frame) {
        memcpy(rx_header_buffer, input_frame_header, P2M_FRAME_HEADER_DEFAULT_SIZE);
        if (input_payload_size > payload_buffer_size) {
            cerr<<"ERROR: In  Phy2Mac::updateMacPostTask input_payload_size: ";
            cerr<< dec << input_payload_size << " > payload_buffer_size" <<endl;
            exit(EXIT_FAILURE);
        }
        memcpy(rx_payload_buffer, input_frame_payload, input_payload_size);
    }
    
    if (need_to_deliver_data_frame) {
        deliverRxFrame();
    }
    
    return(EXIT_SUCCESS);
}
/////////////////////////////////////////////////////////////////////////


int Phy2Mac::createDataTxFrame(
    unsigned char data_destination_id, 
    unsigned int data_payload_size,
    unsigned char* data_payload
    )
{ 
    if (data_payload_size > payload_buffer_size) {
        cerr << "ERROR: In Phy2Mac::createDataFrame()" <<endl;
        cerr << "       requested data_payload_size: " << data_payload_size <<endl;
        cerr << "       but buffer size is: " << payload_buffer_size <<endl;
        exit(EXIT_FAILURE);
    }
    memcpy(tx_payload_buffer, data_payload, data_payload_size);
    tx_payload_size = data_payload_size;
    
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] = (tx_data_frame_id >>8) & 0xff;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID+1] = tx_data_frame_id     & 0xff;
    tx_data_frame_id++;
    
    tx_header_buffer[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
    tx_header_buffer[P2M_HEADER_FIELD_DESTINATION_ID] = data_destination_id;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_DATA;

    tx_num_all_frames++;    tx_num_data_frames++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int Phy2Mac::emulateDataTxFrame(
    unsigned char test_destination_id, 
    unsigned int test_payload_size
    )
{ 
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] = (tx_data_frame_id >>8) & 0xff;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID+1] = tx_data_frame_id     & 0xff;
    tx_data_frame_id++;

    tx_header_buffer[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
    tx_header_buffer[P2M_HEADER_FIELD_DESTINATION_ID] = test_destination_id;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_TEST;
    
    memcpy(tx_payload_buffer, test_frame_payload, test_payload_size);
    tx_payload_size = test_payload_size;
    
    tx_num_all_frames++;    tx_num_data_frames++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

int Phy2Mac::createFrameFromPacketStore(unsigned char destination_id)
{ 
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] = (tx_data_frame_id >>8) & 0xff;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID+1] = tx_data_frame_id     & 0xff;

    tx_header_buffer[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
    tx_header_buffer[P2M_HEADER_FIELD_DESTINATION_ID] = destination_id;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_DATA;
 
   // cout << "ps.size(): " << ps.size() << endl; 
    if(ps->size() >= tx_data_frame_id)
    {
        memcpy(tx_payload_buffer, ps->get_frame((long int)tx_data_frame_id, 0), P2M_FRAME_PAYLOAD_DEFAULT_SIZE);
    }
    else
    {
	std::cout << "!";
	std::cout.flush();
        memcpy(tx_payload_buffer, test_frame_payload, P2M_FRAME_PAYLOAD_DEFAULT_SIZE);
    }
    tx_payload_size = P2M_FRAME_PAYLOAD_DEFAULT_SIZE;
    
    tx_data_frame_id++;

    tx_num_all_frames++;    tx_num_data_frames++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

int Phy2Mac::createHeartbeatTxFrame()
{ 
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] = 0;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID+1] = 0;
    tx_header_buffer[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
    tx_header_buffer[P2M_HEADER_FIELD_DESTINATION_ID] = P2M_DESTINATION_ID_BROADCAST;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_HEARTBEAT;
    
    //TODO PLACEHOLDER could fetch any special payload for heartbeat frame
    memcpy(tx_payload_buffer, heartbeat_frame_payload, 
            P2M_FRAME_PAYLOAD_DEFAULT_SIZE);
    tx_payload_size = P2M_FRAME_PAYLOAD_DEFAULT_SIZE;
    
    tx_num_all_frames++;    tx_num_heartbeat_frames++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int Phy2Mac::createIdleMacTxFrame()
{ 
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] = 0;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_ID+1] = 0;
    tx_header_buffer[P2M_HEADER_FIELD_SOURCE_ID] = node_id;
    tx_header_buffer[P2M_HEADER_FIELD_DESTINATION_ID] = P2M_DESTINATION_ID_NULL;
    tx_header_buffer[P2M_HEADER_FIELD_FRAME_TYPE] = P2M_FRAME_TYPE_IDLE_MAC;
    
    //Idle frames just send header + 1 dummy byte
    tx_payload_size = 1;
    /*
    // If idle frame did need to have a payload:
    memcpy(tx_payload_buffer, idle_mac_frame_payload, 
        P2M_FRAME_PAYLOAD_DEFAULT_SIZE);
    tx_payload_size = P2M_FRAME_PAYLOAD_DEFAULT_SIZE;
    */
    tx_num_all_frames++;    tx_num_idle_mac_frames++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int Phy2Mac::fetchTxFrame() 
{
    
     
    if (1) {    //TODO PLACEHOLDER for interface to network layer code
                //      replace if (1) with actual check of network layer
                //     replace emulateDataTxFrame with actual copy
        if(using_tun_tap)
            createFrameFromPacketStore(ps->get_next_frame_destination());
        else
            emulateDataTxFrame(123, P2M_FRAME_PAYLOAD_DEFAULT_SIZE);
        
        tx_num_fetched_net_frames++;
        
    } else {    // Network has no data, send idle frame (header only)
        createIdleMacTxFrame();
    }
        
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int Phy2Mac::deliverRxFrame()
{
    
   //TODO PLACEHOLDER for delivering to the network layer
   
   // unsigned short rx_frame_id = (rx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] << 8 |
   //             rx_header_buffer[P2M_HEADER_FIELD_FRAME_ID +1] );
       
    unsigned int packet_id = (  rx_header_buffer[P2M_HEADER_FIELD_FRAME_ID] << 8 |
                                rx_header_buffer[P2M_HEADER_FIELD_FRAME_ID + 1]);
    if(using_tun_tap)
        ps->add_frame(packet_id, 0, rx_payload_buffer, P2M_FRAME_PAYLOAD_DEFAULT_SIZE);
    rx_num_delivered2net_frames++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


unsigned char* Phy2Mac::getRxHeaderPtr()
{   
    return(&rx_header_buffer[0]);
}
//////////////////////////////////////////////////////////////////////////


unsigned int Phy2Mac::getRxPayloadSize()
{   
    return(rx_payload_size);
}
//////////////////////////////////////////////////////////////////////////


unsigned char* Phy2Mac::getRxPayloadPtr()
{   
    return(&rx_payload_buffer[0]);
}
//////////////////////////////////////////////////////////////////////////


unsigned char* Phy2Mac::getTxHeaderPtr()
{   
    return(&tx_header_buffer[0]);
}
//////////////////////////////////////////////////////////////////////////


unsigned int Phy2Mac::getTxPayloadSize()
{   
    return(tx_payload_size);
}
//////////////////////////////////////////////////////////////////////////


unsigned char* Phy2Mac::getTxPayloadPtr()
{   
    return(&tx_payload_buffer[0]);
}
//////////////////////////////////////////////////////////////////////////


unsigned short Phy2Mac::getTxDataFrameID()
{   
    return(tx_data_frame_id);
}
//////////////////////////////////////////////////////////////////////////


void Phy2Mac::printFrameStats()
{
    cout << "Phy2Mac frame statistics for frames to/from this node_id: "<< dec << node_id << endl;;
    cout << "-----------------------------------------------------------------------------\n";
    cout << "rx_num_all_frames:            " << setw(7) << dec << rx_num_all_frames <<"   ";
    cout << "tx_num_all_frames:            " << setw(7) << dec << tx_num_all_frames << endl;
    
    cout << "rx_num_data_this_node_frames: " << setw(7) << dec << rx_num_data_this_node_frames <<"   ";
    cout << "tx_num_data_frames:           " << setw(7) << dec << tx_num_data_frames << endl;
    cout << "rx_num_data_any_node_frames:  " << setw(7) << dec << rx_num_data_this_node_frames <<endl;
    
    cout << "rx_num_heartbeat_frames:      " << setw(7) << dec << rx_num_heartbeat_frames <<"   ";
    cout << "tx_num_heartbeat_frames:      " << setw(7) << dec << tx_num_heartbeat_frames << endl;

    cout << "rx_num_idle_mac_frames:       " << setw(7) << dec << rx_num_idle_mac_frames <<"   ";
    cout << "tx_num_idle_mac_frames:       " << setw(7) << dec << tx_num_idle_mac_frames << endl;
    
    cout << "rx_num_test_frames:           " << setw(7) << dec << rx_num_test_frames <<"   ";
    cout << "tx_num_test_frames:           " << setw(7) << dec << tx_num_test_frames << endl;
    
    cout << "rx_num_delivered2net_frames:  " << setw(7) << dec << rx_num_delivered2net_frames <<"   ";
    cout << "tx_num_fetched_net_frames:    " << setw(7) << dec << tx_num_fetched_net_frames << endl;

    cout << "\n";
}
//////////////////////////////////////////////////////////////////////////


void Phy2Mac::resetFrameStats()
{
    rx_num_all_frames = 0;
    rx_num_idle_mac_frames = 0; 
    rx_num_data_any_node_frames = 0;
    rx_num_data_this_node_frames = 0;
    rx_num_heartbeat_frames = 0;
    rx_num_test_frames = 0;
    rx_num_delivered2net_frames = 0;
    
    tx_num_all_frames = 0;
    tx_num_idle_mac_frames = 0; 
    tx_num_data_frames = 0;
    tx_num_heartbeat_frames = 0;
    tx_num_test_frames = 0;
    tx_num_fetched_net_frames = 0;
}
//////////////////////////////////////////////////////////////////////////


int Phy2Mac::assessHeartbeatStats(
    bool waveform_is_normal
)
{

    // If no expected activity do not update any statistics 
    if (heartbeat_activity == HEARTBEAT_ACTIVITY_NONE ) {
        return(EXIT_SUCCESS);
    }
    // Else, in HEARTBEAT_ACTIVITY_PER_SCHEDULE 
    
    heartbeat_sample_ctr++;
    if (heartbeat_sample_ctr == P2M_HEARTBEAT_SAMPLE_WINDOW) {
        
        switch (heartbeat_policy) {
            case HEARTBEAT_POLICY_LOCKED_TO_FDD :
                heartbeat_policy_selects_normal_mode = true;
                break;
            case HEARTBEAT_POLICY_LOCKED_TO_FH :
                heartbeat_policy_selects_normal_mode = false;
                break;
                
            case HEARTBEAT_POLICY_A :
                // for limited statistics keep status quo
                if (heartbeat_opportunity_ctr < 2) {
                    heartbeat_policy_selects_normal_mode = waveform_is_normal;
                    break;
                }

                if ( (2 * heartbeat_jammed_ctr) <= 
                        (heartbeat_opportunity_ctr -heartbeat_missing_ctr -
                         heartbeat_errored_ctr)) {
                    
                    heartbeat_policy_selects_normal_mode = true; 
                } else {
                    heartbeat_policy_selects_normal_mode = false; 
                }
                break;
                
            case HEARTBEAT_POLICY_B :
                // keep status quo
                heartbeat_policy_selects_normal_mode = waveform_is_normal;
                break;

            default :
                cerr << "ERROR: In Phy2Mac::updateHeartbeat() unknown policy"<<endl;
                exit(EXIT_FAILURE);
                break;
        }
        
        session_heartbeat_opportunity_ctr += heartbeat_opportunity_ctr;
        session_heartbeat_valid_ctr += heartbeat_valid_ctr;
        session_heartbeat_errored_ctr += heartbeat_errored_ctr;
        session_heartbeat_jammed_ctr += heartbeat_jammed_ctr;
        resetHeartbeatWindowStats();
    }
        
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


bool Phy2Mac::doesHeartbeatSelectNormalMode()
{
    return(heartbeat_policy_selects_normal_mode);
}
//////////////////////////////////////////////////////////////////////////


void Phy2Mac::printHeartbeatStats()
{
    cout << "Phy2Mac heartbeat statistics"<<endl;
    cout << "--------------------------------------------------" << endl;
    cout << "Per window statistics" <<endl;
    cout << "  heartbeat_opportunity_ctr:          "<<setw(5) <<heartbeat_opportunity_ctr <<endl;
    cout << "  heartbeat_missing_ctr:              "<<setw(5) <<heartbeat_missing_ctr  <<endl;
    cout << "  heartbeat_valid_ctr:                "<<setw(5) <<heartbeat_valid_ctr  <<endl;
    cout << "  heartbeat_errored_ctr:              "<<setw(5) <<heartbeat_errored_ctr <<endl;
    cout << "  heartbeat_jammed_ctr:               "<<setw(5) <<heartbeat_jammed_ctr <<endl;
    cout << " "<<endl;
    cout << "  heartbeat_sample_ctr:               "<<setw(5) <<heartbeat_sample_ctr <<endl;
    cout << "  heartbeat_sample_window:            "<<setw(5) <<heartbeat_sample_window <<endl;
    cout << "  --->heartbeat selected mode is ";
    if (heartbeat_policy_selects_normal_mode) {
        cout << "TRUE = FDD mode\n" << endl;
    } else {
        cout << "FALSE = FH mode\n" << endl;
    }
    cout << "Per session statistics" <<endl;
    cout << "  session_heartbeat_opportunity_ctr:  "<<setw(5)<< session_heartbeat_opportunity_ctr <<endl;
    cout << "  session_heartbeat_missing_ctr:      "<<setw(5)<< session_heartbeat_missing_ctr <<endl;
    cout << "  session_heartbeat_valid_ctr:        "<<setw(5)<< session_heartbeat_valid_ctr <<endl;
    cout << "  session_heartbeat_errored_ctr:      "<<setw(5)<< session_heartbeat_errored_ctr <<endl;
    cout << "  session_heartbeat_jammed_ctr:       "<<setw(5)<< session_heartbeat_jammed_ctr <<endl;
}
//////////////////////////////////////////////////////////////////////////


void Phy2Mac::resetHeartbeatWindowStats()
{
    heartbeat_opportunity_ctr = 0;
    heartbeat_missing_ctr = 0;
    heartbeat_valid_ctr = 0;
    heartbeat_errored_ctr = 0;
    heartbeat_jammed_ctr = 0;
    heartbeat_sample_window = P2M_HEARTBEAT_SAMPLE_WINDOW;
    heartbeat_sample_ctr = 0;
}
//////////////////////////////////////////////////////////////////////////


void Phy2Mac::resetHeartbeatStats()
{
    resetHeartbeatWindowStats();
    
    session_heartbeat_opportunity_ctr = 0;
    session_heartbeat_missing_ctr = 0;
    session_heartbeat_valid_ctr = 0;
    session_heartbeat_errored_ctr = 0;
    session_heartbeat_jammed_ctr = 0;
}
//////////////////////////////////////////////////////////////////////////
void Phy2Mac::close_interface()
{
    ps->close_interface();
}
