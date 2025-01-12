#pragma once
#include <iostream>
#include <cassert>
#include <mutex>
#include <unordered_map>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cc_memory_pool
{
	// 获取bytes的最大值
	static const size_t MAX_MEM_SIZE = 256 * 1024;
	// ThreadCache从CentralCache拿取obj个数最大值 (32768)
	static const size_t MAX_FETCH_NUM = MAX_MEM_SIZE / 8;
	// 自由链表最大个数（thread cache中）
	static const size_t NFREELISTS = 208;
	// span链表最大个数 (central cache中)
	static const size_t NSPANLISTS = 208;
	// page链表最大个数 (page cache中)
	static const size_t NPAGELISTS = 128;
	// 规定一页4KB (4KB = 2^12B)
	static const size_t PAGE_SHIFT = 12;

#if (defined(_WIN64) && defined(_WIN32)) || defined(__x86_64__) || defined(__ppc64__)
	typedef unsigned long long PageID;
#elif defined(_WIN32) || defined(__i386__) || defined(__ppc__)
	typedef unsigned int PageID;
#endif

	// 向系统申请内存
	inline void* systemAlloc(size_t kpage)
	{
		void* ptr = nullptr;

#if defined(_WIN32) || defined(_WIN64)
		ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
		ptr = mmap(NULL, kpage << PAGE_SHIFT, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

		if (ptr == nullptr)
		{
			throw std::bad_alloc();
		}

		return ptr;
	}

	inline void systemDealloc(void* ptr, size_t pageSize)
	{
#if defined(_WIN32) || defined(_WIN64)
		VirtualFree(ptr, 0, MEM_RELEASE);
#else
		munmap(ptr, pageSize);
#endif
	}

	// 计算存取的大小等函数
	class SizeClass
	{
	public:
		// 接受一个大小bytes，返回其向上调整后的结果
		static size_t roundUp(size_t bytes);

		// 接受一个大小bytes，返回对应free链表的下标
		static size_t index(size_t bytes);

		// 计算 Thread Cache 从 Central Cache 拿取obj个数的阈值 (即能拿到的最大值)
		static size_t numFetchObj(size_t bytes);

		// 计算 Central Cache 从 Page Cache 申请一个span时拿取的页数 (保证一定够用)
		static size_t numFetchPage(size_t bytes);

	private:
		static inline size_t _roundUp(size_t bytes, size_t align_num)
		{
			return ((bytes + align_num - 1) & ~(align_num - 1));
		}
		static inline size_t _index(size_t bytes, size_t align_shift)
		{
			return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
		}
	};

	//存储内存对象obj的自由链表
	class FreeList
	{
	public:
		// 给一个自由链表的头结点，创建FreeList对象
		FreeList(void* head = nullptr);

		void push(void* obj);
		void pushRange(void* begin, void* end, size_t n);

		void* pop();
		size_t popRange(void*& begin, void*& end, size_t n);

		size_t& batchSize();
		bool empty();

		//获得自由链表的头指针
		void*& head();

		size_t size();

		static void*& nextObj(void* obj);

	private:
		void* _freeList = nullptr;
		size_t _size = 0;		//free链表的节点个数

		/*
			batchSize是freeList的批量获取个数
			（即一次从该CentralCache中批量获取的obj个数，每次获取后，其值加1，到达阈值停止）
			用于ThreadCache从CentralCache中获取obj
		*/
		size_t _batchSize = 0;
	};

	struct Span
	{
		Span* _next = nullptr;
		Span* _prev = nullptr;

		PageID _pageID = 0; // 大块内存的起始页号
		size_t _npage = 0;  // 页数

		int _useCount = 0;  // 被使用的内存对象数
		FreeList _freeList; // 存放内存对象的free链表

		bool _isUsing = false;//是否正在被使用
	};
	// 管理Span结构体的桶 (带头双向链表)
	class SpanList
	{
	public:
		SpanList();

		// 用于遍历链表
		Span* begin();

		Span* end();

		bool empty();

		void insert(Span* pos, Span* span);

		void erase(Span* pos);

		Span* popFront();

		void pushFront(Span* span);

		std::mutex& getMutex();

	private:
		Span* _dummy;    // 守卫节点
		std::mutex _mtx; // 这个span链表的桶锁
	};
}

