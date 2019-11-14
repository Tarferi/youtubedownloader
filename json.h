#pragma once
#include "common.h"

class JsonString;
class JsonObject;
class JsonNull;
class JsonBoolean;
class JsonArray;
class JsonNumber;

class JsonValue {
public:

	void store(char* fileName) {
		FILE* f;
		WriteBuffer wb;
		if (this->write(&wb)) {
			if (!fopen_s(&f, fileName, "wb")) {
				unsigned char* data;
				unsigned int length;
				wb.getWrittenData(&data, &length);
				fwrite(data, sizeof(char), length, f);
				fclose(f);
			}
		}
	}

	JsonString* asString() {
		return ( JsonString*) this;
	}

	JsonObject* asObject() {
		return ( JsonObject*) this;
	}

	JsonNull* asNull() {
		return ( JsonNull*) this;
	}

	JsonBoolean* asBoolean() {
		return ( JsonBoolean*) this;
	}

	JsonArray* asArray() {
		return ( JsonArray*) this;
	}

	JsonNumber* asNumber() {
		return ( JsonNumber*) this;
	}

	JsonValue() {}

	static JsonValue* read(char** data);

	virtual ~JsonValue() {

	}

	virtual bool isBoolean() {
		return false;
	}

	virtual bool isString() {
		return false;
	}

	virtual bool isNull() {
		return false;
	}

	virtual bool isObject() {
		return false;
	}

	virtual bool isArray() {
		return false;
	}

	virtual bool isNumber() {
		return false;
	}

	virtual bool write(WriteBuffer* wb) = 0;

	bool isValid = false;
};

class JsonBoolean : public JsonValue {
public:
	virtual bool isBoolean() {
		return true;
	}

	JsonBoolean(char** data) {
		this->value = STARTS_WITH(*data, "true");
		*data += this->value ? strlen("true") : strlen("false");
		this->isValid = true;
	}

	bool value;

	virtual bool write(WriteBuffer* wb) {
		bool err = false;
		wb->writeFixedLengthString(( unsigned char*) (value ? "true" : "false"), &err);
		if (err) {
			return false;
		}
		return true;
	}
};

class JsonString : public JsonValue {
public:

	static bool __strncmp(JsonString* x1, JsonString* x2) {
		return strcmp(x1->getValue(), x2->getValue()) ? false : true;
	}

	virtual bool isString() {
		return true;
	}

	virtual ~JsonString() {
		if (this->value != nullptr) {
			free(this->value);
		}
	}

	bool setValue(char* value) {
		GET_CLONED_STRING(newData, value, {return false;});
		char* originalValue = this->value;
		this->value = newData;
		if (!this->escape()) {
			free(this->value);
			this->value = originalValue;
			return false;
		} else {
			free(originalValue);
		}
		return true;
	}

	char* getValue() {
		return value;
	}

	bool unescape() {
		bool error = false;
		unsigned int len = strlen(this->value);
		unsigned int chars_needed = 1;
		for (unsigned int i = 0; i < len; i++) {
			char c = this->value[i];
			if (c == '\\' && i != len - 1) { // Something escaped
				char n = this->value[i + 1];
				if (n == '"' || n == '\\') {
					i++;
				}
			}
			chars_needed++;
		}

		MALLOC_N(newStr, char, chars_needed, {return false; });
		unsigned int written = 0;
		for (unsigned int i = 0; i < len; i++) {
			char c = this->value[i];
			if (c == '\\' && i != len - 1) { // Something escaped
				char n = this->value[i + 1];
				if (n == '"' || n == '\\') {
					i++;
				}
			}
			newStr[written] = this->value[i];
			written++;
		}
		newStr[written] = 0;
		free(this->value);
		this->value = newStr;

		return true;
	}

