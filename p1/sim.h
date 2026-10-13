#ifndef SIM_H
#define SIM_H

#include <cstdint>
#include <vector>
#include <list>

// Structure for cache parameters
struct cache_params_t {
    uint32_t BLOCKSIZE;
    uint32_t L1_SIZE;
    uint32_t L1_ASSOC;
    uint32_t L2_SIZE;
    uint32_t L2_ASSOC;
    uint32_t PREF_N;
    uint32_t PREF_M;
};

// Cache line structure
struct CacheLine {
    bool valid;
    uint32_t tag;
};

// L1 Cache class declaration
class L1_cache {
public:
    L1_cache(uint32_t size, uint32_t block_size, uint32_t associativity);
    bool access(uint32_t address, char type); // Handle read or write
    void display_stats() const;               // Display cache stats

private:
    uint32_t cache_size, block_size, associativity, num_sets;
    uint32_t hits, misses;

    // Cache representation: each set is a vector of CacheLines
    std::vector<std::vector<CacheLine>> cache;

    // LRU tracking: a list per set to track LRU order
    std::vector<std::list<uint32_t>> lru_tracker;

    // Utility functions
    uint32_t get_set_index(uint32_t address) const;
    uint32_t get_tag(uint32_t address) const;
};

#endif
