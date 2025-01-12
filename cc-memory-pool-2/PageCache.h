#pragma once
#include "common.h"
#include "ObjectPool.h"

namespace cc_memory_pool
{
	class PageCache
	{
	public:
		static PageCache* getInstance();

		std::mutex& getMutex();

		// 根据指定的页数，创建一个新的span
		Span* newSpan(size_t npage);

		// 通过内存对象的地址，得到其所属的Span
		Span* mapObjToSpan(void* obj);

		// 将空闲的span归还给 Page Cache
		void releaseSpanToPageCache(Span* span);

	private:
		PageCache() {}
		PageCache(const PageCache& other) = delete;
		PageCache& operator=(const PageCache& other) = delete;

		SpanList _spanLists[NPAGELISTS + 1]; // 直接映射，下标i表示：链表中每个span维护i个page, 
											 // PageCache里仅靠span的pageID和npage标识其维护的页空间.
		std::mutex _mtx;                     // PageCache整体的锁
		static PageCache _instance;

		std::unordered_map<PageID, Span*> _idToSpanMap;//哈希表（页号 映射 页所在的span）

		static ObjectPool<Span> spanPool;
	};
}
