#pragma once
#include "lib/IOCP netServer/network.h"
#include "lib/IOCP netServer/session.h"

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
	bool sendPacket(UINT64 sessionID, packet _packet);
	bool sendPacket(std::vector<UINT64>& sessionIDs, serializer* _packet);
	bool sendPacket(std::vector<UINT64>& sessionIDs, packet _packet);

	inline char getRandKey()
	{
		return rand() & 0x000000FF;
	}

	inline char getStatickey()
	{
		return 0x32; // 50
	}
	
	chattingContent* content;
};