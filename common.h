#pragma once
#ifdef _DEBUG
#define DUMP_MEMLEAKS
#endif


#ifdef DUMP_MEMLEAKS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "stdlib.h"
#include <stdarg.h>
#include "string.h"
#include <stdio.h>
#include <cstdint>


#define INCLUDE_MINIZ

namespace nsEndian {
	enum Enum {
		BigEndian,
		LittleEndian
	};
}

typedef nsEndian::Enum Endian;

typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef int8_t bool8;

#define PRINT_R(ignore, format, ...) {char buffer[2048]; sprintf_s(buffer, format, __VA_ARGS__); wb->writeFixedLengthString((unsigned char*) buffer, error); if(*error) {return;}}

#define MALLOC(target, type, size, failBlock) target = (type*) malloc(sizeof(type)*(size)); if(!target){failBlock};

#define MALLOC_N(target, type, size, failBlock) type* target; MALLOC(target, type, size, failBlock);

#define COMMON_CONSTR_SEC_DER(name, base) name(unsigned char* name, unsigned int size, ReadBuffer* buffer, bool isSanc) : base(name, size, buffer, isSanc) {};

#define COMMON_CONSTR_SEC(name) name(unsigned char* name, unsigned int size, ReadBuffer* buffer) : Section(name, size, buffer) {};

#define COMMON_CONSTR_SEC_BS(name) name(unsigned char* name, unsigned int size, ReadBuffer* buffer) : BasicSection(name, size, buffer) {};

#define ENDS_WIDTH(name, suffix) (strlen(name) >= strlen(suffix) ? !strcmp(&(name[strlen(name) - strlen(suffix)]), (char*) suffix): false)

#define GET_CLONED_DATA_SZ(target, type, string, length, spec_sz, failBlock) MALLOC_N(target, type, length + spec_sz, failBlock);if(target){ memcpy(target, string, length*sizeof(type));}

#define GET_CLONED_DATA(target, type, string, length, failBlock) GET_CLONED_DATA_SZ(target, type, string, length, 0, failBlock)

#define GET_CLONED_STRING_LEN(target, string, length, failBlock) GET_CLONED_DATA_SZ(target, char, string, length, 1, failBlock); target[length] = 0

#define GET_CLONED_STRING(target, string, failBlock) GET_CLONED_STRING_LEN(target, string, strlen((char*)string), failBlock)

#define ENDS_WITH(name, suffix) (strlen(name) >= strlen(suffix) ? !strcmp(&(name[strlen(name) - strlen(suffix)]), (char*) suffix): false)

#define STARTS_WITH(name, prefix) (strlen(name) >= strlen(prefix) ? !_memicmp((char*) (name), (char*) (prefix), strlen((char*) prefix)): false)

#define FREE_NOT_NULL(x) if(x!=nullptr){free(x);x=nullptr;}

#define DELETE_NOT_NULL(x) if(x!=nullptr){delete x;x=nullptr;}

#define ARRAY_FOR_EACH(array, item, type, BLOCK) {for(unsigned int i = 0; i < (array)->getSize(); i++) {type item = (array)->get(i);{BLOCK}}}

#define MAP_FOR_EACH(map, key, keyType, value, valueType, BLOCK) {\
	for(unsigned int i = 0; i < (map)->getSize(); i++) {\
		keyType key;\
		valueType value;\
		(map)->get(i, &key, &value);\
		{\
			BLOCK\
		}\
	}\
}

#define SIZE_TO_STRBUF(bufName, size) char bufName[32]; if (size == 0) {sprintf_s(bufName, "undetermined");} else {sprintf_s(bufName, getSizeStr(size), getSizeInt(size));}

#define ARRAY_DEFAULT_SIZE 64
#define ARRAY_INCREASE_FACTOR 2;


static int initValue = ARRAY_DEFAULT_SIZE;

#define LOG_ERROR(section, fmt, ...) fprintf(stderr, "[" section "] " fmt "\n" , __VA_ARGS__);

