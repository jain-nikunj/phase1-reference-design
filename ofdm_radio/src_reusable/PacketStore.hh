/* PacketStore.hh
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef PACKETSTORE_HH_
#define PACKETSTORE_HH_

#include <TxPayload.hh>
#include <RxPayload.hh>
#include <TunTap.hh>
#include <list>
#include <thread>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "timer.h"
#include "Logger.hh"

#define PACKET_NOT_COMPLETE 101
#define PACKET_COMPLETE     102


class PacketStore
{
    public:
        PacketStore(std::string tap_name, unsigned int node_id, unsigned int num_nodes_in_net, 
                    unsigned char* nodes_in_net, unsigned int frame_size, bool using_tun_tap);
        ~PacketStore();
        int add_frame(long int packet_id, unsigned int frame_id, unsigned char* data, unsigned int total_packet_len);
        void readPackets();
        bool data_is_streaming();
        unsigned char* get_frame(long int packet_id, unsigned int frame_id);
        unsigned char* get_next_frame();
        int get_next_frame_destination();
        unsigned char* get_next_frame_for_destination(unsigned int dest_id, long int* packet_id, unsigned int* frame_id, unsigned int* frame_size, unsigned int* total_packet_len);
        int size();
        unsigned int get_written_packets();
        void close_interface();
    private:
        std::list<RxPayload> rx_packets;
        std::list<unsigned int> completed_packets;
        std::list<TxPayload> tx_packets;
        std::thread readThread;
        std::string interface;
        TunTap* tt;
        unsigned int frame_len;
        unsigned int next_packet;
        unsigned int written_packets;
        unsigned int num_nodes_in_net;
        bool data_flowing;
        bool continue_reading;
        bool using_tun_tap;
};



#endif  // PACKETSTORE_HH_
