#include "common.h"

ReadBuffer::ReadBuffer(FILE* file, bool* error) {
	this->size = 0;
	this->dataSize = 0;
	this->position = 0;
	this->rawData = nullptr;
	unsigned char buffer[READBUFFER_BUFFER_SIZE];
	while (true) {
		size_t read = fread(buffer, sizeof(char), READBUFFER_BUFFER_SIZE, file);
		if (read == 0) {
			break;
		} else {
			this->append(buffer, read, error);
		}
	}

}

//int IDs = 0;

ReadBuffer::ReadBuffer(unsigned char* buffer, unsigned int size, bool* error) {
	this->size = 0;
	this->dataSize = 0;
	this->position = 0;
	this->rawData = nullptr;
	this->append(buffer, size, error);
}

ReadBuffer::~ReadBuffer() {
	if (this->size != 0) {
		free(this->rawData);
		this->size = 0;
		this->position = 0;
		this->dataSize = 0;
		this->rawData = nullptr;
	}
}

unsigned char ReadBuffer::readByte(bool* error) {
	if (this->dataSize == this->position) {
		*error = true;
		return 0;
	}
	unsigned char byte = this->rawData[this->position];
	this->position++;
	return byte;
}

unsigned short ReadBuffer::readShort(bool* error) {
	if (this->dataSize + 1 == this->position) {
		*error = true;
		return 0;
	}
	position += 2;
	if (endian == Endian::LittleEndian) {
		return (this->rawData[this->position - 2]) + (this->rawData[this->position - 1] << 8);
	} else {
		return (this->rawData[this->position - 1]) + (this->rawData[this->position - 2] << 8);
	}
}

unsigned int ReadBuffer::readInt(bool* error) {
	if (this->dataSize + 3 == this->position) {
		*error = true;
		return 0;
	}
	position += 4;
	if (endian == Endian::LittleEndian) {
		return (this->rawData[this->position - 4]) + (this->rawData[this->position - 3] << 8) + (this->rawData[this->position - 2] << 16) + (this->rawData[this->position - 1] << 24);
	} else {
		return (this->rawData[this->position - 1]) + (this->rawData[this->position - 2] << 8) + (this->rawData[this->position - 3] << 16) + (this->rawData[this->position - 4] << 24);
	}
}

unsigned char* ReadBuffer::readArray(unsigned int length, bool* error) {
	if (this->position + length > this->dataSize) {
		*error = true;
		return 0;
	}
	MALLOC_N(data, unsigned char, length, {*error = true; return 0; });
	memcpy(data, &(this->rawData[this->position]), sizeof(unsigned char)* length);
	this->position += length;
	return data;
}

unsigned char* ReadBuffer::readFixedLengthString(unsigned int length, bool* error) {
	if (this->position + length > this->dataSize) {
		*error = true;
		return 0;
	}
	MALLOC_N(data, unsigned char, length + 1, {*error = true; return 0; });
	memcpy(data, &(this->rawData[this->position]), sizeof(unsigned char)* length);
	this->position += length;
	data[length] = 0;
	return data;
}

unsigned int ReadBuffer::getPosition() {
	return this->position;
}

void ReadBuffer::setPosition(unsigned int position) {
	if (position > this->dataSize) {
		throw 1;
	}
	this->position = position;
}

bool ReadBuffer::isDone() {
	return this->position >= this->dataSize;
}

void ReadBuffer::append(unsigned char* buffer, int size, bool* error) {
	if (this->dataSize + size > this->size) { // Not enough space
		if (this->rawData != nullptr) { // There is something already
			unsigned char* toFree = this->rawData;
			unsigned int newSize = this->size * 2;
			MALLOC(this->rawData, unsigned char, newSize, {*error = true; free(toFree); this->rawData = nullptr; this->size = 0; return; });
			memcpy(this->rawData, toFree, this->size);
			this->size = newSize;
			free(toFree);
			this->append(buffer, size, error);
		} else { // Not enough space, but no previous data
			unsigned int newSize = READBUFFER_BUFFER_SIZE;
			MALLOC(this->rawData, unsigned char, newSize, {*error = true;  this->rawData = nullptr; this->size = 0; return; });
			this->size = newSize;
			this->append(buffer, size, error);
		}
	} else { // Enough space
		memcpy(&(this->rawData[this->dataSize]), buffer, size);
		this->dataSize += size;
	}
}

WriteBuffer::WriteBuffer() {
	this->size = 0;
	this->position = 0;
	this->dataSize = 0;
	this->rawData = nullptr;
}

WriteBuffer::~WriteBuffer() {
	if (this->size != 0) {
		if (this->rawData != nullptr) {
			free(this->rawData);
			this->rawData = nullptr;
		}
		this->size = 0;
		this->dataSize = 0;
		this->position = 0;
	}
}

void WriteBuffer::writeByte(unsigned char value, bool* error) {
	this->ensureEnoughSpace(256, error);
	if (*error) {
		return;
	}
	this->rawData[this->position] = value;
	this->position++;
	if (this->position > this->dataSize) {
		this->dataSize = this->position;
	}
}

void WriteBuffer::writeShort(unsigned short value, bool* error) {
	if (endian == Endian::LittleEndian) {
		this->writeByte((value >> 0) & 0xff, error);
		if (*error) {
			return;
		}
		this->writeByte((value >> 8) & 0xff, error);
	} else {
		this->writeByte((value >> 8) & 0xff, error);
		if (*error) {
			return;
		}
		this->writeByte((value >> 0) & 0xff, error);
	}
}

