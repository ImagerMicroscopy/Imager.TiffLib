#ifndef FILELOADERCLASS_H
#define FILELOADERCLASS_H

#include "StorageWrapperClass.h"

#include <map>
#include <set>

#include "tinyxml2.h"
#include "TIFFFile.h"

#include "MiscUtils.h"

using AcquisitionName = std::string;

inline const char* kNoStagePositionNameInformationStr = "__no information on stage position name__";

class FileLoaderClass : public StorageWrapperClass {
public:
    FileLoaderClass(const std::string& filePath);
    virtual ~FileLoaderClass();

    std::string getStorageLocation() const override { return _filePath; }
    const std::string& getSerializedImagerProgram() const override { return _imagerProgramDescription; }
	const std::vector<std::string> getAcquisitionNames() const override;
	const std::vector<std::string> getDetectorNames() const override;
	int getNumberOfStoredImages(const AcqTypeAndDetName& acqTypeAndDetName) const override;
	StagePosition getStagePosition(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	double getTimePoint(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	std::int64_t getNumberOfDetections() const override;
	std::int64_t getDetectionIndex(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	const std::vector<int>& getDetectionIndecesForChannel(const AcqTypeAndDetName& acqTypeAndDetName) const override;
	std::int64_t getImageIdxForDetectionIdxForChannel(const AcqTypeAndDetName& acqTypeAndDetName, const int detectionIndex) const override;
	std::int64_t getDetectionIdxForImageIdxForChannel(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;
	const std::vector<std::string>& getStagePositionNames() const override;
	std::string getStagePositionName(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;

	const std::vector<std::string>& getSmartProgramDecisions() const override;
	AcquiredImage getImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) final;

private:
    AcquiredImage _derivedReadImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex);

    std::string _loadOMEString();
    void _parseOMEXML();

    void _extractImagerMetaData(tinyxml2::XMLElement *omeElem);
	
	tinyxml2::XMLElement* _findMapAnnotationByID(tinyxml2::XMLElement* structuredAnnotationsElem, const char* refID,
        										 tinyxml2::XMLElement*& lastMapAnnotationElem);

    std::string _filePath;
    TIFFFile _tiffFile;
    std::uint64_t _nImagesTotal;
    std::vector<uint64_t> _ifdOffsets;
    tinyxml2::XMLDocument _omeDoc;

    std::string _imagerProgramDescription;
    std::set<std::string> _acquisitionNames;
	std::set<std::string> _detectorNames;
	std::map<AcqTypeAndDetName, std::vector<int>>_imageIndicesForChannel;
	std::map<AcqTypeAndDetName, std::vector<int>>_detectionIndicesForChannel;
	std::map<AcqTypeAndDetName, std::vector<double>> _imagesTimepoints;
	std::map<AcqTypeAndDetName, std::vector<StagePosition>> _imagesStagePositions;
	std::map<AcqTypeAndDetName, std::map<int, int>> _imageIdxDetIdxMapForChannel;
	std::map<AcqTypeAndDetName, std::map<int, int>> _detIdxImageIdxMapForChannel;

	std::map<AcquisitionName, std::vector<std::int64_t>> _imagesDetectionIndices;
	std::vector<std::string> _imagesStagePositionNamesAtDetectionIndices;


	int64_t _maxdetectionIdx = 0;
	std::vector<std::string> _smartProgramDecisions;
};

#endif