	bool escape() {
		unsigned int len = strlen(this->value);
		unsigned int escapeesNeeded = 0;
		for (unsigned int i = 0; i < len; i++) {
			char c = this->value[i];
			if (c == '"') {
				escapeesNeeded++;
			} else if (c == '\\') {
				if (value[i + 1] == 'u') { // Unicode escape
					escapeesNeeded++;
					i += 3;
				} else {
					i++;
				}
			}
		}


		if (escapeesNeeded > 0) {
			GET_CLONED_DATA(newData, char, this->value, len + 1 + escapeesNeeded, {return false;});
			int strI = 0;
			for (unsigned int i = 0; i < len; i++) {
				char c = this->value[i];
				if (c == '"') {
					newData[strI] = '\\';
					strI++;
				} else if (c == '\\') {
					if (i >= len - 1) { // Invalid string
						free(newData);
						return false;
					}
					if (value[i + 1] == 'u') { // Unicode escape
						if (i >= len - 5) { // Invalid string
							free(newData);
							return false;
						}
						char tmp[5];
						tmp[0] = value[i + 2];
						tmp[1] = value[i + 3];
						tmp[2] = value[i + 4];
						tmp[3] = value[i + 5];
						tmp[4] = 0;
						unsigned int val = ( unsigned int) strtol(tmp, nullptr, 16);
						if (val > 0xff) { // Multibyte
							newData[strI] = ( char) (val >> 8);
							strI++;
							newData[strI] = ( char) (val & 0xff);
							strI++;
							i += 5;
						} else { // Single byte
							newData[strI] = ( char) val;
							strI++;
							i += 5;
						}

						continue;
					} else {
						newData[strI] = '\\';
						strI++;
						i++;
						newData[strI] = this->value[i];
						strI++;
						continue;
					}
				}
				newData[strI] = c;
				strI++;
			}
			newData[len + escapeesNeeded] = 0;
			free(this->value);
			this->value = newData;
		}
		return true;
	}

	JsonString(char* data) {

		GET_CLONED_STRING(newData, data, {return;});
		this->value = newData;
		if (!escape()) {
			free(this->value);
			this->value = nullptr;
			return;
		}
		this->isValid = true;
	}

	JsonString(char** data) {
		char* str = *data;
		str++;
		unsigned int len = strlen(str);
		for (unsigned int i = 0; i < len; i++) {
			char c = str[i];
			if (c == '\\') {
				i++;
				continue;
			} else if (c == '"') {
				GET_CLONED_STRING_LEN(newStr, str, i, {return;});
				this->value = newStr;
				*data += i + 2;
				this->isValid = true;
				if (!escape()) {
					free(this->value);
					this->value = nullptr;
				}
				return;
			}
		}
	}

	virtual bool write(WriteBuffer* wb) {
		bool err = false;
		wb->writeFixedLengthString(( unsigned char*) ("\""), &err);
		if (err) { return false; }
		wb->writeFixedLengthString(( unsigned char*) value, &err);
		if (err) { return false; }
		wb->writeFixedLengthString(( unsigned char*) ("\""), &err);
		if (err) { return false; }
		return true;
	}

private:

	char* value = nullptr;
};

class JsonNull : public JsonValue {
public:
	virtual bool isNull() {
		return true;
	}

	JsonNull(char** data) {
		*data += strlen("null");
		this->isValid = true;
	}


	virtual bool write(WriteBuffer* wb) {
		bool err = false;
		wb->writeFixedLengthString(( unsigned char*) ("null"), &err);
		if (err) {
			return false;
		}
		return true;
	}
};

class JsonNumber : public JsonValue {
public:

	virtual bool isNumber() {
		return true;
	}

	virtual ~JsonNumber() {
		if (value != nullptr) {
			free(value);
			value = nullptr;
		}
	}

	JsonNumber(unsigned int number) {
		this->isValid = false;
		char buffer[32];
		sprintf_s(buffer, "%d", number);
		GET_CLONED_STRING(newString, buffer, {return;});
		this->value = newString;
		this->isValid = true;
	}

	JsonNumber(char** data) {
		char* str = *data;
		unsigned int len = strlen(str);
		for (unsigned int i = 0; i < len; i++) {
			char c = str[i];
			if (c >= '0' && c <= '9') {
				continue;
			} else if (c == '.') {
				continue;
			} else if (c == 'f') {
				continue;
			} else if (c == '+' || c == '-') {
				continue;
			}
			GET_CLONED_DATA(newStr, char, str, i + 1, {return;});
			newStr[i] = 0;
			*data = str + i;
			value = newStr;
			isValid = true;
			return;
		}
	}

	char* value = nullptr;

	virtual bool write(WriteBuffer* wb) {
		bool err = false;
		wb->writeFixedLengthString(( unsigned char*) value, &err);
		if (err) {
			return false;
		}
		return true;
	}
};

