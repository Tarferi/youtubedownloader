#include "Youtube.h"
#include "md5.h"
#include "Json.h"
#include <Windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

Youtube::Youtube(YoutubeSettings* settings) {
	this->settings = settings;
}

Youtube::~Youtube() {
	DELETE_NOT_NULL(this->fmtList);
}

YoutubeVideoInfo* Youtube::getVideo(bool* stop, char* url, bool all) {
	if (this->fmtList == nullptr) {
		this->fmtList = FormatList::create();
		if (this->fmtList == nullptr) {
			return nullptr;
		}
	}
	YoutubeVideoInfo* info = new YoutubeVideoInfo(settings, fmtList, stop, url, all);
	if (!info->valid) {
		delete info;
		return nullptr;
	}
	return info;
}

Array<FormatList::FormatListItem*>* Youtube::getFormats() {
	if (this->fmtList == nullptr) {
		this->fmtList = FormatList::create();
		if (this->fmtList == nullptr) {
			return nullptr;
		}
	}
	return this->fmtList->get();
}

ReadBuffer* YoutubeGetter::read(YoutubeSettings* settings, bool* stop, char* _url) {
	return read(settings, stop, _url, 0, nullptr, nullptr);
}

ReadBuffer* raw_read(bool* stop, wchar_t* url, wchar_t* requestType, bool isHttps, unsigned int remoteLength, DownloadProgress reporter, void* reporterInstance) {
	URL_COMPONENTS urlComp;
	LPCWSTR pwszUrl1 = url;
	DWORD dwUrlLen = 0;
	bool error = false;

	// Initialize the URL_COMPONENTS structure.
	ZeroMemory(&urlComp, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(urlComp);

	// Set required component lengths to non-zero 
	// so that they are cracked.
	urlComp.dwSchemeLength = ( DWORD) -1;
	urlComp.dwHostNameLength = ( DWORD) -1;
	urlComp.dwUrlPathLength = ( DWORD) -1;
	urlComp.dwExtraInfoLength = ( DWORD) -1;

	// Crack the URL.
	if (!WinHttpCrackUrl(pwszUrl1, ( DWORD) wcslen(pwszUrl1), 0, &urlComp)) {
		return nullptr;
	}

	HINTERNET hSession = WinHttpOpen(L"QSounder", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		return nullptr;
	}
	MALLOC_N(hostName, wchar_t, urlComp.dwHostNameLength + sizeof(wchar_t), {WinHttpCloseHandle(hSession); return nullptr; });
	memcpy(hostName, urlComp.lpszHostName, sizeof(wchar_t)* urlComp.dwHostNameLength);
	hostName[urlComp.dwHostNameLength] = 0;



	HINTERNET hConnect = WinHttpConnect(hSession, hostName, isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
	if (!hConnect) {
		free(hostName);
		WinHttpCloseHandle(hSession);
		return nullptr;
	}




	HINTERNET hRequest = WinHttpOpenRequest(hConnect, requestType, urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		free(hostName);
		return nullptr;
	}

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		free(hostName);
		return nullptr;
	}

	if (!WinHttpReceiveResponse(hRequest, NULL)) {
		int error = GetLastError();
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		free(hostName);
		return nullptr;
	}
	free(hostName);

	DWORD dwSize = 0;
	WriteBuffer wb;
	DWORD totalRead = 0;
	int prevPerc = -1;
	do {
		// Check for available data.
		if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return nullptr;
		}
		int perc = remoteLength > 0 ? (totalRead * 100) / remoteLength : -1;
		if (prevPerc != perc) {
			prevPerc = perc;
			if (reporter != nullptr) {
				reporter(reporterInstance, totalRead, remoteLength);
			} else {
				char __buffer[1024];
				sprintf_s(__buffer, "Reading %d bytes. Total read: %d\r\n", dwSize, totalRead);
				OutputDebugStringA(__buffer);
			}
		}
		totalRead += dwSize;
		MALLOC_N(buffer, char, dwSize + 1, {
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return nullptr;
			});

		// Read the Data.
		ZeroMemory(buffer, dwSize + 1);
		DWORD dwDownloaded;

		if (!WinHttpReadData(hRequest, ( LPVOID) buffer, dwSize, &dwDownloaded)) {
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			free(buffer);
			return nullptr;
		}

		wb.writeArray(( unsigned char*) buffer, ( unsigned int) dwDownloaded, &error);
		if (error || *stop) {
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			free(buffer);
			return nullptr;
		}
		free(buffer);
	} while (dwSize > 0);
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	wb.writeByte(( unsigned char) 0, &error);
	if (error) {
		return nullptr;
	}
	unsigned char* data;
	unsigned int length;
	wb.getWrittenData(&data, &length);

	ReadBuffer* rb = new ReadBuffer(data, length, &error);
	if (error) {
		delete rb;
		return nullptr;
	}
	if (reporter != nullptr) {
		reporter(reporterInstance, rb->getDataSize(), rb->getDataSize());
	}
	return rb;
}

ReadBuffer* YoutubeGetter::read(YoutubeSettings* settings, bool* stop, char* _url, unsigned int remoteLength, DownloadProgress reporter, void* reporterInstance) {
	bool error = false;
	char md5_buffer[1024];
	if (settings->use_video_cache) {
		char* md5 = getMD5(_url);
		sprintf_s(md5_buffer, "%s.bin", md5);
		free(md5);
		ReadBuffer* rb = settings->cache != nullptr ? settings->cache->get(md5_buffer) : nullptr;
		if (rb != nullptr) {
			return rb;
		}
	}

	wchar_t url[1024 * 10]; // 10 kb url limit, what a waste
	unsigned int conv;
	mbstowcs_s(&conv, url, _url, strlen(_url) + 1);//Plus null

	bool isHttps = STARTS_WITH(_url, "https");

	// Check if range download is possible
	ReadBuffer* rb = raw_read(stop, url, L"HEAD", isHttps, remoteLength, nullptr, nullptr);
	if (rb == nullptr) { return nullptr; }

	delete rb;
	rb = raw_read(stop, url, L"GET", isHttps, remoteLength, reporter, reporterInstance);
	if (rb == nullptr) { return nullptr; }

	if (settings->use_video_cache) {
		unsigned int length = rb->getDataSize();
		unsigned char* data = rb->readArray(length, &error);
		rb->setPosition(0);
		if (error) {
			delete rb;
			return nullptr;
		}
		if (settings->cache != nullptr) {
			settings->cache->put(md5_buffer, ( char*) data, length);
		}
		free(data);
	}
	if (reporter != nullptr) {
		reporter(reporterInstance, rb->getDataSize(), rb->getDataSize());
	}
	return rb;
}

