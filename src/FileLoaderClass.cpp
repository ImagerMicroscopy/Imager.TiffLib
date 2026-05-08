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
        return _imageIndicesForChannel.at(acqTypeAndDetName).size();
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
    int linearIdx = _imageIndicesForChannel.at(acqTypeAndDetName).at(imageIndex);
    std::uint64_t imageLength, imageWidth, nBytesInImage;
    int64_t detectionIndex = getDetectionIdxForImageIdxForChannel(acqTypeAndDetName, imageIndex);
    LNBTIFF::PixelFormat pixelFormat;
    _tiffFile.getImageDimensions(linearIdx, imageLength, imageWidth, pixelFormat, nBytesInImage);
    if ((pixelFormat != LNBTIFF::Mono16) && (pixelFormat != LNBTIFF::Float64)) {
        throw std::runtime_error("Not a 16-bit or double image");
    }

    LNBTIFF::PixelFormat outputPixelFormat = pixelFormat;

    double timePoint = _imagesTimepoints.at(acqTypeAndDetName).at(imageIndex);
    StagePosition stagePosition = _imagesStagePositions.at(acqTypeAndDetName).at(imageIndex);
    std::string stagePositionName = _imagesStagePositionNamesAtDetectionIndices.at(detectionIndex);

    std::pair<int, int> imageSize(imageWidth, imageLength);
    std::vector<std::uint8_t> imageData(nBytesInImage);

    _tiffFile.loadImageData(linearIdx, reinterpret_cast<std::uint8_t*>(imageData.data()), imageData.size());

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
        bool haveDetectionIdxAndStagePositionName = false;

        // 1. Try to read properties from the simplified OME-XML encoding
        haveDetectionIdxAndStagePositionName = _tryParseNewOMEFormat(imageElem, channelElem, detectionIndex, stagePositionName, acqName, detectorName);

        if (!haveDetectionIdxAndStagePositionName) {
            // Old format: read from Channel attributes
            _parseOldOMEFormat(channelElem, detectionIndex, stagePositionName, acqName, detectorName);
            if (stagePositionName != kNoStagePositionNameInformationStr) {
                haveDetectionIdxAndStagePositionName = true;
            }
        }

        _acquisitionNames.insert(acqName);
        _detectorNames.insert(detectorName);
        AcqTypeAndDetName acqTypeAndDetName(acqName, detectorName);
        _imageIndicesForChannel[acqTypeAndDetName].push_back(imageIdx);
        int indexWithinAcqDet = _imageIndicesForChannel[acqTypeAndDetName].size() - 1;

        _imagesTimepoints[acqTypeAndDetName].push_back(t);
        _imagesStagePositions[acqTypeAndDetName].push_back(StagePosition(x, y, z));
        if (haveDetectionIdxAndStagePositionName) {
            _imagesDetectionIndices[acqName].push_back(detectionIndex);
            _detectionIndicesForChannel[acqTypeAndDetName].push_back(detectionIndex);
            _indexWithinAcqDetToDetectionIndexMap[acqTypeAndDetName][detectionIndex] = indexWithinAcqDet;
            _detectionIndexToIndexWithinAcqDetMap[acqTypeAndDetName][indexWithinAcqDet] = detectionIndex;
            if (detectionIndex>_maxdetectionIdx){ 
                _maxdetectionIdx = detectionIndex;
            }
            if (_imagesStagePositionNamesAtDetectionIndices.size() <= detectionIndex) {
                _imagesStagePositionNamesAtDetectionIndices.resize(detectionIndex + 1);
            }
            _imagesStagePositionNamesAtDetectionIndices[detectionIndex] = stagePositionName;
        }
        _nImagesTotal += 1;
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
            
            // Populate the cross-reference maps for old format files
            for (size_t i = 0; i < expectedImageIndices.size(); ++i) {
                std::int64_t detIdx = expectedImageIndices[i];
                int indexWithinAcqDet = static_cast<int>(i);
                _detectionIndicesForChannel[acqAndDetNames].push_back(detIdx);
                _indexWithinAcqDetToDetectionIndexMap[acqAndDetNames][detIdx] = indexWithinAcqDet;
                _detectionIndexToIndexWithinAcqDetMap[acqAndDetNames][indexWithinAcqDet] = detIdx;
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

bool FileLoaderClass::_tryParseNewOMEFormat(tinyxml2::XMLElement* imageElem, tinyxml2::XMLElement* channelElem, 
                                               std::int64_t& detectionIndex, std::string& stagePositionName, 
                                               std::string& acqName, std::string& detectorName) {
    tinyxml2::XMLElement* descElem = imageElem->FirstChildElement("Description");
    if (descElem != nullptr && descElem->GetText() != nullptr) {
        try {
            nlohmann::json descJson = nlohmann::json::parse(descElem->GetText());
            if (descJson.contains("DetectionIndex")) {
                detectionIndex = descJson["DetectionIndex"].get<int64_t>();
                
                if (descJson.contains("StagePositionName")) {
                    stagePositionName = descJson["StagePositionName"].get<std::string>();
                } else if (imageElem->Attribute("Name") != nullptr) {
                    stagePositionName = imageElem->Attribute("Name");
                } else {
                    stagePositionName = "";
                }
                
                const char* strPtr = nullptr;
                if (channelElem->QueryStringAttribute("Name", &strPtr) == tinyxml2::XML_SUCCESS) {
                    std::string combinedName(strPtr);
                    size_t pipePos = combinedName.find('|');
                    if (pipePos != std::string::npos) {
                        acqName = combinedName.substr(0, pipePos);
                        detectorName = combinedName.substr(pipePos + 1);
                        return true;
                    }
                }
            }
        } catch (...) {
            // Failed to parse JSON, meaning it's not the latest format
        }
    }
    return false;
}

void FileLoaderClass::_parseOldOMEFormat(tinyxml2::XMLElement* channelElem, 
                                         std::int64_t& detectionIndex, std::string& stagePositionName, 
                                         std::string& acqName, std::string& detectorName) {
    const char* strPtr = nullptr;
    if (channelElem->QueryStringAttribute("DetectorName", &strPtr) != tinyxml2::XML_SUCCESS) {
        detectorName = "UnknownCam";
    } else {
        detectorName = strPtr;
    }

    if (channelElem->Attribute("AcquisitionName") != nullptr) {
        if (channelElem->QueryStringAttribute("AcquisitionName", &strPtr) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No acquisition name found");
        acqName = strPtr;
        if (channelElem->QueryAttribute("DetectionIndex", &detectionIndex) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No detection index found");
        if (channelElem->QueryStringAttribute("StagePositionName", &strPtr) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No stage position name found");
        stagePositionName = strPtr;
    } else {
        if (channelElem->QueryStringAttribute("Name", &strPtr) != tinyxml2::XML_SUCCESS) throw std::runtime_error("No acquisition name found");
        acqName = strPtr;
        stagePositionName = kNoStagePositionNameInformationStr;
    }
}

