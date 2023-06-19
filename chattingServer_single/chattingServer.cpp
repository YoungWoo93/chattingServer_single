
#include "chattingServer.h"
#include "chattingContent.h"

#include "lib/monitoringTools/messageLogger.h"
#include "lib/monitoringTools/performanceProfiler.h"
#include "lib/monitoringTools/resourceMonitor.h"

extern int jobPushCount;
extern int packetRecvCount;

void chattingServer::OnNewConnect(UINT64 sessionID)
{
	JobStruct* job = content->jopPool.New(jobID::newUserData, sessionID);

	content->jobQueue.push(job);

	SetEvent(content->hEvent);
}

void chattingServer::OnDisconnect(UINT64 sessionID)
{
	JobStruct* job = content->jopPool.New(jobID::deleteUserData, sessionID);

	content->jobQueue.push(job);

	SetEvent(content->hEvent);
}

bool chattingServer::OnConnectionRequest(ULONG ip, USHORT port)
{
	return true;
}

void chattingServer::OnRecv(UINT64 sessionID, serializer* _packet)
{
	char code;
	short payloadSize;
	char key;
	char checkSum;

	*_packet >> code >> payloadSize >> key;

	//
	// 패킷 누수 발생시 이곳부터 의심하자
	// 


	if (_packet->decryption(getStatickey()))
	{
		*_packet >> checkSum;
	}
	else
	{
		LOGOUT_EX(logLevel::Error, LO_TXT, "content") << "checkSum error ID " << sessionID << LOGEND;
	}




	JobStruct* job;
	WORD type;
	*_packet >> type;
	switch (type)
	{
	case 1:
	{
		//job = content->jopPool.New(jobID::login, sessionID, _packet);


		job = content->jopPool.New(jobID::login, sessionID, _packet);
		*_packet >> job->accountNo;
		job->contentID = (WCHAR*)_packet->getHeadPtr();
		_packet->moveFront(sizeof(WCHAR) * 20);
		job->contentNickname = (WCHAR*)_packet->getHeadPtr();
		_packet->moveFront(sizeof(WCHAR) * 20);
		job->sessionKey = _packet->getHeadPtr();

		break;
	}

	case 3:
	{
		job = content->jopPool.New(jobID::moveSector, sessionID, _packet);
		*_packet >> job->accountNo >> job->sectorX >> job->sectorY;
		break;
	}

	case 5:
	{
		job = content->jopPool.New(jobID::sendMessage, sessionID, _packet);
		*_packet >> job->accountNo >> job->msgLen;

		job->message = (WCHAR*)_packet->getHeadPtr();

		break;
	}

	default:
	{
		LOG(logLevel::Error, LO_TXT, "비 정의 프로토콜");
		disconnectReq(sessionID);
		return;
	}
	}





	content->jobQueue.push(job);

	SetEvent(content->hEvent);
}


void chattingServer::OnSend(UINT64 sessionID, int sendsize)
{
	return;
}


void chattingServer::OnError(int code, const char* msg)
{
	if (errorCodeComment.find(code) == errorCodeComment.end())
		code = ERRORCODE_NOTDEFINE;

	LOG(logLevel::Error, LO_TXT, errorCodeComment[code] + msg);

	return;
}


bool chattingServer::sendPacket(UINT64 sessionID, serializer* _packet)
{
	_packet->setHeader(getRandKey());
	_packet->encryption(getStatickey());

	sendReq(sessionID, _packet);

	return true;
}

bool chattingServer::sendPacket(UINT64 sessionID, packet _packet)
{
	_packet.setHeader(getRandKey());
	_packet.encryption(getStatickey());

	sendReq(sessionID, _packet);

	return true;
}

bool chattingServer::sendPacket(std::vector<UINT64>& sessionIDs, serializer* _packet)
{
	_packet->setHeader(getRandKey());
	_packet->encryption(getStatickey());


	{
		for (auto sessionID : sessionIDs) {
			sendReq(sessionID, _packet);
		}
	}
	return true;
}

bool chattingServer::sendPacket(std::vector<UINT64>& sessionIDs, packet _packet)
{
	_packet.setHeader(getRandKey());
	_packet.encryption(getStatickey());

	{
		for (auto sessionID : sessionIDs) {
			sendReq(sessionID, _packet);
		}
	}
	return true;
}