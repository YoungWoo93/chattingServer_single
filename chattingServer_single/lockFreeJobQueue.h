#pragma once

#include <Windows.h>

#include "lib/objectPool/objectPool.h"

template <typename T>
class SingletonObjectPool : public ObjectPool<T> {
public:
	static SingletonObjectPool<T>& getInstance() {
		static SingletonObjectPool<T> instance;
		return instance;
	}

private:
	SingletonObjectPool(size_t initialSize = 1000, int chunkSize = 100)
		: ObjectPool<T>(initialSize, chunkSize) {
	}
};

template <typename T>
class queue_LockFreeTLS
{
public:

	queue_LockFreeTLS(size_t initailSize = 1000, int chunkSize = 100, size_t limitSize = -1) : np(SingletonObjectPool<node>::getInstance())
	{

		_size = 0;
		pushs = 0;
		_head = (UINT64)np.Alloc();
		((node*)_head)->key = (UINT64)this;
		_tail = _head;

	}
	~queue_LockFreeTLS()
	{
		return;
		T temp;
		while (_head != 0) {
			node* popNode = MAKE_NODEPTR(_head);
			_head = popNode->key;

			np.Free(popNode);
		}
	}

	long push(T t)
	{
		UINT64 pushCount = InterlockedIncrement(&pushs);
		node* n = np.Alloc();

		UINT64 newKey = MAKE_KEY(n, pushCount);

		n->value = t;
		n->key = (UINT64)this;

		while (true)
		{
			UINT64 tail = _tail;
			UINT64 head = _head;
			UINT64 key = MAKE_NODEPTR(tail)->key;

			if (key == (UINT64)this)
			{
				if (InterlockedCompareExchange(&(MAKE_NODEPTR(tail)->key), newKey, key) == key)
				{
					InterlockedCompareExchange(&_tail, newKey, tail);

					break;
				}
			}
			else {
				InterlockedCompareExchange(&_tail, key, tail);
			}
		}

		return InterlockedIncrement(&_size);
	}

	bool pop(T& t)
	{
		if (InterlockedDecrement(&_size) < 0) {
			InterlockedIncrement(&_size);

			return false;
		}

		while (true)
		{
			UINT64 head = _head;
			UINT64 tail = _tail;
			UINT64 key = MAKE_NODEPTR(head)->key;

			if (key == (UINT64)this) {
				YieldProcessor();
				continue;
			}

			if (head == tail) {
				InterlockedCompareExchange(&_tail, MAKE_NODEPTR(tail)->key, tail);
				continue;
			}

			t = MAKE_NODEPTR(key)->value;
			if (InterlockedCompareExchange(&_head, key, head) == head)
			{
				np.Free(MAKE_NODEPTR(head));

				break;
			}
		}

		return true;
	}

	long size() {
		return _size;
	}

	//private:
	struct node
	{
		node() : key(0), poolNext(0) {
		}
		node(T v) :value(v), key(0), poolNext(0) {
		}
		node(T v, UINT64 k) :value(v), key(k), poolNext(0) {
		}
		~node() {
		}

		T value;
		UINT64 key;
		UINT64 poolNext;
	};

	inline static UINT64 MAKE_KEY(node* nodePtr, UINT64 pushNo)
	{
		return (pushNo << 48) | (UINT64)nodePtr;
	}
	inline static node* MAKE_NODEPTR(UINT64 key)
	{
		return (node*)(0x0000FFFFFFFFFFFF & key);
	}

	UINT64 pushs;

	UINT64 _head;
	UINT64 _tail;

	ObjectPool<node>& np;
	long _size;
};
