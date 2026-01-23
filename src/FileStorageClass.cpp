#include "FileStorageClass.h"

#include <algorithm>
#include <format>

#include "date.h"
#include "json.hpp"

#include "tinyxml2.h"

FileStorageClass::FileStorageClass(const std::string& filePath, const std::string& serializedProgram) :
	_filePath(filePath)
  , _serializedProgram(serializedProgram)
  , _asyncWorkerThread([&] () {_asyncWorker(); })
  , _nImagesSeen(0)
  , _finishedAddingImages(false)
  , _workerHasError(false)
{
    _initialTimePoint = std::chrono::system_clock::now();
    std::string initialImageDescription = _generateOMEXML();

    _outputStream = std::shared_ptr<TIFFWriter>(new TIFFWriter(filePath, initialImageDescription));
}

FileStorageClass::~FileStorageClass() {
    int dummy = 0; 
    _asyncConcurrentQueue.enqueue(dummy);   // causes worker thread to terminate if still running
	if (_asyncWorkerThread.joinable()) {
		_asyncWorkerThread.join();
	}
}

std::string FileStorageClass::getStorageLocation() const {
    return _filePath;
}

const std::vector<std::string> FileStorageClass::getAcquisitionNames() const {
	std::vector<std::string> acqNames;
	for (const AcqTypeAndDetName& n : _imagesAcqTypesAndDetNames) {
		acqNames.push_back(n.first);
	}
	return acqNames;
}

const std::vector<std::string> FileStorageClass::getDetectorNames() const {
	std::vector<std::string> detNames;
	for (const AcqTypeAndDetName& n : _imagesAcqTypesAndDetNames) {
		detNames.push_back(n.second);
	}
	return detNames;
}

void FileStorageClass::addNewImage(const AcqTypeAndDetName& acqTypeAndDetName, AcquiredImage& newImage) {
	if (_finishedAddingImages) {
		throw std::runtime_error("adding image but already finished");
	}
	_imageDimensions[acqTypeAndDetName].push_back(newImage.imageSize);
	_imageIndicesForChannel[acqTypeAndDetName].push_back(_nImagesSeen);
	_imagesTimepoints[acqTypeAndDetName].push_back(newImage.timePoint);
	_imagesStagePositions[acqTypeAndDetName].push_back(newImage.stagePosition);
	_imagesDetectionIndices[acqTypeAndDetName.first].push_back(newImage.detectionIndex);
	if (_imagesStagePositionNames.size() < newImage.detectionIndex + 1) {
		_imagesStagePositionNames.emplace_back(newImage.stagePositionName);
	}
	_imagesAcqTypesAndDetNames.push_back(acqTypeAndDetName);

	int indexWithinAcqAndDet = _imageIndicesForChannel.at(acqTypeAndDetName).size() - 1;
	_queueAsyncImageWrite(acqTypeAndDetName, indexWithinAcqAndDet, newImage);
	_nImagesSeen += 1;
}

void FileStorageClass::addNewSmartProgramDecision(const std::string &decision) {
	_smartProgramDecisions.emplace_back(decision);
}

void FileStorageClass::finishedAddingImages() {
	_finishedAddingImages = true;
	int dummy = 0;
	_asyncConcurrentQueue.enqueue(dummy);  // wakes up worker thread, and will cause it to terminate since no data is present.

	if (_workerHasError) {
        std::string errorMessage("error writing image (disk full?): ");
        errorMessage += _workerErrorMessage;
        throw std::runtime_error(errorMessage);
	}
}

const std::vector<std::string>& FileStorageClass::getSmartProgramDecisions() const {
	return _smartProgramDecisions;
}

AcquiredImage FileStorageClass::getImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) {
	return _derivedGetImage(acqTypeAndDetName, imageIndex);
}

int FileStorageClass::getNumberOfStoredImages(const AcqTypeAndDetName& acqTypeAndDetName) const {
	return _imageIndicesForChannel.at(acqTypeAndDetName).size();
}

