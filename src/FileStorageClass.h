#ifndef FILESTORAGECLASS_H
#define FILESTORAGECLASS_H

#include "StorageWrapperClass.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <memory>
#include <mutex>
#include <deque>
#include <map>

#include "ReaderWriterQueue/atomicops.h"
#include "ReaderWriterQueue/readerwriterqueue.h"

#include "TIFFWriter.h"
#include "MiscUtils.h"

using AcquisitionName = std::string;

class AsyncData {
public:
	AcqTypeAndDetName acqTypeAndDetName;
	int indexWithinAcqAndDet;
	std::pair<int, int> imageDimensions;
    std::vector<std::uint8_t> imageData;
	LNBTIFF::PixelFormat pixelFormat;
};

class FileStorageClass : public StorageWrapperClass {
public:
    FileStorageClass(const std::string& filePath, const std::string& serializedProgram);
    virtual ~FileStorageClass();
    std::string getStorageLocation() const override;
	const std::string& getSerializedImagerProgram() const override { return _serializedProgram; }
	const std::vector<std::string> getAcquisitionNames() const override;
	const std::vector<std::string> getDetectorNames() const override;
	int getNumberOfStoredImages(const AcqTypeAndDetName& acqTypeAndDetName) const override;
	StagePosition getStagePosition(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	double getTimePoint(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	std::int64_t getNumberOfDetections() const override;
	std::int64_t getDetectionIndex(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	const std::vector<std::string>& getStagePositionNames() const override;
	std::string getStagePositionName(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	const std::vector<int>& getDetectionIndicesForChannel(const AcqTypeAndDetName& acqTypeAndDetName) const override;
	
	void addNewImage(const AcqTypeAndDetName& acqTypeAndDetName, AcquiredImage& newImage);
	void addNewSmartProgramDecision(const std::string& decision);
	void finishedAddingImages();

	const std::vector<std::string>& getSmartProgramDecisions() const override;

	AcquiredImage getImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) final;

private:
    void _queueAsyncImageWrite(const AcqTypeAndDetName& acqTypeAndDetName, const int indexWithinAcqTypeAndDet, AcquiredImage& newImage);
    AcquiredImage _derivedGetImage(const AcqTypeAndDetName& acqTypeAndDetName, int imageIndex);
    void _asyncWorker();

    std::string _generateOMEXML() const;

    std::string _filePath;
    std::shared_ptr<LNBTIFF::TIFFWriter> _outputStream;
	std::string _serializedProgram;
	size_t _nImagesSeen;
	bool _finishedAddingImages;

	/// @brief List of all acquisition/detector combinations in the order they were first seen during image addition.
	/// Used to map to the "TheC" indices in the OME-XML.
	std::vector<AcqTypeAndDetName> _acqTypesAndDetNamesInOrderOfImageAddition;

	/// @brief Dimensions (width, height) of each image, mapped by channel (Acquisition + Detector name)
	std::map<AcqTypeAndDetName, std::vector<std::pair<int, int>>> _imageDimensions;
	
	/// @brief Global linear indices (IFDs) of images in the TIFF file, sorted per channel
	std::map<AcqTypeAndDetName, std::vector<int>> _imageIndicesForChannel;

	/// @brief Sequence of detection indices executed for a specific channel
	std::map<AcqTypeAndDetName, std::vector<int>> _detectionIndicesForChannel;
	
	/// @brief Timestamps for each image relative to the start of the program, mapped by channel
	std::map<AcqTypeAndDetName, std::vector<double>> _imagesTimepoints;
	
	/// @brief Physical XYZ stage coordinates for each image in micrometers, mapped by channel
	std::map<AcqTypeAndDetName, std::vector<StagePosition>> _imagesStagePositions;

	/// @brief List of stage position labels/names indexed directly by detection index
	std::vector<std::string> _imagesStagePositionNames;

	/// @brief List of serialised JSON strings tracking dynamic or algorithmic decisions made during the program
	std::vector<std::string> _smartProgramDecisions;

	/// @brief Ordered list of channel identifiers mapping 1:1 with the global image stream sequence
	std::vector<AcqTypeAndDetName> _imagesAcqTypesAndDetNames;
	// -----------------------------------

	std::chrono::system_clock::time_point _initialTimePoint;
    moodycamel::BlockingReaderWriterQueue<int> _asyncConcurrentQueue;  // exists only to synchronize the worker thread
    std::deque<std::shared_ptr<AsyncData>> _asyncQueue;
    std::mutex _queueMutex;

    std::thread _asyncWorkerThread;
    volatile bool _workerHasError;
	std::string _workerErrorMessage;
};

#endif