#define LOG_LEAK(addr)  fprintf(stderr, "Leaking 0x%X\n", ((unsigned int)addr));

char* getSizeStr(int bytes);

double getSizeInt(int bytes);

template<typename type> class Array {

	class ArrayProxy {
		Array* array;
		int index;
	public:
		ArrayProxy(Array* array, int index) {
			this->array = array;
			this->index = index;
		}
		type operator= (type value) { array->set(index, value); return array->get(index); }
		operator type() { return array->get(index); }

	};

public:

	void remove(unsigned int index) {
		for (unsigned int i = index; i < this->dataSize - 1; i++) {
			this->rawData[i] = this->rawData[i + 1];
		}
		this->rawData[this->dataSize - 1] = (type) nullptr;
		this->dataSize--;
	}

	bool set(unsigned int index, type value) {
		bool error = false;
		if (index > this->size) {
			this->ensureAdditionalSize(this->size - index, &error);
		}
		if (error) {
			return false;
		}
		this->rawData[index] = value;
		if (index > this->dataSize) {
			this->dataSize = index + 1;
		}
		return true;
	}

	type get(unsigned int index) {
		return this->rawData[index];
	}

	unsigned int getSize() {
		return this->dataSize;
	}

	bool append(type value) {
		bool error = false;
		if (this->dataSize + sizeof(type) >= this->size) {
			this->ensureAdditionalSize(32 * sizeof(type), &error);
		}
		if (error) {
			return false;
		}
		this->rawData[this->dataSize] = value;
		this->dataSize++;
		return true;
	}

	bool insert(unsigned int index, type value) {
		if (!append(value)) { return false; }
		for (unsigned int i = this->dataSize - 1; i > index; i--) {
			this->rawData[i] = this->rawData[i - 1];
		}
		this->rawData[index] = value;
		return true;
	}

	bool indexOf(type value, unsigned int* index) {
		for (unsigned int i = 0; i < this->getSize(); i++) {
			type fn = this->get(i);
			if (fn == value) {
				*index = i;
				return true;
			}
		}
		return false;
	}

	void freeItems() {
		for (unsigned int i = 0; i < this->getSize(); i++) {
			type fn = this->get(i);
			free(fn);
		}
	}

	ArrayProxy operator[] (unsigned int index) {
		return ArrayProxy(this, index);
	}

	~Array() {
		if (this->rawData != nullptr) {
			free(this->rawData);
			this->rawData = nullptr;
		}
	}


private:

	type* rawData = nullptr;

	unsigned int size = 0;

	unsigned  int dataSize = 0;

	void ensureAdditionalSize(unsigned int size, bool* error) {
		if (this->dataSize + size > this->size) {
			if (this->rawData != nullptr) {
				unsigned int newSize = this->size * ARRAY_INCREASE_FACTOR;
				MALLOC_N(newData, type, newSize, {*error = true; return;});
				memset(newData, 0, newSize * sizeof(type));
				memcpy(newData, this->rawData, this->size * sizeof(type));
				this->size = newSize;
				free(this->rawData);
				this->rawData = newData;
			} else {
				unsigned int newSize = ARRAY_DEFAULT_SIZE;
				MALLOC(this->rawData, type, newSize, {*error = true; return;});
				memset(this->rawData, 0, newSize * sizeof(type));
				this->size = newSize;
				this->dataSize = 0;
			}
			this->ensureAdditionalSize(size, error);
		}
	}

};

