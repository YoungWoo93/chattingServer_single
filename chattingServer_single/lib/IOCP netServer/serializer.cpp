#include "Serializer.h"

void serializer::clear()
{
	headPtr = bufferPtr;
	tailPtr = bufferPtr;
}

char* serializer::resize(const size_t _size)
{
	char* tempPtr;

	if (useHeap)
		tempPtr = (char*)HeapAlloc(heap, 0, _size);
	else
		tempPtr = (char*)_aligned_malloc(_size, 64);

	memmove(tempPtr, headPtr, tailPtr - headPtr);
	headPtr = tempPtr;
	tailPtr = tempPtr + (tailPtr - headPtr);
	bufferSize = _size;

	if (useHeap)
		HeapFree(heap, 0, bufferPtr);
	else
		_aligned_free((char*)bufferPtr);

	bufferPtr = tempPtr;

	return bufferPtr;
}

char* serializer::getBufferPtr()
{
	return bufferPtr;
}
size_t serializer::getBufferSize()
{
	return bufferSize;
}

char* serializer::getHeadPtr()
{
	return headPtr;
}

char* serializer::getTailPtr()
{
	return tailPtr;
}

size_t serializer::push(const char* _data, size_t _size)
{
	while (useableSize() < _size)
		resize(bufferSize * 2);

	memmove(tailPtr, _data, _size);
	moveRear(_size);

	return _size;
}

size_t serializer::pop(char* _data, size_t _size)
{
	if (size() < _size)
		_size = size();

	memmove(_data, headPtr, _size);
	moveFront(_size);

	return _size;
}

size_t serializer::peek(char* _buffer, size_t _size)
{
	if (size() < _size)
		_size = size();

	memmove(_buffer, headPtr, _size);

	return _size;
}


int serializer::moveFront(const int _size)
{
	int offset;
	if (_size >= 0)
		offset = min(_size, size());
	else
		offset = max(_size, bufferPtr - headPtr);

	headPtr += offset;
	return offset;
}
int serializer::moveRear(const int _size)
{
	int offset;
	if (_size >= 0)
		offset = min(_size, useableSize());
	else
		offset = max(_size, headPtr - tailPtr);

	tailPtr += offset;
	return offset;
}



serializer& serializer::operator << (const bool v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	char temp = (v ? 1 : 0);

	memmove(tailPtr, &temp, sizeof(char));
	moveRear(sizeof(char));

	return *this;
}

serializer& serializer::operator << (const char v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}
serializer& serializer::operator << (const unsigned char v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

serializer& serializer::operator << (const wchar_t v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

//

serializer& serializer::operator << (const short v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}
serializer& serializer::operator << (const unsigned short v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

serializer& serializer::operator << (const int v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}
serializer& serializer::operator << (const unsigned int v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

serializer& serializer::operator << (const long v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}
serializer& serializer::operator << (const unsigned long v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

serializer& serializer::operator << (const long long v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}
serializer& serializer::operator << (const unsigned long long v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

//

serializer& serializer::operator << (const float v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}

serializer& serializer::operator << (const double v) {
	while (useableSize() < sizeof(v))
		resize(bufferSize * 2);

	memmove(tailPtr, &v, sizeof(v));
	moveRear(sizeof(v));

	return *this;
}
serializer& serializer::operator << (const std::string& v)
{
	int length = v.size();

	while (useableSize() < length + sizeof(int))
		resize(bufferSize * 2);

	memmove(tailPtr, &length, sizeof(int));
	moveRear(sizeof(int));

	memmove(tailPtr, v.c_str(), v.size());
	moveRear(length);

	return *this;
}
//

serializer& serializer::operator >> (std::string& v)
{
	int length;

	memmove(&length, headPtr, sizeof(int));
	moveFront(sizeof(int));

	v.assign(headPtr, length);
	moveFront(length);

	return *this;
}
//
serializer& serializer::operator >> (bool& v) {
	char temp;

	memmove(&temp, headPtr, sizeof(char));
	moveFront(sizeof(char));

	v = (temp == 0 ? false : true);

	return *this;
}

serializer& serializer::operator >> (char& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}
serializer& serializer::operator >> (unsigned char& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

serializer& serializer::operator >> (wchar_t& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

//

serializer& serializer::operator >> (short& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}
serializer& serializer::operator >> (unsigned short& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

serializer& serializer::operator >> (int& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}
serializer& serializer::operator >> (unsigned int& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

serializer& serializer::operator >> (long& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}
serializer& serializer::operator >> (unsigned long& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

serializer& serializer::operator >> (long long& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}
serializer& serializer::operator >> (unsigned long long& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

//

serializer& serializer::operator >> (float& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}

serializer& serializer::operator >> (double& v) {
	memmove(&v, headPtr, sizeof(v));
	moveFront(sizeof(v));

	return *this;
}
//

/*/ custom data type //

/*/



serializer& serializer::operator = (const serializer& v) {
	clear();
	resize(v.bufferSize);

	memmove(bufferPtr, v.bufferPtr, bufferSize);
	headPtr = bufferPtr + (v.headPtr - v.bufferPtr);
	tailPtr = bufferPtr + (v.tailPtr - v.bufferPtr);


	return *this;
}

