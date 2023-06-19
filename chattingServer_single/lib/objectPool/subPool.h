#pragma once

template <typename T>
class subPool {
public:
	virtual void subPush(void* value) = 0;
	virtual void* subPop() = 0;

	virtual int getUseSize() = 0;

	virtual int getMaxSize() = 0;
};