char* URLDecode(char* input) {

#define HEX_DIGIT(x) (x >= 'a' && x <= 'f') ? 10 + (x-'a') : ((x >= 'A' && x <= 'F' ) ? 10 + (x - 'A') : x - '0' )

	unsigned int len = strlen(input);
	MALLOC_N(output, char, len + 1, {return nullptr; });
	unsigned int o = 0;
	for (unsigned int i = 0; i < len; i++, o++) {
		unsigned char c = input[i];
		if (c == '%' && i + 2 < len) {
			char x1 = input[i + 1];
			char x2 = input[i + 2];
			x1 = HEX_DIGIT(x1);
			x2 = HEX_DIGIT(x2);
			c = (x1 << 4) + x2;
			output[o] = c;
			i += 2;
		} else {
			output[o] = c;
		}
	}
	output[o] = 0;
	return output;
}

bool __strncmp(char* x1, char* x2) {
	return !strcmp(x1, x2);
}

JsonObject* parseURL(char* input) {
	JsonObject* map = new JsonObject();
	char* rest = input;
	while (strlen(rest) > 0) {
		char* keyEnd = strstr(rest, "=");
		int diff = keyEnd - rest;
		if (diff < 0) { // Should never happen
			break;
		}
		bool end = false;
		GET_CLONED_STRING_LEN(newKey, rest, diff, {
			delete map;
			return nullptr;
			});
		char* valueEnd = strstr(keyEnd, "&");
		diff = (valueEnd - keyEnd);
		if (diff < 0) {
			end = true;
			diff = strlen(keyEnd);
		}
		GET_CLONED_STRING_LEN(newValue, keyEnd + 1, diff - 1, {
			delete map;
			free(newKey);
			return nullptr;
			});
		if (!map->add(newKey, newValue)) {
			delete map;
			free(newKey);
			free(newValue);
			return nullptr;
		}
		free(newKey);
		free(newValue);
		rest = keyEnd + 1 + diff;
		if (end) {
			break;
		}
	}

	return map;
}

JsonObject* parseAdaptiveString(YoutubeDecipherer* cipher, char* str) {
	JsonObject* obj = parseURL(str);
	if (obj == nullptr) { return nullptr; }
	MAP_FOR_EACH(obj->values, key, JsonString*, value, JsonValue*, {
		char* v = value->asString()->getValue();
		char* newV = URLDecode(v);
		if (newV == nullptr) { delete obj; return nullptr; }
		if (!value->asString()->setValue(newV)) { free(newV); delete obj; return false; }
		free(newV);
		});
	if (!obj->contains("url")) { delete obj; return nullptr; }
	if (obj->contains("sp") && obj->contains("s")) { // Deciphers
		JsonString* url = obj->get("url")->asString();
		JsonString* s = obj->get("s")->asString();
		char* decipherer = cipher->decipher(s->getValue());
		if (decipherer == nullptr) { delete obj; return nullptr; }

		char* appendix = "&sig=";
		unsigned int urlLen = strlen(url->getValue());
		unsigned int appendixLen = strlen(appendix);
		unsigned int deciphererLen = strlen(decipherer);

		MALLOC_N(newUrl, char, urlLen + deciphererLen + appendixLen + 1, {free(decipherer); delete obj; return nullptr; });
		memcpy(newUrl, url->getValue(), sizeof(char)* strlen(url->getValue()));
		memcpy(newUrl + urlLen, "&sig=", sizeof(char) * 5);
		memcpy(newUrl + urlLen + appendixLen, decipherer, sizeof(char)* strlen(decipherer));
		newUrl[urlLen + deciphererLen + appendixLen] = 0;
		free(decipherer);
		if (!url->setValue(newUrl)) {
			free(newUrl);
			delete obj;
			return nullptr;
		}
		free(newUrl);
	}
	return obj;
}

JsonArray* getStreams(YoutubeDecipherer* cipher, JsonObject* info, char* name) {

	if (!info->contains(name)) { return new JsonArray(); }
	JsonValue* _adaptives = info->get(name);
	if (!_adaptives->isString()) { return nullptr; }
	JsonString* adaptives = _adaptives->asString();
	char* adaptivesDecoded = URLDecode(adaptives->getValue());
	if (adaptivesDecoded == nullptr) { return nullptr; }

	JsonArray* output = new JsonArray();
	int restLen = strlen(adaptivesDecoded);
	char* rest = adaptivesDecoded;
	while (true) {
		char* delimPos = strstr(rest, ",");
		int diff = delimPos - rest;
		if (diff < 0 || diff > restLen) { // Last one
			if (strlen(rest) == 0) {
				break;
			}
			JsonObject* item = parseAdaptiveString(cipher, rest);
			if (item == nullptr) { free(adaptivesDecoded); delete output; return false; }
			if (!output->add(item)) { free(adaptivesDecoded); delete item; delete output; return false; }
			break;
		}
		GET_CLONED_DATA(newStr, char, rest, diff + 1, {free(adaptivesDecoded); delete output; return false; });
		newStr[diff] = 0;
		JsonObject* item = parseAdaptiveString(cipher, newStr);
		free(newStr);
		if (item == nullptr) { free(adaptivesDecoded); delete output; return false; }
		if (!output->add(item)) { free(adaptivesDecoded); delete item; delete output; return false; }
		rest = delimPos + 1;
		restLen -= diff + 1;
	}
	free(adaptivesDecoded);
	return output;
}

bool skipHTTPHeader(ReadBuffer* rb) {
	return true;
	unsigned int dataSize = rb->getDataSize();
	unsigned int position = rb->getPosition();
	rb->setPosition(0);
	bool error = false;
	unsigned char* rawData = rb->readArray(dataSize, &error);
	if (error) {
		return false;
	}
	rb->setPosition(position);

	unsigned char previous = 0;
	for (unsigned int i = position; i < dataSize - 4; i++) {
		if (rawData[i] == '\r') {
			if (rawData[i + 1] == '\n') {
				if (rawData[i + 2] == '\r') {
					if (rawData[i + 3] == '\n') {
						position = i + 4;
						rb->setPosition(position);
						free(rawData);
						return true;
					}
				}
			}
		}
	}
	printf("%s", rawData);
	free(rawData);
	return false;
}

char* getInfoPage(YoutubeSettings* settings, bool* stop, char* videoID) {
	WriteBuffer wb;
	bool error = false;
	wb.writeFixedLengthString((unsigned char*) "https://www.youtube.com/get_video_info?el=detailpage&ps=default&eurl=&hl=en_US&video_id=", &error);
	if (error) {
		return nullptr;
	}
	wb.writeZeroDelimitedString(( unsigned char*) videoID, &error);
	if (error) {
		return nullptr;
	}
	char* infoURL;
	unsigned int infoURLLength;
	wb.getWrittenData(( unsigned char**) & infoURL, &infoURLLength);
	YoutubeGetter yg;
	ReadBuffer* rb = yg.read(settings, stop, infoURL);
	if (rb == nullptr) {
		return nullptr;
	}
	if (!skipHTTPHeader(rb)) {
		delete rb;
		return nullptr;
	}
	int len = rb->getDataSize() - rb->getPosition();
	char* rest = ( char*) rb->readArray(len, &error);
	if (error) {
		delete rb;
		return nullptr;
	}
	delete rb;
	return rest;
}

