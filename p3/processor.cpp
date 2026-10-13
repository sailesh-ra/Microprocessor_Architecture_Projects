#include <climits>
#include <iomanip>
#include <iostream>
#include "processor.h"

// Constructor: Initialize the out-of-order processor with configuration and trace file
OutOfOrderProcessor::OutOfOrderProcessor(
    const ProcessorParameters& config, 
    FILE* traceFile
) : 
    m_config(config),
    m_traceFile(traceFile),
    m_reorderBuffer(config.robSize),
    m_renameTable(ARF_SIZE),
    m_issueQueue(config.iqSize),
    m_robHead(0),
    m_robTail(0),
    m_instructionCount(0),
    m_cycleCount(0),
    m_simulationComplete(false)
{
    // Initialize processor structures to their starting state
    initializeStructures();
}

// Reset all processor pipeline and tracking structures to their initial state
void OutOfOrderProcessor::initializeStructures() {
    // Clear all pipeline buffers
    m_decodeBuffer.clear();
    m_renameBuffer.clear();
    m_registerReadBuffer.clear();
    m_dispatchBuffer.clear();
    m_writebackBuffer.clear();
    
    // Reset Reorder Buffer to initial state
    m_robHead = 0;
    m_robTail = 0;
    std::fill(m_reorderBuffer.begin(), m_reorderBuffer.end(), ReorderBufferEntry());
    m_reorderBuffer.resize(m_config.robSize);
    
    // Reset Rename Table to initial state
    std::fill(m_renameTable.begin(), m_renameTable.end(), RenameTableEntry());
    m_renameTable.resize(ARF_SIZE);
    
    // Reset Issue Queue to initial state
    std::fill(m_issueQueue.begin(), m_issueQueue.end(), IssueQueueEntry());
    m_issueQueue.resize(m_config.iqSize);
}

// Main simulation loop: Execute all pipeline stages for each cycle
void OutOfOrderProcessor::simulate() {
    do {   
        // Execute pipeline stages in reverse order to model dependencies
        retireStage();      // Commit completed instructions
        writebackStage();   // Complete instruction execution
        executeStage();     // Process instructions in execution
        issueStage();       // Select instructions for execution
        dispatchStage();    // Prepare instructions for issue
        registerReadStage();// Read source register values
        renameStage();      // Allocate rename resources
        decodeStage();      // Decode fetched instructions
        fetchStage();       // Fetch new instructions
    } while (advanceCycle());
}

// Fetch stage: Read new instructions from trace file into decode buffer
void OutOfOrderProcessor::fetchStage() {
    // Prevent fetching if decode buffer is full
    if (m_decodeBuffer.size() >= m_config.width) {
        return;
    }

    // Read instructions from trace file
    for (size_t i = 0; i < m_config.width; i++) {
        uint64_t pc;
        int opType, dest, src1, src2;
        
        // Attempt to read instruction from trace file
        int ret = fscanf(m_traceFile, "%lx %d %d %d %d", &pc, &opType, &dest, &src1, &src2);
        
        // Check for end of trace file
        if (ret == EOF || ret < 5) {
            m_simulationComplete = true;
            return;
        }

        // Create and add instruction to decode buffer
        Instruction instruction = createInstruction(
            pc, opType, dest, src1, src2, m_instructionCount++
        );
        
        // Record fetch cycle information
        instruction.fetchCycle = m_cycleCount;
        instruction.fetchDuration = 1;
        
        m_decodeBuffer.push_back(instruction);
    }
}

// Decode stage: Prepare instructions for renaming
void OutOfOrderProcessor::decodeStage() {
    // Mark decode cycle for new instructions
    for (auto& inst : m_decodeBuffer) {
        if (inst.decodeCycle == -1) {
            inst.decodeCycle = m_cycleCount;
        }
    }
    
    // Check if rename buffer has space
    if (m_renameBuffer.size() == m_config.width) return;

    // Move instructions from decode buffer to rename buffer
    while (!m_decodeBuffer.empty() && m_renameBuffer.size() < m_config.width) {
        Instruction inst = m_decodeBuffer.front();        
        inst.decodeDuration = m_cycleCount - inst.decodeCycle + 1;
        m_renameBuffer.push_back(inst);
        m_decodeBuffer.pop_front();
    }
}

