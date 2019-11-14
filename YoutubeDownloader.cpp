#include "common.h"
#include "Youtube.h"
#include "output.h"

void printError(char* message) {
	fprintf(stderr, message);
}

int printUsage(Output* output) {
#define ARG_CMD(name, shortName, arg, description) printf("\t%s %s %s\t - %s\r\n", shortName, name, arg, description);
#define OPT_BOOL(name, shortName, description) printf("\t%s %s \t - %s\r\n", shortName, name, description);
#define OPT_ARG(name, shortName, arg, description) printf("\t%s %s %s\t - %s\r\n", shortName, name, arg, description);
	printf("=== Youtube Video Downloader === \r\nMade by: iThief\r\n\r\nUsage:\r\n");
	printf("\tytdl.exe [mode] [options]\r\n\t(order of mode and options is not important)\r\n\r\n");
	printf("Available modes:\r\n");
	ARG_CMD("--list", "-l", "[URL]\t", "Outputs listing of available formats and other meta data. Can be used with -j, -nc, -o, -fmt");
	ARG_CMD("--download", "-d", "[URL]", "Downloads given video in selected format. Can be combined with -j, -nc, -o, -fmt, -f");
	ARG_CMD("--interactive", "-i", "[URL]", "Interactive version of downloader. URL is optional. Can be combined with -j, -nc, -fmt");
	printf("\r\nAvailable options:\r\n");
	OPT_BOOL("--no-cache\t\t", "-nc", "Do not use cached results and do not create cache files.");
	OPT_BOOL("--json\t\t", "-j", "Use JSON format as output. If used in -d mode, output consists of progress and/or error messages.");
	OPT_ARG("--fmt-endpoint", "-fmt", "[URL]", "Use custom FMT parsing endpoint. Default behavior is to use built-in parser.");
	OPT_ARG("--format", "-f", "[FORMAT]\t", "Specify which formats to download. See below.");
	OPT_ARG("--output", "-o", "[FILE]\t", "If used in -d mode, this represents template for downloaded files. Otherwise it's file to which all output should be stored in. If omitted in -d mode, video name is used as a template");
	OPT_BOOL("--peek\t\t", "-p", "If used in -d mode, simulates download without actually downloading anything.");
	OPT_ARG("--cache-file", "-cf", "[FILE]\t", "Use cache file. If omitted, default \"youtube.cache\" file is used.");
	OPT_ARG("--max-cache-size", "-mcs", "[SIZE]\t", "Maximum allowed cache size in bytes. If exceeded, cache is trimmed randomly.");
	printf("\r\nFormats:\r\n");
	printf("Basic syntax: -[A|V|AV][ID]\r\n");
	printf("\t Download specific format ID as listed in -l and -i mode.\r\n\r\n");
	printf("Advanced syntax: -[A|V|AV][QUALITY][+-]?/[EXTENSION_LIST|\\*]\r\n");
	printf("\t-[A|V|AV] - if \"Audio only\", \"Video only\" or \"Audio and video\" should be queried\r\n");
	printf("\t[QUALITY][+|-]? String representation of a desired quality. If \"+\" (\"-\") is used, query exact quality or higher (lower) if no such quality is available.\r\n");
	printf("\t/[EXTENSION_LIST*|\\*] - List of extensions (formats) that identifies priorities of identical quality results. If * is used, any format is chosen.\r\n");
	printf("\r\nFormat examples:\r\n");
	printf("\t-V360p+/mp4,flv,*\t - Download video in quality 360p or higher, first look for mp4, then look for flv and then take any available format\r\n");
	printf("\t-A128k-/*\t\t - Download any audio file in quality 128k or lower, format doesn't matter\r\n");
	printf("\t-A128k-/*-V1080p/*\t - Download any audio file in quality 128k or lower, format doesn't matter and then download a video file in quality 1080p, format doesn't matter\r\n");
	printf("\r\nFMT Endpoints:\r\n");
	printf("\tSupported are REST endpoints in maple3142/ytdl format.\r\n\tExample endpoint: https://maple3142-ytdl-demo.glitch.me/api\r\n");
	return 2;
}

struct FormatSpecifier {
	enum FormatSpecifierE {
		NONE,
		BEST,
		WORST,
		SPECIFIED_PLUS,
		SPECIFIED_MINUS,
		SPECIFIED_EXACTLY,
		NUMERIC,
	};
	FormatSpecifierE quality;
	bool audio;
	bool video;
	char* spec;
	Array<char*>* formats;
};