bool YoutubeDecipherer::gatherCiphers(char* code, char* fname) {
	funcs = new Map<char*, CipherFuncType*>(__strncmp);
	{
		char* rest = code;

		// Locate deciptherer
		char* dc1 = "(a){a=a.split(\"\");";
		rest = strstr(code, dc1);
		if (!rest) { return nullptr; }
		rest += strlen(dc1);
		char* ender = strstr(rest, "}");
		while (true) {
			// Locate object separator
			char* objSep = strstr(rest, ".");
			if (!objSep || objSep > ender) { break; }
			objSep++;
			char* parName = strstr(objSep, "(");
			if (!parName) { return false; }
			parName++;

			// Clone function name
			GET_CLONED_STRING_LEN(cfname, objSep, ( int) (parName - objSep) - 1, {return false; });
			if (!strcmp(cfname, "join")) {
				free(cfname);
				break;
			}
			rest = strstr(parName, ")") + 2;

			if (!funcs->contains(cfname)) { // Unknown cipher, figure out what it is
				MALLOC_N(stype, CipherFuncType, 1, {free(cfname); return false; });
				char buffer[1024];
				stype->isReverse = false;
				stype->isSlice = false;
				stype->isSwap = false;
				// Is is slice?
				sprintf_s(buffer, "%s:function(a,b){a.splice(0,b)}", cfname);
				if (strstr(code, buffer)) { stype->isSlice = true; }
				// Reverse?
				sprintf_s(buffer, "%s:function(a){a.reverse()}", cfname);
				if (strstr(code, buffer)) { stype->isReverse = true; }
				// Swap?
				sprintf_s(buffer, "%s:function(a,b){var c=a[0];a[0]=a[b%%a.length];a[b%%a.length]=c}", cfname);
				if (strstr(code, buffer)) { stype->isSwap = true; }
				// Store
				if (!funcs->put(cfname, stype)) { free(cfname); return false; }
			} else {
				free(cfname);
			}
		}

	}

	{
		// Collect correct functions
		char* rest = code;

		// Locate deciptherer
		char* dc1 = "function(a){a=a.split(\"\");";
		rest = strstr(code, dc1);
		if (!rest) { return false; }
		rest += strlen(dc1);
		char* ender = strstr(rest, "}");
		while (true) {
			// Locate object separator
			char* objSep = strstr(rest, ".");
			if (!objSep || objSep > ender) { break; }
			objSep++;
			char* parName = strstr(objSep, "(");
			if (!parName) { return false; }
			parName++;

			// Clone function name
			GET_CLONED_STRING_LEN(cfname, objSep, ( int) (parName - objSep) - 1, {return false; });
			if (!strcmp(cfname, "join")) {
				free(cfname);
				break;
			}

			rest = strstr(parName, ")") + 2;
			char* nextParam = strstr(parName, ",") + 1;

			MALLOC_N(sequence, CiptherFuncCall, 1, {free(cfname); return false; });

			// Identify parameters
			GET_CLONED_STRING_LEN(newNextParam, parName, ( int) (nextParam - parName) - 1, {free(sequence); free(cfname); return false; });
			if (!strcmp(newNextParam, "a")) {
				sequence->index1 = -1;
			} else if (!strcmp(newNextParam, "b")) {
				sequence->index1 = -2;
			} else {
				sequence->index1 = atoi(newNextParam);
			}
			free(newNextParam);

			GET_CLONED_STRING_LEN(newNextParam2, nextParam, ( int) (rest - nextParam) - 2, {free(sequence); free(cfname); return false; });
			if (!strcmp(newNextParam2, "a")) {
				sequence->index2 = -1;
			} else if (!strcmp(newNextParam2, "b")) {
				sequence->index2 = -2;
			} else {
				sequence->index2 = atoi(newNextParam2);
			}
			free(newNextParam2);

			sequence->type = funcs->get(cfname);
			if (!sequence->type) { free(sequence); free(cfname); return false; }
			if (!sequences.append(sequence)) { free(sequence); free(cfname); return false; }
			free(cfname);
		}
	}
	return true;
}

bool YoutubeDecipherer::applyCipher(CiptherFuncCall* call, char* data) {
	if (call->type->isReverse) {
		unsigned int len = strlen(data);
		for (unsigned int i = 0; i < len / 2; i++) {
			char c = data[(len - i) - 1];
			data[(len - i) - 1] = data[i];
			data[i] = c;
		}
		return true;
	} else if (call->type->isSwap) {
		unsigned int len = strlen(data);
		char c = data[0];
		data[0] = data[call->index2 % len];
		data[call->index2 % len] = c;
		return true;
	} else if (call->type->isSlice) {
		unsigned int diff = call->index2;
		unsigned int len = strlen(data);
		for (unsigned int i = diff; i < len; i++) {
			data[i - diff] = data[i];
		}
		data[len - diff] = 0;
		return true;
	} else {
		return false;
	}
}

char* getTitleFromHTML(char* html) {
	char* lookup = "<meta property=\"og:title\" content=\"";
	char* l = strstr(html, lookup);
	int llen = strlen(lookup);
	int len = strlen(html);
	int diff = l - html;
	if (diff >= 0 && diff < len - llen) {
		l += llen;
		char* q = strstr(l, "\"");
		diff = q - l;
		if (diff >= 0 && diff < len - 2) {
			GET_CLONED_DATA(title, char, l, diff + 1, {return nullptr; });
			title[diff] = 0;

#define IS_ESC(seq, repl) if(STARTS_WITH(esc, seq)){title[writeIndex] = repl;writeIndex++;readIndex+=strlen(seq);continue;}

			// Unescape
			int writeIndex = 0;
			for (unsigned int readIndex = 0, len = strlen(title); readIndex < len; readIndex++) {
				char c = title[readIndex];
				if (c == '&') {
					char* esc = title + readIndex + 1;
					IS_ESC("quot;", '"');
					IS_ESC("amp;", '&');
					IS_ESC("lt;", '<');
					IS_ESC("gt;", '>');
				}
				title[writeIndex] = c;
				writeIndex++;
			}
			title[writeIndex] = 0;

			return title;
		}
	}
	return nullptr;
}

YoutubeDecipherer::YoutubeDecipherer(YoutubeSettings* settings, bool* stop, char* videoID) {
	this->valid = false;

	// Get source
	char* source = getPlayerSource(settings, stop, videoID);
	if (source == nullptr) { return; }

	// Locate correct decrypt function name
	char* decryptFunctionName = getPlayerDecryptFunctionName(source);
	if (decryptFunctionName == nullptr) { free(source); return; }

	if (!gatherCiphers(source, decryptFunctionName)) {
		free(source);
		free(decryptFunctionName);
		return;
	}
	free(source);
	free(decryptFunctionName);

	this->valid = true;
}

