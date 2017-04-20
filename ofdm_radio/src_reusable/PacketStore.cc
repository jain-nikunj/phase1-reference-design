/* PacketStore.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

#include<PacketStore.hh>
#include<Phy2Mac.h>
PacketStore::PacketStore(std::string tap_name, unsigned int node_id, unsigned int num_nodes_in_net, 
                         unsigned char* nodes_in_net, unsigned int frame_size, bool
                         using_tun_tap)
{
    this->frame_len = frame_size;
    this->next_packet = 0;
    this->data_flowing = false;
    this->continue_reading = true;
    this->using_tun_tap = using_tun_tap;
    this->written_packets = 0;
    this->num_nodes_in_net = num_nodes_in_net;
    if(using_tun_tap)
    {
        tt = new TunTap(tap_name, node_id, num_nodes_in_net, nodes_in_net);
        readThread = std::thread(&PacketStore::readPackets, this);
    }
}

PacketStore::~PacketStore()
{
    if(using_tun_tap)
        delete tt;
}
//Tx Side Functions
unsigned char* PacketStore::get_frame(long int packet_id, unsigned int frame_id)
{
    for(std::list<TxPayload>::iterator it = tx_packets.begin(); it != tx_packets.end(); it++)
    {
        if((*it).id == packet_id)
        {
            return (*it).get_frame(frame_id);
        }
    }
    return NULL;
}

unsigned char* PacketStore::get_next_frame()
{
    return get_frame(next_packet++, 0);
}


int PacketStore::get_next_frame_destination()
{
    for(std::list<TxPayload>::iterator it = tx_packets.begin(); it != tx_packets.end(); it++)
    {
        if((*it).id == next_packet)
        {
            return (*it).destination_id;
        }
    };
    return -1;
}

unsigned char* PacketStore::get_next_frame_for_destination(unsigned int dest_id, long int *packet_id, unsigned int* frame_id, unsigned int*
        frame_size, unsigned int* total_packet_len)
{
    for(std::list<TxPayload>::iterator it = tx_packets.begin(); it != tx_packets.end(); it++)
    {
        if((*it).destination_id == dest_id && !(*it).retrieved)
        {
            unsigned char* result = (*it).get_next_frame(packet_id, frame_id, frame_size, total_packet_len);
            tx_packets.erase(it);
            return result;
            //return (*it).get_next_frame(packet_id, frame_id, frame_size, total_packet_len); 
        }
    }
    total_packet_len = 0;
    return NULL;
}

void PacketStore::readPackets()
{
    unsigned int dest_id = 0;
    unsigned char* data = new unsigned char[P2M_FRAME_PAYLOAD_MAX_SIZE];
    long int i = 0;
    while(continue_reading)
    {
        
        unsigned int total = tt->cread((char*)data, P2M_FRAME_PAYLOAD_MAX_SIZE);
        if(total > 0 && total <= P2M_FRAME_PAYLOAD_MAX_SIZE)
        {
            //We assign the node ID to the last digits of the IP address
            //That byte is always the 33rd byte of the payload
            dest_id = data[33];
            if(dest_id > 0 && dest_id <= num_nodes_in_net)
            {
                data_flowing = true;
                TxPayload payload(i, dest_id, data, total, frame_len);
                tx_packets.push_back(payload);
                i++;
                std::stringstream report;
                report << "storing " << i << " for " << dest_id << ", total: " << total;
            }
        }
        else
        {
            data_flowing = false;
        }
    }
}

bool PacketStore::data_is_streaming()
{
    return data_flowing;
}

int PacketStore::size()
{
    return tx_packets.size();
}

//Rx Side function
int PacketStore::add_frame(long int packet_id, unsigned int frame_id, unsigned char* data, unsigned int total_packet_len)
{
    //Look through list of completed packets so we don't accidentally try to 
	//send a packet to the tun interface more than once
//	std::list<unsigned int>::iterator iter = std::find(completed_packets.begin(), completed_packets.end(), packet_id);
//	if(iter == completed_packets.end())//If packet_id is not in list of competed packets
//	{
		//Look through list of received packets and see 
		//if we've received any frames for packet with id packet_id
		for(std::list<RxPayload>::iterator it = rx_packets.begin(); it != rx_packets.end(); it++)
		{
			if((*it).id == packet_id)
			{
				(*it).add_frame(frame_id, data);
				if((*it).isComplete())
				{
					unsigned int result = tt->cwrite((char*)(*it)._payload, (*it).payload_size);
					if(result == (*it).payload_size)
					{
                        written_packets++;
						completed_packets.push_back(packet_id);
						return PACKET_COMPLETE;
					}
				}
				return PACKET_NOT_COMPLETE;
			}
		}
		//packet with id packet_id must not have been added yet
		RxPayload rxp(packet_id, total_packet_len, frame_len);
		rxp.add_frame(frame_id, data);
		if(rxp.isComplete())
		{
			unsigned int result = tt->cwrite((char*)rxp._payload, rxp.payload_size);
			if(result == rxp.payload_size)
            {
                written_packets++;
                completed_packets.push_back(packet_id);
				return PACKET_COMPLETE;
            }
		}

		rx_packets.push_back(rxp);
		return PACKET_NOT_COMPLETE;
//	}
//	else
//		return PACKET_COMPLETE;
}
unsigned int PacketStore::get_written_packets()
{
    return written_packets;
}

void PacketStore::close_interface()
{
    continue_reading = false;
    readThread.join();
    tt->close_interface();
}

