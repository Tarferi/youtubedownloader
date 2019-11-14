#pragma once
#include "common.h"

class Output {

public:

	enum FormatTypeE {
		AUDIO_AND_VIDEO,
		AUDIO_ONLY,
		VIDEO_ONLY
	};

	virtual bool printError(char* error) = 0;

	virtual bool printVideoNameAndDuration(char* videoName, unsigned int duration) = 0;

	virtual bool printNoAvailableFormats() = 0;

	virtual bool printFormatsHeader(FormatTypeE format, unsigned int totalItems) = 0;

	virtual bool printItem(FormatTypeE oformat, unsigned int index, char* quality, char* format, unsigned int length) = 0;

	virtual bool printDownloadeditem(FormatTypeE oformat, unsigned int index, char* quality, char* format, unsigned int length, char* fileName, char* directURL) = 0;

	virtual bool print() = 0;

	virtual ~Output();

};

typedef Output::FormatTypeE OutputFormat;

class ConsoleOutput : public Output {

public:

	virtual bool printError(char* error) override;

	virtual bool printVideoNameAndDuration(char* videoName, unsigned int duration) override;

	virtual bool printNoAvailableFormats() override;

	virtual bool printFormatsHeader(OutputFormat format, unsigned int totalItems) override;

	virtual bool printItem(OutputFormat oformat, unsigned int index, char* quality, char* format, unsigned int length) override;

	virtual bool printDownloadeditem(FormatTypeE oformat, unsigned int index, char* quality, char* format, unsigned int length, char* fileName, char* directURL) override;

	virtual bool print() override;

	static ConsoleOutput* create();

	virtual ~ConsoleOutput();

private:

	ConsoleOutput();
	WriteBuffer* wb = nullptr;

};

class JSONOutput : public Output {

public:

	class Item {
	public:
		Item(unsigned int index, char* format, char* quality, unsigned int length) {
			this->valid = false;
			GET_CLONED_STRING(newFormat, format, {return; });
			GET_CLONED_STRING(newQuality, quality, {free(newFormat); return;});
			this->format = newFormat;
			this->quality = newQuality;
			this->length = length;
			this->index = index;
			this->valid = true;
		}

		virtual ~Item() {
			FREE_NOT_NULL(format);
			FREE_NOT_NULL(quality);
		}

		bool isValid() {
			return valid;
		}

		unsigned int index;
		char* format = nullptr;
		char* quality = nullptr;
		unsigned int length;
	private:
		bool valid;
	};

	class Items {
	public:

		Items(char* name, unsigned int len) {
			valid = false;
			this->videoAndAudio = new Array< JSONOutput::Item*>();
			this->videoOnly = new Array< JSONOutput::Item*>();
			this->audioOnly = new Array< JSONOutput::Item*>();
			GET_CLONED_STRING(newName, name, {return; });
			this->videoName = newName;
			this->videoLength = len;
			valid = true;
		}

		bool isValid() {
			if (!valid) { return false; }
			for (unsigned int i = 0; i < videoAndAudio->getSize(); i++) {
				JSONOutput::Item* itm = videoAndAudio->get(i);
				if (!itm->isValid()) {
					return false;
				}
			}
			for (unsigned int i = 0; i < videoOnly->getSize(); i++) {
				JSONOutput::Item* itm = videoOnly->get(i);
				if (!itm->isValid()) {
					return false;
				}
			}
			for (unsigned int i = 0; i < audioOnly->getSize(); i++) {
				JSONOutput::Item* itm = audioOnly->get(i);
				if (!itm->isValid()) {
					return false;
				}
			}

		}

		virtual ~Items() {
			for (unsigned int i = 0; i < videoAndAudio->getSize(); i++) {
				JSONOutput::Item* itm = videoAndAudio->get(i);
				delete itm;
			}
			for (unsigned int i = 0; i < videoOnly->getSize(); i++) {
				JSONOutput::Item* itm = videoOnly->get(i);
				delete itm;
			}
			for (unsigned int i = 0; i < audioOnly->getSize(); i++) {
				JSONOutput::Item* itm = audioOnly->get(i);
				delete itm;
			}
			FREE_NOT_NULL(videoName);
			DELETE_NOT_NULL(videoAndAudio);
			DELETE_NOT_NULL(videoOnly);
			DELETE_NOT_NULL(audioOnly);
		}

		Array<JSONOutput::Item*>* videoAndAudio = nullptr;
		Array<JSONOutput::Item*>* videoOnly = nullptr;
		Array<JSONOutput::Item*>* audioOnly = nullptr;
		char* videoName;
		unsigned int videoLength;
	private:

		bool valid;
	};

	virtual bool printError(char* error) override;

	virtual bool printVideoNameAndDuration(char* videoName, unsigned int duration) override;

	virtual bool printNoAvailableFormats() override;

	virtual bool printFormatsHeader(OutputFormat format, unsigned int totalItems) override;

	virtual bool printItem(OutputFormat oformat, unsigned int index, char* quality, char* format, unsigned int length) override;

	virtual bool printDownloadeditem(FormatTypeE oformat, unsigned int index, char* quality, char* format, unsigned int length, char* fileName, char* directURL) override;

	virtual bool print() override;

	static JSONOutput* create();

	virtual ~JSONOutput();

	Items* getItems();

private:

	void* _obj = nullptr;
	JSONOutput();
};
