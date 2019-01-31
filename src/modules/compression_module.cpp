#include "compression_module.hpp"

#include <fstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#define __DEBUG
#include "common/debug.hpp"

size_t compression_module_t::compress(void * data, int data_type, size_t data_size, unsigned char ** compressed_data){
	size_t compressed_size = 0;
	if(error_bound == 0){
		// lossless (zstd)
		// compressed_size = sz_lossless_compress(confparams_cpr->losslessCompressor, confparams_cpr->gzipMode, (unsigned char *) data, data_size, compressed_data);
		compressed_size = sz_lossless_compress(1, confparams_cpr->gzipMode, (unsigned char *) data, data_size, compressed_data);
	}
	else{
		// lossy (sz)
		*compressed_data = SZ_compress_args(data_type, data, &compressed_size, error_bound_mode, 0, error_bound, 0, dimensions[4], dimensions[3], dimensions[2], dimensions[1], dimensions[0]);
	}
	return compressed_size;
}

void compression_module_t::decompress(unsigned char * compressed_data, int data_type, size_t compressed_size, size_t data_size, void ** data){
	if(error_bound == 0){
		// lossless (zstd)
		size_t size = sz_lossless_decompress(1, compressed_data, (unsigned long)compressed_size, (unsigned char **) data, data_size);
	}
	else{
		// lossy (sz)
		*data = SZ_decompress(data_type, compressed_data, compressed_size, dimensions[4], dimensions[3], dimensions[2], dimensions[1], dimensions[0]);
	}
}

