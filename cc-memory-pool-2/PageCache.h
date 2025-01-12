#pragma once
#include "common.h"
#include "ObjectPool.h"

namespace cc_memory_pool
{
	class PageCache
	{
	public:
		static PageCache* getInstance();

		std::mutex& getMutex();

		// ����ָ����ҳ��������һ���µ�span
		Span* newSpan(size_t npage);

		// ͨ���ڴ����ĵ�ַ���õ���������Span
		Span* mapObjToSpan(void* obj);

		// �����е�span�黹�� Page Cache
		void releaseSpanToPageCache(Span* span);

	private:
		PageCache() {}
		PageCache(const PageCache& other) = delete;
		PageCache& operator=(const PageCache& other) = delete;

		SpanList _spanLists[NPAGELISTS + 1]; // ֱ��ӳ�䣬�±�i��ʾ��������ÿ��spanά��i��page, 
											 // PageCache�����span��pageID��npage��ʶ��ά����ҳ�ռ�.
		std::mutex _mtx;                     // PageCache�������
		static PageCache _instance;

		std::unordered_map<PageID, Span*> _idToSpanMap;//��ϣ��ҳ�� ӳ�� ҳ���ڵ�span��

		static ObjectPool<Span> spanPool;
	};
}
