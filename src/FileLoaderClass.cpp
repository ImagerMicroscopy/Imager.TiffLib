#include "FileLoaderClass.h"

#include <format>
#include "ImagerProgramInterpreter.h"

FileLoaderClass::FileLoaderClass(const std::string& filePath) :
    _filePath(filePath),
	_tiffFile(filePath)
{
    std::string ome = _loadOMEString();
    if (_omeDoc.Parse(ome.c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Couldn't parse image description as XML");
    }
	_parseOMEXML();
}

FileLoaderClass::~FileLoaderClass() {
}

const std::vector<std::string> FileLoaderClass::getAcquisitionNames() const {
	std::vector<std::string> acqNames;
	for (const auto& n : _acquisitionNames) {
		acqNames.push_back(n);
	}
	return acqNames;
}

const std::vector<std::string> FileLoaderClass::getDetectorNames() const {
	std::vector<std::string> detNames;
	for (const auto& n : _detectorNames) {
		detNames.push_back(n);
	}
	return detNames;
}

AcquiredImage FileLoaderClass::getImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) {
	return _derivedReadImage(acqTypeAndDetName, imageIndex);
}

int FileLoaderClass::getNumberOfStoredImages(const AcqTypeAndDetName& acqTypeAndDetName) const {
    if (_imageIndicesForChannel.count(acqTypeAndDetName) > 0) {
        int numimages = 0;
            for (const auto& [key, value] : _imageIndicesForChannel) {
                if (key.first==acqTypeAndDetName.first)
                {
                    numimages += value.size();
                }
            }

        return numimages;
	} else {
		return 0;
	}
}

