#include <string.h>
#define __VER__ (strrchr(__FILE__, '_') ? strrchr(__FILE__, '_') + 1 : __FILE__)
#define _USE_LOGGER_

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "MemoryPool")

#include "network.h"

#include <WinSock2.h>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "MemoryPool/MemoryPool/MemoryPool.h"

#include "lib/monitoringTools/messageLogger.h"
#include "lib/monitoringTools/performanceProfiler.h"
#include "lib/monitoringTools/resourceMonitor.h"
#include "lib/monitoringTools/dump.h"


#include "errorDefine.h"
#include "packet.h"
#include "session.h"




Network::Network() {

};

Network::~Network()
{
	if (isRun)
		stop();

	HANDLE arr[USHRT_MAX];
	int threads = 0;

	arr[threads++] = hAcceptThread;
	CloseHandle(hAcceptThread);
	arr[threads++] = hNetworkThread;
	CloseHandle(hNetworkThread);

	for (auto it : threadIndexMap) {
		arr[threads++] = it.first;
		CloseHandle(it.first);
	}

	auto ret = WaitForMultipleObjects(threads, arr, true, 10 * 1000);
	if (ret == WAIT_TIMEOUT)
	{
		/////
		// 10초 내로 종료 실패, 추가 동작 정의 필요
		////
	}

	delete[] recvMessageTPSArr;
	delete[] sendMessageTPSArr;

	delete[] sessionArray;
}

bool Network::start(const USHORT port, const UINT16 maxSessionSize,
	const UINT8 workerThreadSize, const UINT8 runningThreadSize,
	const int backLogSize, const ULONG host)
{
	workerThreadCount = workerThreadSize;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		OnError(ERRORCODE_WSASTARTUP);
		return false;
	}

	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET) {
		OnError(ERRORCODE_SOCKET, "listen socket");
		return false;
	}

	linger l;
	l.l_linger = 0;
	l.l_onoff = 1;
	if (setsockopt(listen_socket, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) != 0)
		LOG(logLevel::Error, LO_TXT, "Error in set Linger " + to_string(listen_socket) + " socket, error : " + to_string(GetLastError()));

	int optVal = 0;
	setsockopt(listen_socket, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, sizeof(optVal));

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.S_un.S_addr = htonl(host);
	serverAddr.sin_port = htons(port);

	int ret = bind(listen_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (ret == SOCKET_ERROR) {
		OnError(ERRORCODE_BIND);
		return false;
	}

	ret = listen(listen_socket, SOMAXCONN_HINT(backLogSize));
	if (ret == SOCKET_ERROR) {
		OnError(ERRORCODE_LISTEN);
		return false;
	}

	{
		IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadSize);
		if (IOCP == NULL) {
			writeMessageBuffer("CreateIoCompletionPort, GetLastError %d", GetLastError());
			OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

			return false;
		}

		sessionCount = 0;
		maxSession = maxSessionSize;
		workerThreadCount = workerThreadSize;
		runningThreadCount = runningThreadSize;
		for (int i = maxSession - 1; i >= 0; i--)
			indexStack.push(i);

		sessionArray = new session[maxSession];
		for (int i = 0; i < maxSession; i++)
			sessionArray[i].core = this;


		recvMessageTPSArr = new unsigned int[workerThreadCount];
		sendMessageTPSArr = new unsigned int[workerThreadCount];

		isRun = true;
	}

	hAcceptThread = CreateThread(NULL, 0, acceptThread, this, 0, NULL);
	if (hAcceptThread == NULL) {
		isRun = false;
		writeMessageBuffer("CreateThread, GetLastError %d, acceptThread", GetLastError());
		OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

		return false;
	}

	HANDLE hThread;
	for (int i = 0; i < workerThreadCount; i++) {
		hThread = CreateThread(NULL, 0, workerThread, this, 0, NULL);
		if (hThread == NULL) {
			isRun = false;
			writeMessageBuffer("CreateThread, GetLastError %d, workerThread", GetLastError());
			OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

			return false;
		}
		threadIndexMap[hThread] = i;
	}

	hNetworkThread = CreateThread(NULL, 0, networkThread, this, 0, NULL);
	if (hNetworkThread == NULL) {
		isRun = false;
		writeMessageBuffer("CreateThread, GetLastError %d, networkThread", GetLastError());
		OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

		return false;
	}

	return true;
}

void Network::stop() {
	isRun = false;
	Sleep(5000);
	if (listen_socket != INVALID_SOCKET)
		closesocket(listen_socket);

	if (IOCP != nullptr)
	{
		for (int i = 0; i < threadIndexMap.size(); i++)
			PostQueuedCompletionStatus(IOCP, 0, (ULONG_PTR)nullptr, nullptr);
		CloseHandle(IOCP);
	}

	WSACleanup();
}

