#ifndef MEASUREMENTIMAGESTORAGEDLL_H
#define MEASUREMENTIMAGESTORAGEDLL_H

// http://www.flounder.com/ultimateheaderfile.htm


#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #if defined(COMPILING_MeasurementImagesStorageDLL_H) || defined(COMPILING_MEASUREMENTIMAGESTORAGEDLL)
        #define LIBSPEC __declspec(dllexport)
    #else
        #define LIBSPEC __declspec(dllimport)
    #endif
#else
    #if defined(COMPILING_MeasurementImagesStorageDLL_H) || defined(COMPILING_MEASUREMENTIMAGESTORAGEDLL)
        #if defined(__has_attribute) && __has_attribute(visibility)
            #define LIBSPEC __attribute__((visibility("default")))
        #else
            #define LIBSPEC
        #endif
    #else
        #define LIBSPEC
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MIS_API_VERSION 1

#define MIS_PIXELFORMAT_MONO8 2
#define MIS_PIXELFORMAT_MONO16 0
#define MIS_PIXELFORMAT_FLOAT64 1

/** @brief Returns the API version. 
 * 
 * Provided so the caller can check if the API version aligns with their expectation.
*/

LIBSPEC int32_t MISAPIVersion();

/**
 * @brief Opens an existing Measurement Image Storage file.
 *
 * @param outputFilePath Path to the existing storage file.
 * @return A storer ID on success, or a negative value on error.
 */
LIBSPEC int MISOpenFile(const char* outputFilePath, int* storerId);

/**
 * @brief Creates a new Measurement Image Storage file.
 *
 * @param outputFilePath Path to the file to create.
 * @param measurementDescriptor Human-readable description of the measurement.
 * @return A storer ID on success, or a negative value on error.
 */
LIBSPEC int MISNewStorage(const char* outputFilePath,
                              const char* measurementDescriptor,
                              int* storerId);

/**
 * @brief Closes an open storage instance.
 *
 * @param storerID Identifier returned by MISOpenFile or MISNewStorage.
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISClose(int64_t storerID);

/**
 * @brief Adds a new image and its metadata to the storage.
 *
 * @param storerID Storage instance identifier.
 * @param acqTypeName Acquisition type name.
 * @param detectorName Detector name.
 * @param timePoint Time point of acquisition.
 * @param stageX Stage X position.
 * @param stageY Stage Y position.
 * @param stageZ Stage Z position.
 * @param detectionIndex Detection index.
 * @param stagePositionName Name of the stage position.
 * @param pixelFormat Pixel format of the image data.
 * @param nRows Number of image rows.
 * @param nCols Number of image columns.
 * @param data Pointer to image pixel data (row-major).
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISAddNewImage(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    double timePoint,
    double stageX,
    double stageY,
    double stageZ,
    std::int64_t detectionIndex,
    char* stagePositionName,
    int pixelFormat,
    int nRows,
    int nCols,
    uint8_t* data);

/**
 * @brief Adds an encoded smart program decision to the storage.
 *
 * @param storerID Storage instance identifier.
 * @param encodedSmartProgramDecision Encoded decision string.
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISAddSmartProgramDecision(
    int64_t storerID,
    char* encodedSmartProgramDecision);

/**
 * @brief Retrieves the number of detections stored.
 *
 * @param storerID Storage instance identifier.
 * @param numDetections Output pointer receiving the number of detections.
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISGetNumberOfDetections(
    int64_t storerID,
    int64_t* numDetections);

/**
 * @brief Retrieves all acquisition type names.
 *
 * @param storerID Storage instance identifier.
 * @param acqTypeNamesPtr Pointer to an array of strings.
 * @param nAcqTypes Output number of acquisition types.
 * @return 0 on success, non-zero on error.
 *
 * @note Caller must free the array using MISFreeStringArray().
 */
LIBSPEC int MISGetAcquisitionNames(
    int64_t storerID,
    char*** acqTypeNamesPtr,
    int* nAcqTypes);

