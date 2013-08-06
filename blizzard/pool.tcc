template <typename T, int objects_per_page>
inline pool<T, objects_per_page>::page::page() : next(0), data(0), free_node(data)
{
	data = new T[objects_per_page];
	free_node = data;
}

template <typename T, int objects_per_page>
inline pool<T, objects_per_page>::page::~page()
{
	delete[] data;

	if (next)
	{
		delete next;
	}
}

template <typename T, int objects_per_page>
inline bool pool<T, objects_per_page>::page::full()const
{
	return free_node >= data + objects_per_page;
}

template <typename T, int objects_per_page>
inline void pool<T, objects_per_page>::page::attach(pool<T, objects_per_page>::page * pg)
{
	next = pg;
}

template <typename T, int objects_per_page>
inline T * pool<T, objects_per_page>::page::allocate()
{
	if (full())
	{
		 return 0;
	}
	else
	{
		T * ret = free_node;

		free_node++;

		return ret;
	}
}

template <typename T, int objects_per_page>
inline pool<T, objects_per_page>::pool() : root(0), pages_num(0), objects_num(0)
{
	root = new page;

	pages_num = 1;
	objects_num = 0;

	free_nodes.reserve(objects_per_page);
}

template <typename T, int objects_per_page>
inline pool<T, objects_per_page>::~pool()
{
	delete root;
}

template <typename T, int objects_per_page>
inline u_int32_t pool<T, objects_per_page>::allocated_pages()const
{
	return pages_num;
}

template <typename T, int objects_per_page>
inline u_int32_t pool<T, objects_per_page>::allocated_objects()const
{
	return objects_num;
}

template <typename T, int objects_per_page>
inline u_int32_t pool<T, objects_per_page>::allocated_bytes()const
{
	return objects_num * sizeof(T);
}

template <typename T, int objects_per_page>
inline size_t pool<T, objects_per_page>::page_size()const
{
	return objects_per_page * sizeof(T);
}

template <typename T, int objects_per_page>
inline T * pool<T, objects_per_page>::allocate()
{
	T * ret_ptr = 0;

	if (!free_nodes.empty())
	{
		ret_ptr = free_nodes.pop();
	}
	else
	{
		ret_ptr = root->allocate();

		if (0 == ret_ptr)
		{
			page * n = root;
			root = new page;
			root->attach(n);

			pages_num++;

			ret_ptr = root->allocate();
		}
	}

	objects_num++;

	return ret_ptr;
}

template <typename T, int objects_per_page>
inline void pool<T, objects_per_page>::free(T * elem)
{
	free_nodes.push(elem);
	objects_num--;
}
