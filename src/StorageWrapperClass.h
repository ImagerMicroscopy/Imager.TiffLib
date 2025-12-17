#ifndef STORAGEWRAPPERCLASS_H
#define STORAGEWRAPPERCLASS_H

#include <string>
#include <vector>

#include "MiscUtils.h"

typedef std::pair<std::string, std::string> AcqTypeAndDetName;

class StorageWrapperClass {
public:
    StorageWrapperClass() { ; }
    virtual ~StorageWrapperClass() { ; }

    virtual const std::string& getSerializedImagerProgram() const = 0;
    virtual std::string getStorageLocation() const = 0;
    virtual const std::vector<std::string> getAcquisitionNames() const = 0;
	virtual const std::vector<std::string> getDetectorNames() const = 0;
	virtual int getNumberOfStoredImages(const AcqTypeAndDetName& acqTypeAndDetName) const = 0;
	virtual StagePosition getStagePosition(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const = 0;
	virtual double getTimePoint(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const = 0;
	virtual std::int64_t getNumberOfDetections() const = 0;
	virtual std::int64_t getDetectionIndex(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const = 0;
	virtual const std::vector<std::string>& getStagePositionNames() const = 0;
	virtual std::string getStagePositionName(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) const = 0;

	virtual const std::vector<std::string>& getSmartProgramDecisions() const = 0;

	virtual AcquiredImage getImage(const AcqTypeAndDetName& acqTypeAndDetName, const int imageIndex) = 0;

protected:
	size_t indexForAcquisitionName(const std::string& acqName) const;
};

#endif