bool parseFormat(Array<FormatSpecifier*>* data, char* format) {
#define fmt_next(format, num) format=&(format[num]);
	auto parseFormat = [](char** format, FormatSpecifier::FormatSpecifierE* data, char** spec, Array<char*>* formats) {
		if (*data != FormatSpecifier::FormatSpecifierE::NONE || (*spec) != nullptr || formats->getSize() != 0) { // Already specified
			return false;
		}
		// Can be (best,worst,xxxp,xxxk,xxx)
		if (STARTS_WITH(*format, "best")) {
			*data = FormatSpecifier::FormatSpecifierE::BEST;
			*format = &((*format)[4]);
		} else if (STARTS_WITH(*format, "worst")) {
			*data = FormatSpecifier::FormatSpecifierE::WORST;
			*format = &((*format)[5]);
		} else if ((*format)[0] >= '0' && (*format)[0] <= '9') { // Must be number (or error)
			*data = FormatSpecifier::FormatSpecifierE::NUMERIC;
			unsigned int maxlen = strlen(*format);
			WriteBuffer wb;
			bool error = false;
			for (unsigned int i = 0; i < maxlen; i++) {
				char c = (*format)[i];
				if (c >= '0' && c <= '9') {
					wb.writeByte(c, &error);
					if (error) {
						return false;
					}
				} else if (c == 'k' || c == 'p' || c == 'K' || c == 'P') {
					c = c == 'K' ? 'k' : c == 'P' ? 'p' : c; // To lowercase
					wb.writeByte(c, &error);
					if (error) {
						return false;
					}
					if (c == 'p' && i < maxlen - 2) { // 2 more characters at least
						char* next = &((*format)[i + 1]);
						if (STARTS_WITH(next, "60")) {
							i += 2;
							wb.writeArray(( unsigned char*) next, 2, &error);
							if (error) {
								return false;
							}
						}
					}


					*data = FormatSpecifier::FormatSpecifierE::SPECIFIED_EXACTLY;
					if (i != maxlen - 1) { // At least one more character
						i++;
						char next = (*format)[i];
						if (next == '+') {
							i++;
							*data = FormatSpecifier::FormatSpecifierE::SPECIFIED_PLUS;
						} else if (next == '-') {
							i++;
							*data = FormatSpecifier::FormatSpecifierE::SPECIFIED_MINUS;
						}
					}
					char* fmt = *format;
					*format = &(fmt[i]);
					break;
				} else { // Only numbers
					char* fmt = *format;
					*format = &(fmt[i]);
					break;
				}
			}
			wb.writeByte(0, &error);
			if (error) {
				return false;
			}
			unsigned char* wdata;
			unsigned int wdatalen;
			wb.getWrittenData(&wdata, &wdatalen);
			GET_CLONED_STRING(specStr, wdata, {return false; });
			*spec = specStr;

		} else {
			return false;
		}

		if ((*data) == FormatSpecifier::FormatSpecifierE::NUMERIC) { // Numeric doesn't expect format
			return true;
		}

		// Have number, can follow '/' and formats
		char next = (*format)[0];
		if (next != '/') {
			return false;
		}
		fmt_next((*format), 1);
		unsigned int maxlen = strlen(*format);
		WriteBuffer wb;

		auto appendFormat = [formats](WriteBuffer* wbb) {
			bool error = false;
			wbb->writeByte(0, &error);
			if (error) {
				return false;
			}
			unsigned char* wdata;
			unsigned int wdatalen;
			wbb->getWrittenData(&wdata, &wdatalen);
			wbb->setPosition(0);
			GET_CLONED_STRING(newData, ( char*) wdata, {return false;});
			if (!formats->append(( char*) newData)) {
				free(newData);
				return false;
			}
			return true;
		};

		bool error = false;
		for (unsigned int i = 0; i < maxlen; i++) {
			char c = (*format)[0];
			if (c == ',' || c == '-') { // Delimiter
				if (!appendFormat(&wb)) {
					return false;
				}
				if (c == '-') {
					break;
				}
			} else {
				wb.writeByte(c, &error);
				if (error) {
					return false;
				}
			}
			fmt_next((*format), 1);
		}
		// Last item not delimited
		if (wb.getPosition() > 0) {
			if (!appendFormat(&wb)) {
				return false;
			}
		}
		return true;
	};

	char* fmtEnd = &format[strlen(format)];


	//  Check if raw number is given
	unsigned int numTest = atoi(format);
	char numTestBuffer[32];
	sprintf_s(numTestBuffer, "%d", numTest);
	if (!strcmp(format, numTestBuffer)) { // Number is provided, don't parse
		GET_CLONED_STRING(newNumber, format, {return false; });

		MALLOC_N(item, FormatSpecifier, 1, {return false; });
		memset(item, 0, sizeof(FormatSpecifier));
		if (!data->append(item)) {
			free(newNumber);
			delete item;
			return false;
		}
		item->quality = FormatSpecifier::FormatSpecifierE::NUMERIC;
		item->spec = newNumber;
		item->formats = new Array<char*>();
	} else {
		while (format != fmtEnd) {
			MALLOC_N(item, FormatSpecifier, 1, {return false;});
			memset(item, 0, sizeof(FormatSpecifier));
			if (!data->append(item)) {
				delete item;
				return false;
			}
			item->audio = false;
			item->video = false;
			item->formats = new Array<char*>();
			if (STARTS_WITH(format, "-AV") || STARTS_WITH(format, "VA") || STARTS_WITH(format, "av") || STARTS_WITH(format, "va")) {
				fmt_next(format, 3);
				item->audio = true;
				item->video = true;
				if (!parseFormat(&format, &(item->quality), &(item->spec), item->formats)) {
					return false;
				}
			} else if (STARTS_WITH(format, "-V") || STARTS_WITH(format, "-v")) {
				fmt_next(format, 2);
				item->video = true;
				if (!parseFormat(&format, &(item->quality), &(item->spec), item->formats)) {
					return false;
				}
			} else if (STARTS_WITH(format, "-A") || STARTS_WITH(format, "-a")) {
				fmt_next(format, 2);
				item->audio = true;
				if (!parseFormat(&format, &(item->quality), &(item->spec), item->formats)) {
					return false;
				}
			} else {
				return false;
			}
		}
	}
	return true;
}

