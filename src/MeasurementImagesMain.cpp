

#include <vector>
#include <string>
#include <cassert>
#include <cmath>
#include <map>
#include <mutex>
#include <functional>
#include <stdexcept>

#include "XOPStandardHeaders.h"

#include "BasicTIFFWriter.h"
#include "FileStorageClass.h"
#include "FileLoaderClass.h"
#include "IgorUtils.h"

int gNextLoaderStorerID = 0;

template <typename T> class LoaderStorageSaver {
public:
	int insert(std::shared_ptr<T> loaderStorer) {
		std::lock_guard lock(_mapMutex);
		int thisID = gNextLoaderStorerID;
		gNextLoaderStorerID += 1;
		_map.insert({thisID, loaderStorer});
		return thisID;
	}

	std::shared_ptr<T> get(int id) {
		std::lock_guard lock(_mapMutex);
		return _map.at(id);
	}

	void erase(int id) {
		std::lock_guard lock(_mapMutex);
		if (_map.count(id) == 0) {
			throw std::runtime_error("trying to erase unknown storerID");
			return;
		}
		_map.erase(id);
	}

	void clear() {
		std::lock_guard lock(_mapMutex);
		_map.clear();
	}

private:
	std::map<int, std::shared_ptr<T>> _map;
	std::mutex _mapMutex;
};

LoaderStorageSaver<StorageWrapperClass> gLoaderStorerSaver;

LoaderStorageSaver<BasicTIFFWriter> gBasicTIFFWriterSaver;
LoaderStorageSaver<BasicTIFFReader> gBasicTIFFReaderSaver;

void ClearSavers() {
    gLoaderStorerSaver.clear();
	gBasicTIFFWriterSaver.clear();
	gBasicTIFFReaderSaver.clear();
}

int HandleExceptions(std::function<void()> f) {
    try { f(); }
    catch (int e) {
        return e;
    }
    catch (std::exception& e) {
		XOPNotice("MeasurementImageStorage exception: ");
        XOPNotice(e.what());
        XOPNotice("\r");
        return GENERAL_BAD_VIBS;
    }
    catch (...) {
        XOPNotice("MeasurementImageStorage unknown exception");
        return GENERAL_BAD_VIBS;
    }
    return 0;
}

class Finally {
	public:
		Finally(std::function<void()> f) :
			_f(f)
		{}
		~Finally() {
			_f();
		}
	private:
	std::function<void()> _f;
};

// Operation template: IMSNewStorage channelNames=string:channelNames, nImagesPerChannel=wave:wNImagesPerChannel, dataFolder=dataFolderRef:dataFolder, diskFolder=string:diskFolderPath

// Runtime param structure for IMSNewStorage operation.
#pragma pack(2)	// All structures passed to Igor are two-byte aligned.
struct IMSNewStorageRuntimeParams {
	// Flag parameters.

	// Main parameters.

	// Parameters for channelNames keyword group.
	int channelNamesEncountered;
	waveHndl channelNames_channelNames;
	int channelNamesParamsSet[1];

	// Parameters for detectorNames keyword group.
	int detectorNamesEncountered;
	waveHndl detectorNames_detectorNames;
	int detectorNamesParamsSet[1];

	// Parameters for imageDimensions keyword group.
	int imageDimensionsEncountered;
	waveHndl imageDimensions_wImageDimensions;
	int imageDimensionsParamsSet[1];

	// Parameters for serializedProgram keyword group.
	int serializedProgramEncountered;
	Handle serializedProgram_program;
	int serializedProgramParamsSet[1];

	// Parameters for filePath keyword group.
	int filePathEncountered;
	Handle filePath_filePath;
	int filePathParamsSet[1];

	// These are postamble fields that Igor sets.
	int calledFromFunction;					// 1 if called from a user function, 0 otherwise.
	int calledFromMacro;					// 1 if called from a macro, 0 otherwise.
};
typedef struct IMSNewStorageRuntimeParams IMSNewStorageRuntimeParams;
typedef struct IMSNewStorageRuntimeParams* IMSNewStorageRuntimeParamsPtr;
#pragma pack()	// Reset structure alignment to default.

