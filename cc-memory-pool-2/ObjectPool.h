#pragma once
#include <iostream>
#include "common.h"

using namespace cc_memory_pool;

template <class T>
class ObjectPool {
	const size_t PAGE_NUM = 2;//一次申请的页数
	const size_t OBJ_SIZE = sizeof(T);//对象大小

public:
	ObjectPool() : _memory(nullptr), _freeList(nullptr), _restMem(0) {}

	template <typename... Args>
	T* New(Args &&...args) 
	{
		T* t;
		try 
		{
			if (_freeList != nullptr)
			{  
				// free链表有闲置内存块 (pop_front)
				void* next = *(void**)_freeList;
				t = (T*)_freeList;
				_freeList = next;
			}
			else 
			{
				if (_restMem < OBJ_SIZE)
				{  
					// 剩余空间不足，先扩容
					_memory = (char*)systemAlloc(PAGE_NUM);
					if (_memory == nullptr) 
					{
						throw std::bad_alloc();
					}
					_restMem = PAGE_NUM << PAGE_SHIFT;
				}
				// 取出大小为OBJ_SIZE的空间返回
				t = (T*)_memory;
				// 取出的大小
				size_t takeSize = OBJ_SIZE >= sizeof(void*) ? OBJ_SIZE : sizeof(void*);
				//_memory覆盖掉取出的空间
				_memory += takeSize;
				// 更新剩余空间
				_restMem -= OBJ_SIZE;
			}
		}
		catch (const std::exception& e) 
		{
			std::cout << e.what() << std::endl;
			return nullptr;
		}

		// placement new
		T* obj_ptr = new (t) T(std::forward<Args>(args)...);
		return obj_ptr;
	}

	T* New() 
	{
		T* t;
		try 
		{
			if (_freeList != nullptr) 
			{
				void* next = *(void**)_freeList;
				t = (T*)_freeList;
				_freeList = next;
			}
			else 
			{
				if (_restMem < OBJ_SIZE)
				{  
					_memory = (char*)systemAlloc(PAGE_NUM);
					if (_memory == nullptr) 
					{
						throw std::bad_alloc();
					}
					_restMem = PAGE_NUM << PAGE_SHIFT;
				}
				t = (T*)_memory;
				size_t takeSize = OBJ_SIZE >= sizeof(void*) ? OBJ_SIZE : sizeof(void*);
				_memory += takeSize;
				_restMem -= OBJ_SIZE;
			}
			return t;
		}
		catch (const std::exception& e) 
		{
			std::cout << e.what() << std::endl;
			return nullptr;
		}
	}

	void Delete(T* obj) 
	{
		obj->~T();
		// 将obj头插到自由链表中
		// 用归还块的前4/8个字节存储next指针
		*(void**)obj = _freeList;
		_freeList = (void*)obj;
	}

private:
	// 一大块用于“分配”的空间，将被切分为多个内存块，每块大小sizeof(T)
	char* _memory;
	// _memory中剩余的空间
	size_t _restMem = 0;
	// 自由链表，管理用户“归还”的内存块
	void* _freeList = nullptr;
};