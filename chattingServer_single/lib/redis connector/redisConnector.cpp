#include <iostream>
#include <string>

#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>

#ifdef _DEBUG
#pragma comment(lib, "lib/redis connector/Debug/cpp_redis")
#pragma comment(lib, "lib/redis connector/Debug/tacopie")
#else
#pragma comment(lib, "lib/redis connector/Release/cpp_redis")
#pragma comment(lib, "lib/redis connector/Release/tacopie")
#endif

#include "redisConnector.h"


using namespace std;

cpp_redis::client* redisConnector::TLSRedisClient = nullptr;

redisConnector::redisConnector(){

}
redisConnector::~redisConnector(){

}


void redisConnector::get(const string& _fild, string& _value)
{
	if (TLSRedisClient == nullptr)
	{
		TLSRedisClient = new cpp_redis::client();
		TLSRedisClient->connect();
	}

	TLSRedisClient->get(_fild, [&_value](cpp_redis::reply& reply) {
		_value = reply.as_string();
		});

	TLSRedisClient->sync_commit();
}
void redisConnector::set(const string& _fild, const string& _value, int _expire)
{
	if (TLSRedisClient == nullptr)
	{
		TLSRedisClient = new cpp_redis::client();
		TLSRedisClient->connect();
	}

	TLSRedisClient->set(_fild, _value);
	
	if (_expire != 0)
		TLSRedisClient->expire(_fild, _expire);

	TLSRedisClient->sync_commit();
}

void redisConnector::disconnect()
{
	if (TLSRedisClient != nullptr)
		TLSRedisClient->disconnect();
}