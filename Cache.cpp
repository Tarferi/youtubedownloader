#include "Cache.h"

#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <share.h>
#include <filesystem>

#define READ_INT(var, errorBlock) unsigned int var; {\
		unsigned char header[4];\
		if (!myRead((char*)header, 4, fileHandle)) {errorBlock} else {\
			var = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | (header[3] << 0);\
		}}

bool myRead(char* buffer, size_t length, FILE* stream) {
	size_t _read = 0;
	while (_read != length) {
		size_t nowRead = fread(&(buffer[_read]), 1, length - _read, stream);
		if (ferror(stream) || nowRead == 0) {
			return false;
		}
		_read += nowRead;
	}
	return true;
}

bool myWrite(char* buffer, unsigned int length, FILE* stream) {
	size_t _written = 0;
	while (_written != length) {
		size_t nowWritten = fwrite(&(buffer[_written]), 1, length - _written, stream);
		if (ferror(stream) || nowWritten == 0) {
			return false;
		}
		_written += nowWritten;
	}
	return true;
}


Cache::Cache(char* path, size_t maxSize) {
	FILE* f;
	this->fileHandle = nullptr;
	this->isValid = false;
	GET_CLONED_STRING(newPath, path, {return; });
	this->path = newPath;
	if (fopen_s(&f, path, "ab+")) {
		return;
	}
	this->maxCache = maxSize;
	this->fileHandle = f;
	ensureSize();
	this->isValid = true;
}

Cache::~Cache() {
	if (this->fileHandle != nullptr) {
		fclose(this->fileHandle);
		this->fileHandle = nullptr;
	}
	FREE_NOT_NULL(path);
}

void resetCacheFile(FILE* fileHandle, size_t sz) {
	_chsize(_fileno(fileHandle), sz);
};

void resetCacheFile(FILE* fileHandle) {
	resetCacheFile(fileHandle, 0);
};

ReadBuffer* Cache::get(char* checksum) {
	if (strlen(checksum) >= CACHE_CHECKSUM_LEN) {
		return nullptr;
	}
	fseek(fileHandle, 0, SEEK_SET);


	fseek(fileHandle, 0, SEEK_END);
	unsigned long long fileSize = ftell(fileHandle);



	// Iterate over the file
	fseek(fileHandle, 0, SEEK_SET);
	while (true) {
		if (ftell(fileHandle) + 4 + CACHE_CHECKSUM_LEN >= fileSize) { // At the end of the file
			break;
		}

		// Read entry
		char buffer[CACHE_CHECKSUM_LEN];
		if (!myRead(buffer, CACHE_CHECKSUM_LEN, fileHandle)) {
			resetCacheFile(fileHandle);
			return false;
		}

		// Read entry size
		READ_INT(entrySize, {resetCacheFile(fileHandle); return false;});
		unsigned long long pos = ftell(fileHandle);
		if (pos + entrySize > fileSize) {
			return false;
			resetCacheFile(fileHandle);
		}

		if (strncmp(buffer, checksum, CACHE_CHECKSUM_LEN - 1)) { // No match, adjust position
			fseek(fileHandle, entrySize, SEEK_CUR);
		} else { // Found a match
			MALLOC_N(data, char, entrySize, {return false; });
			if (!myRead(( char*) data, entrySize, fileHandle)) {
				free(data);
				return false;
			}
			bool error = false;
			unsigned char* uncompressedData;
			unsigned int uncompressedDataLength;
			decompress(data, entrySize, ( char**) & uncompressedData, &uncompressedDataLength, &error);
			free(data);
			if (error) {
				return nullptr;
			}
			ReadBuffer* rb = new ReadBuffer(uncompressedData, uncompressedDataLength, &error);
			free(uncompressedData);
			if (error) {
				delete rb;
				return nullptr;
			}
			return rb;
		}
	}
	return nullptr;
}

