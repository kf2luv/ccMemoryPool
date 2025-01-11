#pragma once
#include "ThreadCache.h"

namespace cc_memory_pool
{
    thread_local ThreadCache* pTLSThreadCache = nullptr;

    void* ccAlloc(size_t size)
    {
        if (pTLSThreadCache == nullptr)
        {
            pTLSThreadCache = new ThreadCache;
        }
        std::cout << std::this_thread::get_id() << ": " << pTLSThreadCache << std::endl;
        return pTLSThreadCache->allocate(size);
    }

    void ccFree(void* obj, size_t size)
    {
        pTLSThreadCache->deallocate(obj, size);
    }
}
