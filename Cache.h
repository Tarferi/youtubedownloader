#pragma once
#include "common.h"

class Cache {

public:

	static const unsigned int CACHE_CHECKSUM_LEN = 48;

	bool isValid = false;

	Cache(char* path, size_t maxSize);

	virtual ~Cache();

	ReadBuffer* get(char* checksum);

	bool put(char* checksum, char* data, unsigned int dataLength);

private:

	void ensureSize();

	FILE* fileHandle = nullptr;
	char* path = nullptr;
	size_t maxCache;
};

