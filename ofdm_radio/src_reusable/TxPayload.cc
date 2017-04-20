/* TxPayload.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

#include <TxPayload.hh>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


TxPayload::TxPayload(long int id, unsigned int destination_id, unsigned char *payload, unsigned int payload_size, unsigned int frame_size)
{
	this->id = id;
    this->destination_id = destination_id;
	this->payload_size = payload_size;
	this->frame_size = frame_size;

    if(this->payload_size < this->frame_size)
        this->frame_size = this->payload_size;
    
    this->next_frame = 0;
    retrieved = false;
	_payload = new unsigned char[payload_size];

	memcpy(_payload, payload, payload_size);
	frames_per_packet = payload_size / frame_size + 1;
    last_frame_size = payload_size % frame_size;
	if(last_frame_size == 0)
    {
		frames_per_packet--;
        last_frame_size = frame_size;
    }

    frame_transmitted = new bool[frames_per_packet];
    for(unsigned int i = 0; i < frames_per_packet; i++)
        frame_transmitted[i] = false;

}


TxPayload::~TxPayload()
{
}


unsigned int TxPayload::get_frames_per_packet()
{
	return frames_per_packet;
}

bool TxPayload::allFramesTransmitted()
{
    for(unsigned int i = 0; i < frames_per_packet; i++)
    {
        if(!frame_transmitted[i])
        {
            return false;
        }
    }

    return true;
}

unsigned char* TxPayload::get_frame(unsigned int frame_id)
{

	if(frame_id >= frames_per_packet)
	{
		std::cout << "Error, frame_id = " << frame_id << " but there are only " << frames_per_packet << " frames available." << std::endl;
		return 0;
	}
	else
	{
        frame_transmitted[frame_id] = true;
		return _payload + (frame_id * frame_size);
	}
}

unsigned char* TxPayload::get_next_frame(long int *packet_id, unsigned int* frame_id, unsigned int* frame_size, unsigned int* total_packet_len)
{
    if(next_frame < frames_per_packet)
    {
        frame_transmitted[next_frame] = true;
        *frame_id = next_frame;
        *frame_size = this->frame_size;
        *packet_id = this->id;
	*total_packet_len = this->payload_size;
        unsigned int return_index = next_frame;
        next_frame++;
        if(allFramesTransmitted())
        {
            retrieved = true;
        }

        if(return_index == frames_per_packet - 1)
            *frame_size = last_frame_size;
        
        return _payload + (return_index * this->frame_size);
    }
    return NULL;
}
