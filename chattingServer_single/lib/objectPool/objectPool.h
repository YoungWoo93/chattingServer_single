#pragma once

#include <Windows.h>

#include "chunkPool.h"
#include "subPool.h"

template <typename T> class threadLocalPool;

template <typename T>
class ObjectPool
{
public:
	ObjectPool()
		:maxBlockCount(0), useBlockCount(0), topBlock(0), blockSize(100), uniqueCount(0), s(100)
	{

	}

	ObjectPool(int initNodeSize, int chunkBlockSize = 100)
		:maxBlockCount((initNodeSize + chunkBlockSize - 1) / chunkBlockSize), useBlockCount(maxBlockCount), topBlock(0), blockSize(chunkBlockSize), uniqueCount(0), s(chunkBlockSize)
	{
		for (int i = 0; i < maxBlockCount; i++) {
			chunkBlock<T>* newChunk = new chunkBlock<T>(chunkBlockSize);
			newChunk->nextChunkBlock = (chunkBlock<T>*)this;
			FreeChunk(newChunk);
		}
	}

	virtual	~ObjectPool() {
	}

	int getCurrentThreadUseCount()
	{
		if ((threadLocalPool<T>*)TLSSubPool == nullptr)
			TLSSubPool = new threadLocalPool<T>(this);

		return TLSSubPool->getUseSize();
	}
	int getCurrentThreadMaxCount()
	{
		if ((threadLocalPool<T>*)TLSSubPool == nullptr)
			TLSSubPool = new threadLocalPool<T>(this);

		return (T*)(((threadLocalPool<T>*)TLSSubPool)->getMaxSize());
	}

	T* Alloc()
	{
		if (TLSSubPool == 0)
			TLSSubPool = new threadLocalPool<T>(this);

		return (T*)(((threadLocalPool<T>*)TLSSubPool)->subPop());

	}

	void Free(T* _value)
	{
		if (TLSSubPool == nullptr)
			TLSSubPool = new threadLocalPool<T>(this);

		TLSSubPool->subPush(_value);
	}

	template <typename... param>
	T* New(param... constructorArgs)
	{
		if ((threadLocalPool<T>*)TLSSubPool == nullptr)
			TLSSubPool = new threadLocalPool<T>(this);

		T* ret = (T*)(((threadLocalPool<T>*)TLSSubPool)->subPop());


		ret = new (ret) T(constructorArgs...);

		return ret;
	}

	void Delete(T* _value)
	{
		_value->~T();

		if ((threadLocalPool<T>*)TLSSubPool == nullptr)
			TLSSubPool = new threadLocalPool<T>(this);

		((threadLocalPool<T>*)TLSSubPool)->subPush(_value);
	}

	bool FreeChunk(chunkBlock<T>* returnedBlock)
	{
		/// <summary>
		/// 락프리 push 구간
		returnedBlock->lastUseTick = GetTickCount64();
		UINT64 returnKey = MAKE_KEY(returnedBlock, InterlockedIncrement16((SHORT*)&uniqueCount));
		chunkBlock<T>* tempTop;

		while (true)
		{
			tempTop = topBlock;
			returnedBlock->nextChunkBlock = (chunkBlock<T>*)tempTop;
			if (InterlockedCompareExchangePointer((PVOID*)&topBlock, (chunkBlock<T>*)returnKey, tempTop) == tempTop)
				break;
		}
		InterlockedDecrement(&useBlockCount);
		/// </summary>

		return true;
	}

	bool FreeChunk(memoryBlock<T>* _head, memoryBlock<T>* _tail, int size)
	{
		if (size == blockSize)
		{
			chunkBlock<T>* chunk = s.pop();
			chunk->curSize = size;
			chunk->headNodePtr = _head;
			chunk->tailNodePtr = _tail;

			FreeChunk(chunk);
		}
		else
		{
			AcquireSRWLockExclusive(&recycleLock);
			recycleChunk->tailNodePtr->next = _head;
			recycleChunk->tailNodePtr = _tail;
			recycleChunk->curSize += size;

			if (recycleChunk->curSize >= blockSize)
			{
				chunkBlock<T>* chunk = s.pop();
				int offset = recycleChunk->curSize - blockSize;

				if (offset > 0)
				{
					chunk->headNodePtr = recycleChunk->headNodePtr;
					memoryBlock<T>* cur = recycleChunk->headNodePtr;

					for (int i = 1; i < offset; i++)
						cur = cur->next;

					recycleChunk->headNodePtr = cur->next;
					recycleChunk->curSize -= offset;
					chunk->curSize = offset;
					chunk->tailNodePtr = cur;


				}

				FreeChunk(recycleChunk);
				recycleChunk = chunk;
			}
			ReleaseSRWLockExclusive(&recycleLock);
		}
		return true;
	}

	void AllocChunk(memoryBlock<T>** _head, memoryBlock<T>** _tail)
	{
		chunkBlock<T>* popBlock;

		if ((long long int)InterlockedIncrement(&useBlockCount) >= maxBlockCount)
		{
			popBlock = newChunkBlock();
		}
		else
		{
			while (true)
			{
				popBlock = topBlock;
				chunkBlock<T>* nextTop = ((chunkBlock<T>*)MAKE_NODEPTR(popBlock))->nextChunkBlock;

				if (InterlockedCompareExchangePointer((PVOID*)&topBlock, nextTop, popBlock) == popBlock)
					break;
			}
		}

		popBlock = ((chunkBlock<T>*)MAKE_NODEPTR(popBlock));
		popBlock->curSize = 0;
		*_head = popBlock->headNodePtr;
		*_tail = popBlock->tailNodePtr;


		s.push(popBlock);
	}


