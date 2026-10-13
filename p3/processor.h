#ifndef OUT_OF_ORDER_PROCESSOR_H
#define OUT_OF_ORDER_PROCESSOR_H

#include <iostream>
#include <vector>
#include <deque>
#include <iomanip>
#include "processor_config.h"

// Number of Architectural Registers
#define ARF_SIZE 67

// OutOfOrderProcessor: Simulates a superscalar out-of-order processor with dynamic scheduling
class OutOfOrderProcessor {
private:
    // Processor Configuration
    ProcessorParameters m_config;  // Stores processor configuration parameters
    FILE* m_traceFile;             // Input trace file for instruction stream

    // Pipeline Stage Buffers
    std::deque<Instruction> m_decodeBuffer;        // Instructions waiting to be decoded
    std::deque<Instruction> m_renameBuffer;        // Instructions waiting for register renaming
    std::deque<Instruction> m_registerReadBuffer;  // Instructions waiting for register read
    std::deque<Instruction> m_dispatchBuffer;      // Instructions waiting to be dispatched
    std::deque<Instruction> m_writebackBuffer;     // Instructions completed execution

    // Reorder Buffer: Tracks instructions to ensure program semantics and precise exceptions
    std::vector<ReorderBufferEntry> m_reorderBuffer;
    int m_robHead;  // Head pointer of Reorder Buffer
    int m_robTail;  // Tail pointer of Reorder Buffer

    // Rename Table: Maps architectural registers to renamed registers
    std::vector<RenameTableEntry> m_renameTable;

    // Issue Queue: Tracks instructions waiting to be executed
    std::vector<IssueQueueEntry> m_issueQueue;

    // Execution List: Tracks instructions currently in execution
    std::vector<ExecutionEntry> m_executionList;

    // Simulation Metrics
    uint64_t m_instructionCount;  // Total number of instructions processed
    uint64_t m_cycleCount;        // Total simulation cycles
    bool m_simulationComplete;    // Flag to indicate simulation completion

    // Private Helper Methods for Resource Status Checks
    bool isReorderBufferFull() const;    // Checks if Reorder Buffer is at capacity
    bool isReorderBufferEmpty() const;   // Checks if Reorder Buffer is empty
    bool isIssueQueueFull() const;       // Checks if Issue Queue is at capacity
    bool isIssueQueueEmpty() const;      // Checks if Issue Queue is empty
    bool isInstructionReady(size_t j) const;  // Checks if an instruction is ready to issue
    bool isExecutionNeeded() const;      // Checks if execution stage needs processing

    // Pipeline Stage Implementations
    void fetchStage();        // Fetch new instructions
    void decodeStage();       // Decode fetched instructions
    void renameStage();       // Rename registers
    void registerReadStage(); // Read register values
    void dispatchStage();     // Dispatch instructions to issue queue
    void issueStage();        // Issue instructions for execution
    void executeStage();      // Execute instructions
    void writebackStage();    // Write back execution results
    void retireStage();       // Commit instructions in order

    // Utility Methods
    void initializeStructures();  // Initialize processor data structures
    void printInstructionDetails(const Instruction& inst) const;  // Debug print instruction details

    // Instruction Creation Utility
    Instruction createInstruction(
        uint64_t pc, 
        int opType, 
        int destReg, 
        int src1Reg, 
        int src2Reg, 
        uint64_t sequenceNum
    );

    // Utility Methods for Tracking Occupancy
    int countROBEntries() const;  // Count valid entries in Reorder Buffer
    int countIQEntries() const;   // Count valid entries in Issue Queue

public:
    // Constructor: Initialize processor with configuration and trace file
    OutOfOrderProcessor(
        const ProcessorParameters& config, 
        FILE* traceFile
    );

    // Main Simulation Methods
    void simulate();         // Run complete simulation
    bool advanceCycle();     // Advance processor by one cycle
    void printSimulationResults() const;  // Display simulation statistics

    // Destructor
    ~OutOfOrderProcessor();
};

#endif // OUT_OF_ORDER_PROCESSOR_H