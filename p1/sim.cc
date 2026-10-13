#include "cache.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>

void print_cache_content(const Cache& cache) {
    std::cout << "Cache contents:\n";
    for (uint32_t i = 0; i < cache.num_sets; ++i) {
        std::stringstream ss;
        ss << "Set " << std::setw(4) << std::setfill('0') << i << ": ";
        const auto& lru_tracker = cache.lru_tracker;
        const auto& cache_data = cache.cache;
        for (int j = cache.associativity - 1; j >= 0; --j) {
            uint32_t way = lru_tracker[i][j];
            if (cache_data[i][way].valid) {
                ss << "  " << std::hex << std::setw(6) << std::setfill('0') << cache_data[i][way].tag << std::dec;
                if (cache_data[i][way].dirty) {
                    ss << "D";
                } else {
                    ss << " ";
                }
            } else {
                ss << "        ";
            }
        }
        std::cout << ss.str() << "\n";
    }
}

void display_stats(const Cache& l1, const Cache* l2) {
    std::cout << "Cache stats:\n";
    std::cout << "a. L1 reads:                   " << l1.reads << "\n";
    std::cout << "b. L1 read misses:             " << l1.read_misses << "\n";
    std::cout << "c. L1 writes:                  " << l1.writes << "\n";
    std::cout << "d. L1 write misses:            " << l1.write_misses << "\n";
    std::cout << "e. L1 miss rate:               " << std::fixed << std::setprecision(4) 
              << static_cast<float>(l1.misses) / (l1.reads + l1.writes) << "\n";
    std::cout << "f. L1 writebacks:              " << l1.writebacks << "\n";
    std::cout << "g. L1 prefetches:              " << l1.prefetches << "\n";
    std::cout << "h. L2 reads (demand):          " << (l2 ? l2->reads - l2->l2_reads_prefetch : 0) << "\n";
    std::cout << "i. L2 read misses (demand):    " << (l2 ? l2->l2_read_misses_demand : 0) << "\n";
    std::cout << "j. L2 reads (prefetch):        " << (l2 ? l2->l2_reads_prefetch : 0) << "\n";
    std::cout << "k. L2 read misses (prefetch):  " << (l2 ? l2->l2_read_misses_prefetch : 0) << "\n";
    std::cout << "l. L2 writes:                  " << (l2 ? l2->writes : 0) << "\n";
    std::cout << "m. L2 write misses:            " << (l2 ? l2->write_misses : 0) << "\n";
    std::cout << "n. L2 miss rate:               " << std::fixed << std::setprecision(4) 
              << (l2 && l2->reads > 0 ? static_cast<float>(l2->misses) / l2->reads : 0.0000) << "\n";
    std::cout << "o. L2 writebacks:              " << (l2 ? l2->writebacks : 0) << "\n";
    std::cout << "p. L2 prefetches:              " << (l2 ? l2->prefetches : 0) << "\n";
    std::cout << "q. memory traffic:             " << (l2 ? l2->demand_memory_traffic + l2->prefetch_memory_traffic : 
                                                           l1.demand_memory_traffic + l1.prefetch_memory_traffic) << "\n";
}

int main(int argc, char* argv[]) {
    if (argc != 9) {
        std::cerr << "Usage: " << argv[0] << " <L1_size> <L1_assoc> <L1_block_size> <L2_size> <L2_assoc> <L2_block_size> <pref_n> <trace_file>\n";
        return 1;
    }
    uint32_t l1_block_size = std::stoul(argv[1]);
    uint32_t l2_block_size = l1_block_size;
    uint32_t l1_size = std::stoul(argv[2]);
    uint32_t l1_assoc = std::stoul(argv[3]);
    uint32_t l2_size = std::stoul(argv[4]);
    uint32_t l2_assoc = std::stoul(argv[5]);
    uint32_t pref_n = std::stoul(argv[6]);
    uint32_t pref_m = std::stoul(argv[7]);
    std::string trace_file_name = argv[8];

    Prefetcher l1_prefetcher(pref_n, pref_m);
    Prefetcher l2_prefetcher(pref_n, pref_m);

    Cache* l2_cache = nullptr;
    if (l2_size > 0) {
        l2_cache = new Cache(l2_size, l2_block_size, l2_assoc, nullptr, &l2_prefetcher);
    }

    Cache l1_cache(l1_size, l1_block_size, l1_assoc, l2_cache, &l1_prefetcher);

    std::ifstream trace_file(trace_file_name);
    if (!trace_file) {
        std::cerr << "Error: Cannot open trace file " << trace_file_name << "\n";
        delete l2_cache;
        return 1;
    }

    std::string line;
    int access_id = 0;
    while (std::getline(trace_file, line)) {
        char access_type;
        uint32_t address;
        std::istringstream iss(line);
        if (iss >> access_type >> std::hex >> address) {
            l1_cache.access(address, access_type, access_id++);
        }
    }

    print_cache_content(l1_cache);
    std::cout << "\n";
    if (l2_cache) {
        print_cache_content(*l2_cache);
        std::cout << "\n";
    }

    display_stats(l1_cache, l2_cache);

    delete l2_cache;
    return 0;
}