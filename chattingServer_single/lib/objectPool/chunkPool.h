#pragma once

#include <Windows.h>

#include "chunkBlock.h"

using namespace std;

template <typename T>
class chunkBlockPool
{
public:
	chunkBlockPool(int _size) :chunkBlockPoolTop(nullptr), chunkBlockPoolSize(0), chunkBlockPoolUniqeCounter(0), chunkBlockSize(_size) {
	}

	void push(chunkBlock<T>* emptyBlock) {
		chunkBlock<T>* tempTop = 0;
		chunkBlock<T>* newTop = (chunkBlock<T>*)(MAKE_KEY(emptyBlock, InterlockedIncrement16((SHORT*)&chunkBlockPoolUniqeCounter)));

		for (;;)
		{
			tempTop = chunkBlockPoolTop;
			emptyBlock->nextChunkBlock = tempTop;

			if (InterlockedCompareExchangePointer((PVOID*)&chunkBlockPoolTop, newTop, tempTop) == tempTop)
				break;
		}

		InterlockedIncrement16((SHORT*)&chunkBlockPoolSize);
	}

	chunkBlock<T>* pop() {
		chunkBlock<T>* ret;

		chunkBlock<T>* tempTop;
		chunkBlock<T>* nextTop;

		if ((short)InterlockedDecrement16((SHORT*)&chunkBlockPoolSize) < 0) {
			InterlockedIncrement16((SHORT*)&chunkBlockPoolSize);
			ret = new chunkBlock<T>();
			ret->maxSize = chunkBlockSize;
		}
		else
		{
			for (;;)
			{
				tempTop = chunkBlockPoolTop;
				ret = (chunkBlock<T>*)MAKE_NODEPTR(tempTop);
				nextTop = ret->nextChunkBlock;

				if (InterlockedCompareExchangePointer((PVOID*)&chunkBlockPoolTop, nextTop, tempTop) == tempTop)
					break;
			}
		}

		return ret;
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

	chunkBlock<T>* chunkBlockPoolTop;	// 인터락
	unsigned short chunkBlockPoolSize;			// 인터락
	unsigned short chunkBlockPoolUniqeCounter;	// 인터락
	unsigned int chunkBlockSize;
};
