#pragma once

#include "IOCP/IOCP/packet.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Windows.h>

#include "customDataStructure/customDataStructure/queue_LockFree_TLS.h"

using namespace std;

class chattingServer;
typedef unsigned long long int userID;
typedef unsigned long long int sessionID;

struct userData
{
	sessionID		_sessionID;
	INT64			AccountNo;
	WCHAR			ID[20];
	WCHAR			Nickname[20];
	unsigned int	lastRecvTick;
	int				sectorX;
	int				sectorY;

	void update()
	{
		lastRecvTick = GetTickCount();
	}
};
enum class jobID {
	newUserData,
	deleteUserData,
	packetProcess,
};

struct JobStruct
{
	JobStruct(jobID _id, userID _targetID, serializer* __serializer) : id(_id), targetID(_targetID), _serializer(__serializer) {
		//_serializer->incReferenceCounter();
	}
	JobStruct(jobID _id, userID _targetID) : id(_id), targetID(_targetID), _serializer(nullptr) {
	}
	JobStruct() : id(jobID::newUserData), targetID(0), _serializer(nullptr) {
	}

	~JobStruct() {

		if (_serializer != nullptr)
		{
			int temp = _serializer->decReferenceCounter();
			if (temp == 0)
				serializerFree(_serializer);
		}
	}

	jobID id;
	userID targetID;
	serializer* _serializer;
};

class chattingContent
{
public:
	chattingContent();
	~chattingContent();

	void update();

	void createNewUser(sessionID _id);
	void deleteUser(userID _id);

	bool loginProcess(userID _id, UINT64 AccountNo, WCHAR* ID, WCHAR* NicName, char* sessionKey);
	void moveSector(userID _id, int _xPos, int _yPos);
	void sendMessage(userID _id, WORD msgLen, WCHAR* message);

	void findNeighborSector(int _xPos, int _yPos, vector<pair<int, int>>& result);
	void findNeighbor(userID _id, vector<userID>& result);

	void timeOutCheck();

public:
	bool run;
	UINT64 jobQSize;
	UINT64 jobTPS;
	HANDLE hEvent;
	chattingServer* server;
	userID IDcount;
	ObjectPool<userData> userPool;
	ObjectPool<JobStruct> jopPool;
	SRWLOCK userContainerLock;
	unordered_map<sessionID, userID> sessionID_userID_mappingTable;
	unordered_map<userID, userData*> userMap;
	vector<vector<unordered_set<userID>>> sectorGrid;
	queue_LockFreeTLS<JobStruct*> jobQueue;
};