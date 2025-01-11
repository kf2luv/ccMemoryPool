#include "ThreadCache.h"

void* cc_memory_pool::ThreadCache::allocate(size_t bytes)
{
	assert(bytes > 0);

	// 根据对齐策略，选择对应的freeList
	size_t idx = SizeClass::index(bytes);
	FreeList& freeList = _freeLists[idx];

	if (freeList.empty())
	{
		// freeList中无内存对象，先去CentralCache拿一些obj到freeList中
		fetchObjFromCentralCache(bytes, freeList);
	}
	return freeList.pop();
}

void cc_memory_pool::ThreadCache::deallocate(void* obj, size_t bytes)
{
	assert(obj != nullptr);
	assert(bytes > 0);

	// 根据对齐策略，选择对应的free链表
	size_t idx = SizeClass::index(bytes);
	FreeList& freeList = _freeLists[idx];

	// 将内存对象插入free链表 
	freeList.push(obj);

	if (freeList.size() >= freeList.maxFetchNum()) 
	{
		// 如果free链表中的obj太多，归还一部分给下层
		CentralCache::getInstance()->releaseObjToCentralCache(freeList, bytes);
	}
}

// threadCache从centrealCache中批量获取内存对象
// bytes: 待获取的内存对象的大小
// free_list：获取到的内存对象，统一放到这里面
void cc_memory_pool::ThreadCache::fetchObjFromCentralCache(size_t bytes, FreeList& freeList)
{
	// 一次从CentralCache拿多少个obj?
	// 太少，频繁去拿，锁竞争问题
	// 太多，用不完，浪费，别的线程还要用
	// 模仿tcmalloc采用慢开始算法

	// 1.计算从中央缓存获取的内存对象个数fetchNum

	// 计算阈值
	size_t threshold = SizeClass::numFetchObj(bytes);
	// 慢开始算法
	size_t fetchNum = 0;
	if (freeList.maxFetchNum() < threshold)
	{
		fetchNum = freeList.maxFetchNum();
		freeList.maxFetchNum()++;
	}
	else
	{
		fetchNum = threshold;
	}


	// 2.获取CentralCache对象
	void* begin = nullptr;
	void* end = nullptr;

	size_t actualNum = CentralCache::getInstance()->getRangeObj(begin, end, fetchNum, bytes);
	assert(actualNum > 0);

	freeList.pushRange(begin, end, actualNum);
}

