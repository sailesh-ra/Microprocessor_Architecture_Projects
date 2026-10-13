#include "cache.h"
#include <algorithm>
#include <iostream>

Cache::PrefetchBuffer::PrefetchBuffer(uint32_t size) : size(size), head(0), tail(0) {
    buffer.resize(size, {false, false, 0});
}

Cache::Cache(uint32_t size, uint32_t block_size, uint32_t associativity, Cache* l2, Prefetcher* prefetcher)
    : cache_size(size), block_size(block_size), associativity(associativity), l2(l2), prefetcher(prefetcher),
      prefetch_buffer(32), // Adjust size as needed
      hits(0), misses(0), reads(0), read_hits(0), read_misses(0), writes(0), write_hits(0), write_misses(0),
      writebacks(0), prefetches(0), demand_memory_traffic(0), prefetch_memory_traffic(0),
      l2_reads_prefetch(0), l2_read_misses_prefetch(0), l2_read_misses_demand(0) {
    
    num_sets = calculate_no_of_sets(size, associativity, block_size);
    cache.resize(num_sets, std::vector<CacheLine>(associativity, {false, false, 0}));
    lru_tracker.resize(num_sets, std::vector<uint32_t>(associativity));

    for (uint32_t i = 0; i < num_sets; ++i) {
        for (uint32_t j = 0; j < associativity; ++j) {
            lru_tracker[i][j] = j;
        }
    }
}

bool Cache::access(uint32_t address, char type, int access_id, bool is_prefetch) {
    if (cache_size == 0 || associativity == 0) {
        if (l2) {
            return l2->access(address, type, access_id, is_prefetch);
        }
        is_prefetch ? prefetch_memory_traffic++ : demand_memory_traffic++;
        return false;
    }

    if (type == 'r' || type == 'R') {
        return cache_read(address, access_id, is_prefetch);
    } else if (type == 'w' || type == 'W') {
        return cache_write(address, access_id);
    }

    return false;
}

bool Cache::cache_read(uint32_t address, int access_id, bool is_prefetch) {
    reads++;
    uint32_t set_index = get_set_index(address);
    uint32_t tag = get_tag(address);

    // Check cache
    for (uint32_t i = 0; i < associativity; ++i) {
        if (cache[set_index][i].valid && cache[set_index][i].tag == tag) {
            hits++;
            read_hits++;
            update_lru(set_index, i);
            return true;
        }
    }

    // Check prefetch buffer
    for (uint32_t i = 0; i < prefetch_buffer.size; ++i) {
        if (prefetch_buffer.buffer[i].valid && prefetch_buffer.buffer[i].tag == tag) {
            insert_to_cache(address, prefetch_buffer.buffer[i]);
            prefetch_buffer.buffer[i].valid = false;
            hits++;
            read_hits++;
            return true;
        }
    }

    // Handle miss
    misses++;
    read_misses++;
    fetch_to_cache(address, is_prefetch);
    
    // Trigger prefetching
    if (prefetcher && !is_prefetch && prefetcher->should_prefetch(address)) {
        std::vector<uint32_t> prefetch_addresses = prefetcher->get_next_prefetch_addresses(address, block_size);
        for (const auto &prefetch_address : prefetch_addresses) {
            if (!is_in_cache(prefetch_address) && !is_in_prefetch_buffer(prefetch_address)) {
                fetch_to_prefetch_buffer(prefetch_address);
                prefetch_memory_traffic++;
            }
        }
    }

    return false;
}

bool Cache::cache_write(uint32_t address, int access_id) {
    writes++;
    uint32_t set_index = get_set_index(address);
    uint32_t tag = get_tag(address);

    // Check cache
    for (uint32_t i = 0; i < associativity; ++i) {
        if (cache[set_index][i].valid && cache[set_index][i].tag == tag) {
            hits++;
            write_hits++;
            cache[set_index][i].dirty = true;
            update_lru(set_index, i);
            return true;
        }
    }

    // Handle miss
    misses++;
    write_misses++;
    fetch_to_cache(address, false);
    cache[set_index][lru_tracker[set_index].back()].dirty = true;
    
    return false;
}

void Cache::update_lru(uint32_t set_index, uint32_t way) {
    auto &lru = lru_tracker[set_index];
    auto it = std::find(lru.begin(), lru.end(), way);
    if (it != lru.end()) {
        lru.erase(it);
        lru.push_back(way);
    }
}

uint32_t Cache::get_set_index(uint32_t address) const {
    return (address / block_size) % num_sets;
}

uint32_t Cache::get_tag(uint32_t address) const {
    return address / (block_size * num_sets);
}

uint32_t Cache::calculate_no_of_sets(uint32_t CACHE_SIZE, uint32_t CACHE_ASSOC, uint32_t CACHE_BLOCKSIZE) const {
    return CACHE_SIZE / (CACHE_ASSOC * CACHE_BLOCKSIZE);
}

void Cache::insert_to_cache(uint32_t address, const CacheLine& line) {
    uint32_t set_index = get_set_index(address);
    uint32_t line_to_replace = lru_tracker[set_index].front();
    
    // Handle eviction and writeback
    if (cache[set_index][line_to_replace].valid && cache[set_index][line_to_replace].dirty) {
        writebacks++;
        if (l2) {
            l2->access((cache[set_index][line_to_replace].tag * num_sets + set_index) * block_size, 'w', 0, false);
        } else {
            demand_memory_traffic++;
        }
    }

    cache[set_index][line_to_replace] = line;
    update_lru(set_index, line_to_replace);
}

bool Cache::is_in_cache(uint32_t address) {
    uint32_t set_index = get_set_index(address);
    uint32_t tag = get_tag(address);
    for (const auto& line : cache[set_index]) {
        if (line.valid && line.tag == tag) {
            return true;
        }
    }
    return false;
}

bool Cache::is_in_prefetch_buffer(uint32_t address) {
    uint32_t tag = get_tag(address);
    for (const auto& line : prefetch_buffer.buffer) {
        if (line.valid && line.tag == tag) {
            return true;
        }
    }
    return false;
}

void Cache::fetch_to_prefetch_buffer(uint32_t address) {
    CacheLine new_line = {true, false, get_tag(address)};
    prefetch_buffer.buffer[prefetch_buffer.tail] = new_line;
    prefetch_buffer.tail = (prefetch_buffer.tail + 1) % prefetch_buffer.size;
    if (prefetch_buffer.tail == prefetch_buffer.head) {
        prefetch_buffer.head = (prefetch_buffer.head + 1) % prefetch_buffer.size;
    }
}

void Cache::fetch_to_cache(uint32_t address, bool is_prefetch) {
    if (l2) {
        bool l2_hit = l2->access(address, 'r', 0, is_prefetch);
        if (!l2_hit) {
            if (!is_prefetch) {
                l2->l2_read_misses_demand++;
                demand_memory_traffic++;
            } else {
                l2->l2_read_misses_prefetch++;
                prefetch_memory_traffic++;
            }
        }
    } else {
        is_prefetch ? prefetch_memory_traffic++ : demand_memory_traffic++;
    }

    CacheLine new_line = {true, false, get_tag(address)};
    insert_to_cache(address, new_line);

    if (prefetcher && !is_prefetch) {
        prefetcher->update_access_history(address);
    }
}