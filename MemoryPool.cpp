#include "StdAfx.h"
#include "MemoryPool.h"
#include <winnt.h>
const uint32_t MEM_MIN_CHUNK_ALLOC_EXP = 3;	/*  2^3 = 8 byte,		 8~128	 */
const uint32_t MEM_FIXED_HARF = 5;			/*  2^(3+5) = 256 byte,  256~4096*/
const uint32_t MEM_MIN_CHUNK_ALLOC_SIZE = 1 << MEM_MIN_CHUNK_ALLOC_EXP;
const uint32_t MEM_MAX_CHUNK_ALLOC_EXP = MEM_MIN_CHUNK_ALLOC_EXP + MEM_FIXED_HARF * 2 -1;
const uint32_t MEM_MAX_CHUNK_ALLOC_SIZE = 1 << MEM_MAX_CHUNK_ALLOC_EXP;
const uint32_t MEM_CHUNK_ALLOC_BOUND_EXP = MEM_FIXED_HARF + MEM_MIN_CHUNK_ALLOC_EXP;
const uint32_t MEM_CHUNK_ALLOC_BOUND = 1 << (MEM_CHUNK_ALLOC_BOUND_EXP - 1);
const uint32_t MEM_MIN_HEAP_ALLOC = MEM_MAX_CHUNK_ALLOC_SIZE - sizeof(MemoryChunk);

HeapAllocator::HeapAllocator() : m_IsInitialized(false), m_LastUsedNumaNodeIndex(0), m_NumaNodeCount(0), m_ToalUsed(0)
{
}
HeapAllocator::~HeapAllocator()
{
}
void HeapAllocator::Initialize()
{
	if (true == m_IsInitialized)
	{
		assert(false);
		return;
	}
	if(GetNumaHighestNodeNumber(&m_NumaNodeCount) == false)
	{
		assert(false);
		return;
	}
	if(m_NumaNodeCount < 0)
	{
		assert(false);
		return;
	}
	m_IsInitialized = true;
}
void HeapAllocator::Finalize()
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}	
#if CHECK_HEAP_DEALLOC
	if (false == m_AllocatedPages.empty())
	{
		assert(false);
		return;
	}
#endif
	assert(m_ToalUsed == 0);
	m_IsInitialized = false;

}
void* HeapAllocator::Allocate(size_t size)
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return nullptr;
	}
	void* result;
	if (0 == m_NumaNodeCount)
	{
		result = VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
	}
	else
	{
		result = VirtualAllocExNuma(GetCurrentProcess(), nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, m_LastUsedNumaNodeIndex++ % m_NumaNodeCount);
	}
#if CHECK_HEAP_DEALLOC
	AllocatedPage allocatedPage;
	allocatedPage.m_Address = result;
	allocatedPage.m_Size = size;	
	{
		AutoLock lock(m_PageLock);
		m_AllocatedPages.push_back(allocatedPage);
	}
	::InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&m_ToalUsed), size);
#endif
	return result;
}
void HeapAllocator::Deallocate(void* p, size_t size)
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}	
#if CHECK_HEAP_DEALLOC
	{
		AutoLock lock(m_PageLock);
		auto iter = std::find_if(m_AllocatedPages.begin(), m_AllocatedPages.end(), [p, size](const AllocatedPage& allocatedPage)
		{
			return allocatedPage.m_Address == p && allocatedPage.m_Size == size;
		});
		if (iter != m_AllocatedPages.end())
		{
			m_AllocatedPages.erase(iter);
		}
		else
		{
			assert(false);
			return;
		}
	}
	::InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&m_ToalUsed), -static_cast<int32_t>(size));
	assert(0 <= m_ToalUsed);
#endif	
	VirtualFree(p, size, MEM_RELEASE);	
	return;
}


MemoryChunk::MemoryChunk() : m_First(nullptr), m_Used(0), m_Total(0), m_IsInitialized(false), m_AllocSize(0)
{
}

void MemoryChunk::Initialize(void* buffer, uint32_t length, uint16_t allocSize)
{
	if (true == m_IsInitialized)
	{
		assert(false);
		return;
	}
	if (length < allocSize)
	{
		assert(false);
		return;
	}
	m_First = buffer;
	m_Used = 0;
	m_Total = length / allocSize;
	m_AllocSize = allocSize;
	::InitializeSListHead(&m_PoolHead);
	m_IsInitialized = true;
}

void* MemoryChunk::Finalize()
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return nullptr;
	}
	if (m_Used != 0)
	{
		assert(false);
		return nullptr;
	}
	else
	{
		m_IsInitialized = false;
		return m_First;
	}
}

