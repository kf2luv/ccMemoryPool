#include "common.h"

cc_memory_pool::FreeList::FreeList(void* head)
	: _freeList(head), _batchSize(1)
{
}

void*& cc_memory_pool::FreeList::head() {
	return _freeList;
}

//头插
void cc_memory_pool::FreeList::push(void* obj)
{
	assert(obj != nullptr);
	nextObj(obj) = _freeList;
	_freeList = obj;

	_size++;
}
void cc_memory_pool::FreeList::pushRange(void* begin, void* end, size_t pushNum)//左闭右闭
{
	assert(begin && end);
	nextObj(end) = _freeList;
	_freeList = begin;

	_size += pushNum;
}

//头删
void* cc_memory_pool::FreeList::pop()
{
	if (_freeList == nullptr)
	{
		return nullptr;
	}
	void* ret = _freeList;
	_freeList = nextObj(_freeList);
	_size--;
	return ret;
}
//返回的链表，end的下一个节点是null
size_t cc_memory_pool::FreeList::popRange(void*& begin, void*& end, size_t popNum)//左闭右闭
{ 
	// pop n个节点，如果不够，就全部pop出去
	void* cur = head();
	int cnt = 1;
	while (nextObj(cur) != nullptr && cnt < popNum)
	{
		cur = nextObj(cur);
		cnt++;
	}

	begin = head();
	head() = nextObj(cur);

	nextObj(cur) = nullptr;
	end = cur;

	_size -= cnt;
	return cnt;
}

// 能从中央缓存获取的最大obj数量
size_t& cc_memory_pool::FreeList::batchSize()
{
	return _batchSize;
}

bool cc_memory_pool::FreeList::empty()
{
	return _freeList == nullptr;
}

void*& cc_memory_pool::FreeList::nextObj(void* obj)
{ // 获取内存对象的下一个对象的地址
	return *(void**)obj;
}

size_t cc_memory_pool::FreeList::size()
{
	return _size;
}