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
	const std::vector<int>& getDetectionIndicesForChannel(const AcqTypeAndDetName& acqTypeAndDetName) const override;
	
	const std::vector<std::string>& getStagePositionNames() const override;
	std::string getStagePositionName(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const override;

	const std::vector<std::string>& getSmartProgramDecisions() const override;
	AcquiredImage getImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) final;

private:
    AcquiredImage _derivedReadImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex);

    std::string _loadOMEString();
    void _parseOMEXML();

    void _extractImagerMetaData(tinyxml2::XMLElement *omeElem);
	
	bool _tryParseNewOMEFormat(tinyxml2::XMLElement* imageElem, tinyxml2::XMLElement* channelElem, 
                                  std::int64_t& detectionIndex, std::string& stagePositionName, 
                                  std::string& acqName, std::string& detectorName);
    void _parseOldOMEFormat(tinyxml2::XMLElement* channelElem, 
                            std::int64_t& detectionIndex, std::string& stagePositionName, 
                            std::string& acqName, std::string& detectorName);

    std::string _filePath;
    LNBTIFF::TIFFFile _tiffFile;
    std::uint64_t _nImagesTotal = 0;
    std::vector<uint64_t> _ifdOffsets;
    tinyxml2::XMLDocument _omeDoc;

    /// @brief JSON object describing the measurement executed by the microscope
    std::string _imagerProgramDescription;
    /// @brief All unique Acquisition types present in the data
    std::set<std::string> _acquisitionNames;
    /// @brief All unique detector names present in the data
	std::set<std::string> _detectorNames;
	/// @brief List of serialised JSON strings tracking dynamic or algorithmic decisions made during the program
	std::vector<std::string> _smartProgramDecisions;
	
	/// @brief For each combination of acquisition type and detector name, we store
	// a vector linking the image index within that channel to the IFD index in the TIFF file.
	std::map<AcqTypeAndDetName, std::vector<int>> _imageIndicesForChannel;

	/// @brief The logical detection index for each image in a given channel.
	std::map<AcqTypeAndDetName, std::vector<int>> _detectionIndicesForChannel;

	/// @brief Timestamps for each image relative to the start of the program, mapped by channel.
	std::map<AcqTypeAndDetName, std::vector<double>> _imagesTimepoints;

	/// @brief Physical XYZ stage coordinates for each image in micrometers, mapped by channel.
	std::map<AcqTypeAndDetName, std::vector<StagePosition>> _imagesStagePositions;
	
	/// @brief List of stage position labels/names indexed directly by detection index
	std::vector<std::string> _imagesStagePositionNamesAtDetectionIndices;

    /// @brief Maximum observed detection index, used to size global loops and queries
	int64_t _maxdetectionIdx = 0;
	
	// -----------------------------------
};

#endif