void* MemoryChunk::Allocate()
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return nullptr;
	}
	PSLIST_ENTRY entry = ::InterlockedPopEntrySList(&m_PoolHead);

	if (nullptr != entry)
	{
		::InterlockedIncrement(&m_Used);
		assert(m_Used < m_Total);
		return static_cast<void*>(entry);
	}
	if (m_Used < m_Total)
	{
		const uint32_t index = ::InterlockedIncrement(&m_Used);
		if (index < m_Total + 1)
		{
			return static_cast<char*>(m_First) + m_AllocSize * (index - 1);
		}
		else
		{
			::InterlockedDecrement(&m_Used);
			return nullptr;
		}
	}
	else
	{
		return nullptr;
	}
}

void MemoryChunk::Deallocate(void* p)
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}
	if (m_First <= p && p < static_cast<char*>(m_First) + m_Total * m_AllocSize)
	{
		SLIST_ENTRY* entry = static_cast<PSLIST_ENTRY>(p);
		::InterlockedPushEntrySList(&m_PoolHead, entry);
		::InterlockedDecrement(&m_Used);
		assert(0 <= m_Used);
	}
	else
	{
		assert(false);
	}
}

FixedMemoryAllocator::FixedMemoryAllocator() : m_AllocSize(0), m_CurrentChunk(nullptr), m_IsInitialized(false)
{
}
void FixedMemoryAllocator::Initialize(uint32_t allocSize, uint32_t chunkSize)
{
	if (true == m_IsInitialized)
	{
		assert(false);
		return;
	}
	assert(MEM_MIN_CHUNK_ALLOC_SIZE <= allocSize && allocSize <= MEM_MAX_CHUNK_ALLOC_SIZE);
	assert(MEM_MAX_CHUNK_ALLOC_SIZE + sizeof(MemoryChunk) < m_ChunkSize);
	m_AllocSize = allocSize;
	m_ChunkSize = chunkSize;
	::InitializeSListHead(&m_PoolHead);
	m_IsInitialized = true;
}

void FixedMemoryAllocator::Finalize()
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}
	m_IsInitialized = false;

	PSLIST_ENTRY entry = ::InterlockedFlushSList(&m_PoolHead);
	
	while (nullptr != entry)
	{
		void* p = entry;
		entry = entry->Next;
		auto iter = m_AddressMap.upper_bound(p);
		--iter;
		m_MemoryChunk[iter->second]->Deallocate(p);
	}
	for(uint32_t index = 0; index < m_MemoryChunk.size(); ++index)
	{
		MemoryChunk* memoryChunk = m_MemoryChunk[index];
		void* buffer = memoryChunk->Finalize();
		memoryChunk->~MemoryChunk();
		if (buffer != nullptr)
		{
			HeapAllocator::GetSingleton().Deallocate(memoryChunk, m_ChunkSize);
		}
		else
		{
			assert(false);
		}
	}
	m_MemoryChunk.clear();
	m_AddressMap.clear();
}		

void* FixedMemoryAllocator::Allocate()
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return nullptr;
	}
	PSLIST_ENTRY entry = ::InterlockedPopEntrySList(&m_PoolHead);
	if (nullptr != entry)
	{
		return static_cast<void*>(entry);
	}
	void* result = nullptr;
	MemoryChunk* currentChunk = m_CurrentChunk;
	if (currentChunk != nullptr)
	{
		result = currentChunk->Allocate();
	}
	if (result == nullptr)
	{
		AutoLock lock(m_MemoryChunkLock);
		if (m_CurrentChunk != currentChunk)
		{
			result = m_CurrentChunk->Allocate();
		}
		if (nullptr == result)
		{						
			void* buffer = HeapAllocator::GetSingleton().Allocate(m_ChunkSize);
			MemoryChunk* newChunk = new (buffer) MemoryChunk();
			newChunk->Initialize(newChunk + 1, m_ChunkSize - sizeof(MemoryChunk), min(m_AllocSize, m_ChunkSize - sizeof(MemoryChunk)));
			m_MemoryChunk.push_back(newChunk);
			result = newChunk->Allocate();
			m_CurrentChunk = newChunk;
			assert(m_AddressMap.find(buffer) == m_AddressMap.end());
			m_AddressMap[buffer] = m_MemoryChunk.size() - 1;			
		}
	}
	return result;
}

void FixedMemoryAllocator::Deallocate(void* p)
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}
	::InterlockedPushEntrySList(&m_PoolHead, static_cast<PSLIST_ENTRY>(p));
}

MemoryPool::MemoryPool() : m_IsInitialized(false)
{
}

