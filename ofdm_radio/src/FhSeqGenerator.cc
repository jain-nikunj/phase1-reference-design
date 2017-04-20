/* FhSeqGenerator.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

#include "FhSeqGenerator.h"

using namespace std;
    
FhSeqGenerator::FhSeqGenerator(
    FhSeqAlgorithm seq_algorithm, 
    unsigned int num_seq_steps,
    unsigned int rf_table_size,
    unsigned int dsp_table_size,
    unsigned int num_channels,
    bool debug
    )
{
    this->seq_algorithm = seq_algorithm;
    this->num_seq_steps = num_seq_steps;
    this->rf_table_size = rf_table_size;
    this->dsp_table_size = dsp_table_size;
    this->num_channels = num_channels;
    this->debug = debug;
    
    // Placeholder for future multichannel development
    if (num_channels > 1) {
        cerr << "ERROR: FreqTableGenerator multichannel support under development." << endl;
        exit(EXIT_FAILURE);
    }

    seq_table.seq_table_size = num_seq_steps;
    seq_table.rf_idx_table = new unsigned int[num_seq_steps];
    seq_table.dsp_idx_table = new unsigned int[num_seq_steps];
    for (unsigned int ctr = 0; ctr < num_seq_steps; ctr++) {
        seq_table.rf_idx_table[ctr] = 0;
        seq_table.dsp_idx_table[ctr] = 0;
    }
    
    resetFhSeqState();
    resetFsgStats();
}
//////////////////////////////////////////////////////////////////////////

    
FhSeqGenerator::~FhSeqGenerator()
{
    try {delete[] seq_table.rf_idx_table;} catch(...) { }
    try {delete[] seq_table.dsp_idx_table;} catch (...) { }
}
//////////////////////////////////////////////////////////////////////////


int FhSeqGenerator::makeSeq()
{
    unsigned int seq_ctr, idx_ctr;
    unsigned int* rf_seq_state2idx;
    unsigned int* dsp_seq_state2idx;
    unsigned int stride, stride_ctr, offset_ctr, pick_idx;
       
    switch (seq_algorithm) {
        
        case FH_SEQ_NONE :
            for (seq_ctr = 0; seq_ctr < num_seq_steps; seq_ctr++) {
                seq_table.rf_idx_table[seq_ctr] = 0;
                seq_table.dsp_idx_table[seq_ctr] = 0;  
            }
            rf_seq_state = 0;
            dsp_seq_state = 0;
            break;
            
        case FH_SEQ_ONE :
            for (seq_ctr = 0; seq_ctr < num_seq_steps; seq_ctr++) {
                seq_table.rf_idx_table[seq_ctr] = 0;
                seq_table.dsp_idx_table[seq_ctr] = 1;  
            }
            rf_seq_state = 0;
            dsp_seq_state = 0;
            break;
            
        case FH_SEQ_TOGGLE :
            seq_table.rf_idx_table[0] = 0;
            seq_table.dsp_idx_table[0] = 1;  
            for (seq_ctr = 1; seq_ctr < num_seq_steps; seq_ctr++) {
                seq_table.rf_idx_table[seq_ctr] = 0;
                if (seq_table.dsp_idx_table[seq_ctr-1] == 1) {
                    seq_table.dsp_idx_table[seq_ctr] = 2;  
                } else {
                    seq_table.dsp_idx_table[seq_ctr] = 1;
                }
            }
            rf_seq_state = 0;
            dsp_seq_state = 0;
            break;
            
        case FH_SEQ_RESTART_SWEEP_RF :
            // Here the state variables and the sequence indicies are the same
            rf_seq_state = 0;
            dsp_seq_state = 0;
            for (seq_ctr = 0; seq_ctr < num_seq_steps; seq_ctr++) {
                seq_table.rf_idx_table[seq_ctr] = rf_seq_state;
                rf_seq_state++;
                if (rf_seq_state >= rf_table_size) {
                    rf_seq_state = 0;
                }
                seq_table.dsp_idx_table[seq_ctr] = 0;
            }
            break;
            
        case FH_SEQ_RESTART_SWEEP_DSP :
            // Here the state variables and the sequence indicies are the same
            dsp_seq_state = 0;
            rf_seq_state = 0;
            for (seq_ctr = 0; seq_ctr < num_seq_steps; seq_ctr++) {
                seq_table.dsp_idx_table[seq_ctr] = dsp_seq_state;
                dsp_seq_state++;
                if (dsp_seq_state >= dsp_table_size) {
                    dsp_seq_state = 0;
                }
                seq_table.rf_idx_table[seq_ctr] = 0;
            }
            break;
            
        case FH_SEQ_RESTART_SWEEP_RFDSP :
            // Here the state variables and the sequence indicies are the same
            rf_seq_state = 0;
            dsp_seq_state = 0;
            seq_ctr = 0;
            while (seq_ctr < num_seq_steps) {
            
                for (rf_seq_state = 0; rf_seq_state < rf_table_size; rf_seq_state++) {
                
                    for (dsp_seq_state = 0; dsp_seq_state < dsp_table_size; dsp_seq_state++) {
                        seq_table.rf_idx_table[seq_ctr] = rf_seq_state;
                        seq_table.dsp_idx_table[seq_ctr] = dsp_seq_state;
                        seq_ctr++;
                        if (seq_ctr >= num_seq_steps) {
                            break;
                        }
                    }
                    
                    if (seq_ctr >= num_seq_steps) {
                        break;
                    }
                }
            }  
            break;
            
        case FH_SEQ_RESTART_SWEEP_DSPRF :
            // Here the state variables and the sequence indicies are the same
            rf_seq_state = 0;
            dsp_seq_state = 0;
            seq_ctr = 0;
            while (seq_ctr < num_seq_steps) {
            
                for (dsp_seq_state = 0; dsp_seq_state < dsp_table_size; dsp_seq_state++) {
                
                    for (rf_seq_state = 0; rf_seq_state < rf_table_size; rf_seq_state++) {
                        seq_table.rf_idx_table[seq_ctr] = rf_seq_state;
                        seq_table.dsp_idx_table[seq_ctr] = dsp_seq_state;
                        seq_ctr++;
                        if (seq_ctr >= num_seq_steps) {
                            break;
                        }
                    }
                    
                    if (seq_ctr >= num_seq_steps) {
                        break;
                    }
                }
            }  
            break;
            
        case FH_SEQ_RESTART_ALG_A : 
        case FH_SEQ_RESTART_ALG_B :
            // Both algorithm A and B start by generating pemutation arrays 
            // for the RF and DSP frequency table indicies.  Algorithm A
            // generates the output sequence from looping over the RF
            // permutation array (outer loop) and the DSP array (inner);
            // algorithm B switches to DSP array as outer loop and RF array
            // as inner loop.
            rf_seq_state = 0;
            dsp_seq_state = 0;
            
            // Generate indicies for RF frequency table sequence
            rf_seq_state2idx = new unsigned int[rf_table_size];
            if (rf_table_size < 4) {
                // Special handling for very small tables
                for (idx_ctr=0; idx_ctr <rf_table_size; idx_ctr++) {
                    rf_seq_state2idx[idx_ctr] = rf_table_size -idx_ctr -1;
                }
            } else {
                // Permute indicies in the style of a row-column interleaver
                stride = (unsigned int)ceil( sqrt(rf_table_size) );
                idx_ctr = 0;
                for (offset_ctr = 0; offset_ctr < stride; offset_ctr++) {
                 
                    for (stride_ctr = 0; stride_ctr < stride; stride_ctr++) {
                        
                        pick_idx = (stride_ctr * stride) +offset_ctr;
                        if (pick_idx < rf_table_size) {
                             rf_seq_state2idx[idx_ctr] = pick_idx;
                             idx_ctr++;
                        } else {
                            // skip
                        }
                    }
                }
            }
            
            // Generate indicies for DSP frequency table sequence
            dsp_seq_state2idx = new unsigned int[dsp_table_size];
            if (dsp_table_size < 4) {
                // Special handling for very small tables
                for (idx_ctr=0; idx_ctr <dsp_table_size; idx_ctr++) {
                    dsp_seq_state2idx[idx_ctr] = dsp_table_size -idx_ctr -1;
                }
            } else {
                // Permute indicies in the style of a row-column interleaver
                stride = (unsigned int)ceil( sqrt(dsp_table_size) );
                idx_ctr = 0;
                for (offset_ctr = 0; offset_ctr < stride; offset_ctr++) {
                 
                    for (stride_ctr = 0; stride_ctr < stride; stride_ctr++) {
                        
                        pick_idx = (stride_ctr * stride) +offset_ctr;
                        if (pick_idx < dsp_table_size) {
                             dsp_seq_state2idx[idx_ctr] = pick_idx;
                             idx_ctr++;
                        } else {
                            // skip
                        }
                    }
                }
            }
            
            // Generate final sequence from RF and DSP table indicies
            seq_ctr = 0;
            if (seq_algorithm == FH_SEQ_RESTART_ALG_A) {
            
                while (seq_ctr < num_seq_steps) {
                    for (rf_seq_state = 0; rf_seq_state < rf_table_size; rf_seq_state++) {
                        for (dsp_seq_state = 0; dsp_seq_state < dsp_table_size; dsp_seq_state++) {
                            seq_table.rf_idx_table[seq_ctr] = rf_seq_state2idx[rf_seq_state];
                            seq_table.dsp_idx_table[seq_ctr] = dsp_seq_state2idx[dsp_seq_state];
                            seq_ctr++;
                            if (seq_ctr >= num_seq_steps) {
                                break;
                            }
                        }
                        
                        if (seq_ctr >= num_seq_steps) {
                            break;
                        }
                    }
                }  
            
            } else {    // Else seq_algorithm == FH_SEQ_RESTART_ALG_B
                
                while (seq_ctr < num_seq_steps) {
                    for (dsp_seq_state = 0; dsp_seq_state < dsp_table_size; dsp_seq_state++) {
                        for (rf_seq_state = 0; rf_seq_state < rf_table_size; rf_seq_state++) {
                            seq_table.rf_idx_table[seq_ctr] = rf_seq_state2idx[rf_seq_state];
                            seq_table.dsp_idx_table[seq_ctr] = dsp_seq_state2idx[dsp_seq_state];
                            seq_ctr++;
                            if (seq_ctr >= num_seq_steps) {
                                break;
                            }
                        }
                        
                        if (seq_ctr >= num_seq_steps) {
                            break;
                        }
                    }
                }  
                
            }   

            delete[] rf_seq_state2idx;
            delete[] dsp_seq_state2idx;
            break;
            
        case FH_SEQ_TEST :
            cerr <<"PLACEHOLDER in makeFhSeq for test code"<<endl;
            break;
            
        default :
            cerr <<"ERROR: In makeFhSeq unsupported seq_algorithm requested."<<endl;
            exit(EXIT_FAILURE);
            break;
    }

    fh_seq_generations++;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int FhSeqGenerator::printSeq() 
{
    unsigned int seq_ctr; 
    
    cout<<"\nCurrent Sequence of Table Indicies for FH Mode"<<endl;
    cout<<  "Seq Step  RF Table Index  DSP Table Index"<<endl; 
    cout<<  "----------------------------------------------"<<endl;
    for (seq_ctr = 0; seq_ctr < num_seq_steps; seq_ctr++) {
        cout << right << setw(8) << dec << seq_ctr << "  ";
        cout << right << setw(14) << dec << seq_table.rf_idx_table[seq_ctr] << "   ";
        cout << right << setw(14) << dec << seq_table.dsp_idx_table[seq_ctr]<< endl;
    }
    cout<<" "<<endl;
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int FhSeqGenerator::resetFhSeqState()
{
    rf_seq_state = 0;
    dsp_seq_state = 0;
    
    fh_state_resets++;
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


unsigned int FhSeqGenerator::getFhSeqRfIdx(
    unsigned int seq_step
    )
{
    if (seq_step >= num_seq_steps) {
        cerr << "ERROR: FhSeqGenerator::getFhSeqRfIdx seq_step > num_seq_steps"<<endl;
        exit(EXIT_FAILURE);
    }
    
    return( seq_table.rf_idx_table[seq_step] );
}
//////////////////////////////////////////////////////////////////////////


unsigned int FhSeqGenerator::getFhSeqDspIdx(
    unsigned int seq_step
    )
{
    if (seq_step >= num_seq_steps) {
        cerr << "ERROR: FhSeqGenerator::getFhSeqDspIdx seq_step > num_seq_steps"<<endl;
        exit(EXIT_FAILURE);
    }
    
    return( seq_table.dsp_idx_table[seq_step] );
}
//////////////////////////////////////////////////////////////////////////


int FhSeqGenerator::printFsgStats() 
{
    cout << "\nFhSeqGenerator internal statistics:" << endl;
    cout <<   "-----------------------------------" << endl;
    cout << "fh_seq_generations:    " <<dec << fh_seq_generations <<endl;
    cout << "fh_state_resets:       " <<dec << fh_state_resets <<endl;
    cout << "scrub_requests:        " <<dec << scrub_requests <<endl;
    cout << "scrub_repaired_rf:     " <<dec << scrub_repaired_rf <<endl;
    cout << "scrub_repaired_dsp:    " <<dec << scrub_repaired_dsp <<endl;
    cout << " " << endl;

    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int FhSeqGenerator::resetFsgStats() 
{
    fh_seq_generations = 0;
    fh_state_resets = 0;
    scrub_requests = 0;
    scrub_repaired_rf = 0;
    scrub_repaired_dsp = 0;
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

int FhSeqGenerator::scrubSeq() 
{
    scrub_requests++;
    
    for (unsigned int idx = 0; idx < num_seq_steps; idx++) {
        if (seq_table.rf_idx_table[idx] >=  rf_table_size) {
            seq_table.rf_idx_table[idx] = 0;
            scrub_repaired_rf++;
        } 
        if (seq_table.dsp_idx_table[idx] >=  dsp_table_size) {
            seq_table.dsp_idx_table[idx] = 0;
            scrub_repaired_dsp++;
        } 
    }
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
