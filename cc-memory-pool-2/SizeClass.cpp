#include "common.h"

size_t cc_memory_pool::SizeClass::roundUp(size_t bytes)
{
    assert(bytes > 0);

    if (bytes <= 128)
    {
        return _roundUp(bytes, 8);
    }
    else if (bytes <= 1024)
    {
        return _roundUp(bytes, 16);
    }
    else if (bytes <= 8 * 1024)
    {
        return _roundUp(bytes, 128);
    }
    else if (bytes <= 64 * 1024)
    {
        return _roundUp(bytes, 1024);
    }
    else if(bytes <= 256 * 1024)
    {
        return _roundUp(bytes, 8192);
    }
    else
    {
        //����256KB����ҳ����
        return _roundUp(bytes, 1 << PAGE_SHIFT);
    }
}

// ����һ����Сbytes�����ض�Ӧfree������±�
size_t cc_memory_pool::SizeClass::index(size_t bytes)
{
    assert(bytes > 0 && bytes <= MAX_MEM_SIZE);
    int group[4] = { 16, 56, 56, 56 };

    if (bytes <= 128)
    {
        return _index(bytes, 3);
    }
    else if (bytes <= 1024)
    {
        return _index(bytes - 128, 4) + group[0];
    }
    else if (bytes <= 8 * 1024)
    {
        return _index(bytes - 1024, 7) + group[0] + group[1];
    }
    else if (bytes <= 64 * 1024)
    {
        return _index(bytes - 8 * 1024, 10) + group[0] + group[1] +
            group[2];
    }
    else
    {
        return _index(bytes - 64 * 1024, 13) + group[0] + group[1] +
            group[2] + group[3];
    }
}

// ���� Thread Cache �� Central Cache ��ȡobj��������ֵ (�����õ������ֵ)
size_t cc_memory_pool::SizeClass::numFetchObj(size_t bytes)
{
    assert(bytes > 0);
    size_t threshold = MAX_FETCH_NUM / bytes; // objС����ֵ��;obj����ֵС

    if (threshold < 2)
    { // ����
        threshold = 2;
    }
    else if (threshold > 512)
    { // ����
        threshold = 512;
    }
    return threshold;
    // �����������������㷨��ʵ����ȡ��obj���� <= threshold
}

// ���� Central Cache �� Page Cache ����һ��spanʱ��ȡ��page�� (��֤һ������)
size_t cc_memory_pool::SizeClass::numFetchPage(size_t bytes)
{
    // ��������ö��ٸ�obj
    size_t num = numFetchObj(bytes);

    // obj������ * ÿ��obj�Ĵ�С = �����Ҫ���ֽ���
    // �����Ҫ���ֽ��� / ҳ��С =  �����Ҫ���ֽ��� >> PAGE_SHIFT = ��Ҫ��ҳ��
    size_t npage = (num * bytes) >> PAGE_SHIFT;

    return npage > 0 ? npage : 1;
}