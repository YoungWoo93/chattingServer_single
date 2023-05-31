#include <clocale>
#include <io.h>
#include <fcntl.h>

#include <thread>



#include "chattingServer.h"
#include "chattingContent.h"

#include "crashDump/crashDump/crashDump.h"

#include "monitoringTools/monitoringTools/messageLogger.h"
#include "monitoringTools/monitoringTools/performanceProfiler.h"
#include "monitoringTools/monitoringTools/resourceMonitor.h"

#ifdef _DEBUG
#pragma comment(lib, "crashDumpD")
#else
#pragma comment(lib, "crashDump")
#endif
#pragma comment(lib, "monitoringTools\\x64\\Release\\monitoringTools")

using namespace std;

void printDay(UINT64 tick)
{
	int ms = tick % 1000;
	tick /= 1000;
	int sec = tick % 60;
	tick /= 60;
	int min = tick % 60;
	tick /= 60;
	int hour = tick % 60;
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
	CCrashDump cd;
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
	UINT64 tpsSum = 0;
	UINT64 jobQSum = 0;
	UINT64 tick = 0;
	for (;;) {
		tick++;
		unsigned long long int nowTick = GetTickCount64();
		UINT64 currentTime = nowTick - startTime;
		auto packtPoolPair = packetPoolMemoryCheck();
		UINT64 jobTPS = c.jobTPS * 1000 / (nowTick - pastTick);
		if (tick > 60)
		{
			tpsSum += jobTPS;
			jobQSum += c.jobQueue.size();
		}
		printDay(currentTime);
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
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjob TPS avr\t:\t" << tpsSum / (tick - 60) << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjobQ size\t:\t" << c.jobQueue.size() << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tjobQ size avr\t:\t" << jobQSum / (tick - 60) << LOGEND;
		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\tmemory use\t:\t" << m.getUserMemorySize() << LOGEND;

		LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "CPU rate : " << m.getAllCPURate()->total << LOGEND;
		for (auto it : *m.getThreadsCPURate())
			LOGOUT_EX(logLevel::Info, LO_TXT | LO_CMD, "perfomence") << "\t" << it.first << "\t:\t" << it.second.total << LOGEND;

		pastTick = nowTick;
		c.jobTPS = 0;
		Sleep(1000);
	}
	c.run = false;
	SetEvent(c.hEvent);
	t.join();

	s.stop();

}