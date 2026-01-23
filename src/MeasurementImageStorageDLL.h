#ifndef MEASUREMENTIMAGESTORAGEDLL_H
#define MEASUREMENTIMAGESTORAGEDLL_H

// http://www.flounder.com/ultimateheaderfile.htm


#include <cstdint>



#ifdef COMPILING_MeasurementImagesStorageDLL_H
#define LIBSPEC __declspec(dllexport)
#else
#define LIBSPEC __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif
	LIBSPEC int64_t MISOpenFile(const char* outputFilePath);
	LIBSPEC int64_t MISNewStorage(const char* outputFilePath, const char* measurementDescriptor);
	LIBSPEC int MISClose(int64_t storerID);

	LIBSPEC int MISAddNewImage(int64_t storerID, char* acqTypeName, char* detectorName, double timePoint, double stageX, double stageY, double stageZ,
							   std::int64_t detectionIndex, char* stagePositionName, int nRows, int nCols, uint16_t* data);
	LIBSPEC int MISAddSmartProgramDecision(int64_t storerID, char* encodedSmartProgramDecision);

	LIBSPEC int MISGetNumberOfDetections(int64_t storerID, int64_t* numDetections);
	LIBSPEC int MISGetAcquisitionNames(int64_t storerID, char*** acqTypeNamesPtr, int* nAcqTypes);
	LIBSPEC int MISGetDetectorNames(int64_t storerID, char*** detectorNamesPtr, int* nDetectors);
	LIBSPEC int MISGetNumberOfImages(int64_t storerID, char* acqTypeName, char* detectorName, int* nImages);
	LIBSPEC int MISGetImageIndexAtDetectionIndex(int64_t storerID, char* acqTypeName, char* detectorName, int64_t detectionIndex, int* imageIdxPtr);
	
	LIBSPEC int MISGetImage(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, uint16_t** dataLocationPtr, int* nRows, int* nCols);
	LIBSPEC int MISReleaseImageData(uint16_t* dataPtr);

	LIBSPEC int MISGetTimePoint(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, double* timePoint);
	LIBSPEC int MISGetStagePosition(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, double* stageX, double* stageY, double* stageZ);
	LIBSPEC int MISGetDetectionIndex(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, int64_t* detectionIndex);
	LIBSPEC int MISGetStagePositionName(int64_t storerID, char* acqTypeName, char* detectorName, int imageIdx, char** namePtr);
	LIBSPEC int MISFreeStagePositionName(char* name);
	
	LIBSPEC int MISGetImagerProgram(int64_t storerID, char** programDescriptionPtr);
	LIBSPEC int MISFreeProgramDescription(char* programDescriptionPtr);

	LIBSPEC int MISGetSmartProgramDecisions(int64_t storerID, char*** encodedSmartProgramDecisionsPtr, int* numberOfDecisionsPtr);
	
	LIBSPEC void MISFreeStringArray(char** array);

#ifdef __cplusplus
}
#endif

#endif
