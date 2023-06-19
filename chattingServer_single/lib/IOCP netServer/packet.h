#pragma once

#include "serializer.h"

#include "MemoryPool/MemoryPool/MemoryPool.h"
//#include "lib/memoryPool/objectPool.h"

#include "lib/monitoringTools/messageLogger.h"
#include "lib/monitoringTools/performanceProfiler.h"
#include "lib/monitoringTools/resourceMonitor.h"
#include "lib/monitoringTools/dump.h"

#include "define.h"

extern ObjectPool<serializer> serializerPool;
extern SRWLOCK serializerPoolLock;
class packet;

std::pair<size_t, size_t> packetPoolMemoryCheck();
void serializerFree(serializer* s);
serializer* serializerAlloc();


class packet {

public:
	serializer* buffer;

	packet() {
		buffer = serializerPool.Alloc();
		buffer->clear();
		buffer->moveRear(sizeof(networkHeader));
		buffer->moveFront(sizeof(networkHeader));
		buffer->incReferenceCounter();
	}



	~packet() {
		if (buffer != nullptr)
		{
			if (buffer->decReferenceCounter() == 0)
				serializerFree(buffer);
		}
	}
	packet(packet& p) {
		buffer = p.buffer;
		buffer->incReferenceCounter();
	}

	packet(serializer* s) {
		buffer = s;
		//buffer->incReferenceCounter();
	}

	int incReferenceCounter(int size = 1)
	{
		if (buffer == nullptr)
			return 0;

		return buffer->incReferenceCounter(size);
	}

	int decReferenceCounter(int size = 1)
	{
		if (buffer == nullptr)
			return 0;

		int ret = buffer->decReferenceCounter(size);
		if (ret == 0)
		{
			serializerPool.Free(this->buffer);
			buffer = nullptr;
		}

		return ret;
	}

	void setHeader(const char _key)
	{
		buffer->setHeader(_key);
	}

	void encryption(char staticKey)
	{
		buffer->encryption(staticKey);
	}

	bool decryption(char staticKey)
	{
		return buffer->decryption(staticKey);
	}



	networkHeader* getPacketHeader()
	{
		return (networkHeader*)buffer->getBufferPtr();
	}

	char* getPayload()
	{
		return buffer->getBufferPtr() + sizeof(networkHeader);
	}

	char* getHeadPtr()
	{
		return buffer->getHeadPtr();
	}

	int moveFront(int _size)
	{
		return buffer->moveFront(_size);
	}
	char* getTailPtr()
	{
		return buffer->getTailPtr();
	}
	int moveRear(int _size)
	{
		return buffer->moveRear(_size);
	}
	size_t size()
	{
		return buffer->size();
	}

	size_t useableSize()
	{
		return buffer->useableSize();
	}


	void getString(char* _str, int len) {
		buffer->getString(_str, len);
	}
	void setString(char* _str, int len) {
		buffer->setString(_str, len);
	}
	void getWstring(wchar_t* _wstr, int len) {
		buffer->getWstring(_wstr, len);
	}
	void setWstring(wchar_t* _wstr, int len) {
		buffer->setWstring(_wstr, len);
	}

	packet& operator << (const char v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned char v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const wchar_t v) {
		(*buffer) << v;
		return *this;
	}

	packet& operator << (const short v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned short v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const int v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned int v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const long v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned long v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const long long v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned long long v) {
		(*buffer) << v;
		return *this;
	}

	packet& operator << (const float v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const double v) {
		(*buffer) << v;
		return *this;
	}



	packet& operator >> (char& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned char& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (wchar_t& v) {
		(*buffer) >> v;
		return *this;
	}

	packet& operator >> (short& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned short& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (int& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned int& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (long& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned long& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (long long& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned long long& v) {
		(*buffer) >> v;
		return *this;
	}

	packet& operator >> (float& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (double& v) {
		(*buffer) >> v;
		return *this;
	}

	packet& operator = (const packet& p) {
		p.buffer->incReferenceCounter();

		if (buffer != nullptr && buffer->decReferenceCounter() == 0)
			serializerPool.Free(this->buffer);

		this->buffer = p.buffer;

		return *this;
	}

	packet& operator = (serializer* s) {
		if (buffer != nullptr && buffer->decReferenceCounter() == 0)
			serializerPool.Free(this->buffer);

		this->buffer = s;

		return *this;
	}


};