// Rename stage: Allocate rename resources and update rename table
void OutOfOrderProcessor::renameStage() {
    // Mark rename cycle for new instructions
    for (auto& inst : m_renameBuffer) {
        if (inst.renameCycle == -1) {
            inst.renameCycle = m_cycleCount;
        }
    }
    
    // Check if ROB and register read buffer have space
    if (isReorderBufferFull() || m_registerReadBuffer.size() == m_config.width)
        return;

    while (!m_renameBuffer.empty() && m_registerReadBuffer.size() < m_config.width) {
        Instruction inst = m_renameBuffer.front();

        // Allocate ROB entry
        m_reorderBuffer[m_robTail].valid = true;
        m_reorderBuffer[m_robTail].ready = false;
        m_reorderBuffer[m_robTail].instruction = inst;

        // Rename source registers
        if (inst.src1Reg != -1 && m_renameTable[inst.src1Reg].valid) {
            inst.src1Rename = m_renameTable[inst.src1Reg].robTag;
        }

        if (inst.src2Reg != -1 && m_renameTable[inst.src2Reg].valid) {
            inst.src2Rename = m_renameTable[inst.src2Reg].robTag;
        }

        m_reorderBuffer[m_robTail].destArchReg = inst.destReg;

        // Update Rename Table for destination register
        if (inst.destReg != -1) {
            m_renameTable[inst.destReg].valid = true;
            m_renameTable[inst.destReg].robTag = m_robTail;
        }

        inst.destRename = m_robTail;

        // Set rename timing and advance
        inst.renameDuration = m_cycleCount - inst.renameCycle + 1;
        m_registerReadBuffer.push_back(inst);
        m_renameBuffer.pop_front();

        // Advance ROB tail
        m_robTail = (m_robTail + 1) % m_config.robSize;
    }
}

// Register Read stage: Prepare instructions for dispatch
void OutOfOrderProcessor::registerReadStage() {
    // Mark register read cycle for new instructions
    for (auto& inst : m_registerReadBuffer) {
        if (inst.regReadCycle == -1) {
            inst.regReadCycle = m_cycleCount;
        }
    }

    // Check if dispatch buffer is full
    if (m_dispatchBuffer.size() == m_config.width)
        return;

    while (!m_registerReadBuffer.empty() && m_dispatchBuffer.size() < m_config.width) {
        Instruction inst = m_registerReadBuffer.front();

        // Check if source registers are ready
        if (inst.src1Rename != -1 && m_reorderBuffer[inst.src1Rename].ready) {
            inst.src1Rename = -1;
        }

        if (inst.src2Rename != -1 && m_reorderBuffer[inst.src2Rename].ready) {
            inst.src2Rename = -1;
        }

        // Set timing and advance
        inst.regReadDuration = m_cycleCount - inst.regReadCycle + 1;
        m_dispatchBuffer.push_back(inst);
        m_registerReadBuffer.pop_front();
    }
}

// Dispatch stage: Move instructions to Issue Queue
void OutOfOrderProcessor::dispatchStage() {
    // Mark dispatch cycle for new instructions
    for (auto& inst : m_dispatchBuffer) {
        if (inst.dispatchCycle == -1) {
            inst.dispatchCycle = m_cycleCount;
        }
    }

    // Check if issue queue is full
    if (isIssueQueueFull()) {
        return;
    }

    while (!m_dispatchBuffer.empty()) {
        // Find an empty slot in the issue queue
        for (size_t i = 0; i < m_config.iqSize; i++) {
            if (m_issueQueue[i].valid) continue;

            // Update source rename dependencies
            if (m_dispatchBuffer.front().src1Rename != -1 && 
                m_reorderBuffer[m_dispatchBuffer.front().src1Rename].ready) {
                m_dispatchBuffer.front().src1Rename = -1;
            }
            if (m_dispatchBuffer.front().src2Rename != -1 && 
                m_reorderBuffer[m_dispatchBuffer.front().src2Rename].ready) {
                m_dispatchBuffer.front().src2Rename = -1;
            }

            // Move to Issue Stage
            m_dispatchBuffer.front().dispatchDuration = 
                m_cycleCount - m_dispatchBuffer.front().dispatchCycle + 1;
            m_issueQueue[i].valid = true;
            m_issueQueue[i].instruction = m_dispatchBuffer.front();
            
            break;
        }
        
        m_dispatchBuffer.pop_front();
    }
}

