#ifndef MISCTYPES_H
#define MISCTYPES_H

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "TIFFDefs.h"

typedef std::tuple<double, double, double> StagePosition;
typedef std::pair<std::string, std::string> AcqTypeAndDetName;

class AcquiredImage {
public:
	AcquiredImage(std::vector<std::uint16_t> imageData, PixelType pixelType, std::pair<int, int> imageSize, double timePoint,
				  StagePosition stagePosition, std::int64_t detectionIndex, const std::string& stagePositionName) :
		imageData(std::move(imageData)),
		pixelType(pixelType),
		imageSize(imageSize),
		timePoint(timePoint),
		stagePosition(stagePosition),
		detectionIndex(detectionIndex),
		stagePositionName(stagePositionName)
	{}

	std::vector<std::uint16_t> imageData;
	PixelType pixelType;
	std::pair<int, int> imageSize;
	double timePoint;
	StagePosition stagePosition;
	std::int64_t detectionIndex;
	std::string stagePositionName;
};

template <typename T>
bool Within(T a, T b, T c) {
	return ((a >= b) && (a <= c));
}

std::vector<std::string> ChannelNamesFromImagerProgram(const std::string& program);

#endif

