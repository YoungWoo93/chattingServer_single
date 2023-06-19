#pragma once

#include <string>
#include <Windows.h>
#include <stdexcept>

#include "define.h"
class packetSerializer;




class serializer
{
public:
	serializer() : bufferPtr((char*)_aligned_malloc(1024, 64)), bufferSize(1024), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(false), heap(0), referenceCounter(0) {	// c++ allocator 를 이용해 64바이트 (캐시라인) 경계에 서도록 짜보자

	}																					//    aligned alloc 구현 이후
	serializer(const size_t _size)
		:bufferPtr((char*)_aligned_malloc(_size, 64)), bufferSize(_size), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(false), heap(0), referenceCounter(0) {
	}
	serializer(HANDLE heap)
		:bufferPtr((char*)HeapAlloc(heap, NULL, 1024)), bufferSize(1024), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(true), heap(heap), referenceCounter(0) {
	}
	serializer(HANDLE heap, const size_t _size)
		:bufferPtr((char*)HeapAlloc(heap, NULL, _size)), bufferSize(_size), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(true), heap(heap), referenceCounter(0) {
	}
	serializer(const serializer& s)
		:bufferPtr((char*)_aligned_malloc(s.bufferSize, 64))
		, bufferSize(s.bufferSize)
		, headPtr(bufferPtr + (s.headPtr - s.bufferPtr))
		, tailPtr(bufferPtr + (s.tailPtr - s.bufferPtr))
		, useHeap(s.useHeap)
		, heap(s.heap)
		, referenceCounter(0) {
		memmove(headPtr, s.headPtr, s.tailPtr - s.headPtr);
	}

	~serializer() {
		if (useHeap)
			HeapFree(heap, 0, bufferPtr);
		else
			_aligned_free(bufferPtr);
	}

	void setHeader(const short _size, const char _key, const char _checkSum)
	{
		networkHeader h(_size, _key, _checkSum);

		memcpy_s(getHeadPtr() - sizeof(networkHeader), sizeof(networkHeader), &h, sizeof(networkHeader));
		moveFront(-(int)sizeof(networkHeader));
	}

	void setHeader(const char _key)
	{
		int payloadSize = (tailPtr - bufferPtr) - sizeof(networkHeader);
		char checkSum = 0;
		char* payload = bufferPtr + sizeof(networkHeader);
		for (int i = 0; i < payloadSize; i++)
			checkSum += payload[i];

		networkHeader* h = (networkHeader*)bufferPtr;
		h = new (h) networkHeader(payloadSize, _key, checkSum);

		moveFront(-(int)sizeof(networkHeader));
	}

	void encryption(char staticKey)
	{
		int inputSize = ((networkHeader*)bufferPtr)->size + 1;
		char* target = &(((networkHeader*)bufferPtr)->checkSum);
		char dynamicKey = ((networkHeader*)bufferPtr)->randKey;

		char pP = 0;
		char pE = 0;

		for (int i = 0; i < inputSize; i++)
		{
			pP = target[i] ^ (pP + dynamicKey + 1 + i);
			target[i] = pE = pP ^ (pE + staticKey + 1 + i);
		}
	}

	bool decryption(char staticKey)
	{
		int inputSize = ((networkHeader*)bufferPtr)->size + 1;
		char* target = &(((networkHeader*)bufferPtr)->checkSum);
		char dynamicKey = ((networkHeader*)bufferPtr)->randKey;

		char pP = 0;
		char pE = 0;

		for (int i = 0; i < inputSize; i++)
		{
			char p = target[i] ^ (pE + staticKey + 1 + i);
			pE = target[i];

			target[i] = p ^ (pP + dynamicKey + 1 + i);
			pP = p;
		}

		int payloadSize = (tailPtr - bufferPtr) - sizeof(networkHeader);
		char* payload = bufferPtr + sizeof(networkHeader);
		for (int i = 0; i < payloadSize; i++)
			((networkHeader*)bufferPtr)->checkSum -= payload[i];

		return ((networkHeader*)bufferPtr)->checkSum == 0;
	}

	size_t push(const char* _data, size_t _size);
	size_t pop(char* _data, size_t _size);
	size_t peek(char* _buffer, size_t _size);

	void clear();
	char* resize(const size_t _size);

	char* getBufferPtr();
	size_t getBufferSize();
	char* getHeadPtr();
	char* getTailPtr();

	int moveFront(const int _size);
	int moveRear(const int _size);

	inline int incReferenceCounter(int size = 1)
	{
		return InterlockedAdd(&referenceCounter, size);
	}

	inline int decReferenceCounter(int size = 1)
	{
		return InterlockedAdd(&referenceCounter, -size);
	}

	inline int setReferenceCounter(int count)
	{
		return InterlockedExchange(&referenceCounter, count);
	}

	const inline size_t size() {
		return tailPtr - headPtr;
	}

	const inline size_t useableSize() {
		return bufferSize - (tailPtr - bufferPtr);
	}

	const inline size_t maxSize() {
		return bufferSize;
	}

	const inline size_t usedSize() {
		return headPtr - bufferPtr;
	}

	

protected:
	char* bufferPtr;
	size_t bufferSize;

	char* headPtr;
	char* tailPtr;

	HANDLE heap;

	volatile LONG referenceCounter;
	bool useHeap;

public:

	/*/ basic data type /*/

	void getString(char* _str, int len) {
		memcpy_s(_str, len, headPtr, size());
		moveFront(len);
	}
	void setString(char* _str, int len) {
		memcpy_s(tailPtr, useableSize(), _str, len);
		moveRear(len);
	}
	void getWstring(wchar_t* _wstr, int len) {
		memcpy_s((char*)_wstr, sizeof(wchar_t) * len, headPtr, size());
		moveFront(sizeof(wchar_t) * len);
	}
	void setWstring(wchar_t* _wstr, int len) {
		memcpy_s(tailPtr, useableSize(), (char*)_wstr, sizeof(wchar_t) * len);
		moveRear(sizeof(wchar_t) * len);
	}

	serializer& operator << (const bool v);  // 1비트 처리로 인한 byte 경계 훼손이 더 문제인듯 입력이되면 에러를 뽑는게 더 나으려나?

	serializer& operator << (const char v);
	serializer& operator << (const unsigned char v);
	serializer& operator << (const wchar_t v);

	serializer& operator << (const short v);
	serializer& operator << (const unsigned short v);
	serializer& operator << (const int v);
	serializer& operator << (const unsigned int v);
	serializer& operator << (const long v);
	serializer& operator << (const unsigned long v);
	serializer& operator << (const long long v);
	serializer& operator << (const unsigned long long v);

	serializer& operator << (const float v);
	serializer& operator << (const double v);

	serializer& operator << (const std::string& v);
	serializer& operator >> (std::string& v);



	serializer& operator >> (bool& v);

	serializer& operator >> (char& v);
	serializer& operator >> (unsigned char& v);
	serializer& operator >> (wchar_t& v);

	serializer& operator >> (short& v);
	serializer& operator >> (unsigned short& v);
	serializer& operator >> (int& v);
	serializer& operator >> (unsigned int& v);
	serializer& operator >> (long& v);
	serializer& operator >> (unsigned long& v);
	serializer& operator >> (long long& v);
	serializer& operator >> (unsigned long long& v);

	serializer& operator >> (float& v);
	serializer& operator >> (double& v);
	//



/*/ custom data type //

/*/
	friend class packetSerializer;

	virtual serializer& operator = (const serializer& v);
};









