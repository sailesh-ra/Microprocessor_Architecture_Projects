#include <stdio.h>
#include <stdlib.h>
#include "sim_bp.h"

static void init_predictor_base(predictor_base* base, unsigned long int size) {
    base->size = size;
    base->prediction_table = (unsigned int*)malloc(size * sizeof(unsigned int));
    if (!base->prediction_table) {
        fprintf(stderr, "Error: Failed to allocate prediction table\n");
        exit(EXIT_FAILURE);
    }
    
    for(unsigned long int i = 0; i < size; i++) {
        base->prediction_table[i] = INIT_COUNTER;
    }
    
    base->predictions = 0;
    base->mispredictions = 0;
}

bimodal_predictor* init_bimodal(unsigned long int M2) {
    bimodal_predictor* bp = (bimodal_predictor*)malloc(sizeof(bimodal_predictor));
    if (!bp) {
        fprintf(stderr, "Error: Failed to allocate bimodal predictor\n");
        exit(EXIT_FAILURE);
    }
    
    init_predictor_base(&bp->base, 1UL << M2);
    return bp;
}

gshare_predictor* init_gshare(unsigned long int M1, unsigned long int N) {
    if(N > M1) {
        fprintf(stderr, "Error: N cannot be larger than M1\n");
        exit(EXIT_FAILURE);
    }
    
    gshare_predictor* gp = (gshare_predictor*)malloc(sizeof(gshare_predictor));
    if (!gp) {
        fprintf(stderr, "Error: Failed to allocate gshare predictor\n");
        exit(EXIT_FAILURE);
    }
    
    init_predictor_base(&gp->base, 1UL << M1);
    gp->history = 0;
    gp->history_mask = (1U << N) - 1;
    return gp;
}

hybrid_predictor* init_hybrid(unsigned long int K, unsigned long int M1, 
                            unsigned long int N, unsigned long int M2) {
    hybrid_predictor* hp = (hybrid_predictor*)malloc(sizeof(hybrid_predictor));
    if (!hp) {
        fprintf(stderr, "Error: Failed to allocate hybrid predictor\n");
        exit(EXIT_FAILURE);
    }
    
    hp->chooser_table = (unsigned int*)malloc((1UL << K) * sizeof(unsigned int));
    if (!hp->chooser_table) {
        fprintf(stderr, "Error: Failed to allocate chooser table\n");
        free(hp);
        exit(EXIT_FAILURE);
    }
    
    // Initialize all chooser counters to 1
    for(unsigned long int i = 0; i < (1UL << K); i++) {
        hp->chooser_table[i] = 1;
    }
    
    hp->bp = init_bimodal(M2);
    hp->gp = init_gshare(M1, N);
    
    return hp;
}

static void free_predictor_base(predictor_base* base) {
    if(base && base->prediction_table) {
        free(base->prediction_table);
        base->prediction_table = NULL;
    }
}

void free_bimodal(bimodal_predictor* bp) {
    if(bp) {
        free_predictor_base(&bp->base);
        free(bp);
    }
}

void free_gshare(gshare_predictor* gp) {
    if(gp) {
        free_predictor_base(&gp->base);
        free(gp);
    }
}

void free_hybrid(hybrid_predictor* hp) {
    if (hp) {
        if (hp->bp) free_bimodal(hp->bp);
        if (hp->gp) free_gshare(hp->gp);
        if (hp->chooser_table) {
            free(hp->chooser_table);
        }
        free(hp);
    }
}

static inline void update_counter(unsigned int* counter, int taken) {
    if (taken) {
        if (*counter < MAX_COUNTER) {
            (*counter)++;
        }
    } else {
        if (*counter > MIN_COUNTER) {
            (*counter)--;
        }
    }
}

static inline unsigned int get_gshare_index(unsigned long int pc, 
                                          unsigned int history,
                                          unsigned int M1,
                                          unsigned int N) {
    // Step 1: Get PC bits after right shift by 2 and masking
    unsigned int temp = (pc >> 2) & ((1U << M1) - 1);
    
    if (N > 0) {
        // Step 2: Extract upper N bits from the temp (PC bits)
        unsigned int upper_pc = (temp >> (M1 - N)) & ((1U << N) - 1);
        
        // Step 3: XOR history with upper PC bits
        unsigned int xored = history ^ upper_pc;
        
        // Step 4: Place XOR result in upper N bits of the index
        unsigned int index = temp;
        // Clear upper N bits of index
        index = index & ((1U << (M1 - N)) - 1);  // Keep lower (M1 - N) bits
        // Place XOR result in upper N bits
        index = index | (xored << (M1 - N));  // Combine
        
        return index;
    }
    
    return temp;
}

