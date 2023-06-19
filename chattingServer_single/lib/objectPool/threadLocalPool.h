#pragma once

#include <Windows.h>

#include "chunkBlock.h"
#include "subPool.h"

template <typename T> class ObjectPool;

template <typename T>
class threadLocalPool : public subPool<T> {
public:
	threadLocalPool(ObjectPool<T>* _mainPool);

	~threadLocalPool();
	void subPush(void* value);
	void* subPop();

	int getUseSize();

	int getMaxSize();

	bool overFlowChecker(memoryBlock<T>* pBlock, char** ptrDuplicate);
	bool overFlowChecker(T* pData, char** ptrDuplicate);

private:
	ObjectPool<T>* mainPoolPtr;
	unsigned short curNodeSize;
	unsigned short releaseThreashold;
	unsigned int chunkBlockSize;
	memoryBlock<T>* tailPtr;
	memoryBlock<T>* nextTailPtr;
	memoryBlock<T>* headPtr;
};

#include "objectPool.h"

template <typename T>
inline threadLocalPool<T>::threadLocalPool(ObjectPool<T>* _mainPool) {
	mainPoolPtr = _mainPool;
	chunkBlockSize = _mainPool->blockSize;
	releaseThreashold = chunkBlockSize * 2 + 1;
	nextTailPtr = nullptr;

	_mainPool->AllocChunk(&headPtr, &tailPtr);
	curNodeSize = chunkBlockSize;
}

template <typename T>
inline threadLocalPool<T>::~threadLocalPool() {
	while (curNodeSize > chunkBlockSize)
	{
		mainPoolPtr->FreeChunk(nextTailPtr->next, tailPtr, chunkBlockSize);
		tailPtr = nextTailPtr;
		curNodeSize -= chunkBlockSize;
	}

	mainPoolPtr->FreeChunk(headPtr, tailPtr, curNodeSize);
}

template <typename T>
inline void threadLocalPool<T>::subPush(void* value)
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

template <typename T>
inline void* threadLocalPool<T>::subPop()
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

template <typename T>
inline int threadLocalPool<T>::getUseSize()
{
	return curNodeSize;
}

template <typename T>
inline int threadLocalPool<T>::getMaxSize()
{
	return releaseThreashold;
}

template <typename T>
inline bool threadLocalPool<T>::overFlowChecker(memoryBlock<T>* pBlock, char** ptrDuplicate)
{

	if ((PTRSIZEINT)(pBlock->next) != (PTRSIZEINT)mainPoolPtr)
	{
		*ptrDuplicate = (char*)(pBlock->next);
		return true;
	}

	return false;
}

template <typename T>
inline bool threadLocalPool<T>::overFlowChecker(T* pData, char** ptrDuplicate)
{
	memoryBlock<T>* targetNode = (memoryBlock<T>*)((char*)(pData)-sizeof(memoryBlock<T>*));

	return overFlowChecker(targetNode, ptrDuplicate);
}
