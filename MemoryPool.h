#pragma once

#include "AutoLock.h"
#include "Singleton.h"

#define CHECK_HEAP_DEALLOC 1

class MemoryPool;
class FixedMemoryAllocator;
class MemoryChunk;
template<typename T> struct SingletonCreationPolicy;
template<> struct SingletonCreationPolicy<MemoryPool>;
template<typename T> struct SingletonDestroyPolicy;
template<> struct SingletonDestroyPolicy<MemoryPool>;

class MemoryPool : public Singleton<MemoryPool>
{
public:	
	void* Allocate(uint32_t size);
	void Deallocate(void* p);
protected:
	MemoryPool();
	void Initialize();
	void Finalize();
private:
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
	static MemoryHead* GetHead(void*p);
	static MemoryHead* FillHead(const uint32_t size, const AllocType allocType, void* p);
	std::vector<FixedMemoryAllocator*> m_pFixedMemoryAllocators;
	std::vector<FixedMemoryAllocator> m_FixedMemoryAllocators;
	bool m_IsInitialized;

	friend struct SingletonCreationPolicy<MemoryPool>;
	friend struct SingletonDestroyPolicy<MemoryPool>;
};

class HeapAllocator : public Singleton<HeapAllocator>
{
public:
	HeapAllocator();
	~HeapAllocator();
	void Initialize();
	void Finalize();
	void* Allocate(uint32_t size);
	void Deallocate(void* p, uint32_t size);
private:
	uint32_t m_LastUsedNumaNodeIndex;
	ULONG m_NumaNodeCount;
	bool m_IsInitialized;
	int32_t m_ToalUsed;

#if CHECK_HEAP_DEALLOC
	struct AllocatedPage
	{
		void* m_Address;
		uint32_t m_Size;
		bool m_IsDeleted;
	};
	SpinLock m_PageLock;
	std::vector<AllocatedPage> m_AllocatedPages;
#endif
};

class FixedMemoryAllocator
{
public:
	FixedMemoryAllocator();
	void Initialize(uint32_t allocSize, uint32_t m_ChunkSize);
	void Finalize();
	void* Allocate();
	void Deallocate(void* p);
private:
	uint32_t m_AllocSize;
	uint32_t m_ChunkSize;
	SLIST_HEADER m_PoolHead;
	
	SpinLock m_MemoryChunkLock;
	MemoryChunk* m_CurrentChunk;
	std::vector<MemoryChunk*> m_MemoryChunk;
	std::map<void*, uint16_t> m_AddressMap;
	bool m_IsInitialized;
};

class MemoryChunk
{
public:
	MemoryChunk();
	void Initialize(void* buffer, uint32_t length, uint16_t allocSize); 
	void* Finalize();
	void* Allocate();
	void Deallocate(void* p);
private:
	void* m_First;
	uint32_t m_Used;
	uint32_t m_Total;
	SLIST_HEADER m_PoolHead;
	uint16_t m_AllocSize;
	bool m_IsInitialized;
};

template<>
struct SingletonCreationPolicy<MemoryPool>
{
	static MemoryPool* Create()
	{
		MemoryPool* result = new MemoryPool();
		result->Initialize();
		return result;
	}
};
template<>
struct SingletonDestroyPolicy<MemoryPool>
{
	static void Destroy(MemoryPool* memoryPool)
	{
		memoryPool->Finalize();
		delete memoryPool;
	}
};