char* YoutubeDecipherer::decipher(char* input) {
	GET_CLONED_STRING(newData, input, {return nullptr; });

	// Execute
	for (unsigned int i = 0; i < sequences.getSize(); i++) {
		CiptherFuncCall* call = sequences.get(i);
		if (!applyCipher(call, newData)) {
			free(newData);
			sequences.freeItems();
			return nullptr;
		}
	}
	return newData;
}

YoutubeDecipherer::~YoutubeDecipherer() {
	sequences.freeItems();
	if (funcs != nullptr) {
		funcs->freeItems();
		delete funcs;
		funcs = nullptr;
	}
	if (title != nullptr) {
		free(title);
		title = nullptr;
	}
}

char* YoutubeDecipherer::getPlayerSource(YoutubeSettings* settings, bool* stop, char* videoID) {
	YoutubeGetter yg;
	char buffer[1024 * 25] = {0};
	sprintf_s(buffer, "https://www.youtube.com/watch?v=%s", videoID);
	ReadBuffer* rb = yg.read(settings, stop, buffer);
	if (!rb) {
		return nullptr;
	}
	if (!skipHTTPHeader(rb)) {
		delete rb;
		return nullptr;
	}
	int len = rb->getDataSize() - rb->getPosition();
	bool error = false;
	char* rest = ( char*) rb->readArray(len, &error);
	if (error) {
		delete rb;
		return nullptr;
	}
	delete rb;

	// Get video title
	this->title = getTitleFromHTML(rest);
	if (title == nullptr) { free(rest); return nullptr; }

	char* nrest = strstr(rest, "player_ias/base");
	if (!nrest) { free(rest); return nullptr; }
	nrest -= 120;
	nrest = strstr(nrest, "script src=");
	nrest = strstr(nrest, "\"") + 1;
	char* nrestEnd = strstr(nrest, "\"");
	GET_CLONED_STRING_LEN(scriptPath, nrest, ( unsigned int) (nrestEnd - nrest), {free(rest); return nullptr; }

	);
	free(rest);
	sprintf_s(buffer, "https://www.youtube.com%s", scriptPath);
	free(scriptPath);

	rb = yg.read(settings, stop, buffer);
	if (!rb) {
		return nullptr;
	}
	if (!skipHTTPHeader(rb)) {
		delete rb;
		return nullptr;
	}
	len = rb->getDataSize() - rb->getPosition();
	error = false;
	rest = ( char*) rb->readArray(len, &error);
	delete rb;
	if (error) {
		return nullptr;
	}

	return rest;
}

char* YoutubeDecipherer::getPlayerDecryptFunctionName(char* script) {

#define IN_RANGE(x, a, b) (x >= a && x <= b)
#define NUM(x) IN_RANGE(x, '0', '9')
#define ALPHA(x) (IN_RANGE(x, 'a', 'z') || IN_RANGE(x, 'A', 'Z'))
#define ALPHANUM(x) (NUM(x) || ALPHA(x))
#define SPEC(x) (x == '_' || x == '@' || x == '$')

	char* rest = strstr(script, "a=a.split(\"\")");
	if (!rest) {
		return nullptr;
	}
	char* lookup = "=function(a){";
	int lookupLen = strlen(lookup);
	for (char* qs = rest; qs > script; qs--) {
		if (!strncmp(qs, lookup, lookupLen)) {
			for (char* o = qs - 1; o > script; o--) {
				char c = o[0];
				if (!ALPHANUM(c) && !SPEC(c)) {
					char* fname = o + 1;
					int fnameLength = (qs - o) - 1;
					GET_CLONED_STRING_LEN(newFname, fname, fnameLength, {return nullptr; });
					return newFname;
				}
			}
		}
	}
	return nullptr;
}

char* JsonUnescape(char* input) {
	GET_CLONED_STRING(newInput, input, {return nullptr; });

	int writeIndex = 0;
	for (unsigned int readIndex = 0, len = strlen(input); readIndex < len; readIndex++) {
		char c = newInput[readIndex];
		if (c == '\\' && readIndex != len - 1) {
			newInput[writeIndex] = newInput[readIndex];
			writeIndex++;
			readIndex++;
			newInput[writeIndex] = newInput[readIndex];
			continue;
		} else {
			newInput[writeIndex] = newInput[readIndex];
			writeIndex++;
		}
	}
	newInput[writeIndex] = 0;
	return newInput;
}

char* getDecipheredURL(YoutubeDecipherer* cipher, char* cipherStr) {
	JsonObject* data = parseURL(cipherStr);

	if (!data->contains("url")) { delete data; return nullptr; }
	if (data->contains("sp") && data->contains("s")) { // Deciphers
		JsonString* url = data->get("url")->asString();
		JsonString* s = data->get("s")->asString();

		char* decodedS = URLDecode(s->getValue());
		if (decodedS == nullptr) { delete data; return nullptr; }

		char* decipherer = cipher->decipher(decodedS);
		free(decodedS);

		if (decipherer == nullptr) { delete data; return nullptr; }

		char* decodedURL = URLDecode(url->getValue());
		if (decodedURL == nullptr) { delete data; return nullptr; }

		char* appendix = "&sig=";
		unsigned int urlLen = strlen(decodedURL);
		unsigned int appendixLen = strlen(appendix);
		unsigned int deciphererLen = strlen(decipherer);

		MALLOC_N(newUrl, char, urlLen + deciphererLen + appendixLen + 1, {free(decodedURL); free(decipherer); delete data; return nullptr; });
		memcpy(newUrl, decodedURL, sizeof(char) * urlLen);
		memcpy(newUrl + urlLen, "&sig=", sizeof(char) * 5);
		memcpy(newUrl + urlLen + appendixLen, decipherer, sizeof(char) * strlen(decipherer));
		newUrl[urlLen + deciphererLen + appendixLen] = 0;
		free(decodedURL);
		free(decipherer);
		delete data;
		return newUrl;
	} else {
		JsonString* url = data->get("url")->asString();

		char* decodedURL = URLDecode(url->getValue());
		if (decodedURL == nullptr) { delete data; return nullptr; }

		delete data;
		return decodedURL;
	}
}

