#include "chattingServer.h"
#include "chattingContent.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cwchar>
#include <Windows.h>

//#ifdef _DEBUG
//#pragma comment(lib, "MemoryPoolD")
//
//#else
//#pragma comment(lib, "MemoryPool")
//
//#endif

//#include "customDataStructure/customDataStructure/queue_LockFree_TLS.h"
#include "lockFreeJobQueue.h"

#include "lib/monitoringTools/messageLogger.h"
#include "lib/monitoringTools/performanceProfiler.h"
#include "lib/monitoringTools/resourceMonitor.h"




using namespace std;

typedef unsigned long long int userID;
typedef unsigned long long int sessionID;


chattingContent::chattingContent() : IDcount(0), userPool(100, 100)
{
	hEvent = CreateEvent(nullptr, false, false, nullptr);
	sectorGrid.assign(51, vector<unordered_set<userID>>(51));
	jobTPS = 0;
	run = true;
}
chattingContent::~chattingContent()
{
	run = false;
	SetEvent(hEvent);
}

void chattingContent::update()
{
	monitoringCurrentThread("contentThread");
	while (run)
	{
		JobStruct* job;

		if (jobQueue.pop(job))
		{
			jobTPS++;
			serializer* jobPacket = job->_serializer;

			switch (job->id) {
			case jobID::newUserData:
			{
				createNewUser(job->targetID);
				break;
			}

			case jobID::deleteUserData:
			{
				deleteUser(job->targetID);
				break;
			}

			case jobID::login:
			{
				//INT64 AccountNo;
				//*jobPacket >> AccountNo;
				//WCHAR* ID = (WCHAR*)jobPacket->getHeadPtr();
				//jobPacket->moveFront(40);
				//WCHAR* Nickname = (WCHAR*)jobPacket->getHeadPtr();
				//jobPacket->moveFront(40);
				//char* SessionKey = jobPacket->getHeadPtr();

				loginProcess(job->targetID, job->accountNo, job->contentID, job->contentNickname, job->sessionKey);

				break;
			}

			case jobID::moveSector:
			{
				moveSector(job->targetID, job->sectorX, job->sectorY);
				break;
			}

			case jobID::sendMessage:
			{
				sendMessage(job->targetID, job->msgLen, job->message);
				break;
			}

			default:
				break;
			}


			jopPool.Delete(job);
		}
		else
		{
			WaitForSingleObject(hEvent, INFINITE);
		}
	}
}

void chattingContent::createNewUser(sessionID _id)
{
	userData* newUserPtr = userPool.Alloc();
	newUserPtr->_sessionID = _id;
	newUserPtr->sectorX = -1;
	newUserPtr->lastRecvTick = GetTickCount();
	newUserPtr->AccountNo = -1;
	newUserPtr->update();

	{
		userID newUserID = _id;

		if (userMap.find(_id) != userMap.end())
			LOGOUT_EX(logLevel::Error, LO_TXT, "content") << "create new user ID " << _id << " error, 새로운 아이디가 이미 존재한다고?" << LOGEND;
		userMap[_id] = newUserPtr;

		if (sessionID_userID_mappingTable.find(_id) != sessionID_userID_mappingTable.end())
			LOGOUT_EX(logLevel::Error, LO_TXT, "content") << "create ID " << _id << " mapping table error, 새로운 아이디가 이미 존재한다고?" << LOGEND;
		sessionID_userID_mappingTable[_id] = _id;
	}
}

void chattingContent::deleteUser(userID _id)
{
	auto target = userMap.find(_id);
	if (target == userMap.end())
	{
		LOGOUT_EX(logLevel::Error, LO_TXT, "content") << "delete user ID " << _id << " error, 지울 대상이 없다고?" << LOGEND;
		return;
	}

	if (target->second->sectorX != -1)
		sectorGrid[target->second->sectorX][target->second->sectorY].erase(_id);

	if (sessionID_userID_mappingTable.find(target->second->_sessionID) == sessionID_userID_mappingTable.end()) {
		LOGOUT_EX(logLevel::Error, LO_TXT, "content") << "delete user ID " << _id << " mapping table error, 삭제할 아이디의 session - user ID 매핑정보가 없다고?" << LOGEND;
	}
	else
		sessionID_userID_mappingTable.erase(target->second->_sessionID);

	userMap.erase(target);
	userPool.Delete(target->second);
}

