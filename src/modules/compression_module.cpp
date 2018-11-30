#include "compression_module.hpp"

#include <cerrno>
#include <cstring>

#define __DEBUG
#include "common/debug.hpp"

static int compress(void * data, unsigned char ** compressed_data){
	if(error_bound == 0){
		// lossless (zstd)
	}
	else{
		// lossy (sz)
		*compressed_data = SZ_compress_args(data_type, data, &compressed_size, error_bound_mode, 0, error_bound, 0, dimensions[4], dimensions[3], dimensions[2], dimensions[1], dimensions[0]);
	}
	return VELOC_SUCCESS;
}

static int decomress(unsigned char * compressed_data, void ** data){
	if(error_bound == 0){
		// lossless (zstd)
	}
	else{
		// lossy (sz)
		*data = SZ_decompress(data_type, compressed_data, compressed_size, dimensions[4], dimensions[3], dimensions[2], dimensions[1], dimensions[0]);
	}
	return VELOC_SUCCESS;
}

compression_module_t::compression_module_t(const config_t &c) : cfg(c) {

    if (!cfg.get_optional("persistent_interval", interval)) {
	INFO("Persistence interval not specified, every checkpoint will be persisted");
	interval = 0;
    }
	// get dimensions & error bound
	dimensions = std::vector<int>(5, 0);
	data_size = 1;
    if (!cfg.get_optional("r1", dimensions[2])){
    	dimensions[2] = 0;
    }
    else data_size *= dimensions[2];
    if (!cfg.get_optional("r2", dimensions[1])){
    	dimensions[1] = 0;
    }
    else data_size *= dimensions[1];
    if (!cfg.get_optional("r3", dimensions[0])){
    	dimensions[0] = 0;
    }
    else data_size *= dimensions[0];
    if(data_size == 0) ERROR("data size is 0, check config file");
    if (!cfg.get_optional("data_type", data_type)){
    	data_type = SZ_DOUBLE;
    	INFO("Set data type to default (DOUBLE)");
    }
    if (!cfg.get_optional("error_mode", error_bound_mode)){
    	error_bound_mode = REL;
    	INFO("Set error bound mode to default (REL)");
    }
    if (!cfg.get_optional("error_bound", error_bound)){
    	error_bound = 0;
    	INFO("Set error bound to default (0)");
    }
    int ret = SZ_Init(NULL);
    if (ret){
	ERROR("SZ initialization failure, error code: " << ret << "; falling back to no compression");
	use_sz = false;
	}
    else {
	INFO("SZ successfully initialized");
	use_sz = true;
    }
}

transfer_module_t::~transfer_module_t() {
    SZ_Finalize();
}

int transfer_module_t::process_command(const command_t &c) {
    std::string local = c.filename(cfg.get("scratch"));

    switch (c.command) {
    case command_t::INIT:
	if (interval < 0)
	    return VELOC_SUCCESS;
	last_timestamp[c.unique_id] = std::chrono::system_clock::now() + std::chrono::seconds(interval);
	return VELOC_SUCCESS;
	
    case command_t::TEST:
	return VELOC_SUCCESS;
	
    case command_t::CHECKPOINT:
	if (interval < 0) 
	    return VELOC_SUCCESS;
	if (interval > 0) {
	    auto t = std::chrono::system_clock::now();
	    if (t < last_timestamp[c.unique_id])
		return VELOC_SUCCESS;
	    else
		last_timestamp[c.unique_id] = t + std::chrono::seconds(interval);
	}
	if(use_sz){
		int fd = open(local, O_RDWR);
		struct stat st;
		fstat(fd, &st);
		void * data = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		unsigned char * compressed_data = NULL;
		compress(data, &compressed_data);
		memcpy(data, compressed_data, compressed_size);
		munmap(data, st.st_size);
		close(fd);
		free(compressed_data);
	}
	return VELOC_SUCCESS;

    case command_t::RESTART:
	if (interval < 0)
	    return VELOC_SUCCESS;
	if (use_sz){
		int fd = open(local, O_RDWR);
		struct stat st;
		fstat(fd, &st);
		compressed_size = st.st_size
		void * compressed_data = mmap(NULL, data_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		void * data = NULL;
		decompress((unsigned char *)compressed_data, &data);
		memcpy(compressed_data, data, data_size);
		munmap(data, data_size);
		close(fd);
		free(data);
	}
	return VELOC_SUCCESS;
	
    default:
	return VELOC_SUCCESS;
    }
}
