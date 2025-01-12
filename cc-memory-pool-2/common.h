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
	// ��ȡbytes�����ֵ
	static const size_t MAX_MEM_SIZE = 256 * 1024;
	// ThreadCache��CentralCache��ȡobj�������ֵ (32768)
	static const size_t MAX_FETCH_NUM = MAX_MEM_SIZE / 8;
	// ����������������thread cache�У�
	static const size_t NFREELISTS = 208;
	// span���������� (central cache��)
	static const size_t NSPANLISTS = 208;
	// page���������� (page cache��)
	static const size_t NPAGELISTS = 128;
	// �涨һҳ4KB (4KB = 2^12B)
	static const size_t PAGE_SHIFT = 12;

#if (defined(_WIN64) && defined(_WIN32)) || defined(__x86_64__) || defined(__ppc64__)
	typedef unsigned long long PageID;
#elif defined(_WIN32) || defined(__i386__) || defined(__ppc__)
	typedef unsigned int PageID;
#endif

	// ��ϵͳ�����ڴ�
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

	// �����ȡ�Ĵ�С�Ⱥ���
	class SizeClass
	{
	public:
		// ����һ����Сbytes�����������ϵ�����Ľ��
		static size_t roundUp(size_t bytes);

		// ����һ����Сbytes�����ض�Ӧfree������±�
		static size_t index(size_t bytes);

		// ���� Thread Cache �� Central Cache ��ȡobj��������ֵ (�����õ������ֵ)
		static size_t numFetchObj(size_t bytes);

		// ���� Central Cache �� Page Cache ����һ��spanʱ��ȡ��ҳ�� (��֤һ������)
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

	//�洢�ڴ����obj����������
	class FreeList
	{
	public:
		// ��һ�����������ͷ��㣬����FreeList����
		FreeList(void* head = nullptr);

		void push(void* obj);
		void pushRange(void* begin, void* end, size_t n);

		void* pop();
		size_t popRange(void*& begin, void*& end, size_t n);

		size_t& batchSize();
		bool empty();

		//������������ͷָ��
		void*& head();

		size_t size();

		static void*& nextObj(void* obj);

	private:
		void* _freeList = nullptr;
		size_t _size = 0;		//free����Ľڵ����

		/*
			batchSize��freeList��������ȡ����
			����һ�δӸ�CentralCache��������ȡ��obj������ÿ�λ�ȡ����ֵ��1��������ֵֹͣ��
			����ThreadCache��CentralCache�л�ȡobj
		*/
		size_t _batchSize = 0;
	};

	struct Span
	{
		Span* _next = nullptr;
		Span* _prev = nullptr;

		PageID _pageID = 0; // ����ڴ����ʼҳ��
		size_t _npage = 0;  // ҳ��

		int _useCount = 0;  // ��ʹ�õ��ڴ������
		FreeList _freeList; // ����ڴ�����free����

		bool _isUsing = false;//�Ƿ����ڱ�ʹ��
	};
	// ����Span�ṹ���Ͱ (��ͷ˫������)
	class SpanList
	{
	public:
		SpanList();

		// ���ڱ�������
		Span* begin();

		Span* end();

		bool empty();

		void insert(Span* pos, Span* span);

		void erase(Span* pos);

		Span* popFront();

		void pushFront(Span* span);

		std::mutex& getMutex();

	private:
		Span* _dummy;    // �����ڵ�
		std::mutex _mtx; // ���span�����Ͱ��
	};
}

