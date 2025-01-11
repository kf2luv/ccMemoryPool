#include "common.h"

cc_memory_pool::SpanList::SpanList()
{
	_dummy = new Span;
	_dummy->_next = _dummy;
	_dummy->_prev = _dummy;
}

// 用于遍历链表
cc_memory_pool::Span* cc_memory_pool::SpanList::begin()
{
	return _dummy->_next;
}

cc_memory_pool::Span* cc_memory_pool::SpanList::end()
{
	return _dummy;
}

bool cc_memory_pool::SpanList::empty()
{
	return _dummy->_next == _dummy;
}

void cc_memory_pool::SpanList::insert(Span* pos, Span* span)
{
	assert(pos != nullptr);
	assert(span != nullptr);

	span->_next = pos->_next;
	span->_prev = pos;

	span->_next->_prev = span;
	span->_prev->_next = span;
}

void cc_memory_pool::SpanList::erase(Span* pos)
{
	assert(pos != nullptr);
	assert(pos != _dummy);

	pos->_prev->_next = pos->_next;
	pos->_next->_prev = pos->_prev;

	pos->_prev = nullptr;
	pos->_next = nullptr;
}

cc_memory_pool::Span* cc_memory_pool::SpanList::popFront()
{
	assert(!empty());
	Span* front = _dummy->_next;
	erase(front);
	return front;
}

void cc_memory_pool::SpanList::pushFront(Span* span)
{
	assert(_dummy != nullptr);
	insert(_dummy, span);
}

std::mutex& cc_memory_pool::SpanList::getMutex()
{
	return _mtx;
}