template<typename keyType, typename valueType> class Map {

	struct MapItem {
		keyType key;
		valueType value;
	};

public:
	Map(bool(*comparator)(keyType x1, keyType x2)) {
		this->comparator = comparator;
	}

	virtual ~Map() {
		ARRAY_FOR_EACH(&keys, item, MapItem*, {
			free(item);
			});
	}

	bool contains(keyType key) {
		ARRAY_FOR_EACH(&keys, item, MapItem*, {
			if (comparator(item->key, key)) {
				return true;
			}
			});
		return false;
	}

	bool indexOf(keyType key, unsigned int* index) {
		unsigned int i = 0;
		ARRAY_FOR_EACH(&keys, item, MapItem*, {
			if (comparator(item->key, key)) {
				*index = i;
				return true;
				}
			i++;
			});
		return false;
	}

	bool put(keyType key, valueType value /*, char* pos */) {
		if (contains(key)) {
			ARRAY_FOR_EACH(&keys, item, MapItem*, {
				if (comparator(item->key, key)) {
					return item->value = value;
				}
				});
			return true;
		}

		MALLOC_N(item, MapItem, 1, {return false;});
		/*
		char buffer[512];
		sprintf_s(buffer, "Alloc %x at %s\r\n", (int)(int*)item, pos);
		OutputDebugStringA(buffer);
		*/
		item->key = key;
		item->value = value;
		if (!keys.append(item)) {
			free(item);
			return false;
		}
		return true;
	}

	valueType get(keyType key) {
		ARRAY_FOR_EACH(&keys, item, MapItem*, {
			if (comparator(item->key, key)) {
				return item->value;
			}
			});
		return (valueType) nullptr;
	}

	void freeItems() {
		ARRAY_FOR_EACH(&keys, item, MapItem*, {
			free(item->key);
			free(item->value);
			});
	}

	unsigned int getSize() {
		return keys.getSize();
	}

	bool get(unsigned int index, keyType* key, valueType* value) {
		MapItem* item = keys[index];
		*key = item->key;
		*value = item->value;
		return true;
	}

private:
	Array<MapItem*> keys;
	bool(*comparator)(keyType x1, keyType x2);
};

#define LIBRARY_API __declspec(dllexport)


#define READBUFFER_BUFFER_SIZE 1024*10

class ReadBuffer {

public:

	ReadBuffer(FILE* file, bool* error);
	ReadBuffer(unsigned char* buffer, unsigned int size, bool* error);
	~ReadBuffer();

	unsigned char readByte(bool* error);
	unsigned short readShort(bool* error);
	unsigned int readInt(bool* error);
	unsigned char* readArray(unsigned int length, bool* error);
	unsigned char* readFixedLengthString(unsigned int length, bool* error);

	unsigned int getPosition();
	void setPosition(unsigned int position);
	bool isDone();

	bool good = true;

	unsigned int getDataSize() {
		return this->dataSize;
	}

	Endian endian = Endian::LittleEndian;
private:

	unsigned int size = 0;
	unsigned int position = 0;
	unsigned int dataSize = 0;
	unsigned char* rawData = nullptr;

	void append(unsigned char* buffer, int size, bool* error);
};

#define WRITEBUFFER_BUFFERSIZE 1024*10
#define WRITEBUFFER_INCREASE_FACTOR 2

class WriteBuffer {
public:
	WriteBuffer();
	~WriteBuffer();

	void writeByte(unsigned char value, bool* error);
	void writeShort(unsigned short value, bool* error);
	void writeInt(unsigned int value, bool* error);
	void writeArray(unsigned char* data, unsigned int length, bool* error);
	void writeFixedLengthString(unsigned char* string, bool* error);
	void writeZeroDelimitedString(unsigned char* string, bool* error);

	unsigned int getPosition();
	void setPosition(unsigned int position);
	void getWrittenData(unsigned char** dataPtr, unsigned int* lengthPtr);

	void writeToFile(char* file, bool* error);

	Endian endian = Endian::LittleEndian;
private:
	unsigned char* rawData;
	unsigned int size;
	unsigned int dataSize;
	unsigned int position;

	void expandBuffer(bool* error);
	void ensureEnoughSpace(unsigned int neededLength, bool* error);
};

#ifdef INCLUDE_MINIZ
#include "miniz.h"
void compress(char* data, unsigned int length, char** outputData, unsigned int* outputLength, bool* error);
void decompress(char* data, unsigned int dataLength, char** outputData, unsigned int* outputLength, bool* error);
#endif