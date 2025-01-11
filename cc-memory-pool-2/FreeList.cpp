#include "common.h"

cc_memory_pool::FreeList::FreeList(void* head)
	: _freeList(head), _batchSize(1)
{
}

void*& cc_memory_pool::FreeList::head() {
	return _freeList;
}

//ͷ��
void cc_memory_pool::FreeList::push(void* obj)
{
	assert(obj != nullptr);
	nextObj(obj) = _freeList;
	_freeList = obj;

	_size++;
}
void cc_memory_pool::FreeList::pushRange(void* begin, void* end, size_t pushNum)//����ұ�
{
	assert(begin && end);
	nextObj(end) = _freeList;
	_freeList = begin;

	_size += pushNum;
}

//ͷɾ
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
//���ص�����end����һ���ڵ���null
size_t cc_memory_pool::FreeList::popRange(void*& begin, void*& end, size_t popNum)//����ұ�
{ 
	// pop n���ڵ㣬�����������ȫ��pop��ȥ
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

// �ܴ����뻺���ȡ�����obj����
size_t& cc_memory_pool::FreeList::batchSize()
{
	return _batchSize;
}

bool cc_memory_pool::FreeList::empty()
{
	return _freeList == nullptr;
}

void*& cc_memory_pool::FreeList::nextObj(void* obj)
{ // ��ȡ�ڴ�������һ������ĵ�ַ
	return *(void**)obj;
}

size_t cc_memory_pool::FreeList::size()
{
	return _size;
}