void WriteBuffer::writeInt(unsigned int value, bool* error) {
	if (endian == Endian::LittleEndian) {
		this->writeShort((value >> 0) & 0xffff, error);
		if (*error) {
			return;
		}
		this->writeShort((value >> 16) & 0xffff, error);
	} else {
		this->writeShort((value >> 16) & 0xffff, error);
		if (*error) {
			return;
		}
		this->writeShort((value >> 0) & 0xffff, error);
	}
}

void WriteBuffer::writeArray(unsigned char* data, unsigned int length, bool* error) {
	this->ensureEnoughSpace(length, error);
	if (*error) {
		return;
	}
	memcpy(&(this->rawData[this->dataSize]), data, length);
	this->position += length;
	if (this->position > this->dataSize) {
		this->dataSize = this->position;
	}
}

void WriteBuffer::writeFixedLengthString(unsigned char* string, bool* error) {
	int length = strlen(( char*) string);
	this->writeArray(string, length, error);
}

void WriteBuffer::writeZeroDelimitedString(unsigned char* string, bool* error) {
	this->writeFixedLengthString(string, error);
	if (*error) {
		return;
	}
	this->writeByte(0, error);
}

unsigned int WriteBuffer::getPosition() {
	return this->position;
}

void WriteBuffer::setPosition(unsigned int position) {
	this->position = position;
}

void WriteBuffer::getWrittenData(unsigned char** dataPtr, unsigned int* lengthPtr) {
	*dataPtr = this->rawData;
	*lengthPtr = this->dataSize;
}

void WriteBuffer::writeToFile(char* file, bool* error) {
	FILE* f;
	if (!fopen_s(&f, file, "wb")) {
		fwrite(this->rawData, 1, this->dataSize, f);
		fclose(f);
		return;
	}
	*error = true;
}

void WriteBuffer::expandBuffer(bool* error) {

	if (this->rawData != nullptr) { // There is something already
		unsigned char* toFree = this->rawData;
		unsigned int newSize = this->size * WRITEBUFFER_INCREASE_FACTOR;
		MALLOC(this->rawData, unsigned char, newSize, {free(toFree); *error = true; return; });
		memcpy(this->rawData, toFree, this->size);
		this->size = newSize;
		free(toFree);
	} else { // Not enough space, but no previous data
		unsigned int newSize = WRITEBUFFER_BUFFERSIZE;
		MALLOC(this->rawData, unsigned char, newSize, {*error = true; return; });
		this->size = newSize;
	}
}

void WriteBuffer::ensureEnoughSpace(unsigned int neededLength, bool* error) {
	if (this->dataSize + neededLength > this->size) {
		this->expandBuffer(error);
		if (*error) {
			return;
		}
		this->ensureEnoughSpace(neededLength, error);
	}
}

char* getSizeStr(int bytes) {
	double b = bytes;
	if (b < 1024 * 10) {
		return "%.0f bytes";
	}
	b /= 1024;
	if (b < 1024 * 10) {
		return "%.0f kB";
	}
	return "%.2f MB";

}
double getSizeInt(int bytes) {

	double b = bytes;
	if (b < 1024 * 10) {
		return b;
	}
	b /= 1024;
	if (b < 1024 * 10) {
		return b;
	}
	return b / 1024;

}

#ifdef INCLUDE_MINIZ

void compress(char* data, unsigned int length, char** outputData, unsigned int* outputLength, bool* error) {
	unsigned long uncompressedLength = length;
	unsigned long compressedLength = mz_compressBound(uncompressedLength);
	char* uncompressedData = data;
	MALLOC_N(compressedData, unsigned char, uncompressedLength + 4, {*error = true; return; });


	// Compress the string.
	int cmp_status = mz_compress2(compressedData + 4, &compressedLength, ( const unsigned char*) uncompressedData, uncompressedLength, 10); // Maximum compression
	if (cmp_status != _MZ_OK) {
		free(compressedData);
		*error = true;
		return;
	}
	compressedData[0] = ( char) ((length >> 24) & 0xff);
	compressedData[1] = ( char) ((length >> 16) & 0xff);
	compressedData[2] = ( char) ((length >> 8) & 0xff);
	compressedData[3] = ( char) ((length >> 0) & 0xff);

	*outputData = ( char*) compressedData;
	*outputLength = ( unsigned int) compressedLength + 4;
}

void decompress(char* data, unsigned int dataLength, char** outputData, unsigned int* outputLength, bool* error) {
	unsigned char* udata = ( unsigned char*) data;
	unsigned long uncompressedLength = (udata[0] << 24) + (udata[1] << 16) + (udata[2] << 8) + (udata[3] << 0);
	unsigned long compressedLength = dataLength - 4;
	char* compressedData = data + 4;
	MALLOC_N(decompressedData, unsigned char, uncompressedLength, {*error = true; return; });

	// Compress the string.
	int cmp_status = mz_uncompress(decompressedData, &uncompressedLength, ( const unsigned char*) (compressedData), compressedLength);
	if (cmp_status != _MZ_OK) {
		free(decompressedData);
		*error = true;
		return;
	}
	*outputData = ( char*) decompressedData;
	*outputLength = ( unsigned int) uncompressedLength;
}
#endif