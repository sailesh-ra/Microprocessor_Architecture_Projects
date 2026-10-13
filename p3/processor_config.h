#ifndef PROCESSOR_CONFIG_H
#define PROCESSOR_CONFIG_H

#include <cstdint>

using namespace std;

// Processor Configuration Parameters
// Defines the key structural constraints and settings for the out-of-order processor
struct ProcessorParameters {
    uint32_t robSize;    // Size of the Reorder Buffer (maximum entries that can be tracked)
    uint32_t iqSize;     // Size of the Issue Queue (maximum instructions waiting to be executed)
    uint32_t width;      // Processor pipeline width (maximum instructions processed per cycle)
};

// Instruction Representation
// Captures detailed information about a single dynamic instruction through its lifecycle
struct Instruction {
    // Static Instruction Identifiers
    uint64_t pc;            // Program Counter (unique instruction address)
    int opType;             // Operation Type (e.g., arithmetic, load, store)
    
    // Register Details
    int destReg;            // Destination Architectural Register
    int destRename;         // Destination Renamed Register
    int src1Reg;            // First Source Architectural Register
    int src1Rename;         // First Source Renamed Register
    int src2Reg;            // Second Source Architectural Register
    int src2Rename;         // Second Source Renamed Register
    
    // Instruction State
    bool valid;             // Indicates if instruction is valid and being processed
    uint64_t sequenceNum;   // Unique dynamic instruction number for tracking

    // Cycle Timestamps for Each Pipeline Stage
    int fetchCycle;
    int decodeCycle;
    int renameCycle;
    int regReadCycle;
    int dispatchCycle;
    int issueCycle;
    int executeCycle;
    int writebackCycle;
    int retireCycle;

    // Stage Duration Tracking
    int fetchDuration;
    int decodeDuration;
    int renameDuration;
    int regReadDuration;
    int dispatchDuration;
    int issueDuration;
    int executeDuration;
    int writebackDuration;
    int retireDuration;

    // Default Constructor: Initialize all fields to default/neutral values
    Instruction() : 
        pc(0), opType(0), 
        destReg(-1), destRename(-1), 
        src1Reg(-1), src1Rename(-1), 
        src2Reg(-1), src2Rename(-1),
        valid(false), sequenceNum(0),
        fetchCycle(-1), decodeCycle(-1), renameCycle(-1),
        regReadCycle(-1), dispatchCycle(-1), issueCycle(-1),
        executeCycle(-1), writebackCycle(-1), retireCycle(-1),
        fetchDuration(0), decodeDuration(0), renameDuration(0),
        regReadDuration(0), dispatchDuration(0), issueDuration(0),
        executeDuration(0), writebackDuration(0), retireDuration(0)
    {}
};

// Rename Table Entry
// Maps architectural registers to renamed registers in the out-of-order pipeline
struct RenameTableEntry {
    bool valid;     // Indicates if the entry is currently in use
    int robTag;     // Reorder Buffer tag associated with this renamed register

    // Default Constructor
    RenameTableEntry() : valid(false), robTag(-1) {}
};

// Reorder Buffer Entry
// Tracks instructions in-flight, ensuring correct program semantics and precise exceptions
struct ReorderBufferEntry {
    bool valid;             // Indicates if entry is occupied
    bool ready;             // Indicates if instruction is ready to retire
    Instruction instruction; // Full instruction details
    int destArchReg;        // Destination architectural register

    // Default Constructor
    ReorderBufferEntry() : 
        valid(false), 
        ready(false), 
        destArchReg(-1) 
    {}
};

// Issue Queue Entry
// Represents an instruction waiting to be issued for execution
struct IssueQueueEntry {
    bool valid;             // Indicates if entry is occupied
    Instruction instruction; // Full instruction details

    // Default Constructor
    IssueQueueEntry() : valid(false) {}
};

// Execution Entry
// Tracks an instruction during its execution phase
struct ExecutionEntry {
    Instruction instruction;  // Instruction being executed
    int remainingCycles;      // Cycles left to complete execution

    // Constructor with initial instruction and execution cycles
    ExecutionEntry(const Instruction& inst, int cycles) : 
        instruction(inst), 
        remainingCycles(cycles) 
    {}
};

#endif // PROCESSOR_CONFIG_H