/**
 * @brief Retrieves all detector names.
 *
 * @param storerID Storage instance identifier.
 * @param detectorNamesPtr Pointer to an array of strings.
 * @param nDetectors Output number of detectors.
 * @return 0 on success, non-zero on error.
 *
 * @note Caller must free the array using MISFreeStringArray().
 */
LIBSPEC int MISGetDetectorNames(
    int64_t storerID,
    char*** detectorNamesPtr,
    int* nDetectors);

/**
 * @brief Retrieves the number of images for a given acquisition and detector.
 *
 * @param storerID Storage instance identifier.
 * @param acqTypeName Acquisition type name.
 * @param detectorName Detector name.
 * @param nImages Output number of images.
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISGetNumberOfImages(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int* nImages);

/**
 * @brief Retrieves image pixel data.
 *
 * @param storerID Storage instance identifier.
 * @param acqTypeName Acquisition type name.
 * @param detectorName Detector name.
 * @param imageIdx Image index.
 * @param dataLocationPtr Output pointer to image data.
 * @param nRows Output number of rows.
 * @param nCols Output number of columns.
 * @return 0 on success, non-zero on error.
 *
 * @note Image data must be released using MISReleaseImageData().
 */
LIBSPEC int MISGetImage(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int imageIdx,
    uint8_t** dataLocationPtr,
    int* nRows,
    int* nCols);

/**
 * @brief Releases image data previously returned by MISGetImage().
 *
 * @param dataPtr Pointer to image data.
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISReleaseImageData(uint8_t* dataPtr);

/**
 * @brief Retrieves the acquisition time point for an image.
 */
LIBSPEC int MISGetTimePoint(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int imageIdx,
    double* timePoint);

/**
 * @brief Retrieves the stage position for an image.
 */
LIBSPEC int MISGetStagePosition(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int imageIdx,
    double* stageX,
    double* stageY,
    double* stageZ);

/**
 * @brief Retrieves the detection index for an image.
 */
LIBSPEC int MISGetDetectionIndex(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int imageIdx,
    int64_t* detectionIndex);

/**
 * @brief Gets the image index corresponding to a detection index.
 * 
 * 'Corresponding' means that it returns the index of the latest image that was acquired
 * at or before this detection index. In other words, it is the most recent available image
 * at the time this detection index occurred. The index '-1' will be returned if no image
 * in this channel had been acquired by the time this detection index occurred.
 *
 * @param storerID Storage instance identifier.
 * @param acqTypeName Acquisition type name.
 * @param detectorName Detector name.
 * @param detectionIndex Detection index.
 * @param imageIdxPtr Output image index.
 * @return 0 on success, non-zero on error.
 */
LIBSPEC int MISGetImageIndex(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int64_t detectionIndex,
    int* imageIdxPtr);

/**
 * @brief Retrieves the stage position name for an image.
 *
 * @note Caller must free the string using MISFreeStagePositionName().
 */
LIBSPEC int MISGetStagePositionName(
    int64_t storerID,
    char* acqTypeName,
    char* detectorName,
    int imageIdx,
    char** namePtr);

/**
 * @brief Frees a stage position name string.
 */
LIBSPEC int MISFreeStagePositionName(char* name);

/**
 * @brief Retrieves the imager program description.
 *
 * @note Caller must free the string using MISFreeProgramDescription().
 */
LIBSPEC int MISGetImagerProgram(
    int64_t storerID,
    char** programDescriptionPtr);

/**
 * @brief Frees an imager program description string.
 */
LIBSPEC int MISFreeProgramDescription(char* programDescriptionPtr);

/**
 * @brief Retrieves encoded smart program decisions.
 *
 * @note Caller must free the array using MISFreeStringArray().
 */
LIBSPEC int MISGetSmartProgramDecisions(
    int64_t storerID,
    char*** encodedSmartProgramDecisionsPtr,
    int* numberOfDecisionsPtr);

/**
 * @brief Frees an array of strings returned by the API.
 */
LIBSPEC void MISFreeStringArray(char** array);
#ifdef __cplusplus
}
#endif

#endif
