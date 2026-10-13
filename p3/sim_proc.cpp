#include <iostream>
#include <string>
#include "processor.h"

int main(int argc, char* argv[]) {
    // Check for correct number of command-line arguments
    if (argc != 5) {
        cerr << "Usage: " << argv[0] 
                  << " <rob_size> <iq_size> <width> <trace_file>" 
                  << endl;
        return 1;
    }

    // Parse configuration parameters first
    ProcessorParameters config;
    
    config.robSize = stoul(argv[1]);    // ROB size is first argument
    config.iqSize = stoul(argv[2]);     // IQ size is second argument
    config.width = stoul(argv[3]);      // Width is third argument

    // Open trace file (fourth argument)
    FILE* traceFile = fopen(argv[4], "r");
    if (!traceFile) {
        cerr << "Error: Could not open trace file " << argv[4] << endl;
        return 1;
    }

    // Create processor instance with configuration and trace file
    OutOfOrderProcessor processor(config, traceFile);

    try {
        processor.simulate();
    }
    catch (const exception& e) {
        cerr << "Simulation error: " << e.what() << endl;
        fclose(traceFile);
        return 1;
    }

    // Print simulation configuration and results
    cout << "# === Simulator Command =========" << endl
         << "# ./sim "  << argv[1] << " " <<
            argv[2] << " " << argv[3] << " " <<
            argv[4] << " " << endl;
    cout << "# === Processor Configuration ==="     << endl;
    cout << "# ROB_SIZE  = "        << argv[1]      << endl;
    cout << "# IQ_SIZE   = "        << argv[2]      << endl;
    cout << "# WIDTH     = "        << argv[3]      << endl;

    // Display final simulation metrics
    processor.printSimulationResults();

    return 0;
}