int printData(YoutubeSettings* settings, Output* output, char* URL, char* format, char* outputFile, bool addExtension, DownloadProgress progressReporter, void* reporterInstance) {

	// Check if format is good
	Array<FormatSpecifier*> fmts;

	auto destroyFormatData = [](Array<FormatSpecifier*>* fs) {
		for (unsigned int specifierIndex = 0; specifierIndex < fs->getSize(); specifierIndex++) {
			FormatSpecifier* f = fs->get(specifierIndex);
			if (f != nullptr) {
				FREE_NOT_NULL(f->spec);
				if (f->formats != nullptr) {
					for (unsigned int formatIndex = 0; formatIndex < f->formats->getSize(); formatIndex++) {
						char* format = f->formats->get(formatIndex);
						free(format);
					}
					delete f->formats;
				}
				free(f);
			}
		}
	};


	if (!parseFormat(&fmts, format)) {
		output->printError("Failed to process given format argument");
		destroyFormatData(&fmts);
		return 1;
	}

	Youtube yt(settings);
	bool stopper = false;
	YoutubeVideoInfo* info = yt.getVideo(&stopper, URL, true);
	if (info == nullptr) {
		output->printError("Failed to get video info");
		destroyFormatData(&fmts);
		return 1;
	}
	if (outputFile == nullptr) { // Use video name
		outputFile = info->videoName;
	}

	Youtube* ytPtr = &yt;

	auto infoGrabber = [ytPtr, info](FormatSpecifier::FormatSpecifierE type, Array<char*>* formats, char* spec, bool needAudio, bool needVideo, unsigned int* index) {

		auto contains = [](char* haystack, char* needle) {
			unsigned int stackLen = strlen(haystack);
			unsigned int needleLen = strlen(needle);
			if (needleLen > stackLen) {
				return false;
			} else if (needleLen == stackLen) {
				if (!strcmp(haystack, needle)) {
					return true;
				} else {
					return false;
				}
			} else {
				for (unsigned int i = 0; i < stackLen - needleLen + 1; i++) {
					char* subStr = &haystack[i];
					if (STARTS_WITH(subStr, needle)) {
						return true;
					}
				}
				return false;
			}
		};

		YoutubeVideoInfoItem* noitem = nullptr;
		if (type == FormatSpecifier::FormatSpecifierE::NUMERIC) { // Correct stream
			int formatID = atoi(spec);
			if (formatID < 0 || formatID >= ( int) info->items.getSize()) {
				return noitem;
			}
			YoutubeVideoInfoItem* item = info->items.get(( unsigned int) formatID);
			*index = formatID;
			return item;
		} else if (type == FormatSpecifier::FormatSpecifierE::BEST) { // Select best (last) stream
			for (unsigned int seekIndex = 0; seekIndex < formats->getSize(); seekIndex++) {
				char* format = formats->get(seekIndex);
				bool anyFormat = (!strcmp(format, "*"));
				for (unsigned int infoIndex = info->items.getSize(); infoIndex > 0; infoIndex--) {
					YoutubeVideoInfoItem* item = info->items.get(infoIndex - 1);
					bool anySubFormat = anyFormat && (needAudio == item->hasAudio) && (needVideo == item->hasVideo);
					if (!strcmp(format, item->format) || anySubFormat) {
						*index = infoIndex - 1;
						return item;
					}
				}
			}
		} else if (type == FormatSpecifier::FormatSpecifierE::WORST) { // Select worst(first) stream
			for (unsigned int seekIndex = 0; seekIndex < formats->getSize(); seekIndex++) {
				char* format = formats->get(seekIndex);
				bool anyFormat = !strcmp(format, "*");
				for (unsigned int infoIndex = 0; infoIndex < info->items.getSize(); infoIndex++) {
					YoutubeVideoInfoItem* item = info->items.get(infoIndex);
					bool anySubFormat = anyFormat && (needAudio == item->hasAudio) && (needVideo == item->hasVideo);
					if (!strcmp(format, item->format) || anySubFormat) {
						*index = infoIndex;
						return item;
					}
				}
			}
		} else if (type == FormatSpecifier::FormatSpecifierE::SPECIFIED_EXACTLY) { // Select specific quality stream and format
			for (unsigned int seekIndex = 0; seekIndex < formats->getSize(); seekIndex++) {
				char* format = formats->get(seekIndex);
				bool anyFormat = !strcmp(format, "*");
				for (unsigned int infoIndex = 0; infoIndex < info->items.getSize(); infoIndex++) {
					YoutubeVideoInfoItem* item = info->items.get(infoIndex);
					bool anySubFormat = anyFormat && (needAudio == item->hasAudio) && (needVideo == item->hasVideo);
					if ((!strcmp(format, item->format) || anySubFormat) && !strcmp(spec, item->quality)) {
						*index = infoIndex;
						return item;
					}
				}
			}
		} else if (type == FormatSpecifier::FormatSpecifierE::SPECIFIED_MINUS) { // Select specific quality (or worse) stream and format
			Array<FormatList::FormatListItem*>* knownFormats = ytPtr->getFormats();
			if (knownFormats == nullptr) { return noitem; }

			while (true) {

				// Find a match
				for (unsigned int seekIndex = 0; seekIndex < formats->getSize(); seekIndex++) {
					char* format = formats->get(seekIndex);
					bool anyFormat = !strcmp(format, "*");
					for (unsigned int infoIndex = 0; infoIndex < info->items.getSize(); infoIndex++) {
						YoutubeVideoInfoItem* item = info->items.get(infoIndex);
						bool anySubFormat = anyFormat && (needAudio == item->hasAudio) && (needVideo == item->hasVideo);
						if ((!strcmp(format, item->format) || anySubFormat) && !strcmp(spec, item->quality)) {
							delete knownFormats;
							*index = infoIndex;
							return item;
						}
					}
				}

				// Find previous quality
				bool hasP = contains(spec, "p");
				bool hasK = contains(spec, "k");
				bool found = false;
				for (unsigned int qualityIndex = 0; qualityIndex < knownFormats->getSize() - 1; qualityIndex++) { // Except for last
					char* quality = knownFormats->get(qualityIndex)->quality;
					if (!strcmp(quality, spec)) { // Have order
						unsigned int order = knownFormats->get(qualityIndex)->qualityOrder;

						// Find highest value smaller than "order"
						unsigned int closest = 0; // Overflow - highest value
						char* newSpec = spec;
						for (unsigned int seekQualityIndex = 0; seekQualityIndex < knownFormats->getSize(); seekQualityIndex++) {
							unsigned int seekOrder = knownFormats->get(seekQualityIndex)->qualityOrder;
							char* seekSpec = knownFormats->get(seekQualityIndex)->quality;
							if (seekOrder < order && (seekOrder > closest || !found)) {
								bool newHasP = contains(seekSpec, "p");
								bool newHasK = contains(seekSpec, "k");
								if (newHasP == hasP && newHasK == hasK) {
									newSpec = seekSpec;
									closest = seekOrder;
									found = true;
								}
							}
						}
						spec = newSpec;
						if (!found) {
							delete knownFormats;
							return noitem;
						}
						break;
					}
				}

				if (!found) { // No previous quality ?
					delete knownFormats;
					return noitem;
				}
			}
		} else if (type == FormatSpecifier::FormatSpecifierE::SPECIFIED_PLUS) { // Select specific quality (or better) stream and format
			Array<FormatList::FormatListItem*>* knownFormats = ytPtr->getFormats();
			if (knownFormats == nullptr) { return noitem; }

			while (true) {

				// Find a match
				for (unsigned int seekIndex = 0; seekIndex < formats->getSize(); seekIndex++) {
					char* format = formats->get(seekIndex);
					bool anyFormat = !strcmp(format, "*");
					for (unsigned int infoIndex = 0; infoIndex < info->items.getSize(); infoIndex++) {
						YoutubeVideoInfoItem* item = info->items.get(infoIndex);
						bool anySubFormat = anyFormat && (needAudio == item->hasAudio) && (needVideo == item->hasVideo);
						if ((!strcmp(format, item->format) || anySubFormat) && !strcmp(spec, item->quality)) {
							delete knownFormats;
							*index = infoIndex;
							return item;
						}
					}
				}

				// Find next quality
				bool hasP = contains(spec, "p");
				bool hasK = contains(spec, "k");
				bool found = false;
				for (unsigned int qualityIndex = 0; qualityIndex < knownFormats->getSize() - 1; qualityIndex++) { // Except for last
					char* quality = knownFormats->get(qualityIndex)->quality;
					if (!strcmp(quality, spec)) { // Have order
						unsigned int order = knownFormats->get(qualityIndex)->qualityOrder;

						// Find smallest value bigger than "order"
						unsigned int closest = 0; // Overflow - highest value
						char* newSpec = spec;
						for (unsigned int seekQualityIndex = 0; seekQualityIndex < knownFormats->getSize(); seekQualityIndex++) {
							unsigned int seekOrder = knownFormats->get(seekQualityIndex)->qualityOrder;
							char* seekSpec = knownFormats->get(seekQualityIndex)->quality;
							if (seekOrder > order && (seekOrder < closest || !found)) {
								bool newHasP = contains(seekSpec, "p");
								bool newHasK = contains(seekSpec, "k");
								if (newHasP == hasP && newHasK == hasK) {
									newSpec = seekSpec;
									closest = seekOrder;
									found = true;
								}
							}
						}
						spec = newSpec;
						if (!found) {
							delete knownFormats;
							return noitem;
						}
						break;
					}
				}

				if (!found) { // No previous quality ?
					delete knownFormats;
					return noitem;
				}
			}
		}
		return noitem;
	};

	auto downloader = [settings, outputFile, info, stopper, addExtension, progressReporter, reporterInstance](YoutubeVideoInfoItem* item, unsigned int* length, char** resultFileNamePtr) {
		struct downloaderStruct {
			DownloadProgress progressReporter;
			unsigned int* lengthPtr;
			void* reporterInstance;
			bool hasPrintedHeader;
			unsigned int lastProgress;
			bool peekOnly;
			bool* stopper;
		} dlReporter;
		dlReporter.progressReporter = progressReporter;
		dlReporter.reporterInstance = reporterInstance;
		dlReporter.lengthPtr = length;
		dlReporter.hasPrintedHeader = false;
		dlReporter.peekOnly = settings->peek;
		dlReporter.stopper = ( bool*) & stopper;
		*length = 0;
		DownloadProgress dlReporterCB = (DownloadProgress)[](void* instance, unsigned int done, unsigned int totalSize) {
			downloaderStruct* dlRep = ( downloaderStruct*) instance;
			unsigned int* lengthPtr = dlRep->lengthPtr;
			*lengthPtr = totalSize;
			if (dlRep->peekOnly) {
				*dlRep->stopper = true;
				return;
			}
			if (dlRep->progressReporter != nullptr) {
				dlRep->progressReporter(dlRep->reporterInstance, done, totalSize);
			} else { // Print report here
				if (!dlRep->hasPrintedHeader) {
					dlRep->hasPrintedHeader = true;
					SIZE_TO_STRBUF(sizeStr, totalSize);

					for (unsigned int i = 0; i < 50 - strlen(sizeStr) - 1; i++) {
						printf("=");
					}
					printf(" %s", sizeStr);
					dlRep->lastProgress = 0;
					printf("\r\n");
				}
				unsigned int donePercent = ((done * 100) / totalSize) / 2;
				if (donePercent != dlRep->lastProgress) {
					for (unsigned int i = 0; i < donePercent - dlRep->lastProgress; i++) {
						printf("|");
					}
					dlRep->lastProgress = donePercent;
					if (dlRep->lastProgress == 50) { // Done
						printf("\r\n");
						for (unsigned int i = 0; i < dlRep->lastProgress; i++) {
							printf("=");
						}
						printf("\r\n\r\n");
					}
				}
			}
		};

		auto getResultFileName = [outputFile, item]() {
			char* noString = nullptr;
			unsigned int olen = strlen(outputFile);
			unsigned int  extlen = strlen(item->format);
			MALLOC_N(outputFmt, char, olen + extlen + 2, {return noString;});
			memset(outputFmt, 0, olen + extlen + 2);
			if (ENDS_WITH(outputFile, item->format)) {
				sprintf_s(outputFmt, olen + 2, "%s", outputFile);
			} else {
				sprintf_s(outputFmt, olen + extlen + 2, "%s.%s", outputFile, item->format);
			}
			return outputFmt;
		};

		YoutubeVideo* video = info->download(( bool*) & stopper, item, dlReporterCB, &dlReporter);
		if (video == nullptr) {
			if (settings->peek) {
				char* resultFileName = getResultFileName();
				if (!resultFileName) {
					return false;
				}
				*resultFileNamePtr = resultFileName;
				return true;
			}
			printError("Downloading video failed");
			return false;
		}
		unsigned char* videoData = video->getRawData();
		unsigned int videoDataLength = video->getRawDataSize();
		if (outputFile != nullptr) {
			FILE* f;
			char* resultFileName = getResultFileName();
			if (!resultFileName) {
				delete video;
				return false;
			}

			if (fopen_s(&f, resultFileName, "wb")) {
				delete video;
				free(resultFileName);
				printError("Failed to open output file");
				return false;
			}
			*resultFileNamePtr = resultFileName;
			//free(outputFmt);
			fwrite(videoData, sizeof(char), videoDataLength, f);
			fclose(f);
		} else {
			fwrite(videoData, sizeof(char), videoDataLength, stdout);
		}
		delete video;
		return true;
	};

	bool dlOk = true;

	for (unsigned int specifierIndex = 0; specifierIndex < fmts.getSize() && dlOk; specifierIndex++) {
		FormatSpecifier* fmt = fmts.get(specifierIndex);

		if (fmt->quality != FormatSpecifier::FormatSpecifierE::NONE) {
			unsigned int index;
			Output::FormatTypeE type = fmt->audio && fmt->video ? Output::FormatTypeE::AUDIO_AND_VIDEO : fmt->audio ? Output::FormatTypeE::AUDIO_ONLY : Output::FormatTypeE::VIDEO_ONLY;
			YoutubeVideoInfoItem* item = infoGrabber(fmt->quality, fmt->formats, fmt->spec, fmt->audio, fmt->video, &index);
			if (item == nullptr) {
				char* resolveFailed = fmt->audio && fmt->video ? "No such video format" : fmt->audio ? "No such audio format" : "No such video format";
				output->printError(resolveFailed);
				dlOk = false;
			} else {
				unsigned int length = 0;
				char* resultFileName = nullptr;
				if (!downloader(item, &length, &resultFileName)) {
					FREE_NOT_NULL(resultFileName);
					char* downloadFail = fmt->audio && fmt->video ? "Failed to download video file" : fmt->audio ? "Failed to download audio file" : "Failed to download video file";
					output->printError(downloadFail);
					dlOk = false;
				} else {
					output->printDownloadeditem(type, index, item->quality, item->format, length, resultFileName, item->url);
					FREE_NOT_NULL(resultFileName);
					dlOk = false;
				}
			}
		}
	}
	destroyFormatData(&fmts);
	delete info;

	if (!dlOk) {
		return 1;
	}

	return 0;
}

