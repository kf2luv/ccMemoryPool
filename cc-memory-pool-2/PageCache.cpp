#include "PageCache.h"

cc_memory_pool::PageCache cc_memory_pool::PageCache::_instance;

cc_memory_pool::PageCache* cc_memory_pool::PageCache::getInstance()
{
	return &_instance;
}

std::mutex& cc_memory_pool::PageCache::getMutex() 
{
	return _mtx;
}

// 从PageCache申请一个存有k页的span
cc_memory_pool::Span* cc_memory_pool::PageCache::newSpan(size_t k)
{
	assert(k > 0 && k <= NPAGELISTS);

	SpanList& spanList = _spanLists[k];

	if (!spanList.empty())
	{
		return spanList.popFront();
	}
	else
	{
		// 遍历后面的页链表，如果后面存在更大page的span，可以对其进行拆分
		for (size_t i = k + 1; i <= NPAGELISTS; i++)
		{
			if (!_spanLists[i].empty())
			{
				// 从_spanLists[i]中取一个span，并对其中的大块内存进行拆分
				// 从头切k页下来，剩下的挂到对应映射的位置

				Span* nSpan = _spanLists[i].popFront();//找到的大空间nSpan
				Span* kSpan = new Span;//nSpan前k个页切出来的kSpan

				kSpan->_pageID = nSpan->_pageID;
				kSpan->_npage = k;
				//将切分出来的大块空间挂载到kSpan的free链表中
				/*kSpan->_freeList = FreeList(nSpan->_freeList.head());*/
				//2024/12/17：不挂，PageCache里仅靠pageID和npage标识span指向的空间
				//挂载逻辑由 CentralCache切分空间后执行

				//将kSpan的每一页pageId都与kSpan建立映射
				for (PageID id = kSpan->_pageID; id < kSpan->_pageID + kSpan->_npage; id++)
				{
					_idToSpanMap[id] = kSpan;
				}

				//nSpan的大块空间往后移动k页
				nSpan->_pageID += k;
				nSpan->_npage -= k;
				//将映射nSpan的起始页号和末尾页号
				_idToSpanMap[nSpan->_pageID] = nSpan;
				_idToSpanMap[nSpan->_pageID + nSpan->_npage - 1] = nSpan;

				//切分后的空间挂载到对应的spanList
				_spanLists[nSpan->_npage].pushFront(nSpan);

				return kSpan;
			}
		}
	}

	// 走到这里代表Page Cache内不存在有效的span，需要向系统（堆）申请一个128page的空间（后续方便被拆分）
	void* memory = systemAlloc(NPAGELISTS);//返回128page空间的起始地址
	assert(memory);

	Span* bigSpan = new Span;
	//修改span的空间，页数和页号
	bigSpan->_freeList = FreeList(memory);
	bigSpan->_npage = NPAGELISTS;

	uintptr_t address = reinterpret_cast<uintptr_t>(memory);
	bigSpan->_pageID = address >> PAGE_SHIFT;

	_spanLists[NPAGELISTS].pushFront(bigSpan);
	// 此时只是新增了大Span，还需要拆分后返回给用户，复用代码
	return newSpan(k);
}

cc_memory_pool::Span* cc_memory_pool::PageCache::mapObjToSpan(void* obj)
{
	assert(obj);
	// 1.计算obj对应的页号
	PageID pageID = (PageID)obj >> PAGE_SHIFT;

	// 2.通过映射，找到对应的Span
	auto it = _idToSpanMap.find(pageID);
	if (it != _idToSpanMap.end()) {
		return it->second;
	}
	else {
		//不可能发生，申请的内存计算出来的pageID一定存在
		assert(false);
		return nullptr;
	}
}


void cc_memory_pool::PageCache::releaseSpanToPageCache(Span* span)
{
	bool canMergePrev = true;
	bool canMergeNext = true;

	//对于span的大块空间，尝试进行合并
	//当前面和后面的相邻页都无法合并时（不存在、正在使用、合并后大于128KB），合并结束
	while (canMergePrev || canMergeNext)
	{
		if (canMergePrev)
		{
			//找前面的相邻页
			auto prevIt = _idToSpanMap.find(span->_pageID - 1);

			//如果前相邻页存在，且满足合并条件
			Span* prevSpan = (prevIt != _idToSpanMap.end()) ? prevIt->second : nullptr;
			if (prevSpan && !prevSpan->_isUsing && (span->_npage + prevSpan->_npage) <= NPAGELISTS)
			{
				//将prevSpan合并到span中
				span->_pageID = prevSpan->_pageID;
				span->_npage += prevSpan->_npage;
				//将prevSpan从对应的 桶 中解开
				_spanLists[prevSpan->_npage].erase(prevSpan);
				//释放prevSpan
				delete prevSpan;
			}
			else
			{
				canMergePrev = false;
			}
		}

		if (canMergeNext)
		{
			//找后面的相邻页
			auto nextIt = _idToSpanMap.find(span->_pageID + span->_npage);

			//如果后相邻页存在，且满足合并条件
			Span* nextSpan = (nextIt != _idToSpanMap.end()) ? nextIt->second : nullptr;
			if (nextSpan && !nextSpan->_isUsing && (span->_npage + nextSpan->_npage) <= NPAGELISTS)
			{
				//将nextSpan合并到span中
				span->_npage += nextSpan->_npage;
				//将nextSpan从对应的 桶 中解开
				_spanLists[nextSpan->_npage].erase(nextSpan);
				//释放nextSpan
				delete nextSpan;
			}
			else
			{
				canMergeNext = false;
			}
		}
	}

	//合并结束(也可能没有合并)，将span挂到对应的桶上
	_spanLists[span->_npage].pushFront(span);
}