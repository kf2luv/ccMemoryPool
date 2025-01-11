# Simple-tcmalloc

总体框架

![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412102134176.png)



## ThreadCache

>定长内存池的问题：为了应对各种应用场景，变长内存池才是最优解。可根据需求，申请任意长度的空间。

如何改造

1. **定长内存池的free链表中，每一个内存块都是相同大小的。**
   
   - 我们需要管理不同大小的内存块，就不能在同一个free链表中，如果这样做，无法判断取出的内存块是多大
   - 所以，要使用多个free链表，每个链表管理一个指定大小的内存块
    
2. **这时候问题是：要用多少个链表，难道每一个大小都搞一个链表？**
   
    > 假设最多可以申请256KB空间，那么就要从1b到256*1024b，每一个大小都映射一个free链表，这样算下来一共262144个，这个数字是很恐怖的，维护链表的成本极大。

    `tcmalloc`采用的是空间对齐（Align）的策略，假设对齐数为8b字节，你申请小于8字节的空间，就给你8字节，你申请小于16字节 (>8) 的空间，就给你16字节，以此类推，每次向上调整到8的整数倍。
    
    - 这样做的优点是：节省free链表的数量，对于同样的最大申请256KB,使用8字节对齐后，free链表总数量为`256KB / 8B = 32768`
    
    - 但同时，这种策略会导致空间浪费的问题，显而易见，给了大于用户申请大小的空间，那用不到的就浪费了，这叫做**内碎片问题**
    
    - 内碎片问题需要控制，只要浪费的不多，都是值得的。
    
   使用哈希桶组织起来。

![enter image description here](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412081716235.png)




3. **再次优化，减少哈希桶的数量。**
    **整体控制在最多10%左右的内碎片浪费**
    
    |   申请的空间大小范围   |  对齐方式    | free_list下标范围     |
    | ---- | ---- | ---- |
    |[1,128]|8byte对齐|freelist[0,16)|
    |[128+1,1024]|16byte对齐|freelist[16,72)|
    |[1024+1,8*1024]|128byte对齐|freelist[72,128)|
    |[8*1024+1,64\*1024]|1024byte对齐|freelist[128,184)|
    |[64*1024+1,256\*1024]|  8192byte对齐|freelist[184,208)|
    
       

**申请逻辑**

用户调用`ThreadCache::allocate(size_t bytes)`，指定申请空间的大小，`ThreadCache`根据根据对齐策略，选择对应的自由链表`freeList`。如果`freeList`不为空，则从中弹出一个内存对象`obj`；否则，需向下一层`CentralCache`申请内存对象。

```cpp
void* cc_memory_pool::ThreadCache::allocate(size_t bytes)
{
	assert(bytes > 0);

	// 根据对齐策略，选择对应的freeList
	size_t idx = SizeClass::index(bytes);
	FreeList& freeList = _freeLists[idx];

	if (freeList.empty())
	{
		// freeList中无内存对象，先去CentralCache拿一些obj到freeList中
		fetchObjFromCentralCache(bytes, freeList);
	}
	return freeList.pop();
}
```



`ThreadCache`每次应从`CentralCache`申请多少个内存对象`obj`？

- 取太少，频繁去拿，锁竞争问题，性能下降
- 取太多，用不完，浪费空间，别的线程还要用

这里仿照tcmalloc，采用**慢启动反馈调度算法**。

<img src="https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412102140454.png" style="zoom: 67%;" />

```cpp
// ThreadCache从CentrealCache中批量获取内存对象
// bytes: 待获取的内存对象的大小
// freeList：获取到的内存对象，统一放到这里面
void cc_memory_pool::ThreadCache::fetchObjFromCentralCache(size_t bytes, FreeList& freeList)
{
	// 1.计算从CentrealCache获取的内存对象个数fetchNum
	// 计算阈值
	size_t threshold = SizeClass::numFetchObj(bytes);
	// 慢开始算法
	size_t fetchNum = 0;
    // 每个freeList有一个最大获取个数，每次获取自增1，到达阈值为止
	if (freeList.maxFetchNum() < threshold)
	{
		fetchNum = freeList.maxFetchNum();
		freeList.maxFetchNum()++;
	}
	else
	{
		fetchNum = threshold;
	}

	// 2.从CentrealCache中获取的内存对象
	void* begin = nullptr;
	void* end = nullptr;

	size_t actualNum = CentralCache::getInstance()->getRangeObj(begin, end, fetchNum, bytes);
	assert(actualNum > 0);

    // 3.将获取到的内存对象放到ThreadCache对应的freeList中
	freeList.pushRange(begin, end, actualNum);
}
```



**释放逻辑**

用户调用`ThreadCache::deallocate(void* obj, size_t bytes)`归还内存对象到`ThreadCache`中对应的自由链表。若归还后自由链表中的内存对象个数过多，则继续向下一层`CentralCache`释放空间。





---


## CentralCache

中央缓存

![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412151549106.png)








Question:
  - 为什么要用双向链表？方便erase
  - Span::use_count有什么用？ 内存释放逻辑使用
  - **为什么只选一个Span去拿(不够就都取出来)，而不在多个Span中拿到想要的obj数量**

---

## PageCache

页缓存

![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412151524208.png)

设计点：
1. 对应页大小的span为空时，查找更大页的biggerSpan，用biggerSpan切分成两份，一份返回，一份挂载到对应位置。
2. 因此，一个线程访问`Page Cache`时，可能会访问多个桶，所以不能用桶锁，必须用整体锁。
3. 向上层提供一个足量的Span对象，上层CentralCache收到后，对其进行切片，使其变长多个对应大小的小块内存对象obj
   ![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412151533425.png)
   注意：系统分配的大块内存，不一定能切成整数个指定大小的小块内存对象。

   使用vs debug观测“大切小”的结果，此处每块小内存对象是`8byte`
   ![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412151545747.png)

Hints:
1. 向系统申请内存空间（对“分页”的理解）
   > 经过实测，Linux系统和Windows系统使用系统调用函数（mmap/VirtualAlloc）都是以页为单位开辟空间，返回的是某一页的起始空间。页的默认大小是`4096byte`。

2. PageCache和CentralCache之间的加解锁逻辑