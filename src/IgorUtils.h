#ifndef UTILS_H
#define UTILS_H

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "XOPStandardHeaders.h"

#include "MiscUtils.h"



template <typename T>
std::vector<T> WaveToVector(waveHndl w) {
    int err = 0;
    size_t nPoints = WavePoints(w);

    // trick to test if the wave is numeric
    BCInt dataOffset;
    err = MDAccessNumericWaveData(w, kMDWaveAccessMode0, &dataOffset);
    if (err) {
        throw int(err);
    }
    std::vector<double> wData(nPoints);
    err = MDGetDPDataFromNumericWave(w, wData.data());
    std::vector<T> result(nPoints);
    for (size_t i = 0; i < nPoints; i += 1) {
        result[i] = wData[i];
    }
    return result;
}

std::string HandleToString(Handle handle);

std::vector<std::string> WaveToStringVector(waveHndl w);
waveHndl StringVectorToWave(const std::vector<std::string>& sv);

template <typename T> waveHndl NumericVectorToWave(const std::vector<T>& vec) {
	waveHndl w;
	CountInt dimSizes[MAX_DIMENSIONS + 1] = { 0 };
	CountInt indices[MAX_DIMENSIONS] = { 0 };
	dimSizes[ROWS] = vec.size();
	int err = MDMakeWave(&w, "FREE", (DataFolderHandle)-1, dimSizes, NT_FP64, 1);
	if (err) throw err;
	for (size_t i = 0; i < vec.size(); i += 1) {
		indices[ROWS] = i;
		double value[2] = { vec.at(i), 0.0 };
		err = MDSetNumericWavePointValue(w, indices, value);
		if (err) throw err;
	}
	return w;
}

std::string HandleToNativePath(Handle h);

std::tuple<std::vector<std::uint8_t>, LNBTIFF::PixelFormat, std::pair<int, int>> ExtractImageDataFromWave(waveHndl w);
AcquiredImage ImageFromWave(waveHndl w, double timePoint, waveHndl stagePositionWave, int64_t detectionIndex, const std::string& stagePositionName);
waveHndl FreeWaveFromImage(const AcquiredImage& image);

#endif
