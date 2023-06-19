#pragma once
#include <Windows.h>
#include <stdio.h>
#include <iostream>


template <typename T>
class LockFreeStack {

	struct node
	{
		node() : value(0), key(0), poolNext(0) {
		}
		node(T v) :value(v), key(0), poolNext(0) {
		}
		node(T v, UINT64 k) :value(v), key(k), poolNext(0) {
		}
		~node() {
		}

		UINT64 getPushNo() {
			return key >> 48;
		}

		UINT64 getNextPtr() {
			return MAKE_NODEPTR(key);
		}

		T value;
		UINT64 key;
		UINT64 poolNext;
	};

	class nodePool
	{

	public:
		nodePool()
			:maxSize(0), useSize(0), topKey(0) {

		}

		nodePool(int _size)
			: maxSize(_size), useSize(0), topKey(0)
		{
			for (int i = 0; i < _size; i++)
			{
				node* newNode = allocNewBlock();
				newNode->poolNext = topKey;

				topKey = MAKE_KEY(newNode, pushCount++);
			}
		}

		virtual	~nodePool() {
			while (topKey != 0) {
				node* temp = MAKE_NODEPTR(topKey);
				topKey = temp->poolNext;

				delete temp;
			}
		}


		void clear() {
			maxSize = 0;
			useSize = 0;
			pushCount = 0;

			while (topKey != nullptr) {
				node* temp = MAKE_NODEPTR(topKey);
				topKey = temp->poolNext;

				delete temp;
			}
		}

		node* Alloc()
		{
			while (true)
			{
				UINT64 compareKey = topKey;
				node* popNode = MAKE_NODEPTR(compareKey);

				if (popNode == nullptr)
				{
					InterlockedIncrement(&maxSize);
					InterlockedIncrement(&useSize);

					return allocNewBlock();
				}


				if (InterlockedCompareExchange(&topKey, popNode->poolNext, compareKey) == compareKey)
				{
					InterlockedIncrement(&useSize);
					return popNode;
				}
			}
		}

		bool Free(node* pData)
		{
			node* freeNode = pData;
			UINT64 compareKey;

			while (true)
			{
				compareKey = topKey;
				freeNode->poolNext = compareKey;

				if (InterlockedCompareExchange(&topKey, MAKE_KEY(freeNode, InterlockedIncrement(&pushCount)), compareKey) == compareKey)
				{
					InterlockedDecrement(&useSize);
					return true;
				}
			}
		}

		int	GetCapacityCount(void)
		{
			return maxSize;
		}

		int	GetUseCount(void)
		{
			return useSize;
		}

	private:
		size_t maxSize;
		size_t useSize;
		UINT64 topKey;
		UINT64 pushCount;

		node* allocNewBlock()
		{
			return new node();
		}

	};


public:
	LockFreeStack() : topKey(0), stackSize(0) {
	};

	~LockFreeStack() {
		while (size()) {
			T temp;
			pop(&temp);
		}

		np.~nodePool();
	}

	void push(T value) {
		node* n = np.Alloc();

		n->value = value;
		//{
		//	if (InterlockedIncrement(&n->value) != 1)
		//		printf("error\n");
		//}

		UINT64 compareKey = 0;
		UINT64 pushKey = MAKE_KEY(n, InterlockedIncrement(&pushCount));

		while (true)
		{
			compareKey = topKey;
			n->key = compareKey;

			if (InterlockedCompareExchange(&topKey, pushKey, compareKey) == compareKey)
			{
				InterlockedIncrement(&stackSize);
				//return std::make_pair<UINT64, UINT64>((UINT64)MAKE_NODEPTR(compareKey), (UINT64)n);

				break;
			}
		}

	}

	void pop(T* n) {
		node* popNode = nullptr;
		UINT64 compareKey = 0;
		UINT64 nextTopKey = 0;

		while (true)
		{
			compareKey = topKey;
			popNode = MAKE_NODEPTR(compareKey);
			nextTopKey = popNode->key;

			if (InterlockedCompareExchange(&topKey, nextTopKey, compareKey) == compareKey)
			{
				InterlockedDecrement(&stackSize);
				*n = popNode->value;
				//InterlockedDecrement(&popNode->value);
				np.Free(popNode);
				//return std::make_pair<UINT64, UINT64>((UINT64)popNode, (UINT64)MAKE_NODEPTR(nextTopKey));
				break;
			}
		}
	}

	T Top() {
		return MAKE_NODEPTR(topKey)->value;
	}

	size_t size() {
		return stackSize;
	}

	inline static UINT64 MAKE_KEY(node* nodePtr, UINT64 pushNo)
	{
		return (pushNo << 48) | (UINT64)nodePtr;
	}
	inline static node* MAKE_NODEPTR(UINT64 key)
	{
		return (node*)(0x0000FFFFFFFFFFFF & key);
	}

	nodePool np;
private:
	unsigned long long int pushCount;
	unsigned long long int stackSize;
	UINT64 topKey;
};

