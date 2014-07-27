#ifndef __BLIZZARD_MEM_CHUNK_HPP__
#define __BLIZZARD_MEM_CHUNK_HPP__

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <coda/error.hpp>
#include "config.hpp"

namespace blizzard {

template<int data_size>
class mem_chunk
{
	uint8_t	page[data_size + 1];
	size_t sz;
	size_t current;
	bool can_expand;

	mem_chunk<data_size>* next;
	void insert_page();

public:
	mem_chunk();
	~mem_chunk();

	size_t page_size()const;
	const void * get_data()const;
	void * get_data();
	size_t get_data_size()const;
	size_t& marker();
	size_t get_total_data_size()const;
	mem_chunk<data_size> * get_next()const;

	bool set_expand(bool exp);

	void reset();

	size_t append_data(const void * data, size_t data_sz);

	bool write_to_fd(int fd, bool& can_write, bool& want_write, bool& wreof);
	bool read_from_fd(int fd, bool& can_read, bool& want_read, bool& rdeof);

	void print();
};

class mem_block
{
	uint8_t *	page;
	size_t		page_capacity;
	size_t		page_sz;
	size_t		current;

public:

	explicit mem_block(size_t max_sz = 0);
	~mem_block();

	size_t size()const;
	size_t capacity()const;
	const void * get_data()const;
	void * get_data();
	size_t& marker();

	void resize(size_t max_sz = 0);

	void reset();

	size_t append_data(const void * data, size_t data_sz);

	bool write_to_fd(int fd, bool& can_write, bool& want_write, bool& wreof);
	bool read_from_fd(int fd, bool& can_read, bool& want_read, bool& rdeof);

	void print();
};

}

#include "mem_chunk.tcc"

#endif /* __BLIZZARD_MEM_CHUNK_HPP__ */
