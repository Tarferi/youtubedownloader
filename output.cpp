#include "output.h"
#include "json.h"

bool write(WriteBuffer* wb, char* message, bool newLineAfter, bool isError) {
	bool error = false;
	wb->writeByte(isError ? 1 : 0, &error);
	if (error) { return false; }

	wb->writeInt(strlen(message) + (newLineAfter ? 3 : 1), &error);
	if (error) { return false; }

	wb->writeArray(( unsigned char*) message, strlen(message), &error);
	if (error) { return false; }

	if (newLineAfter) {
		wb->writeArray((unsigned char*) "\r\n", strlen("\r\n"), &error);
		if (error) { return false; }
	}
	wb->writeByte(0, &error);
	if (error) { return false; }
	return true;

}

bool write(WriteBuffer* wb, unsigned int number, bool newLineAfter, bool isError) {
	char buffer[32];
	sprintf_s(buffer, "%d", number);
	return write(wb, buffer, newLineAfter, isError);
}

bool ConsoleOutput::printError(char* errorMessage) {
	return write(wb, errorMessage, true, true);
}

bool ConsoleOutput::printVideoNameAndDuration(char* videoName, unsigned int duration) {
	if (!write(wb, "Video name: ", false, false)) { return false; }
	if (!write(wb, videoName, true, false)) { return false; }
	if (!write(wb, "Video length: ", false, false)) { return false; }
	if (!write(wb, duration, false, false)) { return false; }
	if (!write(wb, " seconds", true, false)) { return false; }
	return false;
}

bool ConsoleOutput::printNoAvailableFormats() {
	return write(wb, "No available formats", true, false);
}

bool ConsoleOutput::printFormatsHeader(OutputFormat format, unsigned int totalItems) {
	if (!write(wb, "", true, false)) { return false; } // Empty line
	if (format == OutputFormat::AUDIO_AND_VIDEO) {
		if (!write(wb, "Available formats", false, false)) { return false; }
	} else if (format == OutputFormat::AUDIO_ONLY) {
		if (!write(wb, "Available video formats", false, false)) { return false; }
	} else if (format == OutputFormat::VIDEO_ONLY) {
		if (!write(wb, "Available audio formats", false, false)) { return false; }
	} else {
		return false;
	}
	if (!write(wb, " (", false, false)) { return false; }
	if (!write(wb, totalItems, false, false)) { return false; }
	if (!write(wb, "): ", true, false)) { return false; }
	return true;
}

bool ConsoleOutput::printItem(OutputFormat oformat, unsigned int index, char* quality, char* format, unsigned int length) {
	if (!write(wb, "\t", false, false)) { return false; }
	if (!write(wb, index, false, false)) { return false; }
	if (!write(wb, ": [", false, false)) { return false; }
	if (!write(wb, quality, false, false)) { return false; }
	if (!write(wb, "] - ", false, false)) { return false; }
	SIZE_TO_STRBUF(sizeStr, length);
	if (!write(wb, sizeStr, false, false)) { return false; }
	if (!write(wb, " {", false, false)) { return false; }
	if (!write(wb, format, false, false)) { return false; }
	if (!write(wb, "}", true, false)) { return false; }
	return true;
}

bool ConsoleOutput::printDownloadeditem(OutputFormat oformat, unsigned int index, char* quality, char* format, unsigned int length, char* fileName, char* directURL) {
	if (!write(wb, "Downloaded item: ", true, false)) { return false; }
	if (!write(wb, "\t         ID: ", false, false)) { return false; }
	if (!write(wb, index, true, false)) { return false; }
	if (!write(wb, "\t     Format: ", false, false)) { return false; }
	if (!write(wb, format, true, false)) { return false; }
	if (!write(wb, "\t    Quality: ", false, false)) { return false; }
	if (!write(wb, quality, true, false)) { return false; }
	if (!write(wb, "\t       Size: ", false, false)) { return false; }
	SIZE_TO_STRBUF(sizeStr, length);
	if (!write(wb, sizeStr, true, false)) { return false; }
	if (!write(wb, "\tOutput file: ", false, false)) { return false; }
	if (!write(wb, fileName, true, false)) { return false; }
	if (!write(wb, "", true, false)) { return false; }
	return true;
}

bool ConsoleOutput::print() {
	unsigned char* sdata;
	unsigned int length;
	wb->getWrittenData(&sdata, &length);
	bool error = false;
	ReadBuffer rb(sdata, length, &error);
	if (error) { return false; }
	while (rb.getPosition() != length) {
		bool isError = rb.readByte(&error) == 1;
		if (error) { return false; }
		unsigned int len = rb.readInt(&error);
		if (error) { return false; }
		unsigned char* message = rb.readArray(len, &error);
		if (error) { return false; }
		fprintf(error ? stderr : stdout, "%s", message);
		free(message);
	}
	return true;
}

