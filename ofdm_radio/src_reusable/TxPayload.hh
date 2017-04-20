/* TxPayload.hh
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef TXPAYLOAD_HH_
#define TXPAYLOAD_HH_


class TxPayload
{
	public:
		TxPayload(long int id, unsigned int destination_id, unsigned char * payload, unsigned int payload_size, unsigned int frame_size);
        ~TxPayload();
		unsigned char* get_frame(unsigned int frame_id);
		unsigned int get_frames_per_packet();
        unsigned char* get_next_frame(long int* packet_id, unsigned int* frame_id, unsigned int* frame_size, unsigned int* total_packet_len);
        bool allFramesTransmitted();
		long int id;
        unsigned int destination_id;
		unsigned int payload_size;
		unsigned int frame_size;
        unsigned int last_frame_size;
        unsigned int next_frame;
        bool retrieved;
        bool* frame_transmitted;
	private:
		unsigned int frames_per_packet;
		unsigned char *_payload;
};

#endif  // TXPAYLOAD_HH_