// Issue stage: Select and prepare instructions for execution
void OutOfOrderProcessor::issueStage() {
    // Prevent issuing if execution list is full
    if (m_executionList.size() == m_config.width * 5) {
        return;
    }

    // Mark issue cycle for new instructions
    for (auto& entry : m_issueQueue) {
        if (entry.valid && entry.instruction.issueCycle == -1) {
            entry.instruction.issueCycle = m_cycleCount;
        }
    }

    // Issue up to width instructions
    for (size_t i = 0; i < m_config.width; i++) {
        // Find oldest ready instruction
        int oldestCycle = INT_MAX;
        int oldestIdx = -1;

        for (size_t j = 0; j < m_config.iqSize; j++) {
            // Skip invalid or dependent instructions
            if (!m_issueQueue[j].valid) continue;
            if (m_issueQueue[j].instruction.src1Rename != -1) continue;
            if (m_issueQueue[j].instruction.src2Rename != -1) continue;
            
            // Select oldest ready instruction
            if (isInstructionReady(j) &&
                m_issueQueue[j].instruction.fetchCycle < oldestCycle) {
                oldestCycle = m_issueQueue[j].instruction.fetchCycle;
                oldestIdx = j;
            }
        }

        if (oldestIdx == -1) {
            return;  // No ready instructions found
        }

        // Determine execution latency based on operation type
        int execLatency = (m_issueQueue[oldestIdx].instruction.opType == 0) ? 1 : 
                          (m_issueQueue[oldestIdx].instruction.opType == 1) ? 2 : 5;

        // Move to execution list
        m_issueQueue[oldestIdx].instruction.issueDuration = 
            m_cycleCount - m_issueQueue[oldestIdx].instruction.issueCycle + 1;
        ExecutionEntry ex_inst = {m_issueQueue[oldestIdx].instruction, execLatency};
        m_executionList.push_back(ex_inst);

        // Clear issue queue entry
        m_issueQueue[oldestIdx].valid = false;
    }
}

// Execute stage: Process instructions in execution
void OutOfOrderProcessor::executeStage() {
    // Check if execution list has any instructions
    if (m_executionList.empty()) {
        return;
    }

    // Set execute cycle and decrease remaining cycles
    for (auto& execEntry : m_executionList) {
        if (execEntry.instruction.executeCycle == -1) {
            execEntry.instruction.executeCycle = m_cycleCount;
        }
        execEntry.remainingCycles--;
    }

    // Process completed instructions
    while (isExecutionNeeded()) {
        for (size_t i = 0; i < m_executionList.size(); i++) {
            if (m_executionList[i].remainingCycles == 0) {
                // Wake up dependent instructions in issue queue
                for (size_t j = 0; j < m_config.iqSize; j++) {
                    if ((m_issueQueue[j].instruction.src1Rename == m_executionList[i].instruction.destRename) && 
                        m_issueQueue[j].instruction.src1Rename != -1) {
                        m_issueQueue[j].instruction.src1Rename = -1;
                    }
                    if ((m_issueQueue[j].instruction.src2Rename == m_executionList[i].instruction.destRename) && 
                        m_issueQueue[j].instruction.src2Rename != -1) {
                        m_issueQueue[j].instruction.src2Rename = -1;
                    }
                }

                // Wake up dependent instructions in other stages
                for (auto& inst : m_dispatchBuffer) {
                    if (inst.src1Rename == m_executionList[i].instruction.destRename) {
                        inst.src1Rename = -1;
                    }
                    if (inst.src2Rename == m_executionList[i].instruction.destRename) {
                        inst.src2Rename = -1;
                    }
                }

                for (auto& inst : m_registerReadBuffer) {
                    if (inst.src1Rename == m_executionList[i].instruction.destRename) {
                        inst.src1Rename = -1;
                    }
                    if (inst.src2Rename == m_executionList[i].instruction.destRename) {
                        inst.src2Rename = -1;
                    }
                }

                // Check if writeback buffer is full
                if (m_writebackBuffer.size() == m_config.width * 5) {
                    return;
                }

                // Move completed instruction to writeback
                m_executionList[i].instruction.executeDuration = 
                    m_cycleCount - m_executionList[i].instruction.executeCycle + 1;
                m_executionList[i].instruction.valid = true;
                m_writebackBuffer.push_back(m_executionList[i].instruction);

                m_executionList.erase(m_executionList.begin() + i);
            }
        }
    }
}

