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
   
    > 假设最多可以申请256KB空间，那么就要从1b到256*1024b，每一个大小都映射一个free链表，这样算下来一共262144个，这个数字是很吓人的，维护链表的成本极大。

    `tcmalloc`采用的是空间对齐（Align）的策略，假设对齐数为8b字节，你申请小于8字节的空间，就给你8字节，你申请小于16字节 (>8) 的空间，就给你16字节，以此类推，每次向上调整到8的整数倍。
    
    - 这样做的优点是：节省free链表的数量，对于同样的最大申请256KB,使用8字节对齐后，free链表总数量为`256KB / 8B = 32768`
    
    - 但同时，这种策略会导致空间浪费的问题，显而易见，给了大于用户申请大小的空间，那用不到的就浪费了，这叫做**内碎片问题**
    
    - 内碎片问题需要控制，只要浪费的不多，都是值得的。
    


   **每个对齐字节数，对应一个自由链表freeList，使用哈希桶组织起来。**

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
    /* 
    	batchSize是freeList的批量获取个数
    	（即一次从该CentralCache中批量获取的obj个数，每次获取后，其值加1，到达阈值停止）
    */
	if (freeList.batchSize() < threshold)
	{
		fetchNum = freeList.batchSize();
		freeList.batchSize()++;
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

用户调用`ThreadCache::deallocate(void* obj, size_t bytes)`归还内存对象到`ThreadCache`中对应的自由链表。若归还后自由链表中的**内存对象个数达到一定数量**，则继续向下一层`CentralCache`释放空间。



- `ThreadCache`为什么要定期向下层的`CentralCache`释放空间？

  每个线程的`ThreadCache`是独立的，如果一个`ThreadCache`占用了太多的内存对象而不使用，其它线程就要花费更多成本去申请空间，这是不可取的。

  

- `ThreadCache`的自由链表中，内存对象`obj`个数达到多少时，向下层释放，比较合适？释放多少个内存对象合适？

  > 内存对象释放太多，用户不够用又得去下层申请；释放太少，空间闲置率太高，其它线程想用还用不了。

  这里规定 `ThreadCache`某个自由链表中的内存对象个数 >= 该自由链表的`batchSize` 时，向`CentralCache`释放一个批量的内存对象（即`batchSize`个`obj`）。

  `batchSize`可以理解为`freeList`最近一次从下层申请的内存对象个数（实际相差一个，因为申请后`bacthSize++`）。最近一次从下层申请，肯定是因为上一次申请的内存对象被用户都拿光了，用户正在使用中，尚未归还。此时**用户正在使用的内存对象和相应`freeList`中的内存对象总数大概是`batchSize * 2`**，当用户逐步释放内存，直到相应`freeList`中的内存对象个数等于`batchSize`时，表示为用户准备的内存对象有一半不被使用，此时就向下层归还这一半，这样将每个`ThreadCache`的空间闲置率控制在大概50%。

```cpp
void cc_memory_pool::ThreadCache::deallocate(void* obj, size_t bytes)
{
	assert(obj != nullptr);
	assert(bytes > 0);

	// 根据对齐策略，选择对应的free链表
	size_t idx = SizeClass::index(bytes);
	FreeList& freeList = _freeLists[idx];

	// 将内存对象插入free链表 
	freeList.push(obj);

	if (freeList.size() >= freeList.batchSize()) 
	{
		// 如果free链表中的obj太多，归还一部分给下层
		CentralCache::getInstance()->releaseObjToCentralCache(freeList, bytes);
	}
}
```



---



## CentralCache

> `CentralCache`也是一个哈希桶结构，其哈希桶的映射关系跟`ThreadCache`是一样的。不同的是其每个哈希桶位置挂是SpanList链表结构，SpanList管理一个一个的Span。

**Span可以理解为一段连续的内存范围**，由CentralCache从下层PageCache申请而来。每个Span将大内存空间切分成一个一个的小内存对象（对象大小=Span所在哈希桶映射的字节大小），挂载在自己的自由链表中。

![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202412151549106.png)

Span结构体定义如下：

```cpp
struct Span
{
    Span* _next = nullptr;
    Span* _prev = nullptr;

    PageID _pageID = 0; 	// 大块内存的起始页号
    size_t _npage = 0;  	// 页数

    int _useCount = 0;  	// 被使用的内存对象数
    FreeList _freeList; 	// 挂载内存对象的free链表

    bool _isUsing = false;	//是否正在被使用
};
```








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





## 优化



### 大于256KB的内存

> 内存池三层模型规定了用户一次能申请的最大内存空间为256KB。那用户申请更大空间时应该怎么办？分以下几种情况：



假设用户申请空间大小为`size`，一个页大小为`4KB`

1. ` 256KB < size <= 128 * 4KB`  -> ` 64 * 4KB < size <= 128 * 4KB`，即size大小在32页到128页之间。

   这种情况下，不能获取`ThreadCache`和`CentralCache`这两个缓存中的内存对象（它们中的最大内存对象为256KB），但是可以通过`PageCache`申请，因为`PageCache`最大能容纳128页的空间。故有如下设计：

   申请逻辑：

   当` 256KB < size <= 128 * 4KB`时，先将`size`按页对齐（如：`64页+100B`对齐到`65页`），得到页数`kpage`，然后向`PageCache`申请一个`kpage`页的span，此时逻辑与三层模型的逻辑相同，只不过没有对页空间进行切割，而是将其视为一个内存对象。后面归还空间时，也会进行合并。

   

2. `size > 128 * 4KB`

   此时`PageCache`也无法满足需求，只能向系统申请。申请和释放还是托管给`PageCache`：`PageCache`会向系统申请一块大小为`size`的空间，并用一个`Span`来管理，这个`Span`不会进入`PageCache`的哈希桶中，只做一下映射来方便后面的释放

   

   `ccAlloc()`和`ccFree()`的改进

   ```cpp
   void* ccAlloc(size_t size)
   {
       if (size > MAX_MEM_SIZE)
       {
           //将size按页对齐
           size_t align = SizeClass::roundUp(size);
           size_t kPage = align >> PAGE_SHIFT;
           Span* kSpan = nullptr;
           //向PageCache申请一个kpage页的span
           {
               std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
               kSpan = PageCache::getInstance()->newSpan(kPage);
           }
           //将kSpan的页号转换为起始地址
           void* addr = (void*)(kSpan->_pageID << PAGE_SHIFT);
   
           return addr;
       }
   
       if (pTLSThreadCache == nullptr)
       {
           pTLSThreadCache = new ThreadCache;
       }
       return pTLSThreadCache->allocate(size);
   }
   
   void ccFree(void* obj, size_t size)
   {
       if (size > MAX_MEM_SIZE)
       {
           //找到obj所属的span
           Span* span = PageCache::getInstance()->mapObjToSpan(obj);
           {
               //对页缓存进行操作，加整体锁
               std::unique_lock<std::mutex> pageCacheLock(PageCache::getInstance()->getMutex());
               PageCache::getInstance()->releaseSpanToPageCache(span);
           }
       }
       else 
       {
           pTLSThreadCache->deallocate(obj, size);
       }
   }
   ```

   `PageCache`中对应的实现逻辑

   ```cpp
   cc_memory_pool::Span* cc_memory_pool::PageCache::newSpan(size_t k)
   {
   	assert(k > 0);
       
   	//k大于128页
   	if (k > NPAGELISTS)
   	{
   		//直接向系统申请k页空间
   		void* addr = systemAlloc(k);
   
   		Span* kSpan = new Span;
   		kSpan->_npage = k;
   		kSpan->_pageID = (PageID)addr >> PAGE_SHIFT;
   
   		//映射，释放时才能找到
   		_idToSpanMap[kSpan->_pageID] = kSpan;
   
   		return kSpan;
   	}
       
   	//k小于等于128页
       //...
   }
   
   void cc_memory_pool::PageCache::releaseSpanToPageCache(Span* span)
   {
   	assert(span);
       
       //大于128页
   	if (span->_npage > NPAGELISTS)
   	{
   		//直接还给系统
   		void* addr = (void*)(span->_pageID << PAGE_SHIFT);
   		systemDealloc(addr, span->_npage);
   		//移除这个span
   		_idToSpanMap.erase(span->_pageID);
   		delete span;
   		return;
   	}
       
       //小于等于128页
       //...
   }
   ```

   





