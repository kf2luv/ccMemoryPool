#pragma once
#include "PageCache.h"
#include "common.h"

// ����ģʽ������ģʽ
namespace cc_memory_pool
{
    class CentralCache
    {
    public:
        static CentralCache* getInstance();

        // ��CentralCache����bytesӳ���ͰSpanList�У���ȡn��obj����������浽[begin, end]
        // ���ػ�ȡ����objʵ������
        size_t getRangeObj(void*& begin, void*& end, size_t n, size_t bytes);

        // ��ָ��spanList�У���ȡһ����Ч��span
        Span* getOneEffectiveSpan(SpanList& spanList, size_t bytes);

        // ��freeList�й黹�����ڴ�����CentralCache
        void releaseObjToCentralCache(FreeList& freeList, size_t bytes);

    private:
        CentralCache() {}
        CentralCache(const CentralCache& other) = delete;
        CentralCache& operator=(const CentralCache& other) = delete;

        SpanList _spanLists[NSPANLISTS];
        static CentralCache _instance;
    };
}
