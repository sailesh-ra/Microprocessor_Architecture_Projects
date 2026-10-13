#ifndef PREFETCHER_H
#define PREFETCHER_H

#include <vector>
#include <cstdint>

class Prefetcher {
public:
    Prefetcher(uint32_t prefetch_distance, uint32_t prefetch_degree);

    bool should_prefetch(uint32_t address) const;
    std::vector<uint32_t> get_next_prefetch_addresses(uint32_t address, uint32_t block_size) const;
    std::vector<uint32_t> get_stream_buffer() const;
    void add_to_stream_buffer(uint32_t address);
    void update_access_history(uint32_t address);
    bool is_sequential_access() const;

    uint32_t prefetch_distance;
    uint32_t prefetch_degree;
    std::vector<uint32_t> stream_buffer;
    std::vector<uint32_t> access_history;
    uint32_t history_size;
};

#endif // PREFETCHER_H