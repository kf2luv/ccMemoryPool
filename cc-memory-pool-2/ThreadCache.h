#pragma once
#include "CentralCache.h"
#include "common.h"

namespace cc_memory_pool
{
    class ThreadCache
    {
    public:
        // ����ռ�
        void* allocate(size_t bytes);
        // ���ؿռ�
        void deallocate(void* obj, size_t bytes);

    private:
        // ��CentralCache���õ���СΪbytes���ڴ����freeList��
        void fetchObjFromCentralCache(size_t bytes, FreeList& freeList);

    private:
        FreeList _freeLists[NFREELISTS];
    };
}
