#include <clocale>
#include <io.h>
#include <fcntl.h>

#include <thread>
#include <queue>


#include "chattingServer.h"
#include "chattingContent.h"


using namespace std;

void printDay(UINT64 tick)
{
	int ms = tick % 1000;
	tick /= 1000;
	int sec = tick % 60;
	tick /= 60;
	int min = tick % 60;
	tick /= 60;
	int hour = tick % 24;
	tick /= 24;
	UINT64 days = tick;

	printf("[%2llu day...  %02d:%02d:%02d.%03d]\n", days, hour, min, sec, ms);
}



int jobPushCount;
int jobPopCount;
int jobPopFailCount;
int packetRecvCount;

void main()
{
	setlocale(LC_ALL, "");
	dump d;
	chattingServer s;
	chattingContent c;
	resourceMonitor m;

	s.content = &c;
	c.server = &s;

	thread t([&c]() {c.update(); });
	s.start(12001, 20000, 3, 3);

	LOG(logLevel::Info, LO_TXT, string("hi ") + string(__VER__));

	int min = 60;
	int hour = 3600;
	UINT64 startTime = GetTickCount64();
	unsigned long long int pastTick = startTime;
	Sleep(1000);

	queue<UINT64> jobTpsQueue;
	UINT64 jobTpsSum = 0;

	queue<UINT64> jobQSizeQueue;
	UINT64 jobQSizeSum = 0;
	for (;;) {
		unsigned long long int nowTick = GetTickCount64();
		UINT64 currentTime = nowTick - startTime;
		auto packtPoolPair = packetPoolMemoryCheck();
		UINT64 jobQSize = c.jobQueue.size();
		UINT64 jobTPS = c.jobTPS * 1000 / (nowTick - pastTick);
		c.jobTPS = 0;

		if (jobQSizeQueue.size() > 3600)
		{
			jobQSizeSum -= jobQSizeQueue.front();
			jobQSizeQueue.pop();
		
			jobTpsSum -= jobTpsQueue.front();
			jobTpsQueue.pop();
		}
		jobQSizeSum += jobQSize;
		jobQSizeQueue.push(jobQSize);

		jobTpsSum += jobTPS;
		jobTpsQueue.push(jobTPS);
		

		printDay(nowTick);
		//
		LOGOUT(logLevel::Info, LO_CMD) << "hi " << __VER__ << " logging ver " << LOGEND;

		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "traffic") << "\taccept TPS\t: " << s.getAcceptTPS() << "(total " << s.getAcceptTotal() << ")" << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "traffic") << "\tsend TPS\t: " << s.getSendMessageTPS() << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "traffic") << "\trecv TPS\t: " << s.getRecvMessageTPS() << LOGEND;

		LOGOUT(logLevel::Info, LO_CMD) << "\tsessionCount\t: " << (long long int)s.getSessionCount() << LOGEND;
		LOGOUT(logLevel::Info, LO_CMD) << "\tpacketPool\t: " << packtPoolPair.second << " / " << packtPoolPair.first << LOGEND;
		LOGOUT(logLevel::Info, LO_CMD) << "\tuserData\t: " << c.userMap.size() << LOGEND;
		LOGOUT(logLevel::Info, LO_CMD) << "\tuserDataPool\t: " << c.userPool.GetUseCount() << "/" << c.userPool.GetCapacityCount() << LOGEND;

		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjob TPS\t\t:\t" << jobTPS << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjob TPS avr\t:\t" << jobTpsSum / jobTpsQueue.size() << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjobQ size\t:\t" << c.jobQueue.size() << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjobQ size avr\t:\t" << jobQSizeSum / jobQSizeQueue.size() << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tmemory use\t:\t" << m.getUserMemorySize() << LOGEND;

		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "CPU rate : " << m.getAllCPURate()->total << LOGEND;
		for (auto it : *m.getThreadsCPURate())
			LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\t" << it.first << "\t:\t" << it.second.total << LOGEND;

		pastTick = nowTick;
		
		Sleep(1000);
	}
	c.run = false;
	SetEvent(c.hEvent);
	t.join();

	s.stop();

}