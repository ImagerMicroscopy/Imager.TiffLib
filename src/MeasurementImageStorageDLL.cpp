#define COMPILING_MeasurementImagesStorageDLL_H

#include "MeasurementImageStorageDLL.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>

#include "FileStorageClass.h"
#include "FileLoaderClass.h"
#include "MiscUtils.h"

char** AllocateStringVectorArray(const std::vector<std::string>& strings);

std::map<std::int64_t, std::shared_ptr<StorageWrapperClass>> gStorerMap;
std::int64_t gStorerID = 0;
std::mutex gStorerMutex;

void ClearStorers() {
	gStorerMap.clear();
}

int64_t InsertNewStorer(std::shared_ptr<StorageWrapperClass> storer) {
	std::lock_guard<std::mutex> lock(gStorerMutex);
	std::uint64_t id = gStorerID++;
	gStorerMap.insert(std::make_pair(id, storer));
	return id;
}

std::shared_ptr<StorageWrapperClass> GetStorer(uint64_t storerID) {
	std::lock_guard<std::mutex> lock(gStorerMutex);
	return (gStorerMap.at(storerID));
}

void DeleteStorer(uint64_t storerID) {
	std::lock_guard<std::mutex> lock(gStorerMutex);
	if (gStorerMap.count(storerID) > 0) {
		gStorerMap.erase(storerID);
	} else {
		throw std::runtime_error("no such storer ID");
	}
}

int HandleExceptions(std::function<void()> f) {
	try { f(); }
	catch (...) {
		return -1;
	}
	return 0;
}

int64_t MISOpenFile(const char* outputFilePath) {
	try {
		std::shared_ptr<StorageWrapperClass> loader(new FileLoaderClass(outputFilePath));
		std::uint64_t id = InsertNewStorer(loader);
		return id;
	}
	catch (...)
	{
		return -1;
	}
}

int64_t MISNewStorage(const char* outputFilePath, const char* measurementDescriptor) {
	try {
		std::shared_ptr<FileStorageClass> storer(new FileStorageClass(outputFilePath, measurementDescriptor));
		std::uint64_t id = InsertNewStorer(storer);
		return id;
	}
	catch (...)
	{
		return -1;
	}
}

int MISClose(int64_t storerID) {
	return HandleExceptions([&]() {
		DeleteStorer(storerID);
	});
}

/**
 * Adds a new image to the measurement storage file.
 * 
 * @param storerID Handle to the storage instance (must be a storage, not loader)
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param timePoint Time when the image was acquired (in seconds)
 * @param stageX X coordinate of the microscope stage position (in micrometers)
 * @param stageY Y coordinate of the microscope stage position (in micrometers)
 * @param stageZ Z coordinate of the microscope stage position (in micrometers)
 * @param detectionIndex Sequential index indicating the order of detection/acquisition
 * @param stagePositionName Human-readable name for the stage position (can be empty)
 * @param nRows Number of rows (height) in the image
 * @param nCols Number of columns (width) in the image
 * @param data Pointer to 16-bit unsigned integer image data
 * @return 0 on success, -1 on error
 * 
 * The image data is copied into internal storage, so the caller retains ownership
 * of the data pointer and can free it after this call returns.
 * 
 * This function can only be called on storage instances created with MISNewStorage,
 * not on loader instances created with MISOpenFile.
 */
int MISAddNewImage(int64_t storerID, char* acqTypeName, char* detectorName, double timePoint, double stageX, double stageY, double stageZ,
				   std::int64_t detectionIndex, char* stagePositionName, int nRows, int nCols, uint16_t* data) {
	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storerW = GetStorer(storerID);
		FileStorageClass* storer = dynamic_cast<FileStorageClass*>(storerW.get());
		if (storer == nullptr) throw std::runtime_error("Adding image to non-output storer");
		AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);

		std::vector<std::uint8_t> imageData(nRows * nCols * sizeof(std::uint16_t));
		memcpy(imageData.data(), data, imageData.size());
		StagePosition stagePosition = std::make_tuple(stageX, stageY, stageZ);

		AcquiredImage acqImage(std::move(imageData), LNBTIFF::Mono16, {nRows, nCols}, timePoint, stagePosition, detectionIndex, stagePositionName);
		storer->addNewImage(acqTypeAndDetName, acqImage);
	});
}

