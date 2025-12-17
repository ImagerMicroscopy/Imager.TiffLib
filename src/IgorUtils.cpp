#include "IgorUtils.h"

#include <tuple>

#include "json.hpp"

std::string HandleToString(Handle handle) {
    int err;
    std::string convertedString;

    size_t stringLength = GetHandleSize(handle);
	convertedString.resize(stringLength);
	err = GetCStringFromHandle(handle, (char*)(convertedString.data()), stringLength);
	if (err) throw err;
    return convertedString;
}

Handle StringToHandle(const std::string& str) {
	Handle h = WMNewHandle(0);
	if (h == nullptr) throw int(NOMEM);
	int err = PutCStringInHandle(str.data(), h);
	if (err) throw err;
	return h;
}

std::vector<std::string> WaveToStringVector(waveHndl w) {
    int err = 0;
    size_t nPoints = WavePoints(w);
    std::vector<std::string> result;

    Handle h = WMNewHandle(0);
    std::shared_ptr<Handle> deleter(&h, [] (Handle* h) {
        WMDisposeHandle(*h);
    });
    IndexInt indices[MAX_DIMENSIONS];
    indices[1] = 0;
    for (size_t i = 0; i < nPoints; i += 1) {
        indices[0] = i;
        err = MDGetTextWavePointValue(w, indices, h);
        if (err) {
            throw int(err);
        }
        result.push_back(HandleToString(h));
    }
    return result;
}

waveHndl StringVectorToWave(const std::vector<std::string>& sv) {
	waveHndl w;
	CountInt dimSizes[MAX_DIMENSIONS + 1] = { 0 };
	CountInt indices[MAX_DIMENSIONS] = { 0 };
	dimSizes[ROWS] = sv.size();
	int err = MDMakeWave(&w, "FREE", (DataFolderHandle)-1, dimSizes, TEXT_WAVE_TYPE, 1);
	if (err) throw err;
	for (size_t i = 0; i < sv.size(); i += 1) {
		Handle h = StringToHandle(sv.at(i));
		indices[0] = i;
		err = MDSetTextWavePointValue(w, indices, h);
		DisposeHandle(h);
	}
	return w;
}

std::string HandleToNativePath(Handle h) {
    char buf[MAX_PATH_LEN + 1];
    std::string asString = HandleToString(h);
    int err = GetNativePath(asString.c_str(), buf);
    return std::string(buf);
}

std::tuple<std::vector<std::uint16_t>, PixelType, std::pair<int, int>> ExtractImageDataFromWave(waveHndl w) {
	if ((WaveType(w) != (NT_I16 | NT_UNSIGNED)) && (WaveType(w) != NT_FP64)) {
		throw std::runtime_error("unsupported image number type");
	}

	int numDimensions;
	CountInt dimensionSizes[MAX_DIMENSIONS + 1];
	int err = MDGetWaveDimensions(w, &numDimensions, dimensionSizes);
	if (err) {
		throw int(err);
	}
	if ((numDimensions < 2) || (numDimensions > 3)) {
		throw std::runtime_error("need 2D or 3D with one plane wave");
	}
	if ((numDimensions == 3) && (dimensionSizes[2] > 1)) {
		throw std::runtime_error("3d waves must have exactly one plane");
	}

	PixelType pixelType;
	size_t nBytes;
	if (WaveType(w) == (NT_I16 | NT_UNSIGNED)) {
		pixelType = kInt16;
		nBytes = dimensionSizes[0] * dimensionSizes[1] * sizeof(std::uint16_t);
	} else if (WaveType(w) == NT_FP64) {
		pixelType = kFP64;
		nBytes = dimensionSizes[0] * dimensionSizes[1] * sizeof(double);
	} else {
		throw std::runtime_error("unknown pixel type");
	}

	std::pair<int, int> imageSize(dimensionSizes[0], dimensionSizes[1]);
	std:uint16_t* waveData = (std::uint16_t*)(WaveData(w));
	std::vector<std::uint16_t> imageData(waveData, waveData + nBytes / sizeof(std::uint16_t));

	return std::make_tuple(std::move(imageData), pixelType, imageSize);
}

AcquiredImage ImageFromWave(waveHndl w, double timePoint, waveHndl stagePositionWave, int64_t detectionIndex, const std::string& stagePositionName) {
    if (WaveType(stagePositionWave) != NT_FP64) {
        throw std::runtime_error("unsupported stage position type");
    }
    int numDimensions;
    CountInt dimensionSizes[MAX_DIMENSIONS+1];
    int err = MDGetWaveDimensions(stagePositionWave, &numDimensions, dimensionSizes);
    if (err) {
        throw int(err);
    }
    if ((numDimensions != 2) || (dimensionSizes[ROWS] != 1) || (dimensionSizes[COLUMNS] != 3)) {
        throw std::runtime_error("invalid stage dimensions");
    }
    double* stagePosPtr = (double*)(WaveData(stagePositionWave));
    StagePosition stagePosition(stagePosPtr[0], stagePosPtr[1], stagePosPtr[2]);
	PixelType pixelType;
	std::vector<std::uint16_t> imageData;
	std::pair<int, int> imageSize;

	std::tie(imageData, pixelType, imageSize) = ExtractImageDataFromWave(w);
	AcquiredImage acqImage(std::move(imageData), pixelType, imageSize, timePoint, stagePosition, detectionIndex, stagePositionName);
    return acqImage;
}

waveHndl FreeWaveFromImage(const AcquiredImage& image) {
    std::pair<int, int> imageSize = image.imageSize;
    waveHndl w;
    CountInt dimSizes[3] = { imageSize.first, imageSize.second, 0 };
	int waveType = 0;
	if (image.pixelType == kInt16) {
		waveType = NT_I16 | NT_UNSIGNED;
	} else if (image.pixelType == kFP64) {
		waveType = NT_FP64;
	} else {
		throw std::runtime_error("unknown pixel type when making wave");
	}
    int err = MDMakeWave(&w, "dummy", (DataFolderHandle)-1, dimSizes, waveType, 1);
    if (err) {
        throw int(err);
    }
    memcpy(WaveData(w), image.imageData.data(), image.imageData.size() * sizeof(std::uint16_t));
    return w;
}
