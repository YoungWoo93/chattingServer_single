#include "packet.h"



ObjectPool<serializer> serializerPool(10, 100);
SRWLOCK serializerPoolLock;

std::pair<size_t, size_t> packetPoolMemoryCheck()
{
	std::pair<size_t, size_t> ret;
	AcquireSRWLockExclusive(&serializerPoolLock);
	ret.first = serializerPool.GetCapacityCount();
	ret.second = serializerPool.GetUseCount();
	ReleaseSRWLockExclusive(&serializerPoolLock);

	return ret;
}


void serializerFree(serializer* s)
{
	serializerPool.Free(s);
}


serializer* serializerAlloc()
{
	auto ret = serializerPool.Alloc();
	ret->clear(); //여기서 ref count를 0으로 초기화하지는 않는다, 즉 반환할때 0임을 확인하고 반환해야한다. (수정 해야 할 수도 있음)

	return ret;
}

