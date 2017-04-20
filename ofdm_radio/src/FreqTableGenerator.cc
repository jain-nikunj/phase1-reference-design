/* FreqTableGenerator.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

#include "FreqTableGenerator.h"

using namespace std;
    
FreqTableGenerator::FreqTableGenerator(
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
    //double fh_window_large,
    unsigned int num_channels,
    bool uhd_rx_bug_is_present,
    bool debug
    )
{
    // Keep copy of constructor parameters to support future reconfiguration
    this->node_is_basestation = node_is_basestation;
    this->normal_freq = normal_freq;
    this->tx2rx_freq_separation = tx2rx_freq_separation;
    this->fh_freq_max = fh_freq_max;
    this->fh_freq_min = fh_freq_min;
    this->num_fh_prohibited_ranges = num_fh_prohibited_ranges;
    this->fh_prohibited_range_begin = fh_prohibited_range_begin;
    this->fh_prohibited_range_end = fh_prohibited_range_end;
    this->fh_window_small = fh_window_small;
    this->fh_window_medium = fh_window_medium;
    //this->fh_window_large = fh_window_large;
    this->num_channels = num_channels;
    this->uhd_rx_bug_is_present = uhd_rx_bug_is_present;
    this->debug = debug;
    
    // Placeholder for future multichannel development
    if (num_channels > 1) {
        cerr << "ERROR: FreqTableGenerator multichannel support under development." << endl;
        exit(EXIT_FAILURE);
    }
    
    // Frequency division duplex (FDD) frequency settings
    if (node_is_basestation) {
        fdd_freq_rx_rf = normal_freq + tx2rx_freq_separation;
        fdd_freq_tx_rf = normal_freq;
    } else {
        fdd_freq_rx_rf = normal_freq;
        fdd_freq_tx_rf = normal_freq + tx2rx_freq_separation;
    }
    fdd_freq_rx_dsp = 0.0;
    fdd_freq_tx_dsp = 0.0;

    // RF table subject to tx-rx separation and prohibited frequencies
    // The frequency hopping (FH) table generation follows a two stage
    // process that checks both transmit and receive frequency windows
    // against a list of prohibited ranges
    fh_window_large = fh_freq_max - fh_freq_min;
    num_windows_medium = floor(fh_window_large / fh_window_medium);

    // Stage #1: Make starter tables without checking for restrictions
    unsigned int tmp_table_size = num_windows_medium;
    double *tmp_table = new double[tmp_table_size]; 
    bool *win_allowed = new bool[tmp_table_size];
    double win_bw = fh_window_medium;
    double half_win_bw = fh_window_medium / 2.0;
    unsigned int idx;
    for (idx = 0; idx < tmp_table_size; idx++) {
        tmp_table[idx] = fh_freq_min + (idx+1)*win_bw -half_win_bw;
    }

    // Stage #2: Only populate working table with permitted windows
    unsigned int ctr;
    double p_begin, p_end;
    double win_begin_rx, win_end_rx, win_begin_tx, win_end_tx;
    unsigned int num_win_allowed = 0;

    if (num_fh_prohibited_ranges > 0) {
        
        for (idx = 0; idx < tmp_table_size; idx++) {
            win_allowed[idx] = true;
        }
        
        for (idx = 0; idx < tmp_table_size; idx++) {
            win_begin_tx = tmp_table[idx] - half_win_bw;
            win_begin_rx = win_begin_tx + tx2rx_freq_separation;
            win_end_tx = tmp_table[idx] + half_win_bw;
            win_end_rx = win_end_tx + tx2rx_freq_separation;
            
            for (ctr = 0; ctr < num_fh_prohibited_ranges; ctr++) {
                p_begin = fh_prohibited_range_begin[ctr];
                p_end = fh_prohibited_range_end[ctr];
                
                if ( ((win_begin_tx >= p_end) && (win_begin_rx >= p_end)) ||
                     ((win_end_tx <= p_begin) && (win_end_rx <= p_begin))   ) {
                    // then continue
                } else {
                    win_allowed[idx] = false;
                }
            }
            
            if (win_allowed[idx]) {
                num_win_allowed++;
            }
        }
        
        if (num_win_allowed <= 1) {
            cerr << "ERROR: FreqTableGenerator cannot generate a viable set ";
            cerr << "of hopping frequencies that make effective use of this radio" << endl;
            cerr << "       while complying with the prohibited frequency ranges."<<endl;
            cerr << "       Blocked frequency range (begin, end) list:"<<endl;
            for (ctr = 0; ctr < num_fh_prohibited_ranges; ctr++) {
                cerr << "    ("<< scientific << fh_prohibited_range_begin[ctr];
                cerr << ", "<< scientific << fh_prohibited_range_end[ctr] << ")"<< endl;
            }
            cerr << " "<<endl;
            exit(EXIT_FAILURE);
        }
        
        fh_table_rf.freq_rx = new double[num_win_allowed];
        fh_table_rf.freq_tx = new double[num_win_allowed];
        ctr = 0;
        for (idx = 0; idx < tmp_table_size; idx++) {
            if (win_allowed[idx]) {
                if (node_is_basestation) {
                    fh_table_rf.freq_rx[ctr] = tmp_table[idx] + tx2rx_freq_separation;
                    fh_table_rf.freq_tx[ctr] = tmp_table[idx];
                } else {
                    fh_table_rf.freq_rx[ctr] = tmp_table[idx];
                    fh_table_rf.freq_tx[ctr] = tmp_table[idx] + tx2rx_freq_separation;
                }
                ctr++;
            }
        }
        fh_table_rf.freq_table_size = ctr;

    } else {  // Else: no prohibited frequencies --> use whole range 

        fh_table_rf.freq_table_size = tmp_table_size;
        fh_table_rf.freq_rx = new double[tmp_table_size];
        fh_table_rf.freq_tx = new double[tmp_table_size];
        
        for (idx = 0; idx < tmp_table_size; idx++) {
            if (node_is_basestation) {
                fh_table_rf.freq_rx[idx] = tmp_table[idx] + tx2rx_freq_separation;
                fh_table_rf.freq_tx[idx] = tmp_table[idx];
            } else {
                fh_table_rf.freq_rx[idx] = tmp_table[idx];
                fh_table_rf.freq_tx[idx] = tmp_table[idx] + tx2rx_freq_separation;
            }
        }
    }

    // This table is for the "DSP" stage tuning, and assumes the
    // tx-rx separation was done entirely at the "RF" stage 

    if (uhd_rx_bug_is_present) {
        // This is an anti-symmetric (RX inverted) DSP table generation to
        // fix a bug in the UHD manual tuning mode

        num_windows_small = floor(fh_window_medium / fh_window_small);
        unsigned int num_offsets = floor(num_windows_small/2);
        if ( (float)num_windows_small/2.0 == num_offsets ) {  
            num_windows_small--;
            num_offsets--;
        }
        fh_table_dsp.freq_table_size = num_windows_small;
        fh_table_dsp.freq_rx = new double[fh_table_dsp.freq_table_size];
        fh_table_dsp.freq_tx = new double[fh_table_dsp.freq_table_size];
        
        fh_table_dsp.freq_rx[0] = 0;
        fh_table_dsp.freq_tx[0] = 0;
        if (num_offsets < 1) {
            cerr << "WARNING in FreqTableGenerator -- Unable to generate ";
            cerr << "a useful DSP stage tuning table" << endl;
        } else {
            for (idx = 1; idx < (num_offsets +1); idx++) {
                fh_table_dsp.freq_rx[idx] =  -1.0 *idx * fh_window_small;
                
                fh_table_dsp.freq_tx[idx] = idx * fh_window_small;
                
                fh_table_dsp.freq_rx[idx + num_offsets] = 
                        (double)idx * fh_window_small;
                        
                fh_table_dsp.freq_tx[idx + num_offsets] = 
                        -1.0 * (double)idx * fh_window_small;
            }
        }

    } else {

        // This is the symmetric RX-TX approach to DSP table generation that
        // *should* have been implemented by UHD's manual tuning mode

        num_windows_small = floor(fh_window_medium / fh_window_small);
        unsigned int num_offsets = floor(num_windows_small/2);
        if ( (float)num_windows_small/2.0 == num_offsets ) {  
            num_windows_small--;
            num_offsets--;
        }
        fh_table_dsp.freq_table_size = num_windows_small;
        fh_table_dsp.freq_rx = new double[fh_table_dsp.freq_table_size];
        fh_table_dsp.freq_tx = new double[fh_table_dsp.freq_table_size];
        
        fh_table_dsp.freq_rx[0] = 0;
        fh_table_dsp.freq_tx[0] = 0;
        if (num_offsets < 1) {
            cerr << "WARNING in FreqTableGenerator -- Unable to generate ";
            cerr << "a useful DSP stage tuning table" << endl;
        } else {
            for (idx = 1; idx < (num_offsets +1); idx++) {
                fh_table_dsp.freq_rx[idx] = idx * fh_window_small;
                fh_table_dsp.freq_tx[idx] = idx * fh_window_small;
                fh_table_dsp.freq_rx[idx + num_offsets] = 
                        -1.0 * (double)idx * fh_window_small;
                fh_table_dsp.freq_tx[idx + num_offsets] = 
                        -1.0 * (double)idx * fh_window_small;
            }
        }

    }

    // Delete temporary objects used in FH table generation
    delete[] tmp_table;
    delete[] win_allowed;
}
//////////////////////////////////////////////////////////////////////////


FreqTableGenerator::~FreqTableGenerator()
{
    try {delete[] fh_table_rf.freq_rx;} catch (...) {}    
    try {delete[] fh_table_rf.freq_tx;} catch (...) {}     
    try {delete[] fh_table_dsp.freq_rx;} catch (...) {}
    try {delete[] fh_table_dsp.freq_tx;} catch (...) {}
}
//////////////////////////////////////////////////////////////////////////

        
unsigned int FreqTableGenerator::getRfTableSize()
{
    return(fh_table_rf.freq_table_size);
}
//////////////////////////////////////////////////////////////////////////


unsigned int FreqTableGenerator::getDspTableSize()
{
    return(fh_table_dsp.freq_table_size);
}
//////////////////////////////////////////////////////////////////////////


double FreqTableGenerator::getRfTableFreqRx(
    unsigned int table_idx
    )
{
   if (table_idx >=  fh_table_rf.freq_table_size) {
        cerr<<"ERROR: FreqTableGenerator::getRfTableFreqRx table_idx too large"<<endl;
        cerr<<"       requested table_idx = "<< dec <<table_idx << " but ";
        cerr<<"fh_table_rf.freq_table_size = " << dec << fh_table_rf.freq_table_size << endl;
        exit(EXIT_FAILURE);
   }
   
   return( fh_table_rf.freq_rx[table_idx] );
}
/////////////////////////////////////////////////////////////////////////


double FreqTableGenerator::getRfTableFreqTx(
    unsigned int table_idx
    )
{
   if (table_idx >=  fh_table_rf.freq_table_size) {
        cerr<<"ERROR: FreqTableGenerator::getRfTableFreqTx table_idx too large"<<endl;
        exit(EXIT_FAILURE);
   }
   
   return( fh_table_rf.freq_tx[table_idx] );
}
/////////////////////////////////////////////////////////////////////////


double FreqTableGenerator::getDspTableFreqRx(
    unsigned int table_idx
    )
{
   if (table_idx >=  fh_table_dsp.freq_table_size) {
        cerr<<"ERROR: FreqTableGenerator::getDspTableFreqRx table_idx too large"<<endl;
        exit(EXIT_FAILURE);
   }
   
   return( fh_table_dsp.freq_rx[table_idx] );
}
/////////////////////////////////////////////////////////////////////////


double FreqTableGenerator::getDspTableFreqTx(
    unsigned int table_idx
    )
{
   if (table_idx >=  fh_table_dsp.freq_table_size) {
        cerr<<"ERROR: FreqTableGenerator::getDspTableFreqTx table_idx too large"<<endl;
        exit(EXIT_FAILURE);
   }
   
   return( fh_table_dsp.freq_tx[table_idx] );
}
/////////////////////////////////////////////////////////////////////////

        
int FreqTableGenerator::printFreqTableGenerator()
{
    unsigned int ctr; 
    
    cout<<"\nSummary of FreqTableGenerator" <<endl;
    cout << "-----------------------------" <<endl;
    cout << "FDD mode frequencies:" <<endl;
    cout << "fdd_freq_rx_rf: " << scientific << fdd_freq_rx_rf << "  ";
    cout << "fdd_freq_rx_dsp: " << scientific << fdd_freq_rx_dsp << endl;
    cout << "fdd_freq_tx_rf: " << scientific << fdd_freq_tx_rf << "  ";
    cout << "fdd_freq_tx_dsp: " << scientific << fdd_freq_tx_dsp << endl;
    cout << " " <<endl;
    cout << "FH mode frequency tables:" <<endl;
    cout << "    RF stage table size = "<< dec << fh_table_rf.freq_table_size << endl;
    cout << "    Index   Rx            Tx          "<< endl;
    cout << "----------------------------------"<< endl;
    for (ctr = 0; ctr < fh_table_rf.freq_table_size; ctr++) {
        cout<< "    " << right << setw(5) << ctr << "   ";
        cout<< left << setw(10) << scientific << fh_table_rf.freq_rx[ctr] <<"  ";
        cout<< left << setw(10) << scientific << fh_table_rf.freq_tx[ctr] <<endl;
    }
    cout << " " <<endl;
    cout << "    DSP stage table size = "<< dec << fh_table_dsp.freq_table_size << endl;
    cout << "    Index   Rx            Tx          "<< endl;
    cout << "----------------------------------"<< endl;
    for (ctr = 0; ctr < fh_table_dsp.freq_table_size; ctr++) {
        cout<< "    " << right << setw(5) << ctr << "   ";
        cout<< left << setw(10) << scientific << fh_table_dsp.freq_rx[ctr] <<"  ";
        cout<< left << setw(10) << scientific << fh_table_dsp.freq_tx[ctr] <<endl;
    }
    cout << " " <<endl;
   
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
