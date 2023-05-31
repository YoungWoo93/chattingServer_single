#include "chattingServer.h"
#include "chattingContent.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cwchar>
#include <Windows.h>

#include "customDataStructure/customDataStructure/queue_LockFree_TLS.h"
#include "monitoringTools/monitoringTools/messageLogger.h"
#include "monitoringTools/monitoringTools/performanceProfiler.h"
#include "monitoringTools/monitoringTools/resourceMonitor.h"
#pragma comment(lib, "monitoringTools\\x64\\Release\\monitoringTools")



using namespace std;

typedef unsigned long long int userID;
typedef unsigned long long int sessionID;

extern int jobPopCount;
extern int jobPopFailCount;

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
			case jobID::packetProcess:
			{
				userID _userId;
				{
					if (sessionID_userID_mappingTable.find(job->targetID) == sessionID_userID_mappingTable.end())
					{
						LOG(logLevel::Error, LO_TXT, "update find user ID error, 생성되지 않은 아이디에 대한 동작");
						server->disconnectReq(job->targetID);
						break;
					}

					_userId = sessionID_userID_mappingTable[job->targetID];

					if (userMap.find(_userId) == userMap.end())
					{
						LOG(logLevel::Error, LO_TXT, "update find user error, 생성되지 않은 유저에 대한 동작");

						break;
					}
					userMap[_userId]->update();
				}
				WORD type;
				*job->_serializer >> type;

				switch (type)
				{
				case 0: {
					LOG(logLevel::Error, LO_TXT, "비 정의 프로토콜 0");
					server->disconnectReq(job->targetID);
					break; }
				case 1: {
					INT64 AccountNo;
					*job->_serializer >> AccountNo;
					WCHAR* ID = (WCHAR*)job->_serializer->getHeadPtr();
					job->_serializer->moveFront(40);
					WCHAR* Nickname = (WCHAR*)job->_serializer->getHeadPtr();
					job->_serializer->moveFront(40);
					char* SessionKey = job->_serializer->getHeadPtr();
					if (loginProcess(_userId, AccountNo, ID, Nickname, SessionKey))
					{
						// 로그인 성공 후 동작
						//
						//
					}
					else
					{
						// 로그인 실패 후 동작
						//
						//
					}
					break; }
				case 2: {
					LOG(logLevel::Error, LO_TXT, "비 정의 프로토콜 2");
					server->disconnectReq(job->targetID);
					break; }
				case 3: {
					INT64 AccountNo;
					WORD SectorX, SectorY;
					*job->_serializer >> AccountNo >> SectorX >> SectorY;

					moveSector(_userId, SectorX, SectorY);
					break; }
				case 4: {
					LOG(logLevel::Error, LO_TXT, "비 정의 프로토콜 4");
					server->disconnectReq(job->targetID);
					break;
				}
				case 5: {
					INT64 AccountNo;
					WORD msgLen;
					*job->_serializer >> AccountNo >> msgLen;

					WCHAR* message = (WCHAR*)job->_serializer->getHeadPtr();

					sendMessage(_userId, msgLen, message);
					break; }
				case 6: {
					LOG(logLevel::Error, LO_TXT, "비 정의 프로토콜 6");
					server->disconnectReq(job->targetID);
					break;
				}
				case 7: {
					break;
				}
				default:
					LOG(logLevel::Error, LO_TXT, "비 정의 프로토콜 " + to_string(type));
					server->disconnectReq(job->targetID);
				}

				break;
			}
			default:
				break;
			}

			jopPool.Delete(job);
		}
		else
		{
			jobPopFailCount++;
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

		//LOGOUT_EX(logLevel::Info, LO_TXT, "content") << "create ID " << _id << LOGEND;
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

	//LOGOUT_EX(logLevel::Info, LO_TXT, "content") << "delete user ID " << _id << LOGEND;
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

	serializer* p = serializerAlloc();
	p->incReferenceCounter();
	p->clean();
	p->moveRear(sizeof(serializer::packetHeader));
	p->moveFront(sizeof(serializer::packetHeader));
	{
		(*p) << (WORD)2 << (BYTE)1 << target->AccountNo;
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

	target->sectorX = _xPos;// / 10;
	target->sectorY = _yPos;// / 10;
	sectorGrid[target->sectorX][target->sectorY].insert(_id);

	serializer* p = serializerAlloc();
	p->incReferenceCounter();
	p->clean();
	p->moveRear(sizeof(serializer::packetHeader));
	p->moveFront(sizeof(serializer::packetHeader));
	{
		(*p) << (WORD)4 << target->AccountNo << (WORD)_xPos << (WORD)_yPos;
	}

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

	vector<pair<int, int>> neighbors;
	findNeighborSector(target->sectorX, target->sectorY, neighbors);

	for (auto pos : neighbors)
	{
		for (auto id : sectorGrid[pos.first][pos.second])
			result.push_back(id);
	}
}
unsigned long long int freeInSendMessage;

void chattingContent::sendMessage(userID _id, WORD msgLen, WCHAR* message)
{
	userData* target = userMap[_id];

	{
		if (userMap.find(_id) == userMap.end())
			LOG(logLevel::Error, LO_TXT, "sendMessage ID error, 보낼 대상이 없다고?");

	}

	serializer* p = serializerAlloc();
	p->incReferenceCounter();
	p->clean();
	p->moveRear(sizeof(serializer::packetHeader));
	p->moveFront(sizeof(serializer::packetHeader));

	{
		(*p) << (WORD)6 << target->AccountNo;

		wcsncpy_s((WCHAR*)p->getTailPtr(), p->useableSize() / sizeof(WCHAR), target->ID, sizeof(target->ID) / sizeof(WCHAR));
		p->moveRear(sizeof(target->ID));

		wcsncpy_s((WCHAR*)p->getTailPtr(), p->useableSize() / sizeof(WCHAR), target->Nickname, sizeof(target->Nickname) / sizeof(WCHAR));
		p->moveRear(sizeof(target->Nickname));

		(*p) << msgLen;
		char* t = p->getTailPtr();
		wcsncpy_s((WCHAR*)p->getTailPtr(), p->useableSize() / sizeof(WCHAR), message, msgLen / sizeof(WCHAR));
		p->moveRear(msgLen);
	}

	vector<userID> result;

	findNeighbor(_id, result);
	//result.push_back(_id);

	vector<sessionID> sendTargets;
	for (auto id : result)
	{
		if (userMap.find(id) == userMap.end())
		{
			LOG(logLevel::Error, LO_TXT, "findNeighbor ID error, 찾을 대상이 없다고?");
		}
		else
		{
			sendTargets.push_back(userMap[id]->_sessionID);
		}
	}

	{
		server->sendPacket(sendTargets, p);
	}

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
