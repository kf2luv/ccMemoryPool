#pragma once
#include "ThreadCache.h"

namespace cc_memory_pool
{
	thread_local ThreadCache* pTLSThreadCache = nullptr;

	void* ccAlloc(size_t size)
	{
		if (size > MAX_MEM_SIZE)
		{
			//将size按页对齐
			size_t align = SizeClass::roundUp(size);
			size_t kPage = align >> PAGE_SHIFT;
			Span* kSpan = nullptr;
			//向PageCache申请一个kpage页的span
			{
				std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
				kSpan = PageCache::getInstance()->newSpan(kPage);
			}
			//将kSpan的页号转换为起始地址
			void* addr = (void*)(kSpan->_pageID << PAGE_SHIFT);
			 
			return addr;
		}

		if (pTLSThreadCache == nullptr)
		{
			pTLSThreadCache = new ThreadCache;
		}
		//std::cout << std::this_thread::get_id() << ": " << pTLSThreadCache << std::endl;
		return pTLSThreadCache->allocate(size);
	}

	void ccFree(void* obj, size_t size)
	{
		if (size > MAX_MEM_SIZE)
		{
			//找到obj所属的span
			Span* span = PageCache::getInstance()->mapObjToSpan(obj);
			{
				//对页缓存进行操作，加整体锁
				std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
				PageCache::getInstance()->releaseSpanToPageCache(span);
			}
		}
		else 
		{
			pTLSThreadCache->deallocate(obj, size);
		}
	}
}