class JsonArray : public JsonValue {
public:

	virtual bool isArray() {
		return true;
	}

	JsonArray() {
		values = new Array<JsonValue*>();
		this->isValid = true;
	}

	bool add(char* value) {
		JsonString* valueStr = new JsonString(value);
		if (valueStr->getValue() == nullptr) { delete valueStr; return false; }
		if (!add(valueStr)) {
			delete valueStr;
			return false;
		}
		return true;
	}

	bool add(JsonValue* value) {
		if (!values->append(value)) { return false; }
		return true;
	}

	void clear() {
		system("cls");
	}

	JsonArray(char** data) {
		values = new Array<JsonValue*>();
		*data = *data + 1;
		while (true) {
			JsonValue* val = JsonValue::read(data);
			if (val == nullptr) {
				if (values->getSize() == 0 && (*data)[0] == ']') { // Empty array
					this->isValid = true;
					*data = *data + 1;
				} else {
					this->isValid = false;
					return;
				}
			} else if (!val->isValid) {
				delete val;
				this->isValid = false;
				return;
			}
			if (!values->append(val)) {
				delete val;
				return;
			}

			char* str = *data;
			unsigned int len = strlen(str);
			bool cont = false;
			for (unsigned int i = 0; i < len; i++) {
				char c = str[i];
				if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
					continue;
				} else if (c == ',') {
					cont = true;
					*data += i + 1;
					break;
				} else if (c == ']') {
					*data += i + 1;
					this->isValid = true;
					return;
				}
			}
			if (!cont) {
				this->isValid = false;
				return;
			}
		}
	}

	virtual bool write(WriteBuffer* wb) {
		bool err = false;
		wb->writeFixedLengthString((unsigned char*)"[", &err);
		if (err) { return false; }

		int size = values->getSize();
		int index = 0;
		ARRAY_FOR_EACH(values, item, JsonValue*, {
			index++;
			if (!item->write(wb)) {
				return false;
			}
			if (index != size) { // Last
				wb->writeFixedLengthString((unsigned char*)",", &err);
				if (err) { return false; }
			}

			});

		wb->writeFixedLengthString((unsigned char*)"]", &err);
		if (err) { return false; }
		return true;
	}

	virtual ~JsonArray() {
		if (values != nullptr) {
			ARRAY_FOR_EACH(values, item, JsonValue*, {
				delete item;
				});
			delete values;
		}
	}

	Array<JsonValue*>* values = nullptr;
};

class JsonObject : public JsonValue {
public:

	virtual bool isObject() {
		return true;
	}