JsonObject* convertMetaStream(YoutubeDecipherer* cipher, JsonObject* metaStream) {
	JsonObject* ms = new JsonObject();

	// Copy itag
	if (!metaStream->contains("itag")) { delete ms; return nullptr; }
	if (!metaStream->get("itag")->isNumber()) { delete ms; return nullptr; }
	JsonNumber* itag = metaStream->get("itag")->asNumber();
	if (!ms->add("itag", itag->value)) { delete ms; return nullptr; }

	// Copy type
	if (!metaStream->contains("mimeType")) { delete ms; return nullptr; }
	if (!metaStream->get("mimeType")->isString()) { delete ms; return nullptr; }
	JsonString* mimeType = metaStream->get("mimeType")->asString();
	if (!ms->add("type", mimeType->getValue())) { delete ms; return nullptr; }

	// Copy content length
	if (!metaStream->contains("contentLength")) { delete ms; return nullptr; }
	if (!metaStream->get("contentLength")->isString()) { delete ms; return nullptr; }
	JsonString* contentLength = metaStream->get("contentLength")->asString();
	if (!ms->add("clen", contentLength->getValue())) { delete ms; return nullptr; }

	// Copy content length
	if (!metaStream->contains("qualityLabel")) { delete ms; return nullptr; }
	if (!metaStream->get("qualityLabel")->isString()) { delete ms; return nullptr; }
	JsonString* quality = metaStream->get("qualityLabel")->asString();
	if (!ms->add("quality_label", contentLength->getValue())) { delete ms; return nullptr; }

	// Decipher url
	if (!metaStream->contains("cipher")) { delete ms; return nullptr; }
	if (!metaStream->get("cipher")->isString()) { delete ms; return nullptr; }
	JsonString* cipherStr = metaStream->get("cipher")->asString();
	char* decipheredCipher = getDecipheredURL(cipher, cipherStr->getValue());
	if (decipheredCipher == nullptr) { delete ms; return nullptr; }
	if (!ms->add("url", decipheredCipher)) { delete ms; return nullptr; }
	free(decipheredCipher);

	if (!ms->isValid) { delete ms; return nullptr; };
	return ms;
}

bool convertArrayOfMetaStreams(YoutubeDecipherer* cipher, JsonArray* metaStreams, JsonArray* existingStreams) {
	ARRAY_FOR_EACH(metaStreams->values, item, JsonValue*, {
		if (item->isObject()) {
			JsonObject* newObj = convertMetaStream(cipher, item->asObject());
			if (newObj != nullptr) {
				if (!existingStreams->add(newObj)) { delete newObj; return false; }
			}
		}
		});
	return true;
}

JsonObject* getMetaData(YoutubeDecipherer* cipher, JsonObject* data, JsonArray* existingStreams, JsonArray* existingAdaptives, bool all) {

	if (!data->contains("player_response")) { return nullptr; }
	JsonValue* _fflags = data->get("player_response");
	if (!_fflags->isString()) { return false; }
	JsonString* fflags = _fflags->asString();
	char* decodedFlags = URLDecode(fflags->getValue());
	if (decodedFlags == nullptr) { return nullptr; }
	char* c = decodedFlags;
	JsonValue* _flags = JsonValue::read(&c);
	free(decodedFlags);
	if (_flags == nullptr) { return nullptr; }
	if (!_flags->isValid) { delete _flags; return false; }
	if (!_flags->isObject()) { delete _flags; return nullptr; }
	JsonObject* flags = _flags->asObject();

	JsonObject* meta = new JsonObject();

	if (!flags->contains("videoDetails")) { delete flags; delete meta; return nullptr; }
	if (!flags->get("videoDetails")->isObject()) { delete flags; delete meta; return nullptr; }
	JsonObject* videoDetails = flags->get("videoDetails")->asObject();

	if (!videoDetails->contains("lengthSeconds")) { delete flags; delete meta; return nullptr; }
	if (!videoDetails->get("lengthSeconds")->isString()) { delete flags; delete meta; return nullptr; }
	JsonString* length = videoDetails->get("lengthSeconds")->asString();

	meta->add("length_seconds", length->getValue());
	meta->add("title", cipher->getTitle());

	// Append adaptive and main metastreams (if nothing else is available)
	if ((existingStreams->values->getSize() == 0 && existingAdaptives->values->getSize() == 0) || all) {
		if (!flags->contains("streamingData")) { delete flags; delete meta; return nullptr; }
		if (!flags->get("streamingData")->isObject()) { delete flags; delete meta; return nullptr; }
		JsonObject* streamingData = flags->get("streamingData")->asObject();


		if (!streamingData->contains("formats")) { delete flags; delete meta; return nullptr; }
		if (!streamingData->get("formats")->isArray()) { delete flags; delete meta; return nullptr; }
		JsonArray* formats = streamingData->get("formats")->asArray();
		if (!convertArrayOfMetaStreams(cipher, formats, existingStreams)) { delete flags; delete meta; return nullptr; }


		if (!streamingData->contains("adaptiveFormats")) { delete flags; delete meta; return nullptr; }
		if (!streamingData->get("adaptiveFormats")->isArray()) { delete flags; delete meta; return nullptr; }
		JsonArray* adaptiveFormats = streamingData->get("adaptiveFormats")->asArray();
		if (!convertArrayOfMetaStreams(cipher, adaptiveFormats, existingAdaptives)) { delete formats; delete flags; delete meta; return nullptr; }
	}

	delete flags;
	return meta;
}


bool assesQuality(FormatList* fmtList, JsonObject* adaptive, bool* hasAudio, bool* hasVideo, char** formatName, char** quality) {
	bool _hasAudio = false;
	bool _hasVideo = false;
	char* _formatName = "Unknown";
	char* _quality = "Unknown";
	if (adaptive->contains("itag")) {
		JsonValue* _itag = adaptive->get("itag");
		const int maxItagDigits = 32;
		char itag[maxItagDigits + 1];
		memset(itag, 0, maxItagDigits + 1);
		if (_itag->isString()) {
			JsonString* itagg = ( JsonString*) _itag;
			char* itagStr = itagg->getValue();
			if (strlen(itagStr) > maxItagDigits) {
				return false;
			}
			sprintf_s(itag, "%s", itagStr);
		} else if (_itag->isNumber()) {
			JsonNumber* itagg = ( JsonNumber*) _itag;
			char* itagStr = itagg->value;
			if (strlen(itagStr) > maxItagDigits) {
				return false;
			}
			sprintf_s(itag, "%s", itagStr);
		} else {
			return false;
		}

		if (fmtList->getSupport(itag, &_hasAudio, &_hasVideo, &_formatName, &_quality)) {
			*hasAudio = _hasAudio;
			*hasVideo = _hasVideo;
			*formatName = _formatName;
			*quality = _quality;
			return true;
		} else {
			return false; // Invalid or unsupported itag
		}
	} else if (adaptive->contains("quality_label")) {
		JsonValue* _quality = adaptive->get("quality_label");
		if (_quality->isString()) {
			JsonString* quality = ( JsonString*) _quality;
			return quality->getValue();
		}
	}
	return false;
}

int getQualityIndex(FormatList* fmtList, char* quality, char* format) {
	return fmtList->getOrder(quality, format);
}