void MemoryPool::Initialize()
{
	if (true == m_IsInitialized)
	{
		assert(false);
		return;
	}
	HeapAllocator::GetSingleton().Initialize();
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	const uint32_t chunkAllocSize = max(systemInfo.dwPageSize, MEM_MAX_CHUNK_ALLOC_SIZE);
	for(int16_t index = MEM_MIN_CHUNK_ALLOC_EXP; index <= MEM_MAX_CHUNK_ALLOC_EXP; ++index)
	{
		m_FixedMemoryAllocators.push_back(FixedMemoryAllocator());
		m_FixedMemoryAllocators.back().Initialize(1 << index, chunkAllocSize);
	}
	m_pFixedMemoryAllocators.reserve(1 << MEM_FIXED_HARF);
	m_pFixedMemoryAllocators.push_back(&m_FixedMemoryAllocators[0]);
	for(int16_t index = 1; index < MEM_FIXED_HARF; ++index)
	{
		for(int16_t index2 = 0; index2 < 1 << (index - 1); ++index2)
		{
			m_pFixedMemoryAllocators.push_back(&m_FixedMemoryAllocators[index]);
		}
	}
	m_pFixedMemoryAllocators.push_back(&m_FixedMemoryAllocators[MEM_FIXED_HARF]);
	for(int16_t index = 1; index < MEM_FIXED_HARF; ++index)
	{	
		for(int16_t index2 = 0; index2 < 1 << (index - 1); ++index2)
		{
			m_pFixedMemoryAllocators.push_back(&m_FixedMemoryAllocators[MEM_FIXED_HARF + index]);
		}
	}
	m_IsInitialized = true;
}
void MemoryPool::Finalize()
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}
	for(FixedMemoryAllocator& fixedMemoryAllocator : m_FixedMemoryAllocators)
	{
		fixedMemoryAllocator.Finalize();
	}
	HeapAllocator::GetSingleton().Finalize();
	m_IsInitialized = false;
}
void* MemoryPool::Allocate(uint32_t size)
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return nullptr;
	}
	MemoryHead* memoryHead = nullptr;
	const uint32_t sizeToAlloc = size + sizeof(MemoryHead);
	if (MEM_MIN_HEAP_ALLOC < sizeToAlloc)
	{
		void* p = HeapAllocator::GetSingleton().Allocate(sizeToAlloc);
		memoryHead = FillHead(size, AllocType_Heap, p);
	}
	else
	{
		void* p = nullptr;
		if (sizeToAlloc > MEM_CHUNK_ALLOC_BOUND)
		{
			const uint16_t HarfIndex = 1 << (MEM_FIXED_HARF - 1);
			const uint16_t allocatorIndex = HarfIndex + ((sizeToAlloc - 1) >> MEM_CHUNK_ALLOC_BOUND_EXP);
			p = m_pFixedMemoryAllocators[allocatorIndex]->Allocate();
		}
		else
		{
			const uint16_t allocatorIndex = (sizeToAlloc - 1) >> MEM_MIN_CHUNK_ALLOC_EXP;
			p = m_pFixedMemoryAllocators[allocatorIndex]->Allocate();
		}
		memoryHead = FillHead(size, AllocType_Chunk, p);
	}
	if (memoryHead != nullptr)
	{
		return ++memoryHead;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}
void MemoryPool::Deallocate(void* p)
{
	if (false == m_IsInitialized)
	{
		assert(false);
		return;
	}
 	MemoryHead* memoryHead = GetHead(p);
	if (memoryHead->m_IsValid == TRUE)
	{
		const uint32_t size = memoryHead->m_Size;
		const uint32_t sizeToAlloc = size + sizeof(MemoryHead);
		if (MEM_MIN_HEAP_ALLOC < sizeToAlloc)
		{
			assert(memoryHead->m_AllocType == AllocType_Heap);
			HeapAllocator::GetSingleton().Deallocate(memoryHead, sizeToAlloc);
		}
		else
		{
			memoryHead->m_IsValid = false;
			assert(memoryHead->m_AllocType == AllocType_Chunk);
			if (sizeToAlloc > MEM_CHUNK_ALLOC_BOUND)
			{
				const uint16_t HarfIndex = 1 << (MEM_FIXED_HARF - 1);
				const uint16_t allocatorIndex = HarfIndex + ((sizeToAlloc - 1) >> MEM_CHUNK_ALLOC_BOUND_EXP);
				m_pFixedMemoryAllocators[allocatorIndex]->Deallocate(memoryHead);
			}
			else
			{
				const uint16_t allocatorIndex = (sizeToAlloc - 1) >> MEM_MIN_CHUNK_ALLOC_EXP;
				m_pFixedMemoryAllocators[allocatorIndex]->Deallocate(memoryHead);
			}
		}
	}
	else
	{
		assert(false);
	}
}
MemoryPool::MemoryHead* MemoryPool::FillHead(const uint32_t size, const AllocType allocType, void* p)
{
	MemoryHead* result = static_cast<MemoryHead*>(p);
	result->m_Size = size;
	result->m_AllocType = allocType;
	result->m_IsValid = 1;
	return result;
}

MemoryPool::MemoryHead* MemoryPool::GetHead(void* p)
{
	return static_cast<MemoryHead*>(p) - 1;
}