ConsoleOutput* ConsoleOutput::create() {
	return new ConsoleOutput();
}

ConsoleOutput::~ConsoleOutput() {
	delete wb;
}

ConsoleOutput::ConsoleOutput() {
	this->wb = new WriteBuffer();
}

Output::~Output() {}


bool append(JsonObject* obj, char* key, char* string, bool mustNotExit) {
	if (obj->containsString(key) && mustNotExit) {
		return false;
	}
	JsonString* newString = new JsonString(string);
	if (!newString->isValid) {
		delete newString;
		return false;
	}
	if (!obj->add(key, newString)) {
		delete newString;
		return false;
	}
	return true;
}

bool append(JsonObject* obj, char* key, unsigned int value, bool mustNotExit) {
	if (obj->containsString(key) && mustNotExit) {
		return false;
	}
	JsonNumber* newNumber = new JsonNumber(value);
	if (!newNumber->isValid) {
		delete newNumber;
		return false;
	}
	if (!obj->add(key, newNumber)) {
		delete newNumber;
		return false;
	}
	return true;
}

bool JSONOutput::printError(char* errorMessage) {
	JsonObject* obj = ( JsonObject*) _obj;
	if (!obj->containsArray("errors")) {
		JsonArray* errors = new JsonArray();
		if (!errors->isValid) {
			delete errors;
			return false;
		}
		if (!obj->add("errors", errors)) {
			delete errors;
			return false;
		}
	}
	JsonArray* errors = obj->getArray("errors");
	JsonString* newString = new JsonString(errorMessage);
	if (!newString->isValid) {
		delete newString;
		return false;
	}
	if (!errors->add(newString)) {
		delete newString;
		return false;
	}
	return true;
}

bool JSONOutput::printVideoNameAndDuration(char* videoName, unsigned int duration) {
	JsonObject* obj = ( JsonObject*) _obj;
	if (!append(obj, "VideoName", videoName, true) || !append(obj, "VideoDuration", duration, true)) {
		return false;
	}
	return true;
}

bool JSONOutput::printNoAvailableFormats() {
	JsonObject* obj = ( JsonObject*) _obj;
	if (obj->containsArray("formats")) {
		return false;
	}
	JsonArray* formats = new JsonArray();
	if (!formats->isValid) {
		delete formats;
		return false;
	}
	if (!obj->add("formats", formats)) {
		delete formats;
		return false;
	}
	return true;
}

bool JSONOutput::printFormatsHeader(OutputFormat format, unsigned int totalItems) {
	JsonObject* obj = ( JsonObject*) _obj;
	char* keyName;
	if (format == OutputFormat::AUDIO_AND_VIDEO) {
		keyName = "Formats";
	} else if (format == OutputFormat::AUDIO_ONLY) {
		keyName = "Audio";
	} else if (format == OutputFormat::VIDEO_ONLY) {
		keyName = "Video";
	} else {
		return false;
	}
	if (obj->contains(keyName)) { // Multiple declaration?
		return false;
	}
	JsonArray* arr = new JsonArray();
	if (!arr->isValid) {
		delete arr;
		return false;
	}
	if (!obj->add(keyName, arr)) {
		delete arr;
		return false;
	}
	return true;
}

bool JSONOutput::printItem(OutputFormat oformat, unsigned int index, char* quality, char* format, unsigned int length) {
	JsonObject* obj = ( JsonObject*) _obj;
	char* keyName;
	if (oformat == OutputFormat::AUDIO_AND_VIDEO) {
		keyName = "Formats";
	} else if (oformat == OutputFormat::AUDIO_ONLY) {
		keyName = "Audio";
	} else if (oformat == OutputFormat::VIDEO_ONLY) {
		keyName = "Video";
	} else {
		return false;
	}
	if (!obj->containsArray(keyName)) { // Multiple declaration?
		return false;
	}
	JsonArray* arr = obj->getArray(keyName);
	JsonObject* itemObj = new JsonObject();
	if (!itemObj->isValid) {
		delete itemObj;
		return false;
	}
	if (!arr->add(itemObj)) {
		delete itemObj;
		return false;
	}
	if (!append(itemObj, "Index", index, true)) { return false; }
	if (!append(itemObj, "Quality", quality, true)) { return false; }
	if (!append(itemObj, "Format", format, true)) { return false; }
	if (!append(itemObj, "Size", length, true)) { return false; }
	return true;
}