bool isSupportedQuality(FormatList* fmtList, JsonObject* adaptive) {
	bool hasAudio = false;
	bool hasVideo = false;
	char* quality = "";
	char* formatName = "";
	if (assesQuality(fmtList, adaptive, &hasAudio, &hasVideo, &formatName, &quality)) {
		return true;
	}
	return false;
}

bool getSupportedCodecVideoURL(YoutubeSettings* settings, FormatList* fmtList, bool* stop, char* videoID, Array<YoutubeVideoInfoItem*>* items, char** videoName, unsigned int* videoLength, bool all) {
	JsonValue* data = nullptr;
	if (settings->use_custom_fmt_parser) {
		char* infoPage = getInfoPage(settings, stop, videoID);
		if (!infoPage) { return false; }
		if (*stop) { free(infoPage); return false; }
		YoutubeDecipherer cipher(settings, stop, videoID);
		if (!cipher.isGood()) { free(infoPage); return false; }

		JsonObject* meta = parseURL(infoPage);

		free(infoPage);
		if (meta == nullptr) { return false; }

		JsonArray* adaptives = getStreams(&cipher, meta, "adaptive_fmts");
		if (adaptives == nullptr) { delete meta; return false; }
		JsonArray* streams = getStreams(&cipher, meta, "url_encoded_fmt_stream_map");
		if (streams == nullptr) { delete meta; delete adaptives; return false; }
		JsonObject* dataObj = new JsonObject();
		if (!dataObj->add("adaptive", adaptives)) { delete meta; delete dataObj; delete adaptives; delete streams; return false; }
		if (!dataObj->add("stream", streams)) { delete meta; delete dataObj; delete streams; return false; }
		JsonObject* newMeta = getMetaData(&cipher, meta, streams, adaptives, all);
		delete meta;
		if (newMeta == nullptr) { delete dataObj; return false; }
		if (!dataObj->add("meta", newMeta)) { delete newMeta; delete dataObj; return false; }
		data = dataObj;
	} else {
		char buffer[1024];
		sprintf_s(buffer, settings->url_parser_endpoint, videoID);
		YoutubeGetter yg;
		ReadBuffer* rb = yg.read(settings, stop, buffer);
		if (rb == nullptr) { return false; }
		unsigned int l = rb->getDataSize();
		bool err = false;
		char* str = ( char*) rb->readArray(l, &err);
		char* processStr = str;
		if (err) { return false; }
		data = JsonValue::read(&processStr);
		free(str);
	}
	if (data == nullptr) { return false; }
	if (!data->isValid) { delete data; return false; }
	if (!data->isObject()) { delete data; return false; }

	JsonObject* obj = data->asObject();
	if (obj->contains("stream")) {
		JsonValue* _streamArray = obj->get("stream");
		if (_streamArray->isArray()) {
			JsonArray* streamArray = ( JsonArray*) _streamArray;
			for (unsigned int i = 0; i < streamArray->values->getSize(); i++) {
				JsonValue* _stream = streamArray->values->get(i);
				if (_stream->isObject()) {
					JsonObject* stream = ( JsonObject*) _stream;
					if ((stream->contains("type") || stream->contains("mimeType")) && stream->contains("url")) {
						JsonValue* _type = stream->get(stream->contains("type") ? "type" : "mimeType");
						JsonValue* _url = stream->get("url");

						int clen = 0;
						if (stream->contains("clen")) {
							JsonValue* _clen = stream->get("clen");
							if (_clen->isString()) {
								JsonString* _clenS = ( JsonString*) _clen;
								char* clenStr = _clenS->getValue();
								clen = strtoul(clenStr, nullptr, 10);
							}
						} else if (stream->contains("contentLength")) {
							JsonValue* _clen = stream->get("contentLength");
							if (_clen->isString()) {
								JsonString* _clenS = ( JsonString*) _clen;
								char* clenStr = _clenS->getValue();
								clen = strtoul(clenStr, nullptr, 10);
							}
						}

						if (_type->isString() && _url->isString()) {
							JsonString* type = ( JsonString*) _type;
							JsonString* url = ( JsonString*) _url;
							if (STARTS_WITH(type->getValue(), "video/mp4;") || all) {
								if (isSupportedQuality(fmtList, stream)) {

									bool hasAudio = false;
									bool hasVideo = false;
									char* quality = "";
									char* formatName = "";
									assesQuality(fmtList, stream, &hasAudio, &hasVideo, &formatName, &quality);

									MALLOC_N(item, YoutubeVideoInfoItem, 1, {delete data; return false; });
									memset(item, 0, sizeof(YoutubeVideoInfoItem));
									GET_CLONED_STRING(newUrl, url->getValue(), {delete data; free(item); return false; });
									GET_CLONED_STRING(rawType, type->getValue(), {delete data; free(item); free(newUrl); return false;});
									if (!items->append(item)) {
										delete data;
										free(item);
										free(newUrl);
										free(rawType);
									}
									item->hasAudio = hasAudio;
									item->hasVideo = hasVideo;
									item->format = formatName;
									item->url = newUrl;
									item->rawFormat = rawType;
									item->quality = quality;
									item->length = clen;
								}
							}
						}
					}
				}
			}
		}
	}

	if (obj->contains("adaptive")) {
		JsonValue* _adaptiveArray = obj->get("adaptive");
		if (_adaptiveArray->isArray()) {
			JsonArray* adaptiveArray = ( JsonArray*) _adaptiveArray;
			for (unsigned int i = 0; i < adaptiveArray->values->getSize(); i++) {
				JsonValue* _adaptive = adaptiveArray->values->get(i);
				if (_adaptive->isObject()) {
					JsonObject* adaptive = ( JsonObject*) _adaptive;
					if ((adaptive->contains("type") || adaptive->contains("mimeType")) && adaptive->contains("url")) {
						JsonValue* _type = adaptive->get(adaptive->contains("type") ? "type" : "mimeType");
						JsonValue* _url = adaptive->get("url");
						if (_type->isString() && _url->isString()) {
							JsonString* type = ( JsonString*) _type;
							JsonString* url = ( JsonString*) _url;
							char* typeStr = type->getValue();

							bool hasAudio = false;
							bool hasVideo = false;
							char* quality = "";
							char* formatName = "";
							assesQuality(fmtList, adaptive, &hasAudio, &hasVideo, &formatName, &quality);

							int clen = 0;
							if (adaptive->contains("clen")) {
								JsonValue* _clen = adaptive->get("clen");
								if (_clen->isString()) {
									JsonString* _clenS = ( JsonString*) _clen;
									char* clenStr = _clenS->getValue();
									clen = strtoul(clenStr, nullptr, 10);
								}
							} else if (adaptive->contains("contentLength")) {
								JsonValue* _clen = adaptive->get("contentLength");
								if (_clen->isString()) {
									JsonString* _clenS = ( JsonString*) _clen;
									char* clenStr = _clenS->getValue();
									clen = strtoul(clenStr, nullptr, 10);
								}
							}
							if (STARTS_WITH(typeStr, "audio/mp4;") || all) {
								if (isSupportedQuality(fmtList, adaptive)) {

									bool hasAudio = false;
									bool hasVideo = false;
									char* quality = "";
									char* formatName = "";
									assesQuality(fmtList, adaptive, &hasAudio, &hasVideo, &formatName, &quality);

									MALLOC_N(item, YoutubeVideoInfoItem, 1, {delete data; return false; });
									memset(item, 0, sizeof(YoutubeVideoInfoItem));
									GET_CLONED_STRING(newUrl, url->getValue(), {delete data; free(item); return false; });
									GET_CLONED_STRING(rawType, type->getValue(), {delete data; free(item); free(newUrl);  return false; });
									if (!items->append(item)) {
										delete data;
										free(item);
										free(newUrl);
										free(rawType);
									}
									item->hasAudio = hasAudio;
									item->hasVideo = hasVideo;
									item->format = formatName;
									item->rawFormat = rawType;
									item->url = newUrl;
									item->quality = quality;
									item->length = clen;
								}
							}
						}
					}
				}
			}
		}
	}

	if (obj->contains("meta")) { // Extract video name from meta
		JsonValue* _meta = obj->get("meta");
		if (_meta->isObject()) {
			JsonObject* meta = ( JsonObject*) _meta;
			if (meta->contains("player_response")) {
				JsonValue* _resp = meta->get("player_response");
				if (_resp->isString()) {
					JsonString* resp = ( JsonString*) _resp;
					if (resp->unescape()) {
						char* respStr = resp->getValue();
						JsonValue* respData = JsonValue::read(&respStr);
						if (respData != nullptr) {
							if (respData->isValid) {
								if (respData->isObject()) {
									JsonObject* respO = ( JsonObject*) respData;
									if (respO->contains("videoDetails")) {
										JsonValue* _vd = respO->get("videoDetails");
										if (_vd->isObject()) {
											JsonObject* vd = ( JsonObject*) _vd;
											if (vd->contains("title")) {
												JsonValue* _title = vd->get("title");
												if (_title->isString()) {
													JsonString* title = ( JsonString*) _title;
													char* t = title->getValue();
													JsonString* newTitle = new JsonString(t);
													if (newTitle->isValid) {
														if (!meta->add("title", newTitle)) {
															delete newTitle;
														}
													} else {
														delete newTitle;
													}
												}
											}
										}
									}
								}
							}
							delete respData;
						}
					}
				}
			}
		}
	}
	*videoLength = 0;

	GET_CLONED_STRING(newNameStr, "Invalid video", {delete data; return false; });
	if (obj->contains("meta")) {
		JsonValue* _meta = obj->get("meta");
		if (_meta->isObject()) {
			JsonObject* meta = ( JsonObject*) _meta;
			if (meta->contains("title")) {
				JsonValue* _title = meta->get("title");
				if (_title->isString()) {
					JsonString* title = ( JsonString*) _title;
					GET_CLONED_STRING(newNewNameStr, title->getValue(), {free(newNameStr); delete data; return false; });
					free(newNameStr);
					newNameStr = newNewNameStr;
				}
			}
			if (meta->contains("length_seconds")) {
				JsonValue* _length = meta->get("length_seconds");
				if (_length->isString()) {
					JsonString* length = ( JsonString*) _length;
					char* lenStr = (( JsonString*) length)->getValue();
					unsigned int len = strtoul(lenStr, nullptr, 10);
					*videoLength = len;
				}
			}
		}
	}

	*videoName = newNameStr;
	delete data;

	// Remove duplicates
	bool changed = false;
	do {
		changed = false;
		for (unsigned int i = 1; i < items->getSize(); i++) {
			YoutubeVideoInfoItem* search = items->get(i);
			bool break2 = false;
			for (unsigned int o = 1; o < items->getSize(); o++) {
				if (i != o) {
					YoutubeVideoInfoItem* item = items->get(o);
					if (item->hasAudio == search->hasAudio && item->hasVideo == search->hasVideo) { // Same streams
						bool sameFormat = !strcmp(item->format, search->format);
						bool sameURL = !strcmp(item->url, search->url);
						bool sameSize = item->length == search->length;
						if (sameURL || (sameFormat && sameSize)) { // Same URL 
							items->remove(o);
							free(item->rawFormat);
							free(item->url);
							free(item);
							break2 = true;
							changed = true;
							break;
						}
					}
				}
			}
			if (break2) {
				break;
			}
		}
	} while (changed);

	// Sort data
	changed = false;
	do {
		changed = false;
		for (unsigned int i = 1; i < items->getSize(); i++) {
			YoutubeVideoInfoItem* prev = items->get(i - 1);
			YoutubeVideoInfoItem* curr = items->get(i);
			int x1 = getQualityIndex(fmtList, prev->quality, prev->format);
			int x2 = getQualityIndex(fmtList, curr->quality, curr->format);
			if (x1 > x2) {
				changed = true;
				YoutubeVideoInfoItem tmp;
				memcpy(&tmp, curr, sizeof(YoutubeVideoInfoItem));
				memcpy(curr, prev, sizeof(YoutubeVideoInfoItem));
				memcpy(prev, &tmp, sizeof(YoutubeVideoInfoItem));
			}
		}

	} while (changed);

	return true;
}

