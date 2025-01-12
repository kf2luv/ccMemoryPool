#include "ccAlloc.h"
#include <thread>
#include <vector>

struct TestType
{
	int arr[69];
};

void alloc_test_1()
{
	for (int i = 0; i < 10; i++)
	{
		void* ptr = cc_memory_pool::ccAlloc(5);
	}
}

void alloc_test_2()
{
	for (int i = 0; i < 10; i++)
	{
		void* ptr = cc_memory_pool::ccAlloc(8);
	}
}

void alloc_test()
{
	std::thread t1(alloc_test_1);
	std::thread t2(alloc_test_2);

	t1.join();
	t2.join();
}

void alloc_test1()
{
	for (int i = 0; i < 10; i++)
	{
		double* ptr = (double*)cc_memory_pool::ccAlloc(sizeof(double));
		*ptr = i + 1;
		printf("%p: %lf\n", ptr, *ptr);
	}
}

void alloc_test2()
{
	std::cout << "sizeof(TestType): " << sizeof(TestType) << std::endl;

	for (int i = 0; i < 10; i++)
	{
		TestType* ptr = (TestType*)cc_memory_pool::ccAlloc(sizeof(TestType));
		*ptr = TestType();
		printf("%p\n", ptr);
	}
}

void alloc_test3()
{

	for (int i = 0; i < 512; i++)
	{
		int* ptr = (int*)cc_memory_pool::ccAlloc(sizeof(int));
		*ptr = i + 1;
		printf("%p: %lf\n", ptr, *ptr);
	}


	int* ptr = (int*)cc_memory_pool::ccAlloc(sizeof(int));
	*ptr = 512;
	printf("%p: %lf\n", ptr, *ptr);
}

void getSysInfo() {
	std::cout << "######################################" << std::endl;
	// 创建 SYSTEM_INFO 结构体实例
	SYSTEM_INFO sysInfo;

	// 调用 GetSystemInfo 函数填充结构体
	GetSystemInfo(&sysInfo);

	// 输出页面大小
	std::cout << "Page size: " << sysInfo.dwPageSize << " bytes" << std::endl;

	// 额外输出一些相关信息
	std::cout << "Minimum application address: " << sysInfo.lpMinimumApplicationAddress << std::endl;
	std::cout << "Maximum application address: " << sysInfo.lpMaximumApplicationAddress << std::endl;
	std::cout << "Active processor mask: " << sysInfo.dwActiveProcessorMask << std::endl;
	std::cout << "Number of processors: " << sysInfo.dwNumberOfProcessors << std::endl;
	std::cout << "Processor type: " << sysInfo.dwProcessorType << std::endl;

	std::cout << "######################################" << std::endl;
}

void alloc_test4()
{
	std::vector<double*> ptrs;

	for (int i = 0; i < 100000; i++)
	{
		double* ptr = (double*)cc_memory_pool::ccAlloc(sizeof(double));
		ptrs.push_back(ptr);
	}


	for (auto ptr : ptrs) {
		cc_memory_pool::ccFree(ptr, sizeof(double));
	}
}

void alloc_test5()
{
	void* p1 = cc_memory_pool::ccAlloc(1);//第一次申请，threadCache为空
	void* p2 = cc_memory_pool::ccAlloc(2);
	void* p3 = cc_memory_pool::ccAlloc(3);
	void* p4 = cc_memory_pool::ccAlloc(4);
	void* p5 = cc_memory_pool::ccAlloc(5);
	void* p6 = cc_memory_pool::ccAlloc(6);
	void* p7 = cc_memory_pool::ccAlloc(7);

	cc_memory_pool::ccFree(p1, 1);
	cc_memory_pool::ccFree(p2, 2);
	cc_memory_pool::ccFree(p3, 3);
	cc_memory_pool::ccFree(p4, 4);
	cc_memory_pool::ccFree(p5, 5);
	cc_memory_pool::ccFree(p6, 6);
	cc_memory_pool::ccFree(p7, 7);

	//可测试页合并
}


void multiThreadTest1() {
	std::vector<void*> ptrs;

	for (int i = 0; i < 7; i++)
	{
		void* ptr = cc_memory_pool::ccAlloc(5);
		ptrs.push_back(ptr);
	}

	for (auto ptr : ptrs) {
		cc_memory_pool::ccFree(ptr, 5);
	}
}

void multiThreadTest2() {
	std::vector<void*> ptrs;

	for (int i = 0; i < 100; i++)
	{
		void* ptr = cc_memory_pool::ccAlloc(5);
		ptrs.push_back(ptr);
	}

	for (auto ptr : ptrs) {
		cc_memory_pool::ccFree(ptr, 5);
	}
}


void multiThreadTest() {
	std::thread t1(multiThreadTest1);
	std::thread t2(multiThreadTest2);

	t1.join();
	t2.join();
}

void bigAlloc() {
	size_t bigSize1 = 256 * 1024 + 100;
	void* ptr1 = cc_memory_pool::ccAlloc(bigSize1);
	cc_memory_pool::ccFree(ptr1, bigSize1);

	size_t bigSize2 = 129 * 4 * 1024;
	void* ptr2 = cc_memory_pool::ccAlloc(bigSize2);
	cc_memory_pool::ccFree(ptr2, bigSize2);
}

int main()
{
	bigAlloc();
	return 0;
}