bool Cache::put(char* checksum, char* data, unsigned int dataLength) {
	const unsigned int COPY_BUFFER_SIZE = 1024 * 50;

	// Align to size
	char buffer[CACHE_CHECKSUM_LEN];
	memset(buffer, 0, CACHE_CHECKSUM_LEN);
	if (strlen(checksum) > CACHE_CHECKSUM_LEN) {
		return false;
	}
	memcpy(buffer, checksum, strlen(checksum));

	// Compress data
	char* compressedData;
	unsigned int compressedDataLength;
	bool error = false;
	compress(data, dataLength, &compressedData, &compressedDataLength, &error);
	if (error) {
		return false;
	}

	unsigned char sizeBuffer[4];
	sizeBuffer[0] = (compressedDataLength >> 24) & 0xff;
	sizeBuffer[1] = (compressedDataLength >> 16) & 0xff;
	sizeBuffer[2] = (compressedDataLength >> 8) & 0xff;
	sizeBuffer[3] = (compressedDataLength >> 0) & 0xff;

	FILE* tmp;
	if (tmpfile_s(&tmp)) {
		free(compressedData);
		return false;
	}
	if (!myWrite(buffer, CACHE_CHECKSUM_LEN, tmp)) {
		free(compressedData);
		return false;
	}
	if (!myWrite(( char*) sizeBuffer, 4, tmp)) {
		free(compressedData);
		return false;
	}
	if (!myWrite(compressedData, compressedDataLength, tmp)) {
		free(compressedData);
		return false;
	}
	free(compressedData);

	fseek(fileHandle, 0, SEEK_END);
	size_t fileSize = ftell(fileHandle);

	// Copy old file on top of current file
	size_t copied = 0;
	fseek(fileHandle, 0, SEEK_SET);
	while (copied != fileSize) {
		size_t nextSize = copied + COPY_BUFFER_SIZE > fileSize ? fileSize - copied : COPY_BUFFER_SIZE;
		copied += nextSize;
		char buffer[COPY_BUFFER_SIZE];

		if (!myRead(buffer, nextSize, fileHandle)) {
			return false;
		}
		if (!myWrite(buffer, nextSize, tmp)) {
			return false;
		}
	}

	// Copy new cache file back
	fseek(tmp, 0, SEEK_END);
	fileSize = ftell(tmp);
	resetCacheFile(fileHandle);
	copied = 0;
	fseek(fileHandle, 0, SEEK_SET);
	fseek(tmp, 0, SEEK_SET);
	while (copied != fileSize) {
		size_t nextSize = copied + COPY_BUFFER_SIZE > fileSize ? fileSize - copied : COPY_BUFFER_SIZE;
		copied += nextSize;
		char buffer[COPY_BUFFER_SIZE];

		if (!myRead(buffer, nextSize, tmp)) {
			return false;
		}
		if (!myWrite(buffer, nextSize, fileHandle)) {
			return false;
		}
	}
	fclose(tmp);
	ensureSize();
	return true;
}

void Cache::ensureSize() {
	fseek(fileHandle, 0, SEEK_END);
	size_t fileSize = ftell(fileHandle);
	if (fileSize < maxCache) {
		return;
	}

	// Must trim, calculate how many items
	fseek(fileHandle, 0, SEEK_SET);
	size_t totalValidSize = 0;
	while (true) {
		if (ftell(fileHandle) + 4 + CACHE_CHECKSUM_LEN >= fileSize) { // At the end of the file
			break;
		}

		// Read entry
		char buffer[CACHE_CHECKSUM_LEN];
		if (!myRead(buffer, CACHE_CHECKSUM_LEN, fileHandle)) {
			resetCacheFile(fileHandle);
			return;
		}

		// Read entry size
		READ_INT(entrySize, {resetCacheFile(fileHandle); return;});
		size_t pos = ftell(fileHandle);
		if (pos + entrySize > fileSize) {
			return;
			resetCacheFile(fileHandle);
		}

		if (pos + entrySize > maxCache) {
			break;
		}

		// Next
		fseek(fileHandle, entrySize, SEEK_CUR);
		totalValidSize = pos + entrySize;
	}
	resetCacheFile(fileHandle, totalValidSize);
}