StagePosition FileLoaderClass::getStagePosition(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const {
	return _imagesStagePositions.at(acqTypeAndDetName).at(imageIndex);
}
double FileLoaderClass::getTimePoint(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const {
	return _imagesTimepoints.at(acqTypeAndDetName).at(imageIndex);
}

std::int64_t FileLoaderClass::getNumberOfDetections() const {
	return _maxdetectionIdx+1;
}

std::int64_t FileLoaderClass::getDetectionIndex(const AcqTypeAndDetName &acqTypeAndDetName, const int imageIndex) const {
	return _detectionIndexToIndexWithinAcqDetMap.at(acqTypeAndDetName).at(imageIndex);
}

std::int64_t FileLoaderClass::getImageIdxForDetectionIdxForChannel(const AcqTypeAndDetName& acqTypeAndDetName, const int detectionIndex) const {
    return _indexWithinAcqDetToDetectionIndexMap.at(acqTypeAndDetName).at(detectionIndex);
}
std::int64_t FileLoaderClass::getDetectionIdxForImageIdxForChannel(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const {
    return _detectionIndexToIndexWithinAcqDetMap.at(acqTypeAndDetName).at(imageIndex);
}



const std::vector<int>& FileLoaderClass::getDetectionIndicesForChannel(const AcqTypeAndDetName& acqTypeAndDetName) const {
    return _detectionIndicesForChannel.at(acqTypeAndDetName);
}

const std::vector<std::string> & FileLoaderClass::getStagePositionNames() const {
	return _imagesStagePositionNamesAtDetectionIndices;
}

std::string FileLoaderClass::getStagePositionName(const AcqTypeAndDetName &acqTypeAndDetName, const int imageIndex) const {
	int64_t detectionIndex = getDetectionIndex(acqTypeAndDetName, imageIndex);
	return _imagesStagePositionNamesAtDetectionIndices.at(detectionIndex);
}

const std::vector<std::string> & FileLoaderClass::getSmartProgramDecisions() const {
	return _smartProgramDecisions;
}

AcquiredImage FileLoaderClass::_derivedReadImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) {
//	int linearIdx = _imageIndicesForChannel.at(acqTypeAndDetName).at(imageIndex);
//    int linearIdx = imageIndex;
	std::uint64_t imageLength, imageWidth, nBytesInImage;
    int64_t detectionIndex = getDetectionIdxForImageIdxForChannel(acqTypeAndDetName, imageIndex);
	LNBTIFF::PixelFormat pixelFormat;
	_tiffFile.getImageDimensions(imageIndex, imageLength, imageWidth, pixelFormat, nBytesInImage);
	if ((pixelFormat != LNBTIFF::Mono16) && (pixelFormat != LNBTIFF::Float64)) {
		throw std::runtime_error("Not a 16-bit or double image");
	}

	LNBTIFF::PixelFormat outputPixelFormat = pixelFormat;

	double timePoint = _imagesTimepoints.at(acqTypeAndDetName).at(detectionIndex);
	StagePosition stagePosition = _imagesStagePositions.at(acqTypeAndDetName).at(detectionIndex);
	std::string stagePositionName = _imagesStagePositionNamesAtDetectionIndices.at(detectionIndex);

	std::pair<int, int> imageSize(imageWidth, imageLength);
	std::vector<std::uint8_t> imageData(nBytesInImage);

	_tiffFile.loadImageData(imageIndex, reinterpret_cast<std::uint8_t*>(imageData.data()), imageData.size());

	AcquiredImage image(std::move(imageData), outputPixelFormat, imageSize, timePoint, stagePosition, detectionIndex, stagePositionName);
    return image;
}

std::string FileLoaderClass::_loadOMEString() {
	return _tiffFile.getTagStringValueOrError(0, TIFFTAG_IMAGEDESCRIPTION);
}

void FileLoaderClass::_parseOMEXML() {
    // load the imager metadata
	tinyxml2::XMLElement* omeElem = _omeDoc.FirstChildElement("OME");
    
	_extractImagerMetaData(omeElem);
	if (_imagerProgramDescription.empty()) {
		throw std::runtime_error("No Imager program description found");
	}

	tinyxml2::XMLElement* structuredAnnotationsElem = omeElem->FirstChildElement("StructuredAnnotations");
    // Optimization: remember last found MapAnnotation for fast consecutive lookup
    tinyxml2::XMLElement* lastMapAnnotationElem = nullptr;

    tinyxml2::XMLElement* imageElem = omeElem->FirstChildElement("Image");
    if (imageElem == nullptr) throw std::runtime_error("No Image child element found");
    for (int imageIdx = 0; imageElem != nullptr; imageElem = imageElem->NextSiblingElement("Image"), imageIdx += 1) {
        tinyxml2::XMLElement* pixelsElem = imageElem->FirstChildElement("Pixels");
        if (pixelsElem == nullptr) throw std::runtime_error("No Pixels child element found");
        tinyxml2::XMLElement* planeElem = pixelsElem->FirstChildElement("Plane");
        if (planeElem == nullptr) throw std::runtime_error("No Plane child element found");
        char* theStr = nullptr;
        double x, y, z, t;
        if (planeElem->QueryDoubleAttribute("DeltaT", &t) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No timestamp found");
        if (planeElem->QueryDoubleAttribute("PositionX", &x) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No stage position found");
        if (planeElem->QueryDoubleAttribute("PositionY", &y) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No stage position found");
        if (planeElem->QueryDoubleAttribute("PositionZ", &z) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No stage position found");

        tinyxml2::XMLElement* channelElem = pixelsElem->FirstChildElement("Channel");
    	if (channelElem == nullptr)
    		throw std::runtime_error("No channel element found");
    	const char* elemID = nullptr;
    	std::string detectorName, acqName, stagePositionName;
    	std::int64_t detectionIndex = -1;
		const char* strPtr = nullptr;
    	bool haveDetectionIdxAndStagePositionName = false;

    	// First check if this image has per-image MapAnnotation (new compliant format)
    	// Look for AnnotationRef elements in the current Image
    	tinyxml2::XMLElement* imageAnnotationElem = nullptr;
    	if (structuredAnnotationsElem != nullptr) {
    		// Find AnnotationRef elements in the current Image
    		for (tinyxml2::XMLElement* annotationRefElem = imageElem->FirstChildElement("AnnotationRef"); 
    			 annotationRefElem != nullptr; 
    			 annotationRefElem = annotationRefElem->NextSiblingElement("AnnotationRef")) {
    			
    			const char* refID = annotationRefElem->Attribute("ID");
    			if (refID != nullptr && !(strcmp(refID, "Annotation:ImagerMetadata")==0)) {
    				imageAnnotationElem = _findMapAnnotationByID(structuredAnnotationsElem, refID, lastMapAnnotationElem);
    				if (imageAnnotationElem) break;
    			}
    		}
    	}
    	
    	if (imageAnnotationElem != nullptr) {
    		// New format: read from per-image MapAnnotation - be strict about required values
    		tinyxml2::XMLElement* valueElem = imageAnnotationElem->FirstChildElement("Value");
    		if (valueElem == nullptr) throw std::runtime_error("No Value element in per-image MapAnnotation");
    		
            bool foundAcqName = false, foundDetectorName = false, foundDetectionIndex = false, foundStagePositionName = false;
    		
    		for (tinyxml2::XMLElement* mapPairElem = valueElem->FirstChildElement("M"); mapPairElem != nullptr; mapPairElem = mapPairElem->NextSiblingElement("M")) {
    			const char* valueID = mapPairElem->Attribute("K");

                if (valueID != nullptr && strcmp(valueID, "AcquisitionName") == 0) {
    				const char* text = mapPairElem->GetText();
    				if (text == nullptr) throw std::runtime_error("Missing AcquisitionName text in per-image MapAnnotation");
    				acqName = text;
    				foundAcqName = true;
    			} else if (valueID != nullptr && strcmp(valueID, "DetectorName") == 0) {
    				const char* text = mapPairElem->GetText();
    				if (text == nullptr) throw std::runtime_error("Missing DetectorName text in per-image MapAnnotation");
    				detectorName = text;
    				foundDetectorName = true;
    			} else if (valueID != nullptr && strcmp(valueID, "DetectionIndex") == 0) {
    				const char* text = mapPairElem->GetText();
    				if (text == nullptr) throw std::runtime_error("Missing DetectionIndex text in per-image MapAnnotation");
    				detectionIndex = std::stoll(text);
    				foundDetectionIndex = true;
    			} else if (valueID != nullptr && strcmp(valueID, "StagePositionName") == 0) {
    				const char* text = mapPairElem->GetText();
    				if (text == nullptr) {
    					stagePositionName = "";
    				} else {
    					stagePositionName = text;
    				}
    				foundStagePositionName = true;
    			}
    		}

                // Ensure all required values were found in the new format
            if (!foundAcqName) throw std::runtime_error("Missing AcquisitionName in per-image MapAnnotation");
            if (!foundDetectorName) throw std::runtime_error("Missing DetectorName in per-image MapAnnotation");
            if (!foundDetectionIndex) throw std::runtime_error("Missing DetectionIndex in per-image MapAnnotation");
            if (!foundStagePositionName) throw std::runtime_error("Missing StagePositionName in per-image MapAnnotation");

            haveDetectionIdxAndStagePositionName = true;

 
    	} else {
    		// Old format: read from Channel attributes
    		if (channelElem->QueryStringAttribute("DetectorName", &strPtr)  != tinyxml2::XML_SUCCESS) {
    			detectorName = "UnknownCam";	// old files didn't have this attribute
    		} else {
    			detectorName = strPtr;
    		}
    		if (channelElem->Attribute("AcquisitionName") != nullptr) {
    			// new format that also stores detection indices and stage position names
    			if (channelElem->QueryStringAttribute("AcquisitionName", &strPtr) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No acquisition name found");
    			acqName = strPtr;
    			if (channelElem->QueryAttribute("DetectionIndex", &detectionIndex) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No detection index found");
    			if (channelElem->QueryStringAttribute("StagePositionName", &strPtr) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No stage position name found");
    			stagePositionName = strPtr;
    			haveDetectionIdxAndStagePositionName = true;
    		} else {
    			// old format stored the acq name in the "Name" attribute
    			if (channelElem->QueryStringAttribute("Name", &strPtr) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No acquisition name found");
    			acqName = strPtr;
    			stagePositionName = kNoStagePositionNameInformationStr;
    		}

    	}

        _acquisitionNames.insert(acqName);
        _detectorNames.insert(detectorName);
        AcqTypeAndDetName acqTypeAndDetName(acqName, detectorName);
        _imageIndicesForChannel[acqTypeAndDetName].push_back(imageIdx);
        _imagesTimepoints[acqTypeAndDetName].push_back(t);
        _imagesStagePositions[acqTypeAndDetName].push_back(StagePosition(x, y, z));
        if (haveDetectionIdxAndStagePositionName) {
            _imagesDetectionIndices[acqName].push_back(detectionIndex);
            _detectionIndicesForChannel[acqTypeAndDetName].push_back(detectionIndex);
            _indexWithinAcqDetToDetectionIndexMap[acqTypeAndDetName][detectionIndex] = imageIdx;
            _detectionIndexToIndexWithinAcqDetMap[acqTypeAndDetName][imageIdx] = detectionIndex;
            if (detectionIndex>_maxdetectionIdx){ 
                _maxdetectionIdx = detectionIndex;
            }
            if (_imagesStagePositionNamesAtDetectionIndices.size() <= detectionIndex) {
            	_imagesStagePositionNamesAtDetectionIndices.resize(detectionIndex + 1);
            }
            _imagesStagePositionNamesAtDetectionIndices[detectionIndex] = stagePositionName;
        }
        
    }

	if (_imagesDetectionIndices.empty()) {
		// means this was an older file that did not include the detection indices and stage position names directly
		// get these by interpreting the program
		nlohmann::json programDescriptor = nlohmann::json::parse(_imagerProgramDescription);
		nlohmann::json program = programDescriptor["program"]["program"];
		std::shared_ptr<ProgramElement> parsedProgram = ParseImagerProgramElement(program);
		CalculateDetectionIndicesAndStagePositionNames(parsedProgram, _imagesDetectionIndices, _imagesStagePositionNamesAtDetectionIndices);

		// if the user aborted the measurement then there are fewer images in the file than would be expected based on the program
		std::int64_t highestDetectionIndex = -1;
		for (const auto& [acqAndDetNames, imageIndices] : _imageIndicesForChannel) {
			const std::string& acqName = acqAndDetNames.first;
			size_t nImagesInChannel = imageIndices.size();
			std::vector<std::int64_t>& expectedImageIndices = _imagesDetectionIndices[acqName];
			if (expectedImageIndices.size() > nImagesInChannel) {
				expectedImageIndices.resize(nImagesInChannel);
			}
			if (!expectedImageIndices.empty()) {
				highestDetectionIndex = std::max(highestDetectionIndex, expectedImageIndices.at(expectedImageIndices.size() - 1));
			}
		}
		_imagesStagePositionNamesAtDetectionIndices.resize(highestDetectionIndex + 1);
		_maxdetectionIdx = highestDetectionIndex;
	}
}

void FileLoaderClass::_extractImagerMetaData(tinyxml2::XMLElement *omeElem) {
	std::string imagerProgramDescription;

	// check if the file uses the old (incorrect) encoding of the Imager program, or whether it's using the
	// new, hopefully correct one
    tinyxml2::XMLElement* imagerAnnotationElem = omeElem->FirstChildElement("MapAnnotation");
	if (imagerAnnotationElem != nullptr) {
		// old encoding
		const char* elemID = nullptr;
		if ((elemID = imagerAnnotationElem->Attribute("ID")) == nullptr) throw std::runtime_error("Couldn't find MapAnnotation ID");
		if (strcmp(elemID, "Annotation:ImagerMetadata") != 0) throw std::runtime_error("Couldn't find Imager metadata");

		tinyxml2::XMLElement* imagerMetadataElem = imagerAnnotationElem->FirstChildElement("Value");
		if (imagerMetadataElem == nullptr) throw std::runtime_error("No Imager metadata child element");
		const char* valueID = nullptr;
		if ((valueID = imagerMetadataElem->Attribute("K")) == nullptr) throw std::runtime_error("No imager metadata key");
		if (strcmp(valueID, "ImagerProgram") == 0) {
			imagerProgramDescription = imagerMetadataElem->GetText();
		} else {
			throw std::runtime_error("No Imager program description found");
		}
	} else {
		// new encoding
		tinyxml2::XMLElement* structuredAnnotationsElem = omeElem->FirstChildElement("StructuredAnnotations");
        if (structuredAnnotationsElem == nullptr)
            throw std::runtime_error("No StructuredAnnotations element found");
		// find the ImagerMetaData element
        tinyxml2::XMLElement* imagerAnnotationElem = nullptr;
        for (tinyxml2::XMLElement* child = structuredAnnotationsElem->FirstChildElement("MapAnnotation"); child != nullptr; child = child->NextSiblingElement("MapAnnotation")) {
            const char* elemID = child->Attribute("ID");
            if (elemID && strcmp(elemID, "Annotation:ImagerMetadata") == 0) {
                imagerAnnotationElem = child;
                break;
            }
        }
        if (imagerAnnotationElem == nullptr)
            throw std::runtime_error("No Imager metadata annotation found");
		tinyxml2::XMLElement* valueElem = imagerAnnotationElem->FirstChildElement("Value");
        if (valueElem == nullptr)
            throw std::runtime_error("No Value element found");
        // Search through all M elements to find both ImagerProgram and SmartProgramDecisions
        bool foundImagerProgram = false;
        bool foundSmartDecision = false;
        for (tinyxml2::XMLElement* mapPairElem = valueElem->FirstChildElement("M"); mapPairElem != nullptr; mapPairElem = mapPairElem->NextSiblingElement("M")) {
            const char* valueID = mapPairElem->Attribute("K");
            if (valueID != nullptr && strcmp(valueID, "ImagerProgram") == 0) {
                imagerProgramDescription = mapPairElem->GetText();
                foundImagerProgram = true;
            } else if (valueID != nullptr && strcmp(valueID, "SmartProgramDecisions") == 0) {
                // Parse the smart program decisions JSON
                const char* decisionsJson = mapPairElem->GetText();
                if (decisionsJson != nullptr) {
                    nlohmann::json smartProgramObject = nlohmann::json::parse(decisionsJson);
                    if (smartProgramObject.contains("decisions") && smartProgramObject["decisions"].is_array()) {
                        for (const auto& decision : smartProgramObject["decisions"]) {
                            _smartProgramDecisions.push_back(decision.dump());
                        }
                        foundSmartDecision = true;
                    }
                }
            }
        }
        
        if (!foundImagerProgram) {
            throw std::runtime_error("No Imager program description found");
        }
        // older formats didn't have SmartProgramDecisions, so don't throw if not found
        // if (!foundSmartDecision) {
        //     throw std::runtime_error("No Smart program decisions found");
        // }
	}

	_imagerProgramDescription = imagerProgramDescription;
}

// Find the per-image MapAnnotation in the file, with the id given by refID.
// lastMapAnnotationElem is normally a reference to the last found MapAnnotation element (or nullptr),
// because the MapAnnotation for the next image is likely to directly follow it.
// Returns: Pointer to the found MapAnnotation element, or nullptr if not found.
tinyxml2::XMLElement* FileLoaderClass::_findMapAnnotationByID(
    tinyxml2::XMLElement* structuredAnnotationsElem,
    const char* refID,
    tinyxml2::XMLElement*& lastMapAnnotationElem)
{
    if (!structuredAnnotationsElem || !refID) return nullptr;
    tinyxml2::XMLElement* foundMapAnnotation = nullptr;
    tinyxml2::XMLElement* startElem = lastMapAnnotationElem ? lastMapAnnotationElem->NextSiblingElement("MapAnnotation") : structuredAnnotationsElem->FirstChildElement("MapAnnotation");
    tinyxml2::XMLElement* mapAnnotationElem = startElem;
    // Search to end
    for (; mapAnnotationElem != nullptr; mapAnnotationElem = mapAnnotationElem->NextSiblingElement("MapAnnotation")) {
        const char* annotationID = mapAnnotationElem->Attribute("ID");
        if (annotationID && strcmp(annotationID, refID) == 0) {
            foundMapAnnotation = mapAnnotationElem;
            break;
        }
    }
    // If not found, wrap around and search from beginning up to lastMapAnnotationElem
    if (!foundMapAnnotation && lastMapAnnotationElem) {
        for (mapAnnotationElem = structuredAnnotationsElem->FirstChildElement("MapAnnotation");
             mapAnnotationElem != lastMapAnnotationElem;
             mapAnnotationElem = mapAnnotationElem->NextSiblingElement("MapAnnotation")) {
            const char* annotationID = mapAnnotationElem->Attribute("ID");
            if (annotationID && strcmp(annotationID, refID) == 0) {
                foundMapAnnotation = mapAnnotationElem;
                break;
            }
        }
    }
    if (foundMapAnnotation) {
        lastMapAnnotationElem = foundMapAnnotation;
    }
    return foundMapAnnotation;
}