	bool containsString(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(), data)) {
				return value->isString();
			}
			});
		return false;
	}

	bool containsNumber(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(), data)) {
				return value->isNumber();
			}
			});
		return false;
	}

	bool containsArray(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(), data)) {
				return value->isArray();
			}
			});
		return false;
	}

	bool containsObject(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(), data)) {
				return value->isObject();
			}
			});
		return false;
	}

	bool contains(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(),data)) {
				return true;
			}
			});
		return false;
	}

	JsonValue* get(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(),data)) {
				return value;
			}
			});
		return nullptr;
	}

	JsonString* getString(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(),data)) {
				return ( JsonString*) value;
			}
			});
		return nullptr;
	}

	JsonNumber* getNumber(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(),data)) {
				return ( JsonNumber*) value;
			}
			});
		return nullptr;
	}
	JsonArray* getArray(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(),data)) {
				return ( JsonArray*) value;
			}
			});
		return nullptr;
	}

	JsonObject* getObject(char* data) {
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			if (!strcmp(key->getValue(),data)) {
				return ( JsonObject*) value;
			}
			});
		return nullptr;
	}
	JsonObject() {
		values = new Map<JsonString*, JsonValue*>(JsonString::__strncmp);
		this->isValid = true;
	}

	bool add(char* key, char* value) {
		JsonString* valueStr = new JsonString(value);
		if (!valueStr->isValid) { delete valueStr; return false; }
		if (!add(key, valueStr)) {
			delete valueStr;
			return false;
		}
		return true;
	}

	bool add(char* key, JsonValue* value) {
		if (contains(key)) {
			return false;
		}
		JsonString* keyStr = new JsonString(key);
		if (!keyStr->isValid) { delete keyStr; return false; }
		if (!values->put(keyStr, value)) { delete keyStr; return false; }
		return true;
	}

	JsonObject(char** data) {
		*data = *data + 1;
		values = new Map<JsonString*, JsonValue*>(JsonString::__strncmp);
		while (true) {
			JsonValue* _key = JsonValue::read(data);
			if (_key == nullptr) { // Empty
				if (values->getSize() == 0 && (*data)[0] == '}') { // Empty object
					this->isValid = true;
					*data = *data + 1;
				} else {
					this->isValid = false;
				}
				return;
			} else if (!_key->isString()) {
				delete _key;
				this->isValid = false;
				return;
			} else if (!_key->isValid) {
				delete _key;
				this->isValid = false;
				return;
			}
			JsonString* key = ( JsonString*) _key;

			bool cont = false;
			for (unsigned int i = 0, len = strlen(*data); i < len; i++) {
				char c = (*data)[i];
				if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
					continue;
				} else if (c == ':') {
					cont = true;
					*data += i + 1;
					break;
				}
			}
			if (!cont) {
				this->isValid = false;
				return;
			}

			JsonValue* value = JsonValue::read(data);
			if (value == nullptr) {
				delete key;
				this->isValid = false;
				return;
			}
			if (!value->isValid) {
				delete key;
				delete value;
				this->isValid = false;
				return;
			}
			if (!values->put(key, value)) {
				delete key;
				delete value;
				this->isValid = false;
				return;
			}
			for (unsigned int i = 0, len = strlen(*data); i < len; i++) {
				char c = (*data)[i];
				if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
					continue;
				} else if (c == ',') {
					*data += i + 1;
					break;
				} else if (c == '}') {
					*data += i + 1;
					this->isValid = true;
					return;
				}
			}
		}
	}

	virtual bool write(WriteBuffer* wb) {
		bool err = false;
		wb->writeFixedLengthString((unsigned char*)"{", &err);
		if (err) { return false; }

		int size = values->getSize();
		int index = 0;
		MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
			index++;
			if (!key->write(wb)) {
				return false;
			}
			wb->writeFixedLengthString((unsigned char*)":", &err);
			if (!value->write(wb)) {
				return false;
			}
			if (err) { return false; }
			if (index != size) { // Last
				wb->writeFixedLengthString((unsigned char*)",", &err);
				if (err) { return false; }
			}
			});

		wb->writeFixedLengthString((unsigned char*)"}", &err);
		if (err) { return false; }
		return true;
	}

	virtual ~JsonObject() {
		if (values != nullptr) {
			MAP_FOR_EACH(values, key, JsonString*, value, JsonValue*, {
				delete key;
				delete value;
				});
			delete values;
		}
	}

	Map<JsonString*, JsonValue*>* values = nullptr;
};

inline JsonValue* JsonValue::read(char** _data) {
	char* data = *_data;
	for (unsigned int index = 0, len = strlen(data); index < len; index++) {
		char c = data[index];
		if (c == '"') {
			*_data += index;
			JsonValue* val = new JsonString(_data);
			if (!val->isValid) {
				delete val;
				return nullptr;
			}
			return val;
		} else if (c == '{') {
			*_data += index;
			JsonValue* val = new JsonObject(_data);
			if (!val->isValid) {
				delete val;
				return nullptr;
			}
			return val;
		} else if (c == '[') {
			*_data += index;
			JsonValue* val = new JsonArray(_data);
			if (!val->isValid) {
				delete val;
				return nullptr;
			}
			return val;
		} else if (STARTS_WITH(&data[index], "true") || STARTS_WITH(&data[index], "false")) {
			*_data += index;
			JsonValue* val = new JsonBoolean(_data);
			if (!val->isValid) {
				delete val;
				return nullptr;
			}
			return val;
		} else if (STARTS_WITH(data, "null")) {
			*_data += index;
			JsonValue* val = new JsonNull(_data);
			if (!val->isValid) {
				delete val;
				return nullptr;
			}
			return val;
		} else if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
			continue;
		} else if ((c >= '0' && c <= '9') || (c == '+' || c == '-')) {
			*_data += index;
			JsonValue* val = new JsonNumber(_data);
			if (!val->isValid) {
				delete val;
				return nullptr;
			}
			return val;
		}
		return nullptr;
	}
	return nullptr;
}