bool chattingContent::loginProcess(userID _id, UINT64 AccountNo, WCHAR* ID, WCHAR* NicName, char* sessionKey)
{
	userData* target = userMap[_id];
	if (userMap.find(_id) == userMap.end())
	{
		LOG(logLevel::Error, LO_TXT, "loginProcess ID " + to_string(_id) + " error, 로그인 요청된 아이디가 아직 안만들어졌다고?");
		return false;
	}

	target->AccountNo = AccountNo;
	wcscpy_s(target->ID, 20, ID);
	wcscpy_s(target->Nickname, 20, NicName);

	serializer* p = serializerPool.Alloc();
	p->clear();
	p->moveRear(sizeof(networkHeader));
	p->moveFront(sizeof(networkHeader));
	p->incReferenceCounter();

	{
		*p << (WORD)2 << (BYTE)1 << target->AccountNo;
	}

	server->sendPacket(target->_sessionID, p);

	if (p->decReferenceCounter() == 0)
	{
		serializerFree(p);
	}

	return true;
}

void chattingContent::moveSector(userID _id, int _xPos, int _yPos)
{
	userData* target = userMap[_id];
	if (userMap.find(_id) == userMap.end())
	{
		LOG(logLevel::Error, LO_TXT, "delete user ID " + to_string(_id) + " error, 대상이 없다고?");
		return;
	}

	if (target->sectorX != -1)
	{
		if (sectorGrid[target->sectorX][target->sectorY].erase(_id) == 0)
			LOG(logLevel::Error, LO_TXT, "delete user ID " + to_string(_id) + " in sector error, 지울 대상이 해당 섹터에 없다고?");
	}

	target->sectorX = _xPos;
	target->sectorY = _yPos;
	sectorGrid[target->sectorX][target->sectorY].insert(_id);

	serializer* p = serializerPool.Alloc();
	p->clear();
	p->moveRear(sizeof(networkHeader));
	p->moveFront(sizeof(networkHeader));
	p->incReferenceCounter();

	*p << (WORD)4 << target->AccountNo << (WORD)_xPos << (WORD)_yPos;
	server->sendPacket(target->_sessionID, p);

	if (p->decReferenceCounter() == 0)
	{
		serializerFree(p);
	}
}

void chattingContent::findNeighborSector(int _xPos, int _yPos, vector<pair<int, int>>& result)
{
	if (_xPos == -1)
		return;

	//vector<vector<int>> offset = {
	//	{1, 1, 1},
	//	{1, 1, 1},
	//	{1, 1, 1},
	//};

	vector<int> x_Offset = { -1,	0,	1,	-1,	0,	1,	-1,	0,	1 };
	vector<int> y_Offset = { -1,	-1,	-1,	0,	0,	0,	1,	1,	1 };
	for (int i = 0; i < x_Offset.size(); i++)
	{
		int x = _xPos + x_Offset[i];
		int y = _yPos + y_Offset[i];

		if ((0 <= x && x <= 50) && (0 <= y && y <= 50))
			result.push_back({ x, y });
	}
}

void chattingContent::findNeighbor(userID _id, vector<userID>& result)
{
	userData* target = userMap[_id];
	if (userMap.find(_id) == userMap.end())
		LOG(logLevel::Error, LO_TXT, "findNeighbor ID error, 찾을 대상이 없다고?");

	if (target->sectorX == -1)
		return;

	vector<int> x_Offset = { -1,	0,	1,	-1,	0,	1,	-1,	0,	1 };
	vector<int> y_Offset = { -1,	-1,	-1,	0,	0,	0,	1,	1,	1 };
	for (int i = 0; i < x_Offset.size(); i++)
	{
		int x = target->sectorX + x_Offset[i];
		int y = target->sectorY + y_Offset[i];

		if ((0 <= x && x <= 50) && (0 <= y && y <= 50))
		{
			for (auto id : sectorGrid[x][y])
				result.push_back(id);
		}
	}
}

void chattingContent::sendMessage(userID _id, WORD msgLen, WCHAR* message)
{
	userData* target = userMap[_id];

	{
		if (userMap.find(_id) == userMap.end())
			LOG(logLevel::Error, LO_TXT, "sendMessage ID error, 보낼 대상이 없다고?");

	}

	serializer* p = serializerPool.Alloc();
	p->clear();
	p->moveRear(sizeof(networkHeader));
	p->moveFront(sizeof(networkHeader));
	p->incReferenceCounter();

	{
		*p << (WORD)6 << target->AccountNo;
		p->setWstring(target->ID, sizeof(target->ID) / sizeof(WCHAR));
		p->setWstring(target->Nickname, sizeof(target->Nickname) / sizeof(WCHAR));

		*p << msgLen;
		p->setWstring(message, msgLen);
	}



	vector<sessionID> sendTargets;

	findNeighbor(_id, sendTargets);
	server->sendPacket(sendTargets, p);


	if (p->decReferenceCounter() == 0)
	{
		serializerFree(p);
	}
}

void chattingContent::timeOutCheck()
{
	int threshold = 40000;
	// 락 걸자

	for (auto userPair : userMap)
	{
		if (GetTickCount() - userPair.second->lastRecvTick > threshold)
		{
			server->disconnectReq(userPair.second->_sessionID);
		}
	}
}