int printListing(YoutubeSettings* settings, Output* output, char* URL) {
	Youtube yt(settings);
	bool stopper = false;
	YoutubeVideoInfo* info = yt.getVideo(&stopper, URL, true);
	if (info == nullptr) {
		output->printError("Failed to get video info");
		return 1;
	}
	output->printVideoNameAndDuration(info->videoName, info->length);

	unsigned int completeTotal = 0;
	unsigned int adaptiveAudioTotal = 0;
	unsigned int adaptiveVideoTotal = 0;
	for (unsigned int i = 0; i < info->items.getSize(); i++) {
		YoutubeVideoInfoItem* item = info->items.get(i);
		if (item->hasAudio && item->hasVideo) {
			completeTotal++;
		} else 	if (item->hasVideo) {
			adaptiveVideoTotal++;
		} else if (item->hasAudio) {
			adaptiveAudioTotal++;
		}
	}
	if (completeTotal == 0 && adaptiveVideoTotal == 0 && adaptiveAudioTotal == 0) {
		output->printNoAvailableFormats();
	} else {
		// Print complete streams
		if (completeTotal > 0) {
			if (!output->printFormatsHeader(OutputFormat::AUDIO_AND_VIDEO, completeTotal)) {
				delete info;
				return 1;
			}
			for (unsigned int i = 0; i < info->items.getSize(); i++) {
				YoutubeVideoInfoItem* item = info->items.get(i);
				if (item->hasAudio && item->hasVideo) {
					if (!output->printItem(OutputFormat::AUDIO_AND_VIDEO, i, item->quality, item->format, item->length)) {
						delete info;
						return 1;
					}
				}
			}
		}

		// Print adaptive video streams
		if (adaptiveVideoTotal > 0) {
			if (!output->printFormatsHeader(OutputFormat::VIDEO_ONLY, adaptiveVideoTotal)) {
				delete info;
				return 1;
			}
			// Print complete streams
			for (unsigned int i = 0; i < info->items.getSize(); i++) {
				YoutubeVideoInfoItem* item = info->items.get(i);
				if (item->hasVideo && !item->hasAudio) {
					if (!output->printItem(OutputFormat::VIDEO_ONLY, i, item->quality, item->format, item->length)) {
						delete info;
						return 1;
					}
				}
			}
		}
		// Print adaptive audio streams
		if (adaptiveAudioTotal > 0) {
			if (!output->printFormatsHeader(OutputFormat::AUDIO_ONLY, adaptiveAudioTotal)) {
				delete info;
				return 1;
			}
			// Print complete streams
			for (unsigned int i = 0; i < info->items.getSize(); i++) {
				YoutubeVideoInfoItem* item = info->items.get(i);
				if (item->hasAudio && !item->hasVideo) {
					if (!output->printItem(OutputFormat::AUDIO_ONLY, i, item->quality, item->format, item->length)) {
						delete info;
						return 1;
					}
				}

			}
		}
	}
	delete info;
	return 0;
}

