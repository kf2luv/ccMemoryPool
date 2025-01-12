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
        //大于256KB，按页对齐
        return _roundUp(bytes, 1 << PAGE_SHIFT);
    }
}

// 接受一个大小bytes，返回对应free链表的下标
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

// 计算 Thread Cache 从 Central Cache 拿取obj个数的阈值 (即能拿到的最大值)
size_t cc_memory_pool::SizeClass::numFetchObj(size_t bytes)
{
    assert(bytes > 0);
    size_t threshold = MAX_FETCH_NUM / bytes; // obj小，阈值大;obj大，阈值小

    if (threshold < 2)
    { // 下限
        threshold = 2;
    }
    else if (threshold > 512)
    { // 上限
        threshold = 512;
    }
    return threshold;
    // 由于设置了慢启动算法，实际拿取的obj个数 <= threshold
}

// 计算 Central Cache 从 Page Cache 申请一个span时拿取的page数 (保证一定够用)
size_t cc_memory_pool::SizeClass::numFetchPage(size_t bytes)
{
    // 计算最多拿多少个obj
    size_t num = numFetchObj(bytes);

    // obj最大个数 * 每个obj的大小 = 最多需要的字节数
    // 最多需要的字节数 / 页大小 =  最多需要的字节数 >> PAGE_SHIFT = 需要的页数
    size_t npage = (num * bytes) >> PAGE_SHIFT;

    return npage > 0 ? npage : 1;
}