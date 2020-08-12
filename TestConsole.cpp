// TestConsole.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "../SampleLib/MemoryPool.h"
#include "../SampleLib/Thread.h"

struct MemoryHead
{
	uint32_t m_Size : 30;	
	uint32_t m_AllocType : 1;
	uint32_t m_IsValid : 1;
};
enum AllocType
{
	AllocType_Heap = 0,
	AllocType_Chunk,
};
MemoryHead* GetHead(void*p)
{
	return static_cast<MemoryHead*>(p) - 1;
}

int _tmain(int argc, _TCHAR* argv[])
{
	std::function<void(Thread&)> memoryPoolTestTask = [](Thread& thread)
	{
		std::vector<void*> ps;
		for(int32_t index = 0; index <= 128; ++index)
		{
			void* p = MemoryPool::GetSingleton().Allocate(index);
			MemoryHead* h = GetHead(p);
			assert(h->m_AllocType == AllocType_Chunk);
			ps.push_back(p);
		}

		for(int index = 0; index < 123; ++index)
		{
			void* p = MemoryPool::GetSingleton().Allocate((index * 32) + 125);
			MemoryHead* h = GetHead(p);
			assert(h->m_AllocType == AllocType_Chunk);
			ps.push_back(p);
		}
		{
			void* p = MemoryPool::GetSingleton().Allocate(4097);
			MemoryHead* h = GetHead(p);
			assert(h->m_AllocType == AllocType_Heap);
			ps.push_back(p);
		}
		for(void* p : ps)
		{
			MemoryPool::GetSingleton().Deallocate(p);
		}
		thread.Stop();
	};

	std::vector<Thread> threads;

	const int TestThreadCount = 4;
	threads.reserve(TestThreadCount);
	for(int index = 0; index < TestThreadCount; ++index)
	{
		threads.push_back(Thread());
		threads.back().Initialize(memoryPoolTestTask);
	}
	for(int index = 0; index < TestThreadCount; ++index)
	{
		threads[index].Start();
	}
	
	for(int index = 0; index < TestThreadCount; ++index)
	{
		while (threads[index].IsTerminated() == false)
		{
			Sleep(0);
		}
		threads[index].Finalize(); 
	}
	
	return 0;
}