DWORD WINAPI Network::networkThread(LPVOID arg)
{
	Network* core = ((Network*)arg);
	unsigned int pastTick = GetTickCount();
	Sleep(1000);
	while (core->isRun)
	{
		
		unsigned int temp1 = 0;
		unsigned int temp2 = 0;
		unsigned int temp3 = 0;
		for (int i = 0; i < core->workerThreadCount; i++) {
			temp1 += core->sendMessageTPSArr[i];
			core->sendMessageTPSArr[i] = 0;
			temp2 += core->recvMessageTPSArr[i];
			core->recvMessageTPSArr[i] = 0;
		}
		unsigned int nowTick = GetTickCount();

		core->sendMessageTPS = temp1 * 1000 / (nowTick - pastTick);
		core->recvMessageTPS = temp2 * 1000 / (nowTick - pastTick);
		core->acceptTPS = core->acceptTPSArr * 1000 / (nowTick - pastTick);
		core->acceptTPSArr = 0;

		pastTick = nowTick;
		Sleep(1000);
	}

	return 0;
}

DWORD WINAPI Network::acceptThread(LPVOID arg) {
	monitoringCurrentThread("acceptThread");

	Network* core = ((Network*)arg);

	while (core->isRun)
	{
		SOCKET client_sock = accept(core->listen_socket, NULL, NULL);
		core->acceptTPSArr++;
		core->acceptTotal++;
		if (client_sock == INVALID_SOCKET) 
			continue;

		SOCKADDR_IN clientAddr;
		int addrlen = sizeof(clientAddr);
		if (getpeername(client_sock, (SOCKADDR*)&clientAddr, &addrlen) != 0)
			LOG(logLevel::Error, LO_TXT, "Error in getpeername " + to_string(client_sock) + " socket, error : " + to_string(GetLastError()));

		if (!core->OnConnectionRequest(clientAddr.sin_addr.S_un.S_addr, clientAddr.sin_port)) {
			LOG(logLevel::Error, LO_TXT, "OnConnectionRequest fail");
			closesocket(client_sock);
			continue;
		}

		USHORT index = -1;

		if (core->indexStack.size() == 0) {
			LOG(logLevel::Error, LO_TXT, "freeSessionIDSize empty fail");
			closesocket(client_sock);
			continue;
		}
		else
		{
			core->indexStack.pop(&index);
		}

		// 세션 ID 부여 및 초기화
		session* s;
		{
			s = &(core->sessionArray[index]);
			
			UINT64 sessionID = MAKE_SESSION_ID(core->sessionCount++, index);
			s->init(sessionID, client_sock, clientAddr.sin_port, clientAddr.sin_addr.S_un.S_addr);

			if (CreateIoCompletionPort((HANDLE)(s->getSocket()), core->IOCP, (ULONG_PTR)s, NULL) == NULL)
				LOG(logLevel::Error, LO_TXT, "Error in CreateIoCompletionPort " + to_string(s->getSocket()) + " socket, error : " + to_string(GetLastError()));

			core->OnNewConnect(sessionID);
		}

		s->recvIO();

		if (s->decrementIO() == 0)
			core->deleteSession(s);
	}

	return 0;
}

DWORD WINAPI Network::workerThread(LPVOID arg)
{
	monitoringCurrentThread("workerThread");
	Network* core = ((Network*)arg);
	SHORT threadIndex = core->threadIndexMap[GetCurrentThread()];

	while (core->isRun)
	{
		DWORD transfer = 0;
		session* sessionPtr = nullptr;
		OVERLAPPED* overlap = nullptr;

		bool GQCSresult = GetQueuedCompletionStatus(core->IOCP, &transfer, (PULONG_PTR)&sessionPtr, &overlap, INFINITE);
		
		if (sessionPtr == nullptr)
			break;
		else if (GQCSresult == false)
		{
			if (sessionPtr->decrementIO() == 0)
				core->deleteSession(sessionPtr);

			continue;
		}

		if (transfer == 0)
		{
			reqEvents temp;
			if ((temp = core->requestCheck(overlap)) != reqEvents::notDefine)
			{
				if (temp == reqEvents::sendReq)
				{
					serializer* _packet = (serializer*)((unsigned long long int)overlap & (~reqFilter));

					if (sessionPtr->collectSendPacket(_packet))
					{
						sessionPtr->sendIO();
					}
					else 
					{
						if (_packet->decReferenceCounter() == 0)
							serializerFree(_packet);
					}
				}
			}
		}
		else if (sessionPtr->sendOverlappedCheck(overlap))	//send 완료 블록
		{
			core->OnSend(sessionPtr->ID, transfer);
			int tp = sessionPtr->sended(transfer);
			if (tp != -1)
				core->sendMessageTPSArr[threadIndex] += tp;
			else
				continue;
			
			sessionPtr->sendFlag = 0;

			if (sessionPtr->sendBuffer.size() > 0)
			{
				sessionPtr->sendIO();
			}

		}
		else if (sessionPtr->recvOverlappedCheck(overlap))	//recv 완료 블록
		{
			if (!(sessionPtr->recved(transfer)))
				continue; 

			while (true) {
				serializer* p = serializerAlloc();
				int temp = p->incReferenceCounter();
				p->clear();
				p->moveRear(sizeof(networkHeader));
				p->moveFront(sizeof(networkHeader));

				if (!sessionPtr->recvedPacket(p))
				{
					if (p->decReferenceCounter() == 0)
						serializerFree(p);

					break;
				}

				core->recvMessageTPSArr[threadIndex]++;
				core->OnRecv(sessionPtr->ID, p);

				if (p->decReferenceCounter() == 0)
					serializerFree(p);
			}

			sessionPtr->recvIO();
		}

		if (sessionPtr->decrementIO() == 0)
			core->deleteSession(sessionPtr);
	}
	return 0;
}