bool JSONOutput::printDownloadeditem(OutputFormat oformat, unsigned int index, char* quality, char* format, unsigned int length, char* fileName, char* directURL) {
	JsonObject* obj = ( JsonObject*) _obj;
	char* keyName;
	if (oformat == OutputFormat::AUDIO_AND_VIDEO) {
		keyName = "Formats";
	} else if (oformat == OutputFormat::AUDIO_ONLY) {
		keyName = "Audio";
	} else if (oformat == OutputFormat::VIDEO_ONLY) {
		keyName = "Video";
	} else {
		return false;
	}
	if (!obj->containsArray(keyName)) { // Multiple declaration?
		JsonArray* arr = new JsonArray();
		if (!arr->isValid) {
			delete arr;
			return false;
		}
		if (!obj->add(keyName, arr)) {
			delete arr;
			return false;
		}
	}
	JsonArray* arr = obj->getArray(keyName);
	JsonObject* itemObj = new JsonObject();
	if (!itemObj->isValid) {
		delete itemObj;
		return false;
	}
	if (!arr->add(itemObj)) {
		delete itemObj;
		return false;
	}
	if (!append(itemObj, "Index", index, true)) { return false; }
	if (!append(itemObj, "Quality", quality, true)) { return false; }
	if (!append(itemObj, "Format", format, true)) { return false; }
	if (!append(itemObj, "Size", length, true)) { return false; }
	if (!append(itemObj, "FileName", fileName, true)) { return false; }
	if (!append(itemObj, "DirectURL", directURL, true)) { return false; }
	return true;
}

bool JSONOutput::print() {
	WriteBuffer wb;
	JsonObject* obj = ( JsonObject*) _obj;
	if (!obj->write(&wb)) {
		return false;
	}
	bool error = false;
	wb.writeByte(0, &error);
	if (error) { return false; }

	unsigned char* json;
	unsigned int jsonLen;
	wb.getWrittenData(&json, &jsonLen);
	fprintf(stdout, "%s", json);
	return true;
}

JSONOutput* JSONOutput::create() {
	JSONOutput* output = new JSONOutput();
	if (output->_obj == nullptr) {
		delete output;
		return nullptr;
	}
	return output;
}

JSONOutput::JSONOutput() {
	JsonObject* obj = new JsonObject();
	if (!obj->isValid) {
		delete obj;
		return;
	}
	this->_obj = obj;
}

JSONOutput::~JSONOutput() {
	if (this->_obj != nullptr) {
		JsonObject* obj = ( JsonObject*) _obj;
		delete obj;
		this->_obj = nullptr;
	}
}

JSONOutput::Items* JSONOutput::getItems() {
	JsonObject* obj = ( JsonObject*) _obj;
	if (!obj->containsString("VideoName")) { return nullptr; }
	char* videoName = obj->getString("VideoName")->getValue();
	if (!obj->containsNumber("VideoDuration")) { return nullptr; }
	unsigned int videoLength = atoi(obj->getNumber("VideoDuration")->value);

	if (!obj->containsArray("Formats") || !obj->containsArray("Video") || !obj->containsArray("Audio")) { return nullptr; }
	JsonArray* formats = obj->getArray("Formats");
	JsonArray* video = obj->getArray("Video");
	JsonArray* audio = obj->getArray("Audio");

	auto converter = [](JsonArray* array, Array<JSONOutput::Item*>* arr) {
		for (unsigned int i = 0; i < array->values->getSize(); i++) {
			JsonValue* val = array->values->get(i);
			if (!val->isObject()) {
				return false;
			}
			JsonObject* o = ( JsonObject*) val;
			if (!o->containsNumber("Index") || !o->containsString("Quality") || !o->containsString("Format") || !o->containsNumber("Size")) {
				return false;
			}
			unsigned int index = atoi(o->getNumber("Index")->value);
			unsigned int size = atoi(o->getNumber("Size")->value);
			char* quality = o->getString("Quality")->getValue();
			char* format = o->getString("Format")->getValue();

			JSONOutput::Item* item = new JSONOutput::Item(index, format, quality, size);
			if (!arr->append(item)) {
				delete item;
				return false;
			}
		}

		return true;
	};

	JSONOutput::Items* items = new JSONOutput::Items(videoName, videoLength);

	if (!converter(formats, items->videoAndAudio) || !converter(video, items->videoOnly) || !converter(audio, items->audioOnly)) {
		delete items;
		return nullptr;
	}

	return items;
}