int MISAddSmartProgramDecision(int64_t storerID, char* encodedSmartProgramDecision) {
    return HandleExceptions([&]() {
        std::shared_ptr<StorageWrapperClass> storerW = GetStorer(storerID);
        FileStorageClass* storer = dynamic_cast<FileStorageClass*>(storerW.get());
        if (storer == nullptr) throw std::runtime_error("Adding smart program decision to non-output storer");
        storer->addNewSmartProgramDecision(std::string(encodedSmartProgramDecision));
    });
}

/**
 * Retrieves the number of images stored for a specific acquisition/detector combination.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param nImages (output) Number of images stored for this acquisition/detector pair
 * @return 0 on success, -1 on error
 * 
 * This function returns the total count of images that have been stored or loaded
 * for the specified acquisition type and detector combination. Use this to determine
 * the valid range of image indices (0 to nImages-1) for other functions that access
 * individual images.
 */
int MISGetNumberOfImages(int64_t storerID, char* acqTypeName, char* detectorName, int* nImages) {
	return HandleExceptions([&]() {
		AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		*nImages = storer->getNumberOfStoredImages(acqTypeAndDetName);
	});
}

/**
 * Finds the image index with the largest detection index that is less than or equal to the target.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param detectionIndex Target detection index to search for
 * @param imageIdxPtr (output) Image index with detection index <= target, or -1 if none found
 * @return 0 on success, -1 on error
 * 
 * This function performs a binary search to efficiently find the image with the highest
 * detection index that does not exceed the target. If an exact match is found, returns
 * that image's index. If no exact match exists, returns the image with the largest
 * detection index that is still less than the target.
 * 
 * Returns -1 if:
 * - No images exist for the given acquisition/detector combination
 * - The target detection index is smaller than the first image's detection index
 * 
 * Note: Detection indices are assumed to be sorted in ascending order by image index.
 */
int MISGetImageIndexAtDetectionIndex(int64_t storerID, char* acqTypeName, char* detectorName,
	int64_t detectionIndex, int* imageIdxPtr) {
	return HandleExceptions([&]() {
		AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);

		int nImages = storer->getNumberOfStoredImages(acqTypeAndDetName);
		std::vector<int> view = storer->getDetectionIndicesForChannel(acqTypeAndDetName);
		auto it = std::ranges::lower_bound(view, detectionIndex);
		int idx = std::distance(view.begin(), it);
		*imageIdxPtr = storer->getImageIdxForDetectionIdxForChannel(acqTypeAndDetName, view[idx-1]);
		});
}


std::vector<AcquiredImage> gImagesInFlight;
std::mutex gImagesInFlightMutex;

 /**
 * Retrieves image data and dimensions for a specific image.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param imageIdx Index of the image (0-based)
 * @param dataLocationPtr (output) Pointer to 16-bit unsigned integer image data
 * @param nRows (output) Number of rows (height) in the image
 * @param nCols (output) Number of columns (width) in the image
 * @return 0 on success, -1 on error
 * 
 * The returned image data pointer is valid until MISReleaseImageData() is called
 * with the same pointer. The caller must call MISReleaseImageData() to free
 * the memory when done with the image data.
 * 
 * Note: Multiple images can be retrieved simultaneously; each must be released
 * individually with its corresponding data pointer.
 */
int MISGetImage(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, uint16_t** dataLocationPtr, int* nRows, int* nCols) {
    return HandleExceptions([&]() {
        AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
        std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
        AcquiredImage image = storer->getImage(acqTypeAndDetName, imageIdx);

        {
            std::lock_guard<std::mutex> lock(gImagesInFlightMutex);
            gImagesInFlight.emplace_back(std::move(image));
            AcquiredImage& newImage = gImagesInFlight.at(gImagesInFlight.size() - 1);
            *dataLocationPtr = reinterpret_cast<uint16_t*>(newImage.imageData.data());
            *nRows = image.imageSize.first;
            *nCols = image.imageSize.second;
        }
    });
}

/**
 * Releases image data memory previously obtained from MISGetImage().
 * 
 * @param dataPtr The image data pointer returned by MISGetImage()
 * @return 0 on success, -1 on error
 * 
 * This function must be called for every image data pointer obtained from
 * MISGetImage() to prevent memory leaks. After calling this function,
 * the dataPtr becomes invalid and should not be used.
 */
