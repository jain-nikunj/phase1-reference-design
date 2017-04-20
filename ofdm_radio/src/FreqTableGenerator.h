//TODO rework how prohibited frequencies passed as arg

/* FreqTableGenerator.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef FREQTABLEGENERATOR_H_
#define FREQTABLEGENERATOR_H_

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>

typedef struct {
	unsigned int freq_table_size;
	double *freq_rx;
	double *freq_tx;
} freq_table_t;

class FreqTableGenerator
{
public:
    FreqTableGenerator(
        bool node_is_basestation,
        double normal_freq,
        double tx2rx_freq_separation,
        double fh_freq_min,
        double fh_freq_max,
        unsigned int num_fh_prohibited_ranges,
        double * fh_prohibited_range_begin,
        double * fh_prohibited_range_end,
        double fh_window_small,
        double fh_window_medium,
        unsigned int num_channels,
        bool uhd_rx_bug_is_present,
        bool debug
    );
    ~FreqTableGenerator();
    unsigned int getRfTableSize();
    unsigned int getDspTableSize();
    double getRfTableFreqRx(
        unsigned int table_idx
        );
    double getRfTableFreqTx(
        unsigned int table_idx
        );
    double getDspTableFreqRx(
        unsigned int table_idx
        );
    double getDspTableFreqTx(
        unsigned int table_idx
        );
    int printFreqTableGenerator();

private:   
    // Copy/extraction of constructor parameters 
    bool node_is_basestation;
    double normal_freq;
    double tx2rx_freq_separation;
    double fh_freq_min;
    double fh_freq_max;
    
    unsigned int num_fh_prohibited_ranges;
    double *fh_prohibited_range_begin;
    double *fh_prohibited_range_end;
    
    double fh_window_small;
    double fh_window_medium;
    double fh_window_large;
    unsigned int num_channels;
    bool debug;

    // FDD mode frequencies
    double fdd_freq_rx_rf;
    double fdd_freq_rx_dsp;
    double fdd_freq_tx_rf;
    double fdd_freq_tx_dsp;
    
    // FH mode tables of frequencies
    freq_table_t fh_table_rf;
    freq_table_t fh_table_dsp;
    unsigned int num_windows_small;
    unsigned int num_windows_medium;
    
    // Enable UHD bug fix
    bool uhd_rx_bug_is_present;
}; 

#endif // FREQTABLEGENERATOR_H_