char* getVideoID(char* url) {
	char* videoId = strstr(url, "watch?v=");
	if (!videoId) { // Invalid URL
		return nullptr;
	}
	GET_CLONED_STRING(newVideoID, videoId + strlen("watch?v="), {return nullptr; });
	videoId = strstr(newVideoID, "&");
	if (!videoId) {
		return newVideoID;
	}
	int diff = videoId - newVideoID;
	newVideoID[diff] = 0;
	return newVideoID;
}

YoutubeVideo* YoutubeVideoInfo::download(bool* stop, YoutubeVideoInfoItem* item, DownloadProgress reporter, void* reporterInstance) {

	// Download
	YoutubeGetter yg;
	ReadBuffer* rb = yg.read(settings, stop, item->url, item->length, reporter, reporterInstance);
	if (rb == nullptr) {
		return nullptr;
	}
	unsigned int length = rb->getDataSize();
	bool err = false;
	unsigned char* data = rb->readArray(length, &err);
	if (err) {
		delete rb;
		return nullptr;
	}
	delete rb;
	GET_CLONED_STRING(newName, this->videoName, {free(data); return nullptr; });
	if (reporter != nullptr) {
		reporter(reporterInstance, length, length);
	}
	return new YoutubeVideo(newName, data, length);
}

YoutubeVideoInfo::YoutubeVideoInfo(YoutubeSettings* settings, FormatList* fmtList, bool* stop, char* url, bool all) {
	this->valid = false;
	this->settings = settings;

	this->videoID = getVideoID(url);
	if (!this->videoID) { return; }

	// Collect info
	if (!getSupportedCodecVideoURL(settings, fmtList, stop, videoID, &items, &videoName, &length, all)) {
		return;
	}
	this->valid = true;

}