int MISReleaseImageData(uint16_t* dataPtr) {
	return HandleExceptions([&]() {
		std::lock_guard<std::mutex> lock(gImagesInFlightMutex);
		auto it = std::find_if(gImagesInFlight.begin(), gImagesInFlight.end(), [&](const AcquiredImage& im) -> bool {
			return (im.imageData.data() == reinterpret_cast<uint8_t*>(dataPtr));
		});
		if (it == gImagesInFlight.end()) {
			throw std::runtime_error("no such image in flight");
		}
		gImagesInFlight.erase(it);
	});
}

/**
 * Retrieves the time point when a specific image was acquired.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param imageIdx Index of the image (0-based)
 * @param timePoint (output) Time when the image was acquired (in seconds)
 * @return 0 on success, -1 on error
 */
int MISGetTimePoint(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, double* timePoint) {
	return HandleExceptions([&]() {
		AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		*timePoint = storer->getTimePoint(acqTypeAndDetName, imageIdx);
	});
}

/**
 * Retrieves the stage position coordinates for a specific image.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param imageIdx Index of the image (0-based)
 * @param stageX (output) X coordinate of the stage position (in micrometers)
 * @param stageY (output) Y coordinate of the stage position (in micrometers)
 * @param stageZ (output) Z coordinate of the stage position (in micrometers)
 * @return 0 on success, -1 on error
 */
int MISGetStagePosition(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, double* stageX, double* stageY, double* stageZ) {
	return HandleExceptions([&]() {
		AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		std::tie(*stageX, *stageY, *stageZ) = storer->getStagePosition(acqTypeAndDetName, imageIdx);
	});
}

/**
 * Retrieves the total number of detection events across all acquisition/detector combinations.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param numDetections (output) Total number of detection events
 * @return 0 on success, -1 on error
 * 
 * This function returns the total count of all detection events that have occurred
 * during the measurement, regardless of acquisition type or detector.
 */
int MISGetNumberOfDetections(int64_t storerID, int64_t *numDetections) {
	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		*numDetections = storer->getNumberOfDetections();
	});
}

/**
 * Retrieves all acquisition (channel) names from the measurement file.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeNamesPtr (output) Pointer to array of acquisition name strings
 * @param nAcqTypes (output) Number of acquisition names returned
 * @return 0 on success, -1 on error
 * 
 * Note: Use MISFreeStringArray() to free the returned array.
 */
int MISGetAcquisitionNames(int64_t storerID, char*** acqTypeNamesPtr, int* nAcqTypes) {
	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		const std::vector<std::string> acqNames = storer->getAcquisitionNames();
		char** names = AllocateStringVectorArray(acqNames);
		*nAcqTypes = static_cast<int>(acqNames.size());
		*acqTypeNamesPtr = names;
	});
}

/**
 * Retrieves all detector (camera) names from the measurement file.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param detectorNamesPtr (output) Pointer to array of detector name strings
 * @param nDetectors (output) Number of detector names returned
 * @return 0 on success, -1 on error
 * 
 * Note: Use MISFreeStringArray() to free the returned array.
 */
int MISGetDetectorNames(int64_t storerID, char*** detectorNamesPtr, int* nDetectors) {
	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		const std::vector<std::string> detNames = storer->getDetectorNames();
		char** names = AllocateStringVectorArray(detNames);
		*nDetectors = static_cast<int>(detNames.size());
		*detectorNamesPtr = names;
	});
}

/**
 * Retrieves the detection index for a specific image.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param imageIdx Index of the image (0-based)
 * @param detectionIndex (output) The detection index for this image
 * @return 0 on success, -1 on error
 */
int MISGetDetectionIndex(int64_t storerID, char *acqTypeName, char *detectorName, int imageIdx, int64_t *detectionIndex) {
    return HandleExceptions([&]() {
        AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
        std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
        *detectionIndex = storer->getDetectionIndex(acqTypeAndDetName, imageIdx);
    });
}

/**
 * Retrieves the stage position name for a specific image.
 * 
 * @param storerID Handle to the storage/loader instance
 * @param acqTypeName Name of the acquisition type (channel)
 * @param detectorName Name of the detector (camera)
 * @param imageIdx Index of the image (0-based)
 * @param namePtr (output) Pointer to the stage position name string
 * @return 0 on success, -1 on error
 * 
 * Note: Use MISFreeStagePositionName() to free the returned string.
 */