	chunkBlock<T>* newChunkBlock()
	{
		chunkBlock<T>* newChunk = s.pop();
		newChunk->reAlloc();

		InterlockedIncrement(&maxBlockCount);

		return newChunk;
	}

	bool deleteChunkBlock(chunkBlock<T>* target)
	{
		InterlockedDecrement(&maxBlockCount);
		delete target;

		return true;
	}

	unsigned long long int GetUseCount()
	{
		return useBlockCount;
	}
	unsigned long long int GetCapacityCount()
	{
		return maxBlockCount;
	}

	int garbageCollection(unsigned long long int threshold)
	{
		
	}

	inline static UINT64 MAKE_KEY(chunkBlock<T>* nodePtr, UINT64 pushNo)
	{
		return (pushNo << 48) | (UINT64)nodePtr;
	}
	inline static UINT64 MAKE_KEY(UINT64 nodePtr, UINT64 pushNo)
	{
		return (pushNo << 48) | (UINT64)nodePtr;
	}

	inline static chunkBlock<T>* MAKE_NODEPTR(UINT64 key)
	{
		return (chunkBlock<T>*)(0x0000FFFFFFFFFFFF & key);
	}
	inline static chunkBlock<T>* MAKE_NODEPTR(chunkBlock<T>* key)
	{
		return (chunkBlock<T>*)(0x0000FFFFFFFFFFFF & (UINT64)key);
	}

	static __declspec(thread) threadLocalPool<T>* TLSSubPool;

	size_t maxBlockCount;
	size_t useBlockCount;

	chunkBlock<T>* topBlock;
	UINT64 blockSize;

	chunkBlock<T>* recycleChunk;
	SRWLOCK recycleLock;


	unsigned short uniqueCount;
	chunkBlockPool<T> s;
	bool GCFlag;

	//friend class MemoryPool;
};

template <typename T>
__declspec(thread) threadLocalPool<T>* ObjectPool<T>::TLSSubPool = nullptr;




template <typename T>
class threadLocalPool {
public:
	threadLocalPool(ObjectPool<T>* _mainPool) {
		mainPoolPtr = _mainPool;
		chunkBlockSize = _mainPool->blockSize;
		releaseThreashold = chunkBlockSize * 2 + 1;
		nextTailPtr = nullptr;

		_mainPool->AllocChunk(&headPtr, &tailPtr);
		curNodeSize = chunkBlockSize;
	}

	~threadLocalPool() {
		while (curNodeSize > chunkBlockSize)
		{
			mainPoolPtr->FreeChunk(nextTailPtr->next, tailPtr, chunkBlockSize);
			tailPtr = nextTailPtr;
			curNodeSize -= chunkBlockSize;
		}

		mainPoolPtr->FreeChunk(headPtr, tailPtr, curNodeSize);
	}
	inline void subPush(void* value)
	{
		memoryBlock<T>* pushNode = (memoryBlock<T>*)((char*)value - 8);
		char* duplicate;

		if (overFlowChecker(pushNode, &duplicate))
		{
			//cout << "overflow || not legal pool occurrence || reduplication release memory" << endl;
			//
			//error throw
			// overFlow or invalid
			//
			*(char*)nullptr = NULL;
			return;
		}

		if (curNodeSize == chunkBlockSize)
			nextTailPtr = pushNode;

		pushNode->next = headPtr;
		headPtr = pushNode;
		++curNodeSize;

		if (curNodeSize == releaseThreashold)
		{
			mainPoolPtr->FreeChunk(nextTailPtr->next, tailPtr, chunkBlockSize);
			tailPtr = nextTailPtr;
			nextTailPtr = pushNode;
			curNodeSize -= chunkBlockSize;
		}
	}

	inline void* subPop()
	{
		memoryBlock<T>* popNode = headPtr;
		headPtr = headPtr->next;
		--curNodeSize;
		if (curNodeSize < chunkBlockSize)
		{
			nextTailPtr = tailPtr;
			mainPoolPtr->AllocChunk(&nextTailPtr->next, &tailPtr);

			curNodeSize += chunkBlockSize;
		}

		popNode->next = (memoryBlock<T>*)mainPoolPtr;


		return &popNode->data;
	}

	int getUseSize()
	{
		return curNodeSize;
	}

	int getMaxSize()
	{
		return releaseThreashold;
	}

	inline bool overFlowChecker(memoryBlock<T>* pBlock, char** ptrDuplicate)
	{
		//이용우 pBlock->next를 찾기 너무 힘듬
		if ((PTRSIZEINT)(pBlock->next) != (PTRSIZEINT)mainPoolPtr)
		{
			*ptrDuplicate = (char*)(pBlock->next);
			return true;
		}

		return false;
	}

	bool overFlowChecker(T* pData, char** ptrDuplicate)
	{
		memoryBlock<T>* targetNode = (memoryBlock<T>*)((char*)(pData)-sizeof(memoryBlock<T>*));

		return overFlowChecker(targetNode, ptrDuplicate);
	}

private:
	ObjectPool<T>* mainPoolPtr;
	unsigned short curNodeSize;
	unsigned short releaseThreashold;
	unsigned int chunkBlockSize;
	memoryBlock<T>* tailPtr;
	memoryBlock<T>* nextTailPtr;
	memoryBlock<T>* headPtr;
};
