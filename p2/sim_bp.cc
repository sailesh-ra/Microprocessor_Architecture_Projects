#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_bp.h"

int main(int argc, char* argv[]) {
    FILE *FP;
    char *trace_file;
    bp_params params = {0, 0, 0, 0, NULL};
    char outcome;
    unsigned long int addr;
    bimodal_predictor* bp = NULL;
    gshare_predictor* gp = NULL;
    hybrid_predictor* hp = NULL;
    
    // Validate command line arguments
    if (!(argc == 4 || argc == 5 || argc == 7)) {
        fprintf(stderr, "Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    params.bp_name = argv[1];
    
    // Initialize appropriate predictor based on command line arguments
    if(strcmp(params.bp_name, "bimodal") == 0) {
        if(argc != 4) {
            fprintf(stderr, "Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M2 = strtoul(argv[2], NULL, 10);
        trace_file = argv[3];
        printf("COMMAND\n%s bimodal %lu %s\n", argv[0], params.M2, trace_file);
        
        bp = init_bimodal(params.M2);
    }
    else if(strcmp(params.bp_name, "gshare") == 0) {
        if(argc != 5) {
            fprintf(stderr, "Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M1 = strtoul(argv[2], NULL, 10);
        params.N = strtoul(argv[3], NULL, 10);
        trace_file = argv[4];
        printf("COMMAND\n%s gshare %lu %lu %s\n", argv[0], params.M1, params.N, trace_file);
        
        gp = init_gshare(params.M1, params.N);
    }
    else if(strcmp(params.bp_name, "hybrid") == 0) {
        if(argc != 7) {
            fprintf(stderr, "Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.K = strtoul(argv[2], NULL, 10);
        params.M1 = strtoul(argv[3], NULL, 10);
        params.N = strtoul(argv[4], NULL, 10);
        params.M2 = strtoul(argv[5], NULL, 10);
        trace_file = argv[6];
        printf("COMMAND\n%s hybrid %lu %lu %lu %lu %s\n", 
               argv[0], params.K, params.M1, params.N, params.M2, trace_file);
        
        hp = init_hybrid(params.K, params.M1, params.N, params.M2);
    }
    else {
        fprintf(stderr, "Error: Wrong branch predictor name:%s\n", params.bp_name);
        exit(EXIT_FAILURE);
    }
    
    // Open trace file
    FP = fopen(trace_file, "r");
    if(FP == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", trace_file);
        if(bp) free_bimodal(bp);
        if(gp) free_gshare(gp);
        if(hp) free_hybrid(hp);
        exit(EXIT_FAILURE);
    }
    
    // Process each branch in trace
    char str[2];
    while(fscanf(FP, "%lx %s", &addr, str) != EOF) {
        outcome = str[0];
        
        if(strcmp(params.bp_name, "bimodal") == 0) {
            process_bimodal(bp, addr, outcome);
        }
        else if(strcmp(params.bp_name, "gshare") == 0) {
            process_gshare(gp, addr, params.M1, params.N, outcome);
        }
        else if(strcmp(params.bp_name, "hybrid") == 0) {
            process_hybrid(hp, addr, params.K, params.M1, params.N, outcome);
        }
    }
    
    // Print results
    printf("OUTPUT\n");
    if(strcmp(params.bp_name, "bimodal") == 0) {
        print_stats(&bp->base, "BIMODAL");
    }
    else if(strcmp(params.bp_name, "gshare") == 0) {
        print_stats(&gp->base, "GSHARE");
    }
    else if(strcmp(params.bp_name, "hybrid") == 0) {
        printf("number of predictions:\t\t%lu\n", hp->bp->base.predictions);
        printf("number of mispredictions:\t%lu\n", hp->bp->base.mispredictions);
        printf("misprediction rate:\t\t%.2f%%\n", 
               ((float)hp->bp->base.mispredictions/hp->bp->base.predictions) * 100);
       printf("FINAL CHOOSER CONTENTS\n");
        for(unsigned long int i = 0; i < (1UL << params.K); i++) {
            printf("%lu\t%u\n", i, hp->chooser_table[i]);
        }
        printf("FINAL GSHARE CONTENTS\n");
        for(unsigned long int i = 0; i < hp->gp->base.size; i++) {
            printf("%lu\t%u\n", i, hp->gp->base.prediction_table[i]);
        }
        printf("FINAL BIMODAL CONTENTS\n");
        for(unsigned long int i = 0; i < hp->bp->base.size; i++) {
            printf("%lu\t%u\n", i, hp->bp->base.prediction_table[i]);
        }
    }
    
    // Cleanup
    fclose(FP);
    if(bp) free_bimodal(bp);
    if(gp) free_gshare(gp);
    if(hp) free_hybrid(hp);
    return 0;
}