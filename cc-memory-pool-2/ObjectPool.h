#pragma once
#include <iostream>
#include "common.h"

using namespace cc_memory_pool;

template <class T>
class ObjectPool {
	const size_t PAGE_NUM = 2;//һ�������ҳ��
	const size_t OBJ_SIZE = sizeof(T);//�����С

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
				// free�����������ڴ�� (pop_front)
				void* next = *(void**)_freeList;
				t = (T*)_freeList;
				_freeList = next;
			}
			else 
			{
				if (_restMem < OBJ_SIZE)
				{  
					// ʣ��ռ䲻�㣬������
					_memory = (char*)systemAlloc(PAGE_NUM);
					if (_memory == nullptr) 
					{
						throw std::bad_alloc();
					}
					_restMem = PAGE_NUM << PAGE_SHIFT;
				}
				// ȡ����СΪOBJ_SIZE�Ŀռ䷵��
				t = (T*)_memory;
				// ȡ���Ĵ�С
				size_t takeSize = OBJ_SIZE >= sizeof(void*) ? OBJ_SIZE : sizeof(void*);
				//_memory���ǵ�ȡ���Ŀռ�
				_memory += takeSize;
				// ����ʣ��ռ�
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
		// ��objͷ�嵽����������
		// �ù黹���ǰ4/8���ֽڴ洢nextָ��
		*(void**)obj = _freeList;
		_freeList = (void*)obj;
	}

private:
	// һ������ڡ����䡱�Ŀռ䣬�����з�Ϊ����ڴ�飬ÿ���Сsizeof(T)
	char* _memory;
	// _memory��ʣ��Ŀռ�
	size_t _restMem = 0;
	// �������������û����黹�����ڴ��
	void* _freeList = nullptr;
};