int doInteractive(YoutubeSettings* settings, char* URL) {
	struct interData {
		char* url;
		unsigned int pickedFormatIndex;
		unsigned int maxFormatIndex;
		JSONOutput* lastInfo;
		bool use_video_cache;
	} data;
	data.url = nullptr;
	data.lastInfo = nullptr;

	enum State {
		ENTER_URL,
		PRINT_FORMATS,
		PICK_FORMAT,
		PICK_TARGET,
		EXIT
	} state;

	state = URL == nullptr ? ENTER_URL : PRINT_FORMATS;

	auto readLine = []() {
		WriteBuffer wb;
		ReadBuffer* rb = nullptr;
		bool error = false;
		while (true) {
			int c = getchar();
			if (c == EOF || c == '\n') {
				break;
			} else if (c == '\r') {
				continue;
			} else {
				wb.writeByte(( unsigned char) c, &error);
				if (error) {
					return rb;
				}
			}
		}
		wb.writeByte(0, &error);
		if (error) {
			return rb;
		}
		unsigned char* data;
		unsigned int length;
		wb.getWrittenData(&data, &length);
		rb = new ReadBuffer(data, length, &error);
		if (error) {
			delete rb;
			rb = nullptr;
		}
		return rb;
	};

	while (state != State::EXIT) {
		if (state == ENTER_URL) {
			printf("Insert video URL [Exit]: ");
			ReadBuffer* rb = readLine();
			if (!rb) {
				state = State::EXIT;
			} else {
				FREE_NOT_NULL(data.url);
				bool error = false;
				unsigned char* inputURL = rb->readArray(rb->getDataSize(), &error);
				delete rb;
				if (error) {
					state = State::EXIT;
				} else {
					if (!strcmp(( char*) inputURL, "")) {
						free(inputURL);
						state = State::EXIT;
					} else {
						data.url = ( char*) inputURL;
						DELETE_NOT_NULL(data.lastInfo);
						state = State::PRINT_FORMATS;
					}
				}
			}
		} else if (state == PRINT_FORMATS) {
			if (data.lastInfo == nullptr) { // Nothing cached, update
				printf("Collecting video info, please wait...\r\n");
				bool stop = false;
				DELETE_NOT_NULL(data.lastInfo);
				data.lastInfo = JSONOutput::create();
				if (!data.lastInfo) {
					printError("Failed to create output pipe");
					state = State::EXIT;
				} else {
					if (printListing(settings, data.lastInfo, data.url) != 0) { // Failed to get listing (internet error? Format error?)
						DELETE_NOT_NULL(data.lastInfo);
					}
				}
			}

			// Cache updated (or error happened)
			if (data.lastInfo == nullptr && state == PRINT_FORMATS) { // Cache failed to update
				printf("Failed to collect video info...\r\n");
				state = State::ENTER_URL;
			} else if (data.lastInfo != nullptr && state == PRINT_FORMATS) { // Have cached or somehow valid data
				JSONOutput::Items* itms = data.lastInfo->getItems();
				if (itms == nullptr) { // Failed to parse?
					state = State::EXIT;
				} else {
					printf("Video name: %s\r\nVideo length: %d seconds\r\n", itms->videoName, itms->videoLength);

					auto listOutputer = [](Array<JSONOutput::Item*>* arr, char* label) {
						printf("%s:\r\n", label);
						for (unsigned int i = 0; i < arr->getSize(); i++) {
							JSONOutput::Item* item = arr->get(i);
							char sizeBuffer[64];
							if (item->length == 0) {
								sprintf_s(sizeBuffer, "undetermined");
							} else {
								sprintf_s(sizeBuffer, getSizeStr(item->length), getSizeInt(item->length));
							}
							printf("\t%d: [%s] - %s {%s}\r\n", item->index, item->quality, sizeBuffer, item->format);
						}

					};

					if (itms->videoAndAudio->getSize() == 0) {
						printf("No formats to display\r\n");
					} else {
						listOutputer(itms->videoAndAudio, "Available formats");
					}
					listOutputer(itms->videoOnly, "Available video streams");
					listOutputer(itms->audioOnly, "Available audio streams");
					printf("\r\n\tX: Return\r\n\r\n");
					data.maxFormatIndex = itms->videoAndAudio->getSize() + itms->videoOnly->getSize() + itms->audioOnly->getSize() - 1;
					delete itms;
					state = State::PICK_FORMAT;
				}
			}
		} else if (state == State::PICK_FORMAT) {
			printf("Please select a format [X]: ");
			ReadBuffer* rb = readLine();
			if (rb == nullptr) {
				state = State::EXIT;
			} else {
				bool error = false;
				unsigned char* _line = rb->readArray(rb->getDataSize(), &error);
				delete rb;
				if (error) {
					state = State::EXIT;
				} else {
					char* line = ( char*) _line;
					if (!strcmp(line, "X") || !strcmp(line, "x") || !strcmp(line, "")) { // Return
						free(line);
						DELETE_NOT_NULL(data.lastInfo); // Clear cache?
						state = State::ENTER_URL;
					} else { // Format picked
						// Check if user input a correct format number
						unsigned int num = atoi(line);
						free(line);
						if (num < data.maxFormatIndex) {
							data.pickedFormatIndex = num;
							state = PICK_TARGET;
						}
					}

				}

			}
		} else if (state == PICK_TARGET) {
			// Find format
			auto formatSearch = [](Array<JSONOutput::Item*>* arr, unsigned int index, char** format) {
				if ((*format) == nullptr) {
					for (unsigned int i = 0; i < arr->getSize(); i++) {
						JSONOutput::Item* item = arr->get(i);
						if (item->index == index) {
							*format = item->format;
							return;
						}
					}
				}
			};
			JSONOutput::Items* itms = data.lastInfo->getItems();
			if (itms == nullptr) {
				state = State::EXIT;
			} else {
				char* format = nullptr;
				formatSearch(itms->videoAndAudio, data.pickedFormatIndex, &format);
				formatSearch(itms->videoOnly, data.pickedFormatIndex, &format);
				formatSearch(itms->audioOnly, data.pickedFormatIndex, &format);
				if (format == nullptr) {
					delete itms;
					printf("Invalid format selected\r\n");
					state = State::PICK_FORMAT;
				} else {
					printf("\r\nPlease select output file [%s.%s]: ", itms->videoName, format);
					ReadBuffer* rb = readLine();
					if (rb == nullptr) {
						delete itms;
						state = State::EXIT;
					} else {
						bool error = false;
						unsigned char* _line = rb->readArray(rb->getDataSize(), &error);
						delete rb;
						if (error) {
							delete itms;
							state = State::EXIT;
						} else {
							char* line = ( char*) _line;
							if (!strcmp(line, "")) { // Empty string
								unsigned int lineLen = strlen(itms->videoName);
								unsigned int fmtLen = strlen(format);
								MALLOC_N(newLine, char, lineLen + fmtLen + 2, {free(line); line = nullptr; });
								if (line != nullptr) { // Malloc did not fail
									sprintf_s(newLine, lineLen + fmtLen + 2, "%s.%s", itms->videoName, format);
									free(line);
									line = newLine;
								} else { // Malloc failed
									state = State::EXIT;
								}
							}
							delete itms;
							if (line != nullptr) {
								FILE* f;
								if (!fopen_s(&f, line, "wb")) {
									fclose(f);
									printf("Downloading, please wait...\r\n");
									ConsoleOutput* co = ConsoleOutput::create();
									if (co == nullptr) {
										free(line);
										state = State::EXIT;
									} else {
										char formatBuffer[32];
										sprintf_s(formatBuffer, "%d", data.pickedFormatIndex);

										auto progressReporter = [](void* instance, unsigned int done, unsigned int total) {
											unsigned int* instanceVar = ( unsigned int*) instance;
											unsigned int prevPercentage = *instanceVar;
											unsigned int percentage = ((done * 100) / total) / 2;
											if (percentage != prevPercentage) {
												*instanceVar = percentage;
											}
											for (unsigned int i = 0; i < percentage - prevPercentage; i++) {
												printf("I");
											}
										};

										for (unsigned int i = 0; i < 50; i++) {
											printf("_");
										}
										printf("\r\n");
										unsigned int instanceVar = 0;
										if (printData(settings, co, data.url, formatBuffer, line, false, progressReporter, &instanceVar) != 0) {
											printf("\r\n");
											delete co;
											free(line);
											printf("Failed to download file\r\n");
											state = State::PICK_FORMAT;
										} else {
											printf("\r\n");
											delete co;
											free(line);
											printf("Downloaded successfully\r\n");
											state = State::PICK_FORMAT;
										}
									}
								} else {
									printf("Cannot open \"%s\"\r\n", line);
									free(line);
								}
							}
						}
					}
				}
			}
		}
	}

	FREE_NOT_NULL(data.url);
	DELETE_NOT_NULL(data.lastInfo);

	return 0;
}

