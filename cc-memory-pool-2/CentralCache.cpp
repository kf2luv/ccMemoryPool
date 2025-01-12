#include "CentralCache.h"

cc_memory_pool::CentralCache cc_memory_pool::CentralCache::_instance;

cc_memory_pool::CentralCache* cc_memory_pool::CentralCache::getInstance()
{
	return &_instance;
}

// 获取一个有效的span
cc_memory_pool::Span* cc_memory_pool::CentralCache::getOneEffectiveSpan(SpanList& spanList, size_t bytes)
{
	// 1.在Central Cache中映射的spanList中，查找是否存在有效Span
	for (Span* span = spanList.begin(); span != spanList.end(); span = span->_next)
	{
		if (!span->_freeList.empty())
		{ // 当前span是有效的（不判断obj数量吗？）
			return span;
		}
	}
	//(unlock) spanList中无有效span，此时要去下层申请，在这之前先将桶锁释放，允许其它线程向该spanList“释放内存”
	spanList.getMutex().unlock();

	// 2.找不到有效span(spanList为空 or spanList中的span obj都不足)，就要去下层 Page Cache 申请一个新的span，供上层使用
	size_t npage = SizeClass::numFetchPage(bytes);
	Span* span = nullptr;

	//(lock) 对页缓存进行操作，加整体锁，如果此时有多个线程找不到有效的span，那只会有一个线程去底层申请
	//可能存在一个线程去底层申请完后，另一个线程再去申请，这种情况可以发生，一次不够多申请一点，后面申请的次数就少了
	{
		std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
		span = PageCache::getInstance()->newSpan(npage);
		span->_isUsing = true; //标记该span为正在使用，Page Cache不能将其合并
	}
	//(unlock) 切割空间的逻辑不用加锁，因为span是当前线程的私有对象

	// 3.对span分配到的大块内存进行切分，每一块的大小为size
	size_t size = SizeClass::roundUp(bytes);
	char* begin = (char*)(span->_pageID << PAGE_SHIFT);
	char* end = (char*)(begin + (span->_npage << PAGE_SHIFT));
	char* cur = begin;
	while (cur + 2 * size <= end)
	{
		FreeList::nextObj(cur) = cur + size;
		cur = (char*)FreeList::nextObj(cur);
	}
	FreeList::nextObj(cur) = nullptr;
	// 将切分后的自由链表挂载到span上!!!
	span->_freeList = FreeList(begin);
	// 标识该span的切出来的每一个内存对象的大小
	span->_objSize = size;

	// 4.此时span的自由链表中已经包含充足的内存对象，将其插入spanList桶中
	//(lock) 对中央缓存的某个spanList进行操作，先加桶锁
	spanList.getMutex().lock();
	spanList.pushFront(span);

	assert(span);
	return span;
}

// begin和end：输出型参数，获取的内存对象（链表形式）的头结点为begin，尾节点为end
// fetchNum：本次从中央缓存获取的内存对象个数
// bytes：获取的内存对象大小
size_t cc_memory_pool::CentralCache::getRangeObj(void*& begin, void*& end, size_t fetchNum, size_t bytes)
{
	// 由bytes映射桶，找到对应的spanList
	size_t idx = SizeClass::index(bytes);
	SpanList& spanList = _spanLists[idx];

	// 从spanList获取一个有效的Span
	//(lock) 对中央缓存的某个spanList进行操作，先加桶锁
	spanList.getMutex().lock();

	Span* effectiveSpan = getOneEffectiveSpan(spanList, bytes);
	assert(effectiveSpan);

	// 从effectiveSpan中获取fetchNum个对象
	size_t actualNum = effectiveSpan->_freeList.popRange(begin, end, fetchNum);
	effectiveSpan->_useCount += actualNum;

	spanList.getMutex().unlock();

	return actualNum;
}


void cc_memory_pool::CentralCache::releaseObjToCentralCache(FreeList& freeList, size_t bytes)
{
	//1.从free链表中取走一个批量的内存对象
	size_t idx = SizeClass::index(bytes);
	void* begin = nullptr;
	void* end = nullptr;
	size_t actualNum = freeList.popRange(begin, end, freeList.batchSize());

	//2.遍历每一个内存对象obj，依次归还给Central Cache
	void* curObj = begin;

	//要对CentralCache的桶操作，加桶锁
	_spanLists[idx].getMutex().lock();

	while (curObj)
	{
		void* nextObj = FreeList::nextObj(curObj);

		// 1) 找到内存对象curObj所属的span
		Span* span = PageCache::getInstance()->mapObjToSpan(curObj);

		// 2) 将curObj归还给span
		span->_freeList.push(curObj);
		span->_useCount--;

		if (span->_useCount == 0) 
		{
			//合并, 从CentralCache中移除
			//(先从桶中解开Span，再设置_isUsing = false，防止引发野指针)
			_spanLists[idx].erase(span);
			span->_freeList = nullptr;
			span->_isUsing = false;
			span->_objSize = 0;

			_spanLists[idx].getMutex().unlock();
			//归还给下一层（在此之前先解开桶锁，让其它线程可以访问这个桶）
			{
				//对页缓存进行操作，加整体锁
				std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
				PageCache::getInstance()->releaseSpanToPageCache(span);
			}
			_spanLists[idx].getMutex().lock();
		}

		// 3.查找下一个内存对象obj
		curObj = nextObj;
	}

	_spanLists[idx].getMutex().unlock();
}