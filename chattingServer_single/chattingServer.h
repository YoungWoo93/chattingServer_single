#pragma once
#include "IOCP/IOCP/network.h"
#include "IOCP/IOCP/session.h"

#include <vector>

class chattingContent;

class chattingServer : public Network {
public:
	virtual void OnNewConnect(UINT64 sessionID);

	virtual void OnDisconnect(UINT64 sessionID);
	virtual bool OnConnectionRequest(ULONG ip, USHORT port);
	virtual void OnRecv(UINT64 sessionID, serializer* _packet);

	virtual void OnSend(UINT64 sessionID, int sendsize);


	virtual void OnError(int code, const char* msg = "");

	bool sendPacket(UINT64 sessionID, serializer* _packet);
	bool sendPacket(std::vector<UINT64>& sessionIDs, serializer* _packet);

	inline char getRandKey()
	{
		return rand() & 0x000000FF;
	}

	inline char getStatickey()
	{
		return 0x32; // 50
	}

	void encryption(char* input, size_t inputSize, char* output, char staticKey, char dynamicKey)
	{
		char pP = 0;
		char pE = 0;

		for (int i = 0; i < inputSize; i++)
		{
			pP = input[i] ^ (pP + dynamicKey + 1 + i);
			output[i] = pE = pP ^ (pE + staticKey + 1 + i);
		}
	}

	void decryption(char* input, size_t inputSize, char* output, char staticKey, char dynamicKey)
	{
		char pP = 0;
		char pE = 0;

		for (int i = 0; i < inputSize; i++)
		{
			char p = input[i] ^ (pE + staticKey + 1 + i);
			pE = input[i];

			output[i] = p ^ (pP + dynamicKey + 1 + i);
			pP = p;
		}
	}

	chattingContent* content;
};