extern "C" int
ExecuteIMSNewStorage(IMSNewStorageRuntimeParamsPtr p) {
    // Main parameters.
    return HandleExceptions([&]() {
        int err = 0;
        std::vector<std::string> channelNames;
        if (p->channelNamesEncountered) {
            // Parameter: p->channelNames_channelNames (test for NULL handle before using)
            if (p->channelNames_channelNames == nullptr) {
                throw int(EXPECTED_WAVE_REF);
            }
            channelNames = WaveToStringVector(p->channelNames_channelNames);
        } else {
            throw int(EXPECTED_WAVE_REF);
        }

		std::vector<std::string> detectorNames;
		if (p->detectorNamesEncountered) {
			// Parameter: p->detectorNames_detectorNames (test for NULL handle before using)
			if (p->detectorNames_detectorNames == nullptr) {
				throw int(EXPECTED_WAVE_REF);
			}
			detectorNames = WaveToStringVector(p->detectorNames_detectorNames);
		} else {
			throw int(EXPECTED_WAVE_REF);
		}

        std::string serializedProgram;
        if (p->serializedProgramEncountered) {
            // Parameter: p->serializedProgram_program (test for NULL handle before using)
            if (p->serializedProgram_program == nullptr) {
                throw int(USING_NULL_STRVAR);
            }
            serializedProgram = HandleToString(p->serializedProgram_program);
        } else {
            throw int(EXPECTED_STRING);
        }

        std::string filePath;
        if (p->filePathEncountered) {
            // Parameter: p->diskFolder_diskFolderPath (test for NULL handle before using)
            if (p->filePath_filePath == nullptr) {
                throw int(USING_NULL_STRVAR);
            }
            filePath = HandleToNativePath(p->filePath_filePath);
		} else {
			throw int(EXPECTED_STRING);
		}

        std::shared_ptr<FileStorageClass> storer(new FileStorageClass(filePath, serializedProgram));
		int id = gLoaderStorerSaver.insert(storer);

        SetOperationNumVar("V_flag", 0.0);
        SetOperationNumVar("V_IMSID", (double)id);
    });
}

