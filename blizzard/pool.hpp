#ifndef _VAR_OBJECT_ALLOCATOR___
#define _VAR_OBJECT_ALLOCATOR___

#include <sys/types.h>
#include "pool_stack.hpp"

namespace pool_ns
{

/*

Memory allocator for fixed size structures

*/
template <typename T, int objects_per_page = 65536>
class pool
{
protected:

    struct page
    {
        page * next;

        T * data;
        T * free_node;

        page();
        ~page();

        bool full()const;

        T * allocate();

        void attach(page * p);
    };

    stack<T*> free_nodes;

    page * root;

    u_int16_t pages_num;
    u_int32_t objects_num;

public:
    pool();
    ~pool();

    u_int32_t allocated_pages()const;
    u_int32_t allocated_objects()const;
    u_int32_t allocated_bytes()const;

    size_t    page_size()const;

    T * allocate();
    void free(T * elem);
};

#include "pool.tcc"

}

#endif //_VAR_OBJECT_ALLOCATOR___
