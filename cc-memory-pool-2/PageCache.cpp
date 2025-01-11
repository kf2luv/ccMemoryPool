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

// ��PageCache����һ������kҳ��span
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
		// ���������ҳ�������������ڸ���page��span�����Զ�����в��
		for (size_t i = k + 1; i <= NPAGELISTS; i++)
		{
			if (!_spanLists[i].empty())
			{
				// ��_spanLists[i]��ȡһ��span���������еĴ���ڴ���в��
				// ��ͷ��kҳ������ʣ�µĹҵ���Ӧӳ���λ��

				Span* nSpan = _spanLists[i].popFront();//�ҵ��Ĵ�ռ�nSpan
				Span* kSpan = new Span;//nSpanǰk��ҳ�г�����kSpan

				kSpan->_pageID = nSpan->_pageID;
				kSpan->_npage = k;
				//���зֳ����Ĵ��ռ���ص�kSpan��free������
				/*kSpan->_freeList = FreeList(nSpan->_freeList.head());*/
				//2024/12/17�����ң�PageCache�����pageID��npage��ʶspanָ��Ŀռ�
				//�����߼��� CentralCache�зֿռ��ִ��

				//��kSpan��ÿһҳpageId����kSpan����ӳ��
				for (PageID id = kSpan->_pageID; id < kSpan->_pageID + kSpan->_npage; id++)
				{
					_idToSpanMap[id] = kSpan;
				}

				//nSpan�Ĵ��ռ������ƶ�kҳ
				nSpan->_pageID += k;
				nSpan->_npage -= k;
				//��ӳ��nSpan����ʼҳ�ź�ĩβҳ��
				_idToSpanMap[nSpan->_pageID] = nSpan;
				_idToSpanMap[nSpan->_pageID + nSpan->_npage - 1] = nSpan;

				//�зֺ�Ŀռ���ص���Ӧ��spanList
				_spanLists[nSpan->_npage].pushFront(nSpan);

				return kSpan;
			}
		}
	}

	// �ߵ��������Page Cache�ڲ�������Ч��span����Ҫ��ϵͳ���ѣ�����һ��128page�Ŀռ䣨�������㱻��֣�
	void* memory = systemAlloc(NPAGELISTS);//����128page�ռ����ʼ��ַ
	assert(memory);

	Span* bigSpan = new Span;
	//�޸�span�Ŀռ䣬ҳ����ҳ��
	bigSpan->_freeList = FreeList(memory);
	bigSpan->_npage = NPAGELISTS;

	uintptr_t address = reinterpret_cast<uintptr_t>(memory);
	bigSpan->_pageID = address >> PAGE_SHIFT;

	_spanLists[NPAGELISTS].pushFront(bigSpan);
	// ��ʱֻ�������˴�Span������Ҫ��ֺ󷵻ظ��û������ô���
	return newSpan(k);
}

cc_memory_pool::Span* cc_memory_pool::PageCache::mapObjToSpan(void* obj)
{
	assert(obj);
	// 1.����obj��Ӧ��ҳ��
	PageID pageID = (PageID)obj >> PAGE_SHIFT;

	// 2.ͨ��ӳ�䣬�ҵ���Ӧ��Span
	auto it = _idToSpanMap.find(pageID);
	if (it != _idToSpanMap.end()) {
		return it->second;
	}
	else {
		//�����ܷ�����������ڴ���������pageIDһ������
		assert(false);
		return nullptr;
	}
}


void cc_memory_pool::PageCache::releaseSpanToPageCache(Span* span)
{
	bool canMergePrev = true;
	bool canMergeNext = true;

	//����span�Ĵ��ռ䣬���Խ��кϲ�
	//��ǰ��ͺ��������ҳ���޷��ϲ�ʱ�������ڡ�����ʹ�á��ϲ������128KB�����ϲ�����
	while (canMergePrev || canMergeNext)
	{
		if (canMergePrev)
		{
			//��ǰ�������ҳ
			auto prevIt = _idToSpanMap.find(span->_pageID - 1);

			//���ǰ����ҳ���ڣ�������ϲ�����
			Span* prevSpan = (prevIt != _idToSpanMap.end()) ? prevIt->second : nullptr;
			if (prevSpan && !prevSpan->_isUsing && (span->_npage + prevSpan->_npage) <= NPAGELISTS)
			{
				//��prevSpan�ϲ���span��
				span->_pageID = prevSpan->_pageID;
				span->_npage += prevSpan->_npage;
				//��prevSpan�Ӷ�Ӧ�� Ͱ �н⿪
				_spanLists[prevSpan->_npage].erase(prevSpan);
				//�ͷ�prevSpan
				delete prevSpan;
			}
			else
			{
				canMergePrev = false;
			}
		}

		if (canMergeNext)
		{
			//�Һ��������ҳ
			auto nextIt = _idToSpanMap.find(span->_pageID + span->_npage);

			//���������ҳ���ڣ�������ϲ�����
			Span* nextSpan = (nextIt != _idToSpanMap.end()) ? nextIt->second : nullptr;
			if (nextSpan && !nextSpan->_isUsing && (span->_npage + nextSpan->_npage) <= NPAGELISTS)
			{
				//��nextSpan�ϲ���span��
				span->_npage += nextSpan->_npage;
				//��nextSpan�Ӷ�Ӧ�� Ͱ �н⿪
				_spanLists[nextSpan->_npage].erase(nextSpan);
				//�ͷ�nextSpan
				delete nextSpan;
			}
			else
			{
				canMergeNext = false;
			}
		}
	}

	//�ϲ�����(Ҳ����û�кϲ�)����span�ҵ���Ӧ��Ͱ��
	_spanLists[span->_npage].pushFront(span);
}