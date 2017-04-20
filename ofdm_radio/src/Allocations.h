/* Allocations.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef ALLOCATIONS_H_
#define ALLOCATIONS_H_

#include<liquid/liquid.h>
#include<iostream>
#include<cstdlib>

void createOuterPilotAllocation(unsigned char* subcarriers)
{   
    int i;
    for(i = 0; i < 178; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_DATA;
    for(; i < 178 + 52; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_PILOT;
    for(; i < 178 + 52 + 52; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_NULL;
    for(; i < 178 + 52 + 52 + 52; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_PILOT;
    for(; i < 512; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_DATA;

    subcarriers[511] = OFDMFRAME_SCTYPE_NULL;
    subcarriers[0] = OFDMFRAME_SCTYPE_NULL;
    
    //This loop opens a hole of nulls where the inner left pilot cluster is
    //used for testing only
    /*for(int i = 350; i < 410; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_NULL;*/
}

void createInnerPilotAllocation(unsigned char* subcarriers)
{   
    int i;
    for(i = 0; i < 106; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_DATA;
    for(; i < 106 + 52; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_PILOT;
    for(; i < 106 + 52 + 72; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_DATA;
    for(; i < 106 + 52 + 72 + 52; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_NULL;
    for(; i < 106 + 52 + 72 + 52 + 72; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_DATA;
    for(; i < 106 + 52 + 72 + 52 + 72 + 52; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_PILOT;
    for(; i < 512; i++)
        subcarriers[i] = OFDMFRAME_SCTYPE_DATA;
    
    subcarriers[511] = OFDMFRAME_SCTYPE_NULL;
    subcarriers[0] = OFDMFRAME_SCTYPE_NULL;
}

void openNullHole(unsigned char* subcarriers, int start, int end)
{
    start = (start + 256) % 512;
    end = (end + 256) % 512;
    if(start < 0)
    {
        start = 256;
        end = 356;
    }


    if(start > end)
    {
        for(int i = start; i < 512; i++)
        {
            subcarriers[i] = OFDMFRAME_SCTYPE_NULL;
        }
        for(int i = 0; i <= end; i++)
        {
            subcarriers[i] = OFDMFRAME_SCTYPE_NULL;
        }
    }
    else
    {
        for(int i = start; i <= end; i++)
        {
            subcarriers[i] = OFDMFRAME_SCTYPE_NULL;
        }
    }
}

#endif //ALLOCATIONS_H_
