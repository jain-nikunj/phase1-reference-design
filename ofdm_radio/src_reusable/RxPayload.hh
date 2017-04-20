/* RxPayload.hh
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef RXPAYLOAD_HH_
#define RXPAYLOAD_HH_


class RxPayload
{
	public:
		RxPayload(long int id, unsigned int payload_size, unsigned int frame_size);
		~RxPayload();
		bool isComplete();
		bool add_frame(unsigned int frame_id, unsigned char *data);
		void print_payload();
		long int id;
		unsigned int payload_size;
		unsigned char *_payload;
		unsigned int num_frames_remaining();
		bool *frame_received;
	private:
		unsigned int frames_per_packet;
		unsigned int frame_size;
		unsigned int last_frame_size;
};

#endif	//  RXPAYLOAD_HH_
