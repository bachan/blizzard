namespace blizzard
{

template <typename T>
inline static const T& min(const T& a, const T& b)
{
	return a < b ? a : b;
}

template<int data_size>
inline mem_chunk<data_size>::mem_chunk() : sz(0), current(0), can_expand(false), next(0)
{
	page[0] = 0;
}

template<int data_size>
inline mem_chunk<data_size>::~mem_chunk()
{
	reset();
}

template<int data_size>
inline void mem_chunk<data_size>::insert_page()
{
	mem_chunk<data_size> * new_chunk = new mem_chunk<data_size>;
	new_chunk->next = this->next;
	this->next = new_chunk;
}

template<int data_size>
inline void mem_chunk<data_size>::reset()
{
	if (next)
	{
		delete next;
		next = 0;
	}

	sz = 0;
	current = 0;
	can_expand = false;
}

template<int data_size>
inline size_t mem_chunk<data_size>::page_size()const
{
	return data_size;
}

template<int data_size>
inline const void * mem_chunk<data_size>::get_data()const
{
	return page;
}

template<int data_size>
inline void * mem_chunk<data_size>::get_data()
{
	return page;
}

template<int data_size>
inline size_t mem_chunk<data_size>::get_data_size()const
{
	return sz;
}

template<int data_size>
inline size_t& mem_chunk<data_size>::marker()
{
	return current;
}

template<int data_size>
inline size_t mem_chunk<data_size>::get_total_data_size()const
{
	return sz + (next ? next->get_total_data_size() : 0);
}


template<int data_size>
inline typename blizzard::mem_chunk<data_size> * mem_chunk<data_size>::get_next()const
{
	return next;
}

template<int data_size>
inline bool mem_chunk<data_size>::set_expand(bool exp)
{
	bool old = can_expand;
	can_expand = exp;

	return old;
}

template<int data_size>
inline size_t mem_chunk<data_size>::append_data(const void * data, size_t data_sz)
{
	mem_chunk<data_size> * cur_page = this;

	size_t total_to_write = data_sz;

	if (next)
	{
		while (cur_page->next)
		{
			cur_page = cur_page->next;
		}

		return cur_page->append_data(data, data_sz);
	}
	else
	{
		const uint8_t * p = static_cast<const uint8_t*>(data);

		while (total_to_write)
		{
			if (cur_page->sz >= cur_page->page_size())
			{
				if (cur_page->can_expand)
				{
					cur_page->insert_page();
					cur_page = cur_page->next;

					cur_page->set_expand(true);
				}
				else
				{
					return data_sz - total_to_write;
				}
			}

			size_t to_write = min<size_t>(data_sz, cur_page->page_size() - cur_page->sz);

			memcpy(cur_page->page + cur_page->sz, p, to_write);
			p += to_write;
			data_sz -= to_write;

			total_to_write -= to_write;
			cur_page->sz += to_write;
		}

		return data_sz;
	}
}

template<int data_size>
inline void mem_chunk<data_size>::print()
{
	int ch_n = 0;
	mem_chunk<data_size> * cur_page = this;
	while (cur_page)
	{
		char u[1024];
		memset(u, 0, 1024);
		memcpy(u, cur_page->page, cur_page->sz);
		cur_page = cur_page->next;
		ch_n++;
	}
}

template<int data_size>
inline bool mem_chunk<data_size>::write_to_fd(int fd, bool& can_write, bool& want_write, bool& wreof)
{
	bool iswr = false;

	mem_chunk<data_size> * cur_page = this;

	while (cur_page->next && (cur_page->current == cur_page->get_data_size()))
	{
		cur_page = cur_page->next;
	}

	while (true)
	{
		ssize_t to_write = cur_page->get_data_size() - cur_page->current;
		if (to_write)
		{
			ssize_t wr = write(fd, cur_page->page + cur_page->current, to_write);
			if (-1 == wr)
			{
				switch (errno)
				{
				case EPIPE:
					wreof = true;
				case EAGAIN:
					can_write = false;
					return false;
				case EINTR:
					break;
				default:
					log_err(errno, "chunk/write error");
					can_write = false;
					break;
				}
			}
			else if (wr)
			{
				iswr = true;
				cur_page->current += wr;
				if (wr < to_write)
				{
					can_write = false;
					return iswr;
				}
			}
			else
			{
				can_write = false;
				wreof = true;
				return false;
			}
		}
		else
		{
			want_write = false;
			return iswr;
		}

		if (cur_page->next)
		{
			cur_page = cur_page->next;
		}
		else
		{
			want_write = false;
			return iswr;
		}
	}
}

template<int data_size>
inline bool mem_chunk<data_size>::read_from_fd(int fd, bool& can_read, bool& want_read, bool& rdeof)
{
	mem_chunk<data_size> * cur_page = this;
		
	bool failed = true;

	while (cur_page->next && (cur_page->page_size() == cur_page->get_data_size()))
	{
		cur_page = cur_page->next;
	}

	while (true)
	{
		ssize_t to_read = cur_page->page_size() - cur_page->get_data_size();
		if (to_read)
		{
			ssize_t rd = read(fd, cur_page->page + cur_page->get_data_size(), to_read);
			if (-1 == rd)
			{
				if (EAGAIN == errno)
				{
					can_read = false;

					return true;
				}
				else if (EINTR != errno)
				{
					log_err(errno, "chunk/read error");
					can_read = false;
					return true;
				}
				else
				{
					log_debug("chunk/read: EINTR");
				}
			}
			else if (rd)
			{
				cur_page->sz += rd;
								
				failed = false;
				
				if (rd < to_read)
				{
					can_read = false;

					return true;
				}
			}
			else
			{
				log_debug("chunk/read: got EOF");
				can_read = false;
				rdeof = true;

				return true;
			}
		}
		else if (can_expand)
		{
			insert_page();
		}
		else if (failed)
		{
				   return false;
		}

		if (cur_page->next)
		{
			cur_page = cur_page->next;
		}
		else
		{
			want_read = false;

			return true;
		}
	}
}

inline mem_block::mem_block(size_t sz) : page(0), page_capacity(0), page_sz(0), current(0)
{
	resize(sz);
}

inline mem_block::~mem_block()
{
	resize(0);
}

inline size_t mem_block::size()const
{
	return page_sz;
}

inline size_t mem_block::capacity()const
{
	return page_capacity;
}

inline const void * mem_block::get_data()const
{
	return page;
}

inline void * mem_block::get_data()
{
	return page;
}

inline size_t& mem_block::marker()
{
	return current;
}

inline void mem_block::resize(size_t sz)
{
	if (page)
	{
		delete[] page;

		page = 0;
		page_capacity = 0;
	}

	if (sz)
	{
		page = new uint8_t[sz];

		page_capacity = sz;
	}

	page_sz = 0;
	current = 0;
}

inline void mem_block::reset()
{
	page_sz = 0;
	current = 0;
}


inline void mem_block::print()
{
	enum {SZZ = 65536};
	char u[SZZ];

	memset(u, 0, SZZ);
	memcpy(u, page, page_sz);

	log_debug("mem_block(mr:%d,sz:%d,cap:%d)'%s'",
		(int) current,
		(int) page_sz,
		(int) page_capacity, u);
}

inline size_t mem_block::append_data(const void * data, size_t data_sz)
{
	size_t to_write = min<size_t>(data_sz, capacity() - size());
	memcpy(page + size(), data, to_write);
	page_sz += to_write;

	return to_write;
}

inline bool mem_block::write_to_fd(int fd, bool& can_write, bool& want_write, bool& wreof)
{
	while (true)
	{
		ssize_t to_write = size() - marker();
		if (to_write)
		{
			ssize_t wr = write(fd, page + current, to_write);
			if (-1 == wr)
			{
				switch (errno)
				{
				case EPIPE:
					wreof = true;
				case EAGAIN:
					can_write = false;
					return false;
				case EINTR:
					log_debug("block/write: EINTR");
					break;
				default:
					log_err(errno, "block/write error");
					can_write = false;
					break;
				}
			}
			else if (wr)
			{
				current += wr;

				if (wr < to_write)
				{
					can_write = false;
				}
				else
				{
					want_write = false;
				}

				return true;

			}
			else
			{
				can_write = false;
				wreof = true;
				return false;
			}
		}
		else
		{
			want_write = false;
			return false;
		}
	}
}

inline bool mem_block::read_from_fd(int fd, bool& can_read, bool& want_read, bool& rdeof)
{
	while (true)
	{
		ssize_t to_read = capacity() - size();
		if (to_read)
		{
			ssize_t rd = read(fd, page + size(), to_read);
			if (-1 == rd)
			{
				if (EAGAIN == errno)
				{
					can_read = false;

					return false;
				}
				else if (EINTR != errno)
				{
					log_err(errno, "block/read error");
					can_read = false;
					return false;
				}
				else
				{
					log_debug("block/read: EINTR");
				}
			}
			else if (rd)
			{
				page_sz += rd;

				if (rd < to_read)
				{
					can_read = false;
				}
				else
				{
					want_read = false;
				}

				return true;
			}
			else
			{
				can_read = false;
				rdeof = true;

				return false;
			}
		}
		else
		{
			want_read = false;

			return false;
		}
	}
}

}