// Writeback stage: Complete instruction execution and mark ROB entries as ready
void OutOfOrderProcessor::writebackStage() {
    // Set writeback cycle for new instructions in the writeback buffer
    for (auto& inst : m_writebackBuffer) {
        if (m_writebackBuffer.size() && inst.writebackCycle == -1) {
            inst.writebackCycle = m_cycleCount;
        }
    }
    
    // Process instructions in the writeback buffer
    while (!m_writebackBuffer.empty()) {
        // Find corresponding ROB entry for the instruction
        for (size_t i = 0; i < m_config.robSize; i++) {
            // Check if the ROB entry is valid and matches the instruction sequence number
            if (m_reorderBuffer[i].valid && 
                m_reorderBuffer[i].instruction.sequenceNum == m_writebackBuffer.front().sequenceNum) {
                
                // Mark the ROB entry as ready for retirement
                m_reorderBuffer[i].ready = true;

                // Calculate and set writeback duration
                m_writebackBuffer.front().writebackDuration = 
                    m_cycleCount - m_writebackBuffer.front().writebackCycle + 1;
                
                // Update the ROB entry with the instruction details
                m_reorderBuffer[i].instruction = m_writebackBuffer.front();
                
                // Remove the processed instruction from the writeback buffer
                m_writebackBuffer.pop_front();

                // Exit inner loop if writeback buffer is empty
                if (m_writebackBuffer.empty()) {
                    break;
                }
            }
        }
    }
}

// Retire stage: Commit completed instructions from the Reorder Buffer
void OutOfOrderProcessor::retireStage() {
    // Skip if Reorder Buffer is empty
    if (isReorderBufferEmpty()) {
        return;
    }

    // Set retire cycle for ready instructions in the Reorder Buffer
    for (size_t i = 0; i < m_reorderBuffer.size(); i++) {
        if (m_reorderBuffer[i].ready && m_reorderBuffer[i].instruction.retireCycle == -1) {
            m_reorderBuffer[i].instruction.retireCycle = m_cycleCount;
        }
    }

    // Retire up to processor width number of instructions
    for (size_t i = 0; i < m_config.width; i++) {
        // Check if the ROB head entry is valid and ready to retire
        if (m_reorderBuffer[m_robHead].valid && m_reorderBuffer[m_robHead].ready) {
            // Calculate retire duration
            m_reorderBuffer[m_robHead].instruction.retireDuration = 
                m_cycleCount - m_reorderBuffer[m_robHead].instruction.retireCycle + 1;

            // Optionally print instruction details (can be commented out if not needed)
            // Uncomment the following line to print specific instruction details
            //if (m_reorderBuffer[m_robHead].instruction.sequenceNum == 9618)
            printInstructionDetails(m_reorderBuffer[m_robHead].instruction);

            // Clear rename table mapping for the retired instruction's destination register
            if (m_reorderBuffer[m_robHead].instruction.destReg != -1 && 
                m_reorderBuffer[m_robHead].instruction.destRename == 
                    m_renameTable[m_reorderBuffer[m_robHead].instruction.destReg].robTag) {
                
                m_renameTable[m_reorderBuffer[m_robHead].instruction.destReg].valid = false;
                m_renameTable[m_reorderBuffer[m_robHead].instruction.destReg].robTag = -1;
            }

            // Clear the Reorder Buffer entry at the head
            m_reorderBuffer[m_robHead].valid = false;

            // Advance the Reorder Buffer head pointer
            m_robHead = (m_robHead + 1) % m_config.robSize;
        }
    }
}

// Print detailed information about a specific instruction
void OutOfOrderProcessor::printInstructionDetails(const Instruction& inst) const {
    std::cout << inst.sequenceNum << " "
              << "fu{" << inst.opType << "} "
              << "src{" << inst.src1Reg << "," << inst.src2Reg << "} "
              << "dst{" << inst.destReg << "} "
              << "FE{" << inst.fetchCycle << "," << inst.fetchDuration << "} "
              << "DE{" << inst.decodeCycle << "," << inst.decodeDuration << "} "
              << "RN{" << inst.renameCycle << "," << inst.renameDuration << "} "
              << "RR{" << inst.regReadCycle << "," << inst.regReadDuration << "} "
              << "DI{" << inst.dispatchCycle << "," << inst.dispatchDuration << "} "
              << "IS{" << inst.issueCycle << "," << inst.issueDuration << "} "
              << "EX{" << inst.executeCycle << "," << inst.executeDuration << "} "
              << "WB{" << inst.writebackCycle << "," << inst.writebackDuration << "} "
              << "RT{" << inst.retireCycle << "," << inst.retireDuration << "} "
              << std::endl;
}

