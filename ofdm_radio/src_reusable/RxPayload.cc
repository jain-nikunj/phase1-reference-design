/* RxPayload.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

#include <RxPayload.hh>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

RxPayload::RxPayload(long int id, unsigned int payload_size, unsigned int frame_size)
{
	this->id = id;
	this->frame_size = frame_size;
	this->payload_size = payload_size;
	frames_per_packet = payload_size / frame_size + 1;
	last_frame_size = payload_size % frame_size;
	if(last_frame_size == 0)
	{
		frames_per_packet--;
		last_frame_size = frame_size;
	}
	_payload = new unsigned char[payload_size]; 
	frame_received = new bool[frames_per_packet];
	for(unsigned int i = 0; i < frames_per_packet; i++)
		frame_received[i] = false;
}

RxPayload::~RxPayload()
{
}

bool RxPayload::isComplete()
{
	for(unsigned int i = 0; i < frames_per_packet; i++)
		if(!frame_received[i])
			return false;

	return true;
}

bool RxPayload::add_frame(unsigned int frame_id, unsigned char *data)
{
	if(frame_id >= frames_per_packet)
	{
		std::cout << "Invalid frame id: " << frame_id << std::endl;
		return false;
	}
	if(frame_received[frame_id])
	{
		std::cout << "Data for packet " << id << ", frame " << frame_id << " already added." << std::endl;
		return false;
	}
	//If we're copying the last frame, it might be smaller than the rest
	//The constructor sets last_frame_size appropriately
	if(frame_id == frames_per_packet - 1)
	{
		memcpy(_payload + (frame_id * frame_size), data, last_frame_size);
	}
	else
	{
		memcpy(_payload + (frame_id * frame_size), data, frame_size);
	}
	frame_received[frame_id] = true;
	return true;
}

void RxPayload::print_payload()
{
	for(unsigned int i= 0; i < payload_size; i++)
		std::cout << _payload[i] << " ";
	std::cout << std::endl;
}

unsigned int RxPayload::num_frames_remaining()
{
	unsigned int result = 0;
	for(unsigned int i = 0; i < frames_per_packet; i++)
	{
		if(!frame_received[i])
			result++;
	}

	return result;
}