Network::reqEvents Network::requestCheck(void* _overlappedPtr)
{
	unsigned long long int reqStamp = reqFilter & (unsigned long long int)_overlappedPtr;

	if (reqStamp == (unsigned long long int)reqEvents::sendReq)
		return reqEvents::sendReq;
	if (reqStamp == (unsigned long long int)reqEvents::disconnectReq)
		return reqEvents::disconnectReq;

	return reqEvents::notDefine;
}

void Network::deleteSession(session* sessionPtr)
{
	if (InterlockedCompareExchange(&(sessionPtr->IOcount), 0x80000000, 0x00000000) != 0x00000000)
		return;

	UINT64 targetID = sessionPtr->ID;
	USHORT index = (USHORT)sessionPtr->ID;	
	int sock = sessionPtr->socket;

	if (closesocket(sessionPtr->socket) != 0)
		LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "close socket error " << sock << " socket, error " << GetLastError() << LOGEND;
	


	sessionPtr->bufferClear();

	indexStack.push(index);

	OnDisconnect(targetID);
}

size_t Network::getSessionCount()
{
	size_t ret = maxSession - indexStack.size();

	return ret;
}

std::pair<size_t, size_t> Network::getSessionPoolMemory()
{
	pair<size_t, size_t> ret;

	ret.first = 0;
	ret.second = 0;

	return ret;
}
unsigned int Network::getAcceptTotal()
{
	return acceptTotal;
}
unsigned int Network::getAcceptTPS()
{
	return acceptTPS;
}
unsigned int Network::getRecvMessageTPS()
{
	return recvMessageTPS;
}
unsigned int Network::getSendMessageTPS()
{
	return sendMessageTPS;
}



session* Network::findSession(UINT64 sessionID)
{
	session* _session = &sessionArray[(USHORT)sessionID];

	int temp = _session->incrementIO();
	if ((temp & 0x80000000) != 0) {	//이미 delete 된 세션
		if (_session->decrementIO() == 0)
			deleteSession(_session);

		return nullptr;
	}

	if (_session->ID != sessionID){ // 재사용된 세션
		if (_session->decrementIO() == 0)
			deleteSession(_session);

		return nullptr;
	}

	return _session;
}


////////////////////////////////////////////////////////////////////////////////////
// 이하 컨텐츠단에서 먼저 호출 가능한 함수들
////////////////////////////////////////////////////////////////////////////////////


void Network::disconnectReq(UINT64 sessionID)
{
	session* _sessionPtr = findSession(sessionID);

	if (_sessionPtr == nullptr)
		return;

	_sessionPtr->cancelIORegist();

	if (CancelIoEx((HANDLE)_sessionPtr->socket, NULL) == 0)
	{
		int errorCode = GetLastError();
		if (errorCode != ERROR_NOT_FOUND) {
			LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "disconnectReq CancelIoEx error " << errorCode << " index :" << (USHORT)sessionID <<LOGEND;
		}
	}

	if (_sessionPtr->decrementIO() == 0)
		deleteSession(_sessionPtr);
}

void Network::disconnectReq(session* _sessionPtr)
{
	_sessionPtr->cancelIORegist();

	if (CancelIoEx((HANDLE)_sessionPtr->socket, NULL) == 0)
	{
		int errorCode = GetLastError();
		if (errorCode != ERROR_NOT_FOUND) {
			LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "disconnectReq CancelIoEx error " << errorCode << " index :" << (USHORT)_sessionPtr->ID << LOGEND;
		}
	}
}


void Network::sendReq(UINT64 sessionID, serializer* _packet)
{

	session* _sessionPtr = findSession(sessionID);


	if (_sessionPtr == nullptr)
		return;


	_packet->incReferenceCounter();
	_sessionPtr->incrementIO();


	auto reqValue = ((unsigned long long int)_packet | (unsigned long long int)reqEvents::sendReq);
	PostQueuedCompletionStatus(IOCP, 0, (ULONG_PTR)_sessionPtr, (LPOVERLAPPED)reqValue);

	if (_sessionPtr->decrementIO() == 0)
		deleteSession(_sessionPtr);
}

void Network::sendReq(UINT64 sessionID, packet _packet)
{
	session* _sessionPtr = findSession(sessionID);


	if (_sessionPtr == nullptr)
		return;


	_packet.incReferenceCounter();
	_sessionPtr->incrementIO();


	auto reqValue = ((unsigned long long int)_packet.buffer | (unsigned long long int)reqEvents::sendReq);
	PostQueuedCompletionStatus(IOCP, 0, (ULONG_PTR)_sessionPtr, (LPOVERLAPPED)reqValue);

	if (_sessionPtr->decrementIO() == 0)
		deleteSession(_sessionPtr);
}
