#pragma once

#pragma pack(push, 1)
struct networkHeader {
	networkHeader() : code(0), size(0), randKey(0), checkSum(0) {
	}
	networkHeader(short _size, char _key, char _checkSum) : code(0x77), size(_size), randKey(_key), checkSum(_checkSum) {
	}

	char code;
	short size;
	char randKey;
	char checkSum;
};
#pragma pack(pop)