compression_module_t::compression_module_t(const config_t &c) : cfg(c) {

    if (!cfg.get_optional("persistent_interval", interval)) {
	INFO("Persistence interval not specified, every checkpoint will be persisted");
	interval = 0;
    }
	// get error bound
    //if (!cfg.get_optional("data_type", data_type)){
    	data_type = SZ_DOUBLE;
    	INFO("Set data type to default (DOUBLE)");
    //}
    //if (!cfg.get_optional("error_mode", error_bound_mode)){
    	error_bound_mode = REL;
    	INFO("Set error bound mode to default (REL)");
    //}
    //if (!cfg.get_optional("error_bound", error_bound)){
    	error_bound = 1e-8;
    	INFO("Set error bound to default (0)");
    //}
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

compression_module_t::~compression_module_t() {
    SZ_Finalize();
}

int compression_module_t::process_command(const command_t &c) {
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
		int fd = open(local.c_str(), O_RDWR);
		struct stat st;
		fstat(fd, &st);
		void * data = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		// compress region by region
		unsigned char * data_pos = (unsigned char *) data;
		size_t num_regions = *((size_t *) data_pos);
		INFO("num_regions: " << num_regions);
		data_pos += sizeof(size_t);
		std::vector<size_t> variable_sizes = std::vector<size_t>(num_regions, 0);
		for(size_t i=0; i<num_regions; i++){
			// skip id
			data_pos += sizeof(int);
			if(i) variable_sizes[i] = *((size_t *) data_pos) / sizeof(double);
                        else variable_sizes[i] = *((size_t *) data_pos) / sizeof(int);
			INFO("size " << i << ": " << variable_sizes[i]);
			data_pos += sizeof(size_t);
		}
		// record the starting point of data blocks
		unsigned char * data_block_pos = data_pos;
		for(size_t i=0; i<num_regions; i++){
			if(variable_sizes[i] > 100){
				// here we assume that compressed file size is usually less than original size - sizeof(size_t)
				unsigned char * compressed = NULL;
				dimensions[0] = variable_sizes[i];
				size_t compressed_size = compress(data_pos, SZ_DOUBLE, variable_sizes[i]*sizeof(double), &compressed);
				memcpy(data_block_pos, &compressed_size, sizeof(size_t));
				data_block_pos += sizeof(size_t);
				INFO("data block pos " << data_block_pos - (unsigned char *) data << ", compressed_size " << compressed_size);
				memcpy(data_block_pos, compressed, compressed_size);
				data_block_pos += compressed_size;
				data_pos += variable_sizes[i] * sizeof(double);
				free(compressed);
			}
			else{
				// in heatdis, skipped var is int
				data_block_pos += variable_sizes[i] * sizeof(int);
				data_pos += variable_sizes[i] * sizeof(int);
			}
		}
		INFO("compressed size: " << data_block_pos - (unsigned char *) data << ", total size " << data_pos - (unsigned char *) data);
		munmap(data, st.st_size);
		ftruncate(fd, data_block_pos - (unsigned char *) data);
		close(fd);
	}
	return VELOC_SUCCESS;

    case command_t::DECOMPRESS:
	INFO("decompress command with interval " << interval);
	if (interval < 0)
	    return VELOC_SUCCESS;
	if (use_sz){
		size_t origin_data_size = 0;
		std::ifstream f;
    		f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    		try {
			f.open(local.c_str(), std::ios_base::in | std::ios_base::binary);
			size_t num_regions;
			f.read((char *)&num_regions, sizeof(size_t));
			origin_data_size += sizeof(size_t);
			for(size_t i=0; i<num_regions; i++){
				int id;
				size_t region_size;
				f.read((char *)&id, sizeof(int));
				f.read((char *)&region_size, sizeof(size_t));
				origin_data_size += sizeof(int) + sizeof(size_t) + region_size;
			}
		}catch (std::ifstream::failure &e) {
			ERROR("cannot read checkpoint file " << local.c_str() << ", reason: " << e.what());
		}
		f.close();
		int fd = open(local.c_str(), O_RDWR);
		struct stat st;
		fstat(fd, &st);
		if((size_t)st.st_size == origin_data_size){
			close(fd);
			return VELOC_SUCCESS;
		}
		ftruncate(fd, origin_data_size);
		void * data = mmap(NULL, origin_data_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		unsigned char * compressed_data_buf = (unsigned char *) malloc(st.st_size);
		memcpy(compressed_data_buf, data, st.st_size);
		unsigned char * comp_data_pos = compressed_data_buf;
		size_t num_regions = *((size_t *) comp_data_pos);
		comp_data_pos += sizeof(size_t);
		std::vector<size_t> variable_sizes = std::vector<size_t>(num_regions, 0);
		for(size_t i=0; i<num_regions; i++){
			// skip id
			comp_data_pos += sizeof(int);
			if(i) variable_sizes[i] = *((size_t *) comp_data_pos) / sizeof(double);
			else variable_sizes[i] = *((size_t *) comp_data_pos) / sizeof(int);
			INFO("size " << i << ": " << variable_sizes[i]);
			comp_data_pos += sizeof(size_t);
		}
		unsigned char * comp_data_block_pos = comp_data_pos;
		unsigned char * data_pos = (unsigned char *) data + (comp_data_pos - compressed_data_buf);
		for(size_t i=0; i<num_regions; i++){
			INFO("decompress " << i << "-th variable");
			if(variable_sizes[i] > 100){
				dimensions[0] = variable_sizes[i];
				size_t compressed_size = 0;
				memcpy(&compressed_size, comp_data_block_pos, sizeof(size_t));
				comp_data_block_pos += sizeof(size_t);
				void * decompressed_buf = NULL;
				size_t origin_size = variable_sizes[i]*sizeof(double);
				//INFO("compressed size " << compressed_size << ", origin size " << origin_size);
				decompress(comp_data_block_pos, data_type, compressed_size, origin_size, &decompressed_buf);
				memcpy(data_pos, decompressed_buf, origin_size);
				comp_data_block_pos += compressed_size;
				data_pos += origin_size;
				free(decompressed_buf);
			}
			else{
				// in heatdis, skipped var is int
				comp_data_block_pos += variable_sizes[i] * sizeof(int);
				data_pos += variable_sizes[i] * sizeof(int);
			}
		}
		free(compressed_data_buf);
		munmap(data, origin_data_size);
		close(fd);
	}
	return VELOC_SUCCESS;
	
    default:
	return VELOC_SUCCESS;
    }
}
