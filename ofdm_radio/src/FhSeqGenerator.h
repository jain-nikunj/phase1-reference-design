/* FhSeqGenerator.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef FREQSEQGENERATOR_H_
#define FREQSEQGENERATOR_H_

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>

enum FhSeqAlgorithm {
    FH_SEQ_NONE,
    FH_SEQ_ONE,
    FH_SEQ_TOGGLE,
    FH_SEQ_RESTART_SWEEP_RF,
    FH_SEQ_RESTART_SWEEP_DSP,
    FH_SEQ_RESTART_SWEEP_RFDSP,
    FH_SEQ_RESTART_SWEEP_DSPRF,
    FH_SEQ_RESTART_ALG_A,
    FH_SEQ_RESTART_ALG_B,
    FH_SEQ_TEST
};

typedef struct {
	unsigned int seq_table_size;
	unsigned int* rf_idx_table;
    unsigned int* dsp_idx_table;
} seq_table_t;

class FhSeqGenerator
{
public:
    FhSeqGenerator(
        FhSeqAlgorithm seq_algorithm, 
        unsigned int num_seq_steps,
        unsigned int rf_table_size,
        unsigned int dsp_table_size,
        unsigned int num_channels,
        bool debug
    );
    ~FhSeqGenerator();
    int makeSeq();  
    int printSeq();
    int resetFhSeqState();
    unsigned int getFhSeqRfIdx(
        unsigned int seq_step
    );
    unsigned int getFhSeqDspIdx(
        unsigned int seq_step
    );
    int printFsgStats();
    int resetFsgStats();
    int scrubSeq();
private:   
    // Copy of constructor parameters
    FhSeqAlgorithm seq_algorithm;
    unsigned int num_seq_steps;
    unsigned int rf_table_size;
    unsigned int dsp_table_size;
    unsigned int num_channels;
    bool debug;

    // Generated sequence and state variables
    seq_table_t seq_table;
    unsigned int rf_seq_state;
    unsigned int dsp_seq_state;
    
    // Internal statistics
    unsigned int fh_seq_generations;
    unsigned int fh_state_resets;
    unsigned int scrub_requests;
    unsigned int scrub_repaired_rf;
    unsigned int scrub_repaired_dsp;
}; 

#endif // FREQSEQGENERATOR_H_