static inline int get_prediction(unsigned int counter) {
    return counter >= 2;
}

void process_bimodal(bimodal_predictor* bp, unsigned long int pc, char outcome) {
    unsigned long int index = (pc >> 2) & (bp->base.size - 1);
    int prediction = get_prediction(bp->base.prediction_table[index]);
    
    bp->base.predictions++;
    if((outcome == 't' && !prediction) || (outcome == 'n' && prediction)) {
        bp->base.mispredictions++;
    }
    
    update_counter(&bp->base.prediction_table[index], outcome == 't');
}

void process_gshare(gshare_predictor* gp, unsigned long int pc, unsigned int M1, unsigned int N, char outcome) {
    static unsigned long int debug_count = 0;
    const unsigned long int DEBUG_LIMIT = 50;
    
    unsigned int index = get_gshare_index(pc, gp->history, M1, N);
    int prediction = get_prediction(gp->base.prediction_table[index]);
    
    if (debug_count < DEBUG_LIMIT) {
        printf("=%lu\t%lx\t%c\n", debug_count, pc, outcome);
        printf("\tGP: %u\t%u\n", index, gp->base.prediction_table[index]);
    }
    
    gp->base.predictions++;
    if((outcome == 't' && !prediction) || (outcome == 'n' && prediction)) {
        gp->base.mispredictions++;
    }
    
    update_counter(&gp->base.prediction_table[index], outcome == 't');
    
    
    gp->history = ((gp->history >> 1) | ((outcome == 't' ? 1U : 0U) << (N-1))) & gp->history_mask;
}

void process_hybrid(hybrid_predictor* hp, unsigned long int pc, unsigned int K, 
                   unsigned int M1, unsigned int N, char outcome) {
    // Step 1: Get predictions from both predictors
    unsigned int gshare_index = get_gshare_index(pc, hp->gp->history, M1, N);
    unsigned int bimodal_index = (pc >> 2) & (hp->bp->base.size - 1);
    
    int gshare_pred = get_prediction(hp->gp->base.prediction_table[gshare_index]);
    int bimodal_pred = get_prediction(hp->bp->base.prediction_table[bimodal_index]);
    
    // Step 2: Get chooser table index
    unsigned int chooser_index = (pc >> 2) & ((1UL << K) - 1);
    
    // Step 3: Make overall prediction based on chooser
    int use_gshare = hp->chooser_table[chooser_index] >= 2;
    int final_pred = use_gshare ? gshare_pred : bimodal_pred;
    
    // Track predictions
    hp->bp->base.predictions++;
    if((outcome == 't' && !final_pred) || (outcome == 'n' && final_pred)) {
        hp->bp->base.mispredictions++;
    }
    
    // Step 4: Update selected predictor
    if (use_gshare) {
        update_counter(&hp->gp->base.prediction_table[gshare_index], outcome == 't');
    } else {
        update_counter(&hp->bp->base.prediction_table[bimodal_index], outcome == 't');
    }
    
    // Step 5: Always update gshare history
    hp->gp->history = ((hp->gp->history >> 1) | ((outcome == 't' ? 1U : 0U) << (N-1))) & hp->gp->history_mask;
    
    // Step 6: Update chooser counter
    int gshare_correct = (gshare_pred == 1 && outcome == 't') || (gshare_pred == 0 && outcome == 'n');
    int bimodal_correct = (bimodal_pred == 1 && outcome == 't') || (bimodal_pred == 0 && outcome == 'n');
    
    if (gshare_correct != bimodal_correct) {
        if (gshare_correct) {
            // Increment chooser (saturates at 3)
            if (hp->chooser_table[chooser_index] < 3) {
                hp->chooser_table[chooser_index]++;
            }
        } else {
            // Decrement chooser (saturates at 0)
            if (hp->chooser_table[chooser_index] > 0) {
                hp->chooser_table[chooser_index]--;
            }
        }
    }
}

void print_stats(predictor_base* base, const char* predictor_name) {
    printf("number of predictions:\t\t%lu\n", base->predictions);
    printf("number of mispredictions:\t%lu\n", base->mispredictions);
    printf("misprediction rate:\t\t%.2f%%\n", 
           ((float)base->mispredictions/base->predictions) * 100);
    printf("FINAL %s CONTENTS\n", predictor_name);
    
    for(unsigned long int i = 0; i < base->size; i++) {
       printf("%lu\t%u\n", i, base->prediction_table[i]);
   }
}