FormatList* FormatList::create() {
	FormatList* lst = new FormatList();
	if (lst->data == nullptr) {
		delete lst;
		return nullptr;
	}
	return lst;
}

FormatList::FormatList() {
	this->valid = false;

	// Decompress formats
	char* newData;
	unsigned int outputDataLength;
	bool error = false;
	decompress(( char*) compressedFormats, compressedFormatsLength, &newData, &outputDataLength, &error);
	if (error) {
		return;
	}

	char* jsn = newData;
	JsonValue* _val = JsonValue::read(&jsn);
	free(newData);
	if (_val != nullptr) {
		if (_val->isObject()) {
			JsonObject* set = ( JsonObject*) _val;

			if (set->containsArray("items") && set->containsArray("formats") && set->containsArray("qualities")) {

				// Load from JSON
				Map<char*, unsigned int> dataPerQualityCounters(__strncmp);
				Map<char*, unsigned int> qualityCounters(__strncmp);
				Map<char*, unsigned int> formatCounters(__strncmp);

				JsonArray* val = set->getArray("items");
				unsigned int totalItems = val->values->getSize();

				MALLOC_N(data, FormatListItem, totalItems, {delete _val; return;});
				memset(data, 0, sizeof(FormatListItem) * totalItems);
				this->data = data;

				// Extract data
				for (unsigned int i = 0; i < totalItems; i++) {
					JsonValue* item = val->values->get(i);
					FormatListItem* itm = &(data[i]);
					if (item->isObject()) {
						JsonObject* itemO = ( JsonObject*) item;
						if (itemO->containsNumber("id")) {
							char* id = itemO->getNumber("id")->value;
							if (itemO->containsString("type")) {
								char* type = itemO->getString("type")->getValue();
								if (!strcmp(type, "AUDIO")) {
									itm->audio = true;
								} else if (!strcmp(type, "VIDEO")) {
									itm->video = true;
								} else if (!strcmp(type, "AUDIO+VIDEO")) {
									itm->audio = true;
									itm->video = true;
								} else {
									delete _val;
									return;
								}
								if (itemO->containsString("quality")) {
									char* quality = itemO->getString("quality")->getValue();
									if (itemO->containsString("format")) {
										char* format = itemO->getString("format")->getValue();
										unsigned int nextCount = dataPerQualityCounters.contains(quality) ? dataPerQualityCounters.get(quality) + 1 : 0;
										if (!dataPerQualityCounters.put(quality, nextCount)) {
											delete _val;
											return;
										}
										GET_CLONED_STRING(newId, id, {delete _val; return; });
										GET_CLONED_STRING(newQuality, quality, {free(newId); delete _val; return; });
										GET_CLONED_STRING(newFormat, format, {free(newQuality); free(newId); delete _val; return;});
										itm->id = newId;
										itm->format = newFormat;
										itm->quality = newQuality;
										continue;
									}
								}
							}
						}
					}
					delete _val;
					return;
				}

				// Find maximum in quality counters 
				unsigned int maximum = 0;
				MAP_FOR_EACH(&dataPerQualityCounters, key, char*, value, unsigned int, {
					if (value > maximum) {
						maximum = value;
					}
					});


				// First collect format identifiers
				JsonArray* formats = set->getArray("formats");
				for (unsigned int i = 0; i < formats->values->getSize(); i++) {
					JsonValue* format = formats->values->get(i);
					if (!format->isString()) {
						delete _val;
						return;
					}
					char* fmt = (( JsonString*) format)->getValue();
					formatCounters.put(fmt, formatCounters.getSize());
				}

				// Next collect quality identifiers
				JsonArray* qualities = set->getArray("qualities");
				for (unsigned int i = 0; i < qualities->values->getSize(); i++) {
					JsonValue* quality = qualities->values->get(i);
					if (!quality->isString()) {
						delete _val;
						return;
					}
					char* qual = (( JsonString*) quality)->getValue();
					qualityCounters.put(qual, qualityCounters.getSize());
				}

				// Assign quality offsets
				maximum *= 2; // Offset safety first
				for (unsigned int i = 0; i < totalItems; i++) {
					FormatListItem* item = &(data[i]);
					if (!qualityCounters.contains(item->quality) || !formatCounters.contains(item->format)) { // No such quality/format?
						delete _val;
						return;
					}
					unsigned int qualityIndex = qualityCounters.get(item->quality);
					unsigned int formatIndex = formatCounters.get(item->format);
					item->formatOrder = formatIndex;
					item->qualityOrder = maximum * qualityIndex * maximum;
				}
				this->dataSize = totalItems;
				this->valid = true;
			}
		}
	}
	delete _val;
}

FormatList::~FormatList() {
	for (unsigned int i = 0; i < this->dataSize; i++) {
		FormatListItem* item = &(this->data[i]);
		FREE_NOT_NULL(item->format);
		FREE_NOT_NULL(item->quality);
		FREE_NOT_NULL(item->id);
	}
	FREE_NOT_NULL(data);
}

int FormatList::getOrder(char* quality, char* format) {
	unsigned int sameQuality = 0;
	unsigned int qualityOrder = 0;
	for (unsigned int i = 0; i < this->dataSize; i++) {
		FormatListItem* item = &(this->data[i]);
		if (!strcmp(item->quality, quality)) {
			sameQuality++;
			qualityOrder = item->qualityOrder;
			if (!strcmp(item->format, format)) {
				return item->qualityOrder + item->formatOrder;
			}
		}
	}
	return qualityOrder + sameQuality + 1;
}

bool FormatList::isIDSupported(char* ID) {
	for (unsigned int i = 0; i < this->dataSize; i++) {
		FormatListItem* item = &(this->data[i]);
		if (!strcmp(item->id, ID)) {
			return true;
		}
	}
	return false;
}

bool FormatList::getSupport(char* ID, bool* audio, bool* video, char** format, char** quality) {
	for (unsigned int i = 0; i < this->dataSize; i++) {
		FormatListItem* item = &(this->data[i]);
		if (!strcmp(item->id, ID)) {
			*audio = item->audio;
			*video = item->video;
			*format = item->format;
			*quality = item->quality;
			return true;
		}
	}
	return false;
}

Array<FormatList::FormatListItem*>* FormatList::get() {
	Array<FormatList::FormatListItem*>* res = new Array<FormatList::FormatListItem*>();
	for (unsigned int i = 0; i < this->dataSize; i++) {
		FormatListItem* item = &(this->data[i]);
		if (!res->append(item)) {
			delete res;
			return nullptr;
		}
	}
	return res;
}