StagePosition FileStorageClass::getStagePosition(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const {
	return _imagesStagePositions.at(acqTypeAndDetName).at(imageIndex);
}

double FileStorageClass::getTimePoint(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const {
	return _imagesTimepoints.at(acqTypeAndDetName).at(imageIndex);
}

std::int64_t FileStorageClass::getNumberOfDetections() const {
	return _imagesStagePositionNames.size();
}

std::int64_t FileStorageClass::getDetectionIndex(const AcqTypeAndDetName &acqTypeAndDetName, const int imageIndex) const {
	return _imagesDetectionIndices.at(acqTypeAndDetName.first).at(imageIndex);
}

const std::vector<std::string> & FileStorageClass::getStagePositionNames() const {
	return _imagesStagePositionNames;
}

std::string FileStorageClass::getStagePositionName(const AcqTypeAndDetName &acqTypeAndDetName, const int imageIndex) const {
	int64_t detectionIndex = getDetectionIndex(acqTypeAndDetName, imageIndex);
	return _imagesStagePositionNames.at(detectionIndex);
}

void FileStorageClass::_queueAsyncImageWrite(const AcqTypeAndDetName& acqTypeAndDetName, const int indexWithinAcqTypeAndDet, AcquiredImage& newImage) {
    if (_workerHasError) {
        std::string errorMessage("error writing image (disk full?): ");
        errorMessage += _workerErrorMessage;
        throw std::runtime_error(errorMessage);
    }

    std::shared_ptr<AsyncData> queuedData(new AsyncData);
	queuedData->acqTypeAndDetName = acqTypeAndDetName;
	queuedData->imageDimensions = newImage.imageSize;
	queuedData->indexWithinAcqAndDet = indexWithinAcqTypeAndDet;
    std::swap(queuedData->imageData, newImage.imageData);
	queuedData->pixelType = newImage.pixelType;

    {
        std::lock_guard<std::mutex> lock(_queueMutex);
		int dummy = 0;
        _asyncQueue.push_back(queuedData);
        _asyncConcurrentQueue.enqueue(dummy);  // make sure worker thread wakes up
    }
}

AcquiredImage FileStorageClass::_derivedGetImage(const AcqTypeAndDetName& acqTypeAndDetName, int imageIndex) {
	if (_workerHasError) {
        std::string errorMessage("error writing image (disk full?): ");
        errorMessage += _workerErrorMessage;
        throw std::runtime_error(errorMessage);
	}

	// return blank image if not yet seen
	if ((_imageIndicesForChannel.count(acqTypeAndDetName) == 0) || (imageIndex >= _imageIndicesForChannel.at(acqTypeAndDetName).size())) {
		std::vector<std::uint16_t> imageData; imageData.resize(4, 0);
		AcquiredImage image(std::move(imageData), kInt16, {2, 2}, -1.0, StagePosition(), -1, "");
		return image;
	}

    std::pair<int, int> imageSize = _imageDimensions.at(acqTypeAndDetName).at(imageIndex);
    size_t nPixels = imageSize.first * imageSize.second;
	std::int64_t detectionIndex = _imagesDetectionIndices.at(acqTypeAndDetName.first).at(imageIndex);

	AcquiredImage image({}, kInt16, imageSize, _imagesTimepoints.at(acqTypeAndDetName).at(imageIndex),
			            _imagesStagePositions.at(acqTypeAndDetName).at(imageIndex), detectionIndex,
			            _imagesStagePositionNames.at(detectionIndex));
	image.imageData.resize(nPixels);

    // read the data from the queue or just from disk
    // check if the data is in the queue
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        auto it = std::find_if(_asyncQueue.begin(), _asyncQueue.end(), [&] (const std::shared_ptr<AsyncData>& data) -> bool {
            return ((data->acqTypeAndDetName == acqTypeAndDetName) && (data->indexWithinAcqAndDet == imageIndex));
        });
        if (it != _asyncQueue.end()) {
            // data is in the queue - just fetch it from there
            memcpy(image.imageData.data(), (*it)->imageData.data(), nPixels * sizeof(std::uint16_t));
			image.pixelType = (*it)->pixelType;
			return image;
        }
    }
    // if we are still here then we need to fetch the data from disk
    std::pair<std::vector<uint16_t>, PixelType> loadedImage = _outputStream->readImage(_imageIndicesForChannel.at(acqTypeAndDetName).at(imageIndex));
	std::swap(loadedImage.first, image.imageData);
	image.pixelType = loadedImage.second;
	return image;
}

void FileStorageClass::_asyncWorker() {
	size_t nImagesWritten = 0;
	_workerHasError = false;
    try {
        for ( ; ; ) {
            int sync;
            _asyncConcurrentQueue.wait_dequeue(sync);  // serves only to let the thread sleep until data is available
													   // waking up without new data in _asyncQueue means that all images have been received.
            std::shared_ptr<AsyncData> newData;
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                if (_asyncQueue.empty()) {
                    _outputStream->doneAddingImages(_generateOMEXML());
                    _outputStream->flush();
                    return;
                }
                newData = _asyncQueue.front(); // important: we get the front element but do not remove it from the queue
            }
            _outputStream->writeImage(newData->imageData, newData->pixelType, newData->imageDimensions);
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                _asyncQueue.pop_front();  // now that everything is written to file we remove the element from the queue
            }
			nImagesWritten += 1;
        }
    }
	catch (const std::runtime_error& e) {
        _workerHasError = true;
        _workerErrorMessage = e.what();
	}
	catch (const std::logic_error& e) {
        _workerHasError = true;
        _workerErrorMessage = e.what();
	}
    catch (...) {
        _workerHasError = true;
    }
}