int MISGetStagePositionName(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, char** namePtr) {
    return HandleExceptions([&]() {
        AcqTypeAndDetName acqTypeAndDetName(acqTypeName, detectorName);
        std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
        std::string stagePositionName = storer->getStagePositionName(acqTypeAndDetName, imageIdx);
        char* name = new char[stagePositionName.size() + 1];
        strcpy(name, stagePositionName.c_str());
        *namePtr = name;
    });
}

/**
 * Frees the memory allocated by MISGetStagePositionName.
 * 
 * @param name The string pointer returned by MISGetStagePositionName
 * @return 0 on success, -1 on error
 */
int MISFreeStagePositionName(char* name) {
    return HandleExceptions([&]() {
        delete[] name;
    });
}

std::vector<std::string> gImagerProgramsInFlight;
std::mutex gImagerProgramsInFlightMutex;

int MISGetImagerProgram(int64_t storerID, char** programDescriptionPtr) {
	return HandleExceptions([&]() {
		std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
		std::lock_guard<std::mutex> lock(gImagerProgramsInFlightMutex);
		gImagerProgramsInFlight.push_back(storer->getSerializedImagerProgram());
		*programDescriptionPtr = const_cast<char*>(gImagerProgramsInFlight.at(gImagerProgramsInFlight.size() - 1).c_str());
	});
}

int MISFreeProgramDescription(char* programDescriptionPtr) {
	return HandleExceptions([&]() {
		std::lock_guard<std::mutex> lock(gImagerProgramsInFlightMutex);
		auto it = std::find_if(gImagerProgramsInFlight.begin(), gImagerProgramsInFlight.end(), [&](const std::string& s) {
			return (s.c_str() == programDescriptionPtr);
		});
		if (it == gImagerProgramsInFlight.end()) {
			throw std::runtime_error("no program in flight");
		}
		gImagerProgramsInFlight.erase(it);
	});
}

/**
 * Retrieves all smart program decision strings for the given storer.
 *
 * @param storerID The handle identifying the storage or loader instance.
 * @param encodedSmartProgramDecisionPtr (output) On success, set to point to an array of C strings (char*).
 *        The number of strings is returned in numberOfDecisionsPtr.
 *        This array must be freed by calling
 * @param numberOfDecisionsPtr (output) On success, set to the number of decision strings returned.
 *
 * @return 0 on success, -1 on error (e.g., invalid storerID).
 *
 * Usage example:
 *   char** decisions = nullptr;
 *   int numDecisions = 0;
 *   if (MISGetSmartProgramDecision(storerID, &decisions, &numDecisions) == 0) {
 *       for (int i = 0; i < numDecisions; ++i) {
 *           // Use decisions[i] (a null-terminated C string)
 *       }
 *       MISFreeSmartProgramDecision(decisions);
 *   }
 *
 * Notes:
 *   - Do not attempt to free the returned memory with delete[], free(), or any other function
 *     except MISFreeSmartProgramDecision.
 */
int MISGetSmartProgramDecisions(int64_t storerID, char*** encodedSmartProgramDecisionPtr, int* numberOfDecisionsPtr) {
    return HandleExceptions([&]() {
        std::shared_ptr<StorageWrapperClass> storer = GetStorer(storerID);
        std::vector<std::string> decisions = storer->getSmartProgramDecisions();
        *encodedSmartProgramDecisionPtr = reinterpret_cast<char**>(AllocateStringVectorArray(decisions));
		*numberOfDecisionsPtr = static_cast<int>(decisions.size());
	});
}

char** AllocateStringVectorArray(const std::vector<std::string>& strings) {
	size_t n = strings.size();
	if (strings.empty()) return nullptr;

	size_t nBytesForPointers = strings.size() * sizeof(char*);
	size_t lengthOfAllStrings = 0;
	for (const auto& s : strings) {
		lengthOfAllStrings += s.size() + 1;
	}
	char* memoryBlock = new char[nBytesForPointers + lengthOfAllStrings];
	char** stringPointers = reinterpret_cast<char**>(memoryBlock);
	char* stringContentsPtr = memoryBlock + nBytesForPointers;

	for (size_t i = 0; i < strings.size(); ++i) {
		stringPointers[i] = stringContentsPtr;
		strcpy(stringContentsPtr, strings[i].c_str());
		stringContentsPtr += strings[i].size() + 1;
	}
	return reinterpret_cast<char**>(memoryBlock);
}

void MISFreeStringArray(char** array) {
	if (array) {
		delete[] array;
	}
}
