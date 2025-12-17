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
    std::vector<std::uint16_t> imageData;
	PixelType pixelType;
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
    std::shared_ptr<TIFFWriter> _outputStream;
	std::string _serializedProgram;
	size_t _nImagesSeen;
	bool _finishedAddingImages;

	std::map<AcqTypeAndDetName, std::vector<std::pair<int, int>>> _imageDimensions;
	std::map<AcqTypeAndDetName, std::vector<int>>_imageIndicesForChannel;
	std::map<AcqTypeAndDetName, std::vector<double>> _imagesTimepoints;
	std::map<AcqTypeAndDetName, std::vector<StagePosition>> _imagesStagePositions;
	std::map<AcquisitionName, std::vector<std::int64_t>> _imagesDetectionIndices;
	std::vector<std::string> _imagesStagePositionNames;

	std::vector<std::string> _smartProgramDecisions;

	std::vector<AcqTypeAndDetName> _imagesAcqTypesAndDetNames;

	std::chrono::system_clock::time_point _initialTimePoint;
    moodycamel::BlockingReaderWriterQueue<int> _asyncConcurrentQueue;  // exists only to synchronize the worker thread
    std::deque<std::shared_ptr<AsyncData>> _asyncQueue;
    std::mutex _queueMutex;

    std::thread _asyncWorkerThread;
    volatile bool _workerHasError;
	std::string _workerErrorMessage;
};

#endif

