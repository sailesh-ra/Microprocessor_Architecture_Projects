#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <cstdint>
#include "prefetcher.h"

class Cache {
public:
    Cache(uint32_t size, uint32_t block_size, uint32_t associativity, Cache* l2 = nullptr, Prefetcher* prefetcher = nullptr);
    bool access(uint32_t address, char type, int access_id, bool is_prefetch = false);
    
    struct CacheLine {
        bool valid;
        bool dirty;
        uint32_t tag;
    };

    struct PrefetchBuffer {
        std::vector<CacheLine> buffer;
        uint32_t size;
        uint32_t head;
        uint32_t tail;

        PrefetchBuffer(uint32_t size);
    };

    uint32_t cache_size;
    uint32_t block_size;
    uint32_t associativity;
    uint32_t num_sets;
    Cache* l2;
    Prefetcher* prefetcher;
    PrefetchBuffer prefetch_buffer;

    std::vector<std::vector<CacheLine>> cache;
    std::vector<std::vector<uint32_t>> lru_tracker;

    uint32_t hits;
    uint32_t misses;
    uint32_t reads;
    uint32_t read_hits;
    uint32_t read_misses;
    uint32_t writes;
    uint32_t write_hits;
    uint32_t write_misses;
    uint32_t writebacks;
    uint32_t prefetches;
    uint32_t demand_memory_traffic;
    uint32_t prefetch_memory_traffic;
    uint32_t l2_reads_prefetch;
    uint32_t l2_read_misses_prefetch;
    uint32_t l2_read_misses_demand;

    void update_lru(uint32_t set_index, uint32_t way);
    uint32_t get_set_index(uint32_t address) const;
    uint32_t get_tag(uint32_t address) const;
    uint32_t calculate_no_of_sets(uint32_t CACHE_SIZE, uint32_t CACHE_ASSOC, uint32_t CACHE_BLOCKSIZE) const;
    bool cache_read(uint32_t address, int access_id, bool is_prefetch);
    bool cache_write(uint32_t address, int access_id);
    void insert_to_cache(uint32_t address, const CacheLine& line);
    bool is_in_cache(uint32_t address);
    bool is_in_prefetch_buffer(uint32_t address);
    void fetch_to_prefetch_buffer(uint32_t address);
    void fetch_to_cache(uint32_t address, bool is_prefetch);
};

#endif // CACHE_H