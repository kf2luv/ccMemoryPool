#include "ThreadCache.h"

void* cc_memory_pool::ThreadCache::allocate(size_t bytes)
{
	assert(bytes > 0);

	// ���ݶ�����ԣ�ѡ���Ӧ��freeList
	size_t idx = SizeClass::index(bytes);
	FreeList& freeList = _freeLists[idx];

	if (freeList.empty())
	{
		// freeList�����ڴ������ȥCentralCache��һЩobj��freeList��
		fetchObjFromCentralCache(bytes, freeList);
	}
	return freeList.pop();
}

void cc_memory_pool::ThreadCache::deallocate(void* obj, size_t bytes)
{
	assert(obj != nullptr);
	assert(bytes > 0);

	// ���ݶ�����ԣ�ѡ���Ӧ��free����
	size_t idx = SizeClass::index(bytes);
	FreeList& freeList = _freeLists[idx];

	// ���ڴ�������free���� 
	freeList.push(obj);

	if (freeList.size() >= freeList.batchSize()) 
	{
		// ���free�����е�obj̫�࣬�黹һ���ָ��²�
		CentralCache::getInstance()->releaseObjToCentralCache(freeList, bytes);
	}
}

// threadCache��centrealCache��������ȡ�ڴ����
// bytes: ����ȡ���ڴ����Ĵ�С
// free_list����ȡ�����ڴ����ͳһ�ŵ�������
void cc_memory_pool::ThreadCache::fetchObjFromCentralCache(size_t bytes, FreeList& freeList)
{
	// һ�δ�CentralCache�ö��ٸ�obj?
	// ̫�٣�Ƶ��ȥ�ã�����������
	// ̫�࣬�ò��꣬�˷ѣ�����̻߳�Ҫ��
	// ģ��tcmalloc��������ʼ�㷨

	// 1.��������뻺���ȡ���ڴ�������fetchNum

	// ������ֵ
	size_t threshold = SizeClass::numFetchObj(bytes);
	// ����ʼ�㷨
	size_t fetchNum = 0;
	// ��freeList������ȡ�ڴ����
	if (freeList.batchSize() < threshold)
	{
		fetchNum = freeList.batchSize();
		freeList.batchSize()++;
	}
	else
	{
		fetchNum = threshold;
	}


	// 2.��ȡCentralCache����
	void* begin = nullptr;
	void* end = nullptr;

	size_t actualNum = CentralCache::getInstance()->getRangeObj(begin, end, fetchNum, bytes);
	assert(actualNum > 0);

	freeList.pushRange(begin, end, actualNum);
}

