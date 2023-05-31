#ifdef _DEBUG
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "crashDumpD")
#pragma comment(lib, "IOCPD")

#else
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "crashDump")
#pragma comment(lib, "IOCP")

#endif


#include "chattingServer.h"
#include "chattingContent.h"

#include "monitoringTools/monitoringTools/messageLogger.h"
#include "monitoringTools/monitoringTools/performanceProfiler.h"
#include "monitoringTools/monitoringTools/resourceMonitor.h"
#pragma comment(lib, "monitoringTools\\x64\\Release\\monitoringTools")

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
	JobStruct* job = content->jopPool.New(jobID::packetProcess, sessionID, _packet);
	if (job->_serializer->referenceCounter != 1)
	{
		int i = 0;
		i++;
	}

	{
		decryption(&(((serializer::packetHeader*)_packet->getBufferPtr())->checkSum), payloadSize + 1, &((serializer::packetHeader*)_packet->getBufferPtr())->checkSum, getStatickey(), key);

		*_packet >> checkSum;
		char temp = 0;
		for (int i = 0; i < payloadSize; i++)
			temp += (_packet->getBufferPtr() + sizeof(serializer::packetHeader))[i];

		if (checkSum != temp)
			LOGOUT_EX(logLevel::Error, LO_TXT, "content") << "checkSum error ID " << sessionID << LOGEND;
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
	int payloadSize = _packet->size();

	((serializer::packetHeader*)_packet->getBufferPtr())->checkSum = 0;
	for (int i = 0; i < payloadSize; i++)
		((serializer::packetHeader*)_packet->getBufferPtr())->checkSum += (_packet->getBufferPtr() + sizeof(serializer::packetHeader))[i];
	char key = getRandKey();

	((serializer::packetHeader*)_packet->getBufferPtr())->size = payloadSize;
	((serializer::packetHeader*)_packet->getBufferPtr())->randKey = key;
	encryption(&(((serializer::packetHeader*)_packet->getBufferPtr())->checkSum), payloadSize + 1, &(((serializer::packetHeader*)_packet->getBufferPtr())->checkSum), getStatickey(), key);
	setHeader(_packet);


	sendReq(sessionID, _packet);

	return true;
}

bool chattingServer::sendPacket(std::vector<UINT64>& sessionIDs, serializer* _packet)
{
	{
		int payloadSize = _packet->size();

		((serializer::packetHeader*)_packet->getBufferPtr())->checkSum = 0;
		for (int i = 0; i < payloadSize; i++)
			((serializer::packetHeader*)_packet->getBufferPtr())->checkSum += (_packet->getBufferPtr() + sizeof(serializer::packetHeader))[i];
		char key = getRandKey();

		((serializer::packetHeader*)_packet->getBufferPtr())->size = payloadSize;
		((serializer::packetHeader*)_packet->getBufferPtr())->randKey = key;
		encryption(&(((serializer::packetHeader*)_packet->getBufferPtr())->checkSum), payloadSize + 1, &(((serializer::packetHeader*)_packet->getBufferPtr())->checkSum), getStatickey(), key);
		setHeader(_packet);
	}


	{
		for (auto sessionID : sessionIDs) {
			sendReq(sessionID, _packet);
		}
	}
	return true;
}

