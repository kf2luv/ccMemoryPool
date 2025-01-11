#include "CentralCache.h"

cc_memory_pool::CentralCache cc_memory_pool::CentralCache::_instance;

cc_memory_pool::CentralCache* cc_memory_pool::CentralCache::getInstance()
{
	return &_instance;
}

// ��ȡһ����Ч��span
cc_memory_pool::Span* cc_memory_pool::CentralCache::getOneEffectiveSpan(SpanList& spanList, size_t bytes)
{
	// 1.��Central Cache��ӳ���spanList�У������Ƿ������ЧSpan
	for (Span* span = spanList.begin(); span != spanList.end(); span = span->_next)
	{
		if (!span->_freeList.empty())
		{ // ��ǰspan����Ч�ģ����ж�obj�����𣿣�
			return span;
		}
	}
	//(unlock) spanList������Чspan����ʱҪȥ�²����룬����֮ǰ�Ƚ�Ͱ���ͷţ����������߳����spanList���ͷ��ڴ桱
	spanList.getMutex().unlock();

	// 2.�Ҳ�����Чspan(spanListΪ�� or spanList�е�span obj������)����Ҫȥ�²� Page Cache ����һ���µ�span�����ϲ�ʹ��
	size_t npage = SizeClass::numFetchPage(bytes);
	Span* newSpan = nullptr;

	//(lock) ��ҳ������в��������������������ʱ�ж���߳��Ҳ�����Ч��span����ֻ����һ���߳�ȥ�ײ�����
	//���ܴ���һ���߳�ȥ�ײ����������һ���߳���ȥ���룬����������Է�����һ�β���������һ�㣬��������Ĵ���������
	{
		std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
		newSpan = PageCache::getInstance()->newSpan(npage);
		newSpan->_isUsing = true; //��Ǹ�spanΪ����ʹ�ã�Page Cache���ܽ���ϲ�
	}
	//(unlock) �и�ռ���߼����ü�������ΪnewSpan�ǵ�ǰ�̵߳�˽�ж���

	// ��newSpan���䵽�Ĵ���ڴ�����з֣�ÿһ��Ĵ�СΪsize
	size_t size = SizeClass::roundUp(bytes);
	char* begin = (char*)(newSpan->_pageID << PAGE_SHIFT);
	char* end = (char*)(begin + (newSpan->_npage << PAGE_SHIFT));
	char* cur = begin;
	while (cur + 2 * size <= end)
	{
		FreeList::nextObj(cur) = cur + size;
		cur = (char*)FreeList::nextObj(cur);
	}
	FreeList::nextObj(cur) = nullptr;
	// ���зֺ������������ص�newSpan��!!!
	newSpan->_freeList = FreeList(begin);

	// ��ʱnewSpan�������������Ѿ�����������ڴ���󣬽������spanListͰ��

	//(lock) �����뻺���ĳ��spanList���в������ȼ�Ͱ��
	spanList.getMutex().lock();
	spanList.pushFront(newSpan);

	assert(newSpan);
	return newSpan;
}

// begin��end������Ͳ�������ȡ���ڴ����������ʽ����ͷ���Ϊbegin��β�ڵ�Ϊend
// fetchNum�����δ����뻺���ȡ���ڴ�������
// bytes����ȡ���ڴ�����С
size_t cc_memory_pool::CentralCache::getRangeObj(void*& begin, void*& end, size_t fetchNum, size_t bytes)
{
	// ��bytesӳ��Ͱ���ҵ���Ӧ��spanList
	size_t idx = SizeClass::index(bytes);
	SpanList& spanList = _spanLists[idx];

	// ��spanList��ȡһ����Ч��Span
	//(lock) �����뻺���ĳ��spanList���в������ȼ�Ͱ��
	spanList.getMutex().lock();

	Span* effectiveSpan = getOneEffectiveSpan(spanList, bytes);
	assert(effectiveSpan);

	// ��effectiveSpan�л�ȡfetchNum������
	size_t actualNum = effectiveSpan->_freeList.popRange(begin, end, fetchNum);
	effectiveSpan->_useCount += actualNum;

	spanList.getMutex().unlock();

	return actualNum;
}


void cc_memory_pool::CentralCache::releaseObjToCentralCache(FreeList& freeList, size_t bytes)
{
	//1.��free������ȡ��һ���������ڴ����
	size_t idx = SizeClass::index(bytes);
	void* begin = nullptr;
	void* end = nullptr;
	size_t actualNum = freeList.popRange(begin, end, freeList.batchSize());

	//2.����ÿһ���ڴ����obj�����ι黹��Central Cache
	void* curObj = begin;

	//Ҫ��CentralCache��Ͱ��������Ͱ��
	_spanLists[idx].getMutex().lock();

	while (curObj)
	{
		void* nextObj = FreeList::nextObj(curObj);

		// 1) �ҵ��ڴ����curObj������span
		Span* span = PageCache::getInstance()->mapObjToSpan(curObj);

		// 2) ��curObj�黹��span
		span->_freeList.push(curObj);
		span->_useCount--;

		if (span->_useCount == 0) 
		{
			//�ϲ�, ��CentralCache���Ƴ�
			//(�ȴ�Ͱ�н⿪Span��������_isUsing = false����ֹ����Ұָ��)
			_spanLists[idx].erase(span);
			span->_freeList = nullptr;
			span->_isUsing = false;

			_spanLists[idx].getMutex().unlock();
			//�黹����һ�㣨�ڴ�֮ǰ�Ƚ⿪Ͱ�����������߳̿��Է������Ͱ��
			{
				//��ҳ������в�������������
				std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
				PageCache::getInstance()->releaseSpanToPageCache(span);
			}
			_spanLists[idx].getMutex().lock();
		}

		// 3.������һ���ڴ����obj
		curObj = nextObj;
	}

	_spanLists[idx].getMutex().unlock();
}