int main(int argc, char** argv) {
#define BOOL_ARG(argShort, argLong, target, cont) \
if (!strcmp(arg, argShort) || !strcmp(arg, argLong)) {\
	if (target) {\
		printError("Double argument not allowed");\
		return 5;\
	}\
	target = true;\
	if(cont){\
		continue; \
	}\
}
#define PARAM_ARG(argShort, argLong, target, cont) \
if ((!strcmp(arg, argShort) || !strcmp(arg, argLong))&& i != argc - 1) {\
	if (target != nullptr) {\
		printError("Double argument not allowed");\
		return 5;\
	}\
	i++;\
	target = argv[i]; \
	if(cont){\
		continue; \
	}\
}


	bool mode_list = false;
	bool mode_download = false;
	bool mode_interactive = false;
	char* URL = nullptr;
	char* output = nullptr;
	char* format = nullptr;
	bool jsonOutput = false;
	bool noCache = false;
	bool peek = false;
	char* cacheFile = nullptr;
	char* maxCache = nullptr;
	char* fmt_endpoint = nullptr; /* https://maple3142-ytdl-demo.glitch.me/api */

	for (unsigned int i = 1; i < ( unsigned int) argc; i++) {
		char* arg = argv[i];
		BOOL_ARG("-l", "--list", mode_list, false);
		PARAM_ARG("-l", "--list", URL, true);

		PARAM_ARG("-cf", "--cache-file", cacheFile, true);

		PARAM_ARG("-mcs", "--max-cache-size", maxCache, true);

		BOOL_ARG("-i", "--interactive", mode_interactive, true);

		BOOL_ARG("-nc", "--no-cache", noCache, true);
		BOOL_ARG("-p", "--peek", peek, true);
		BOOL_ARG("-j", "--json", jsonOutput, true);

		BOOL_ARG("-d", "--download", mode_download, false);
		PARAM_ARG("-d", "--download", URL, true);

		PARAM_ARG("-o", "--output", output, true);
		PARAM_ARG("-f", "--format", format, true);
		PARAM_ARG("-fmt", "--fmt-endpoint", fmt_endpoint, true);
	}

	if (maxCache != nullptr) {
		size_t cache = atoi(maxCache);
		char tmpBuffer[64];
		sprintf_s(tmpBuffer, "%d", cache);
		if (strcmp(tmpBuffer, maxCache)) {
			printError("Invalid cache size");
			return 1;
		}
	}

	Output* outputObj;
	if (jsonOutput) {
		outputObj = JSONOutput::create();
	} else {
		outputObj = ConsoleOutput::create();
	}
	if (outputObj == nullptr) {
		printError("Failed to create output");
	}
	if (fmt_endpoint != nullptr) {
		char* fmtAppend = "?id=%s&format=1";
		unsigned int len = strlen(fmt_endpoint) + strlen(fmtAppend) + 1;
		MALLOC_N(newFmt, char, len, {});
		if (newFmt == nullptr) {
			printError("Failed to init endpoint");
			return 1;
		}
		sprintf_s(newFmt, len, "%s%s", fmt_endpoint, fmtAppend);
		fmt_endpoint = newFmt;
	}

	YoutubeSettings settings;
	settings.url_parser_endpoint = fmt_endpoint;
	settings.use_video_cache = !noCache;
	settings.use_custom_fmt_parser = fmt_endpoint == nullptr;
	settings.peek = peek;
	settings.cache = nullptr;
	if (settings.use_video_cache) {
		settings.cache = new Cache(cacheFile == nullptr ? "youtube.cache" : cacheFile, maxCache == nullptr ? 0xffffffff : atoi(maxCache));
		if (!settings.cache->isValid) {
			delete settings.cache;
			printError("Failed to open cache file");
			return 1;
		}
	}

	int retValue = 0;
	if (URL == nullptr && !mode_interactive) {
		retValue = printUsage(outputObj);
	} else if (mode_list && !mode_download && !mode_interactive) {
		retValue = printListing(&settings, outputObj, URL);
	} else if (mode_download && format != nullptr && !mode_interactive) {
		retValue = printData(&settings, outputObj, URL, format, output, false, nullptr, nullptr);
	} else if (mode_interactive) {
		retValue = doInteractive(&settings, URL);
	} else {
		retValue = printUsage(outputObj);
	}
	outputObj->print();
	delete outputObj;
	FREE_NOT_NULL(fmt_endpoint);
	DELETE_NOT_NULL(settings.cache);

#ifdef DUMP_MEMLEAKS
	_RPT1(0, "%s\n", "=== begin of memory leak dump ===");
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtDumpMemoryLeaks();
	_RPT1(0, "%s\n", "=== end of memory leak dump ===");
#endif

	return retValue;
}