std::string FileStorageClass::_generateOMEXML() const {
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLElement* omeElem = xmlDoc.NewElement("OME");
    xmlDoc.InsertEndChild(omeElem);
    omeElem->SetAttribute("xmlns", "http://www.openmicroscopy.org/Schemas/OME/2016-06");
    omeElem->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    omeElem->SetAttribute("xsi:schemaLocation", "http://www.openmicroscopy.org/Schemas/OME/2016-06 http://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd");

	// Add the StructuredAnnotations element
	tinyxml2::XMLElement* structuredAnnotationsElem = xmlDoc.NewElement("StructuredAnnotations");
	omeElem->InsertEndChild(structuredAnnotationsElem);

	// add a map annotation for Imager metadata
	tinyxml2::XMLElement* imagerProgramAnnotationElem = xmlDoc.NewElement("MapAnnotation");
	imagerProgramAnnotationElem->SetAttribute("ID", "Annotation:ImagerMetadata");
	structuredAnnotationsElem->InsertEndChild(imagerProgramAnnotationElem);
	
	// create a single Value element with both M elements
	tinyxml2::XMLElement* valueElem = xmlDoc.NewElement("Value");
	imagerProgramAnnotationElem->InsertEndChild(valueElem);
	
	// insert the program
	tinyxml2::XMLElement* mapPairElem = xmlDoc.NewElement("M");
	valueElem->InsertEndChild(mapPairElem);
	mapPairElem->SetAttribute("K", "ImagerProgram");
	mapPairElem->SetText(getSerializedImagerProgram().c_str());
	
	// insert any decisions
	nlohmann::json smartProgramDecisionsArray = nlohmann::json::array();
	for (const auto& decisionStr : _smartProgramDecisions) {
		smartProgramDecisionsArray.push_back(nlohmann::json::parse(decisionStr));
	}
	nlohmann::json smartProgramObject = {
		{"version", 1},
		{"decisions", smartProgramDecisionsArray}};
	std::string encodedDecision = smartProgramObject.dump();
	mapPairElem = xmlDoc.NewElement("M");
	valueElem->InsertEndChild(mapPairElem);
	mapPairElem->SetAttribute("K", "SmartProgramDecisions");
	mapPairElem->SetText(encodedDecision.c_str());

	// Create per-image MapAnnotations for custom metadata
	std::map<AcqTypeAndDetName, int> tempNextIndices;
	for (const auto& id : _imageDimensions) {
		tempNextIndices[id.first] = 0;
	}
	
	for (size_t i = 0; i < _imagesAcqTypesAndDetNames.size(); i++) {
		AcqTypeAndDetName acqTypeAndDetName = _imagesAcqTypesAndDetNames[i];
		int indexWithinAcqAndDet = tempNextIndices.at(acqTypeAndDetName);
		int64_t detectionIndex = _imagesDetectionIndices.at(acqTypeAndDetName.first).at(i);
		
		// Create MapAnnotation for this image
		tinyxml2::XMLElement* imageAnnotationElem = xmlDoc.NewElement("MapAnnotation");
		std::string imageAnnotationID = std::format("Annotation:Image{}Metadata", i);
		imageAnnotationElem->SetAttribute("ID", imageAnnotationID.c_str());
		structuredAnnotationsElem->InsertEndChild(imageAnnotationElem);
		
		tinyxml2::XMLElement* imageValueElem = xmlDoc.NewElement("Value");
		imageAnnotationElem->InsertEndChild(imageValueElem);
		
		// Add acquisition name
		tinyxml2::XMLElement* imageMapPairElem = xmlDoc.NewElement("M");
		imageValueElem->InsertEndChild(imageMapPairElem);
		imageMapPairElem->SetAttribute("K", "AcquisitionName");
		imageMapPairElem->SetText(acqTypeAndDetName.first.c_str());
		
		// Add detector name
		imageMapPairElem = xmlDoc.NewElement("M");
		imageValueElem->InsertEndChild(imageMapPairElem);
		imageMapPairElem->SetAttribute("K", "DetectorName");
		imageMapPairElem->SetText(acqTypeAndDetName.second.c_str());
		
		// Add detection index
		imageMapPairElem = xmlDoc.NewElement("M");
		imageValueElem->InsertEndChild(imageMapPairElem);
		imageMapPairElem->SetAttribute("K", "DetectionIndex");
		imageMapPairElem->SetText(std::to_string(detectionIndex).c_str());
		
		// Add stage position name
		imageMapPairElem = xmlDoc.NewElement("M");
		imageValueElem->InsertEndChild(imageMapPairElem);
		imageMapPairElem->SetAttribute("K", "StagePositionName");
		imageMapPairElem->SetText(_imagesStagePositionNames.at(detectionIndex).c_str());
		
		tempNextIndices[acqTypeAndDetName]++;
	}

	// data structures that maintains sequence numbers for each image kind
	std::map<AcqTypeAndDetName, int> nextIndices;
	for (const auto& id : _imageDimensions) {
		nextIndices[id.first] = 0;
	}

	size_t perPlaneMetaDataMapCounter = 0;
    for (size_t i = 0; i < _imagesAcqTypesAndDetNames.size(); i += 1) {
		AcqTypeAndDetName acqTypeAndDetName = _imagesAcqTypesAndDetNames[i];
		int indexWithinAcqAndDet = nextIndices.at(acqTypeAndDetName);
    	std::string imageID = std::format("Image:{}", i);
    	std::string annotationRefToImageID = std::format("{}:0:0:0", imageID);
        tinyxml2::XMLElement* imageElem = xmlDoc.NewElement("Image");
        omeElem->InsertEndChild(imageElem);
        imageElem->SetAttribute("ID", imageID.c_str());

    	std::string pixelsID = std::format("Pixels:{}", i);
        tinyxml2::XMLElement* pixelsElem = xmlDoc.NewElement("Pixels");
        imageElem->InsertEndChild(pixelsElem);
        pixelsElem->SetAttribute("ID", pixelsID.c_str());
        pixelsElem->SetAttribute("DimensionOrder", "XYZCT");
        pixelsElem->SetAttribute("SizeX", _imageDimensions.at(acqTypeAndDetName).at(indexWithinAcqAndDet).first);
        pixelsElem->SetAttribute("SizeY", _imageDimensions.at(acqTypeAndDetName).at(indexWithinAcqAndDet).second);
        pixelsElem->SetAttribute("SizeZ", 1);
        pixelsElem->SetAttribute("SizeT", 1);
        pixelsElem->SetAttribute("SizeC", 1);

        tinyxml2::XMLElement* planeElem = xmlDoc.NewElement("Plane");
        pixelsElem->InsertEndChild(planeElem);
        planeElem->SetAttribute("DeltaT", _imagesTimepoints.at(acqTypeAndDetName).at(indexWithinAcqAndDet));
        planeElem->SetAttribute("DeltaTUnit", "s");
        double x, y, z;
        std::tie(x, y, z) = _imagesStagePositions.at(acqTypeAndDetName).at(indexWithinAcqAndDet);
    	int64_t detectionIndex = _imagesDetectionIndices.at(acqTypeAndDetName.first).at(i);
        planeElem->SetAttribute("PositionX", x);
        planeElem->SetAttribute("PositionY", y);
        planeElem->SetAttribute("PositionZ", z);
        planeElem->SetAttribute("PositionXUnit", "µm");
        planeElem->SetAttribute("PositionYUnit", "µm");
        planeElem->SetAttribute("PositionZUnit", "µm");
        planeElem->SetAttribute("TheC", 0);
        planeElem->SetAttribute("TheT", 0);
        planeElem->SetAttribute("TheZ", 0);

    	std::string channelID = std::format("Channel:{}:0", i);
        tinyxml2::XMLElement* channelElem = xmlDoc.NewElement("Channel");
        pixelsElem->InsertEndChild(channelElem);
		channelElem->SetAttribute("ID", channelID.c_str());
    	std::string combinedName = std::format("{}/{}", acqTypeAndDetName.first, acqTypeAndDetName.second);
		channelElem->SetAttribute("Name", combinedName.c_str());
		// Custom attributes moved to MapAnnotation for OME-XML compliance

        tinyxml2::XMLElement* tiffDataElem = xmlDoc.NewElement("TiffData");
        pixelsElem->InsertEndChild(tiffDataElem);
        tiffDataElem->SetAttribute("IFD", (int)i);

        std::chrono::system_clock::time_point thisImageTimePoint = _initialTimePoint + std::chrono::microseconds((std::int64_t)(_imagesTimepoints.at(acqTypeAndDetName).at(indexWithinAcqAndDet) * 1.0e6));
        tinyxml2::XMLElement* acqDateElem = xmlDoc.NewElement("AcquisitionDate");
        imageElem->InsertEndChild(acqDateElem);
        std::string formattedDate = date::format("%FT%TZ", thisImageTimePoint);
        acqDateElem->SetText(formattedDate.c_str());

    	// Add the AnnotationRef element to link to the imager program annotation
    	tinyxml2::XMLElement* annotationRefElem = xmlDoc.NewElement("AnnotationRef");
    	annotationRefElem->SetAttribute("ID", "Annotation:ImagerMetadata");
    	imageElem->InsertEndChild(annotationRefElem);
    	
    	// Add the AnnotationRef element to link to this image's metadata
    	annotationRefElem = xmlDoc.NewElement("AnnotationRef");
    	std::string imageAnnotationID = std::format("Annotation:Image{}Metadata", i);
    	annotationRefElem->SetAttribute("ID", imageAnnotationID.c_str());
    	imageElem->InsertEndChild(annotationRefElem);

		nextIndices.at(acqTypeAndDetName) += 1;
    }

    tinyxml2::XMLPrinter printer;
    xmlDoc.Print(&printer);
    std::string xml(printer.CStr());
    return xml;
}