// Print overall simulation results and performance metrics
void OutOfOrderProcessor::printSimulationResults() const {
    // Calculate Instructions Per Cycle (IPC)
    float ipc = static_cast<float>(m_instructionCount) / m_cycleCount;
    
    std::cout << "# === Simulation Results ========"     << std::endl;
    std::cout << "# Dynamic Instruction Count      = "   << m_instructionCount << std::endl;
    std::cout << "# Cycles                         = "   << m_cycleCount << std::endl;
    std::cout << "# Instructions Per Cycle (IPC)   = "   
              << std::fixed << std::setprecision(2) << ipc << std::endl;
}

// Create and initialize a new instruction with default values
Instruction OutOfOrderProcessor::createInstruction(
    uint64_t pc, 
    int opType, 
    int destReg, 
    int src1Reg, 
    int src2Reg, 
    uint64_t sequenceNum
) {
    Instruction inst;
    
    // Set basic instruction properties
    inst.pc = pc;
    inst.opType = opType;
    inst.destReg = destReg;
    inst.src1Reg = src1Reg;
    inst.src2Reg = src2Reg;
    inst.sequenceNum = sequenceNum;

    // Initialize rename and register tracking
    inst.destRename = -1;
    inst.src1Rename = -1;
    inst.src2Rename = -1;
    
    // Set instruction validity
    inst.valid = false;

    // Initialize all cycle tracking to -1 (unset)
    inst.decodeCycle = -1;
    inst.renameCycle = -1;
    inst.regReadCycle = -1;
    inst.dispatchCycle = -1;
    inst.issueCycle = -1;
    inst.executeCycle = -1;
    inst.writebackCycle = -1;
    inst.retireCycle = -1;

    return inst;
}

// Check if the Reorder Buffer is full
bool OutOfOrderProcessor::isReorderBufferFull() const {
    // Count empty slots in the Reorder Buffer
    int emptySlots = 0;
    for (const auto& entry : m_reorderBuffer) {
        if (!entry.valid) {
            emptySlots++;
        }
    }
    
    // Consider ROB full if fewer empty slots than processor width
    return emptySlots < m_config.width;
}

// Check if the Issue Queue is full
bool OutOfOrderProcessor::isIssueQueueFull() const {
    // Count empty slots in the Issue Queue
    int emptySlots = 0;
    for (const auto& entry : m_issueQueue) {
        if (!entry.valid) {
            emptySlots++;
        }
    }
    
    // Consider IQ full if fewer empty slots than processor width
    return emptySlots < m_config.width;
}

// Check if an instruction in the Issue Queue is ready for execution
bool OutOfOrderProcessor::isInstructionReady(size_t j) const {
    // Check if any source register still has an outstanding dependency
    if (m_issueQueue[j].instruction.src1Rename != -1 || 
        m_issueQueue[j].instruction.src2Rename != -1) {
        return false;
    }

    // Both source registers are ready
    return true;
}

// Check if any instruction in the execution list needs processing
bool OutOfOrderProcessor::isExecutionNeeded() const {
    // Iterate through execution list
    for (const auto& execEntry : m_executionList) {
        // If any instruction's execution time has reached zero
        if (execEntry.remainingCycles == 0) {
            return true;
        }
    }
    return false;
}

// Advance the simulation cycle and determine if simulation should continue
bool OutOfOrderProcessor::advanceCycle() {
    // Increment cycle count
    m_cycleCount++;

    // Check if there are still instructions in any pipeline stage
    bool hasInstructions =
        m_decodeBuffer.size() ||
        m_renameBuffer.size() ||
        m_registerReadBuffer.size() ||
        m_dispatchBuffer.size() ||
        !isIssueQueueEmpty() ||
        m_executionList.size() ||
        m_writebackBuffer.size() ||
        !isReorderBufferEmpty();

    return hasInstructions;
}

// Check if the Reorder Buffer is empty
bool OutOfOrderProcessor::isReorderBufferEmpty() const {
    for (const auto& entry : m_reorderBuffer) {
        if (entry.valid) {
            return false;
        }
    }
    return true;
}

// Check if the Issue Queue is empty
bool OutOfOrderProcessor::isIssueQueueEmpty() const {
    for (const auto& entry : m_issueQueue) {
        if (entry.valid) {
            return false;
        }
    }
    return true;
}

// Destructor to clean up resources
OutOfOrderProcessor::~OutOfOrderProcessor() {
    // Close trace file if it's open
    if (m_traceFile) {
        fclose(m_traceFile);
    }
}