#ifndef SIM_BP_H
#define SIM_BP_H

// Command line parameters structure
typedef struct bp_params {
    unsigned long int K;     // Number of PC bits used to index the chooser table
    unsigned long int M1;    // Number of PC bits used to index the gshare table
    unsigned long int M2;    // Number of PC bits used to index the bimodal table
    unsigned long int N;     // Number of global history bits used in gshare
    char* bp_name;          // Name of branch predictor to simulate
} bp_params;

// Common base predictor structure
typedef struct predictor_base {
    unsigned int* prediction_table;      // Table of 2-bit saturating counters
    unsigned long int size;             // Size of prediction table
    unsigned long int predictions;      // Total number of predictions made
    unsigned long int mispredictions;   // Total number of mispredictions
} predictor_base;

// Bimodal predictor structure
typedef struct bimodal_predictor {
    predictor_base base;    // Inherit from base predictor
} bimodal_predictor;

// Gshare predictor structure
typedef struct gshare_predictor {
    predictor_base base;    // Inherit from base predictor
    unsigned int history;        // Global branch history register
    unsigned int history_mask;   // Mask for N bits of history
} gshare_predictor;

// Hybrid predictor structure
typedef struct hybrid_predictor {
    predictor_base base;         // Inherit from base predictor
    bimodal_predictor* bp;      // Bimodal predictor component
    gshare_predictor* gp;       // Gshare predictor component
    unsigned int* chooser_table; // Table of 2-bit counters for choosing between predictors
    unsigned long chooser_size;  // Size of chooser table
} hybrid_predictor;

// Constants for the saturating counter
#define MIN_COUNTER 0
#define MAX_COUNTER 3
#define INIT_COUNTER 2  // weakly taken
#define INIT_CHOOSER 1  // Initial value for chooser counters

// Function declarations
// Initialization functions
bimodal_predictor* init_bimodal(unsigned long int M2);
gshare_predictor* init_gshare(unsigned long int M1, unsigned long int N);
hybrid_predictor* init_hybrid(unsigned long int K, unsigned long int M1, 
                            unsigned long int N, unsigned long int M2);

// Cleanup functions
void free_bimodal(bimodal_predictor* bp);
void free_gshare(gshare_predictor* gp);
void free_hybrid(hybrid_predictor* hp);

// Processing functions
void process_bimodal(bimodal_predictor* bp, unsigned long int pc, char outcome);
void process_gshare(gshare_predictor* gp, unsigned long int pc, unsigned int M1, 
                   unsigned int N, char outcome);
void process_hybrid(hybrid_predictor* hp, unsigned long int pc, unsigned int K,
                   unsigned int M1, unsigned int N, char outcome);

// Output functions
void print_stats(predictor_base* base, const char* predictor_name);
void print_hybrid_stats(hybrid_predictor* hp, unsigned int K);

#endif // SIM_BP_H