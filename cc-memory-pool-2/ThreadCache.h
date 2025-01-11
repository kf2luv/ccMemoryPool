#pragma once
#include "CentralCache.h"
#include "common.h"

namespace cc_memory_pool
{
    class ThreadCache
    {
    public:
        // 分配空间
        void* allocate(size_t bytes);
        // 返回空间
        void deallocate(void* obj, size_t bytes);

    private:
        // 从CentralCache中拿到大小为bytes的内存对象到freeList中
        void fetchObjFromCentralCache(size_t bytes, FreeList& freeList);

    private:
        FreeList _freeLists[NFREELISTS];
    };
}