static int
RegisterIMSNewStorage(void) {
    const char* cmdTemplate;
    const char* runtimeNumVarList;
    const char* runtimeStrVarList;

	// NOTE: If you change this template, you must change the IMSNewStorageRuntimeParams structure as well.
	cmdTemplate = "IMSNewStorage channelNames=wave:channelNames, detectorNames=wave:detectorNames, imageDimensions=wave:wImageDimensions, serializedProgram=string:program, filePath=string:filePath";
	runtimeNumVarList = "V_flag;V_IMSID";
    runtimeStrVarList = "";
    return RegisterOperation(cmdTemplate, runtimeNumVarList, runtimeStrVarList, sizeof(IMSNewStorageRuntimeParams), (void*)ExecuteIMSNewStorage, 0);
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSOpenFileParams {
    Handle filePath;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
    double result;
};
typedef struct IMSOpenFileParams IMSOpenFileParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSOpenFile(IMSOpenFileParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->filePath);
	});

    return HandleExceptions([&]() {
        if (p->filePath == nullptr) {
            throw int(EXPECTED_STRING);
        }
        std::shared_ptr<StorageWrapperClass> loader(new FileLoaderClass(HandleToNativePath(p->filePath)));
		int id = gLoaderStorerSaver.insert(loader);
        p->result = id;
    });
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSAddNewImageParams {
	Handle stagePositionName;
	double detectionIndex;
    waveHndl stagePositionWave;
    double timePoint;
	Handle detectorName;
    Handle acqTypeName;
    waveHndl imageWave;
    double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
    double result;
};
typedef struct IMSAddNewImageParams IMSAddNewImageParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSAddNewImage(IMSAddNewImageParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->stagePositionName);
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});

    return HandleExceptions([&] () {
        if (p->imageWave == nullptr) {
            throw int(EXPECTED_WAVE_REF);
        }
        if (p->stagePositionWave == nullptr) {
            throw int(EXPECTED_WAVE_REF);
        }
        if (p->acqTypeName == nullptr) {
            throw int(EXPECTED_STRING);
        }
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
    	if (p->stagePositionName == nullptr) {
			throw int(EXPECTED_STRING);
		}
        std::shared_ptr<StorageWrapperClass> storerW = gLoaderStorerSaver.get(p->storerID);
        FileStorageClass* storer = dynamic_cast<FileStorageClass*>(storerW.get());
        if (storer == nullptr) throw std::runtime_error("Adding image to non-output storer");
        AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
    	std::string stagePositionName = HandleToString(p->stagePositionName);
        AcquiredImage acqImage = ImageFromWave(p->imageWave, p->timePoint, p->stagePositionWave, p->detectionIndex, stagePositionName);
        storer->addNewImage(acqTypeAndDetName, acqImage);
        p->result = 0;
    });
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSAddSmartProgramDecisionParams {
	Handle encodedSmartProgramDecision;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSAddSmartProgramDecisionParams IMSAddSmartProgramDecisionParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSAddSmartProgramDecision(IMSAddSmartProgramDecisionParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->encodedSmartProgramDecision);
	});

	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storerW = gLoaderStorerSaver.get(p->storerID);
		FileStorageClass* storer = dynamic_cast<FileStorageClass*>(storerW.get());
		if (storer == nullptr) throw std::runtime_error("Adding image to non-output storer");
		storer->addNewSmartProgramDecision(HandleToString(p->encodedSmartProgramDecision));
		p->result = 0;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSNoMoreImagesComingParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSNoMoreImagesComingParams IMSNoMoreImagesComingParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSNoMoreImagesComing(IMSNoMoreImagesComingParams* p) {
	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storerW = gLoaderStorerSaver.get(p->storerID);
		FileStorageClass* storer = dynamic_cast<FileStorageClass*>(storerW.get());
		if (storer == nullptr) throw std::runtime_error("Adding image to non-output storer");
		storer->finishedAddingImages();
		p->result = 0;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMGetImageParams {
    double imageIndex;
	Handle detectorName;
    Handle acqTypeName;
    double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
    waveHndl result;
};
typedef struct IMGetImageParams IMGetImageParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetImage(IMGetImageParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});

    p->result = nullptr;
    return HandleExceptions([=] () {
        if (p->acqTypeName == nullptr) {
            throw int(EXPECTED_STRING);
        }
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
        std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		AcquiredImage image = storer->getImage(acqTypeAndDetName, p->imageIndex);
		waveHndl imageW = FreeWaveFromImage(image);
        p->result = imageW;
    });
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetTimepointParams {
	double imageIndex;
	Handle detectorName;
	Handle acqTypeName;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSGetTimepointParams IMSGetTimepointParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetTimepoint(IMSGetTimepointParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});

	return HandleExceptions([=]() {
		if (p->acqTypeName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		p->result = storer->getTimePoint(acqTypeAndDetName, p->imageIndex);
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetStagePositionParams {
	double imageIndex;
	Handle detectorName;
	Handle acqTypeName;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	waveHndl result;
};
typedef struct IMSGetStagePositionParams IMSGetStagePositionParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetStagePosition(IMSGetStagePositionParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});
	
	p->result = nullptr;
	return HandleExceptions([=]() {
		if (p->acqTypeName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		std::vector<double> stagePosition(3);
		std::tie(stagePosition[0], stagePosition[1], stagePosition[2]) = storer->getStagePosition(acqTypeAndDetName, p->imageIndex);
		p->result = NumericVectorToWave(stagePosition);
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetNumberOfDetectionsParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSGetNumberOfDetectionsParams IMSGetNumberOfDetectionsParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetNumberOfDetections(IMSGetNumberOfDetectionsParams* p) {
	return HandleExceptions([=]() {
		p->result = -1.0;
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		std::int64_t numberOfDetections = storer->getNumberOfDetections();
		p->result = numberOfDetections;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetDetectionIndexParams {
	double imageIndex;
	Handle detectorName;
	Handle acqTypeName;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSGetDetectionIndexParams IMSGetDetectionIndexParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetDetectionIndex(IMSGetDetectionIndexParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});
	p->result = -1.0;
	
	return HandleExceptions([=]() {
		if (p->acqTypeName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		std::int64_t detectionIndex = storer->getDetectionIndex(acqTypeAndDetName, p->imageIndex);
		p->result = detectionIndex;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetStagePositionNamesParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	waveHndl result;
};
typedef struct IMSGetStagePositionNamesParams IMSGetStagePositionNamesParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetStagePositionNames(IMSGetStagePositionNamesParams* p) {
	return HandleExceptions([=]() {
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		p->result = StringVectorToWave(storer->getStagePositionNames());
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetStagePositionNameParams {
	double imageIndex;
	Handle detectorName;
	Handle acqTypeName;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	Handle result;
};
typedef struct IMSGetStagePositionNameParams IMSGetStagePositionNameParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetStagePositionName(IMSGetStagePositionNameParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});

	p->result = nullptr;
	return HandleExceptions([=]() {
		if (p->acqTypeName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		std::string stagePositionName = storer->getStagePositionName(acqTypeAndDetName, p->imageIndex);
		p->result = WMNewHandle(0);
		int err = PutCStringInHandle(stagePositionName.c_str(), p->result);
		if (err) {
			throw int(err);
		}
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetStorageLocationParams {
    double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
    Handle result;
};
typedef struct IMSGetStorageLocationParams IMSGetStorageLocationParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetStorageLocation(IMSGetStorageLocationParams* p) {
    p->result = nullptr;
    return HandleExceptions([=]() {
        std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
        std::string location = storer->getStorageLocation();
        p->result = WMNewHandle(0);
        int err = PutCStringInHandle(location.c_str(), p->result);
        if (err) {
            throw int(err);
        }
    });
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetImagerProgramParams {
    double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
    Handle result;
};
typedef struct IMSGetImagerProgramParams IMSGetImagerProgramParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetImagerProgram(IMSGetImagerProgramParams* p) {
    p->result = nullptr;
    return HandleExceptions([=]() {
        std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
        std::string imagerProgram = storer->getSerializedImagerProgram();
        p->result = WMNewHandle(0);
        int err = PutCStringInHandle(imagerProgram.c_str(), p->result);
        if (err) {
            throw int(err);
        }
    });
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetAcquisitionTypeNamesParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	waveHndl result;
};
typedef struct IMSGetAcquisitionTypeNamesParams IMSGetAcquisitionTypeNamesParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetAcquisitionTypeNames(IMSGetAcquisitionTypeNamesParams* p) {
	p->result = nullptr;
	return HandleExceptions([=]() {
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		p->result = StringVectorToWave(storer->getAcquisitionNames());
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetDetectorNamesParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	waveHndl result;
};
typedef struct IMSGetDetectorNamesParams IMSGetDetectorNamesParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetDetectorNames(IMSGetDetectorNamesParams* p) {
	p->result = nullptr;
	return HandleExceptions([=]() {
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		p->result = StringVectorToWave(storer->getDetectorNames());
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetNumberOfStoredImagesParams {
	Handle detectorName;
	Handle acqTypeName;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSGetNumberOfStoredImagesParams IMSGetNumberOfStoredImagesParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetNumberOfStoredImages(IMSGetNumberOfStoredImagesParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->detectorName);
		WMDisposeHandle(p->acqTypeName);
	});

	return HandleExceptions([=]() {
		if (p->acqTypeName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		if (p->detectorName == nullptr) {
			throw int(EXPECTED_STRING);
		}
		AcqTypeAndDetName acqTypeAndDetName(HandleToString(p->acqTypeName), HandleToString(p->detectorName));
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		p->result = storer->getNumberOfStoredImages(acqTypeAndDetName);
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetSmartProgramDecisionsParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	waveHndl result;
};
typedef struct IMSGetSmartProgramDecisionsParams IMSGetSmartProgramDecisionsParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetSmartProgramDecisions(IMSGetSmartProgramDecisionsParams* p) {
	return HandleExceptions([=] () {
		p->result = nullptr;
		std::shared_ptr<StorageWrapperClass> storer = gLoaderStorerSaver.get(p->storerID);
		std::vector<std::string> encodedDecisions = storer->getSmartProgramDecisions();
		p->result = StringVectorToWave(encodedDecisions);
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMCloseParams {
    double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
    double result;
};
typedef struct IMCloseParams IMCloseParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSClose(IMCloseParams* p) {
    return HandleExceptions([=] () {
		gLoaderStorerSaver.erase(p->storerID);
        p->result = 0;
    });
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSNewBasicStorageParams {
	Handle filePath;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSNewBasicStorageParams IMSNewBasicStorageParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSNewBasicStorage(IMSNewBasicStorageParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->filePath);
	});

	return HandleExceptions([=]() {
		if (p->filePath == nullptr) {
			throw int(EXPECTED_STRING);
		}
		std::shared_ptr<BasicTIFFWriter> storer(new BasicTIFFWriter(HandleToNativePath(p->filePath)));
		int id = gBasicTIFFWriterSaver.insert(storer);
		p->result = id;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSAddNewBasicImageParams {
	waveHndl imageWave;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSAddNewBasicImageParams IMSAddNewBasicImageParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSAddNewBasicImage(IMSAddNewBasicImageParams* p) {
	return HandleExceptions([&]() {
		if (p->imageWave == nullptr) {
			throw int(EXPECTED_WAVE_REF);
		}
		std::shared_ptr<BasicTIFFWriter> storer = gBasicTIFFWriterSaver.get(p->storerID);
		auto[imageData, pixelType, imageSize] = ExtractImageDataFromWave(p->imageWave);
		std::uint16_t* dataAs16Ptr = reinterpret_cast<std::uint16_t*>(imageData.data());
		std::vector<std::uint16_t> dataAs16(dataAs16Ptr, dataAs16Ptr + imageData.size() / sizeof(std::uint16_t));
		storer->addNewImage(dataAs16, pixelType, imageSize);
		p->result = 0;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMCloseBasicParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMCloseBasicParams IMCloseBasicParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSCloseBasicStorage(IMCloseBasicParams* p) {
	return HandleExceptions([=]() {
		gBasicTIFFWriterSaver.erase(p->storerID);
		p->result = 0;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSNewBasicLoaderParams {
	Handle filePath;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSNewBasicLoaderParams IMSNewBasicLoaderParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSNewBasicLoader(IMSNewBasicStorageParams* p) {
	Finally fin([=]() {
		WMDisposeHandle(p->filePath);
	});

	return HandleExceptions([=]() {
		if (p->filePath == nullptr) {
			throw int(EXPECTED_STRING);
		}
		std::shared_ptr<BasicTIFFReader> reader(new BasicTIFFReader(HandleToNativePath(p->filePath)));
		int id = gBasicTIFFReaderSaver.insert(reader);
		p->result = id;
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetNumberOfStoredImagesBasicParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMSGetNumberOfStoredImagesBasicParams IMSGetNumberOfStoredImagesBasicParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetNumberOfStoredImagesBasic(IMSGetNumberOfStoredImagesBasicParams* p) {
	return HandleExceptions([&]() {
		std::shared_ptr<BasicTIFFReader> reader = gBasicTIFFReaderSaver.get(p->storerID);
		p->result = reader->nImages();
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMSGetBasicImageParams {
	double imageIdx;
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	waveHndl imageH;
};
typedef struct IMSGetBasicImageParams IMSGetBasicImageParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMSGetBasicImage(IMSGetBasicImageParams* p) {
	return HandleExceptions([&]() {
		std::shared_ptr<BasicTIFFReader> reader = gBasicTIFFReaderSaver.get(p->storerID);
		BasicTIFFReader::ReadImage readImage = reader->readImage(p->imageIdx);

		std::uint8_t* byteData = reinterpret_cast<std::uint8_t*>(readImage.data.data());
		std::vector<std::uint8_t> imageData(byteData, byteData + (readImage.data.size() * sizeof(std::uint16_t)));
		AcquiredImage acquiredImage(std::move(imageData), LNBTIFF::Mono16, readImage.size, -1.0, StagePosition(), -1, "");
		p->imageH = FreeWaveFromImage(acquiredImage);
	});
}

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct IMCloseBasicLoaderParams {
	double storerID;
	UserFunctionThreadInfoPtr tp; // Pointer to Igor private data
	double result;
};
typedef struct IMCloseBasicLoaderParams IMCloseBasicLoaderParams;
#pragma pack()		// Reset structure alignment to default.

extern "C" int
IMCloseBasicLoader(IMCloseBasicParams* p) {
	return HandleExceptions([=]() {
		gBasicTIFFReaderSaver.erase(p->storerID);
		p->result = 0;
	});
}

static int RegisterOperations(void)		// Register any operations with Igor.
{
	int result;

    result = RegisterIMSNewStorage();
    if (result)
        return result;
	// There are no more operations added by this XOP.

	return 0;
}

static XOPIORecResult
RegisterFunction() {
    int funcIndex;

    funcIndex = (int)GetXOPItem(0);			/* which function invoked ? */
    switch (funcIndex) {
        case 0:
            return (XOPIORecResult)IMSOpenFile;	/* This uses the direct call method - preferred. */
            break;
        case 1:
            return (XOPIORecResult)IMSAddNewImage;	/* This uses the direct call method - preferred. */
            break;
		case 2:
			return (XOPIORecResult)IMSAddSmartProgramDecision;
			break;
		case 3:
			return (XOPIORecResult)IMSNoMoreImagesComing;
			break;
        case 4:
            return (XOPIORecResult)IMSGetImage;
            break;
		case 5:
			return (XOPIORecResult)IMSGetTimepoint;
			break;
		case 6:
			return (XOPIORecResult)IMSGetStagePosition;
			break;
    	case 7:
    		return (XOPIORecResult)IMSGetNumberOfDetections;
    		break;
    	case 8:
    		return (XOPIORecResult)IMSGetDetectionIndex;
    		break;
    	case 9:
    		return (XOPIORecResult)IMSGetStagePositionNames;
    		break;
    	case 10:
    		return (XOPIORecResult)IMSGetStagePositionName;
    		break;
    	case 11:
    		return (XOPIORecResult)IMSGetStorageLocation;
    		break;
    	case 12:
    		return (XOPIORecResult)IMSGetImagerProgram;
    		break;
    	case 13:
    		return (XOPIORecResult)IMSGetAcquisitionTypeNames;
    		break;
    	case 14:
    		return (XOPIORecResult)IMSGetDetectorNames;
    		break;
    	case 15:
    		return (XOPIORecResult)IMSGetNumberOfStoredImages;
    		break;
		case 16:
			return (XOPIORecResult)IMSGetSmartProgramDecisions;
			break;
    	case 17:
    		return (XOPIORecResult)IMSClose;
    		break;
    	case 18:
    		return (XOPIORecResult)IMSNewBasicStorage;
    		break;
    	case 19:
    		return (XOPIORecResult)IMSAddNewBasicImage;
    		break;
    	case 20:
    		return (XOPIORecResult)IMSCloseBasicStorage;
    		break;
    	case 21:
    		return (XOPIORecResult)IMSNewBasicLoader;
    		break;
    	case 22:
    		return (XOPIORecResult)IMSGetNumberOfStoredImagesBasic;
    		break;
    	case 23:
    		return (XOPIORecResult)IMSGetBasicImage;
    		break;
    	case 24:
    		return (XOPIORecResult)IMCloseBasicLoader;
    		break;
        default:
            XOPNotice("trying to register non-existent function\r");
            break;
    }
    return 0;
}

bool CheckWaveInUse(waveHndl w) {
    /*for (auto it : gStorerMap) {
        if (it.second->isWaveInUse(w)) {
            return true;
        }
    }*/
    return false;
}

/*	XOPEntry()

This is the entry point from the host application to the XOP for all
messages after the INIT message.
*/
static void XOPEntry(void) {
    XOPIORecResult  result = 0;

    switch (GetXOPMessage()) {
        case INIT:
            break;
        case CLEANUP:
			ClearSavers();
            break;
        case NEW:
            ClearSavers();
            break;
        case OBJINUSE:
            {
                XOPIORecResult objID = GetXOPItem(0);
                XOPIORecResult objType = GetXOPItem(1);
                result = 0;
                if (objType == WAVE_OBJECT) {
                    result = CheckWaveInUse((waveHndl)objID);
                }
                break;
            }
        case FUNCADDRS:
            result = RegisterFunction();
            break;
        case FUNCTION:
            break;
    }
    SetXOPResult(result);
}

/*	main(ioRecHandle)

This is the initial entry point at which the host application calls XOP.
The message sent by the host must be INIT.

main does any necessary initialization and then sets the XOPEntry field of the
ioRecHandle to the address to be called for future messages.
*/
HOST_IMPORT int XOPMain(IORecHandle ioRecHandle) {
	int result;

	XOPInit(ioRecHandle);							// Do standard XOP initialization.
	SetXOPEntry(XOPEntry);							// Set entry point for future calls.
	if (igorVersion < 800) {
		SetXOPResult(IGOR_OBSOLETE);
		return EXIT_FAILURE;
	}

	if ((result = RegisterOperations())) {
		SetXOPResult(result);
	}
	else {
		SetXOPResult(0);
	}
	return EXIT_SUCCESS;
}
