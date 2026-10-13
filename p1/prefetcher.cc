#include "prefetcher.h"
#include <algorithm>

Prefetcher::Prefetcher(uint32_t prefetch_distance, uint32_t prefetch_degree)
    : prefetch_distance(prefetch_distance), prefetch_degree(prefetch_degree), history_size(10) {
    stream_buffer.reserve(prefetch_degree);
    access_history.reserve(history_size);
}

bool Prefetcher::should_prefetch(uint32_t address) const {
    return is_sequential_access();
}

std::vector<uint32_t> Prefetcher::get_next_prefetch_addresses(uint32_t address, uint32_t block_size) const {
    std::vector<uint32_t> prefetch_addresses;
    if (is_sequential_access()) {
        uint32_t stride = 0;
        if (access_history.size() >= 2) {
            stride = access_history.back() - access_history[access_history.size() - 2];
        }
        for (uint32_t i = 1; i <= prefetch_degree; ++i) {
            prefetch_addresses.push_back(address + i * stride);
        }
    }
    return prefetch_addresses;
}

std::vector<uint32_t> Prefetcher::get_stream_buffer() const {
    return stream_buffer;
}

void Prefetcher::add_to_stream_buffer(uint32_t address) {
    if (stream_buffer.size() >= prefetch_degree) {
        stream_buffer.erase(stream_buffer.begin());
    }
    stream_buffer.push_back(address);
}

void Prefetcher::update_access_history(uint32_t address) {
    if (access_history.size() >= history_size) {
        access_history.erase(access_history.begin());
    }
    access_history.push_back(address);
}

bool Prefetcher::is_sequential_access() const {
    if (access_history.size() < 3) {
        return false;
    }
    
    uint32_t stride = access_history[1] - access_history[0];
    for (size_t i = 2; i < access_history.size(); ++i) {
        if (access_history[i] - access_history[i-1] != stride) {
            return false;
        }
    }
    return true;
}