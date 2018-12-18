#ifndef __COMPRESSION_MODULE_HPP
#define __COMPRESSION_MODULE_HPP

#include "common/config.hpp"
#include "common/command.hpp"
#include "common/status.hpp"

#include <chrono>
#include <deque>
#include <map>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <vector>

#include "sz.h"

class compression_module_t {
    const config_t &cfg;
    // bool use_axl = false;
    // axl_xfer_t axl_type;
    int interval;//, max_versions;
    std::map<int, std::chrono::system_clock::time_point> last_timestamp;
    // typedef std::map<std::string, std::deque<int> > checkpoint_history_t;
    // std::map<int, checkpoint_history_t> checkpoint_history;
    bool use_sz = false;
    int data_type = SZ_DOUBLE;
    int error_bound_mode = REL;
    size_t data_size = 0;
    size_t compressed_size = 0;
    double error_bound = 0;
    std::vector<int> dimensions;
    int compress(void * data, unsigned char ** compressed);
    int decompress(unsigned char * compressed, void ** data);
public:
    compression_module_t(const config_t &c);
    ~compression_module_t();
    int process_command(const command_t &c);
};

#endif //__COMPRESSION_MODULE_HPP