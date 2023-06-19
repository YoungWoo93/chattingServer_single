#pragma once
#include <string>
#include <cpp_redis/cpp_redis>

using namespace std;

class redisConnector
{
public:
	redisConnector();
	~redisConnector();

	redisConnector(const redisConnector& r) = delete;
	redisConnector &operator=(const redisConnector& r) = delete;

public:
	static void get(const string& _fild, string& _value);
	static void set(const string& _fild, const string& _value, int _expire = 0);
	static void disconnect();

	static __declspec(thread) cpp_redis::client* TLSRedisClient;
};