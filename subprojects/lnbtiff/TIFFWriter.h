#ifndef TIFFWRITER_H
#define TIFFWRITER_H

#include <string>
#include <vector>
#include <fstream>

#include "TIFFDefs.h"

class TIFFWriter {
public:
    TIFFWriter(const std::string& filePath, const std::string& initialImageDescription);
    ~TIFFWriter();

    void writeImage(const std::vector<std::uint16_t>& image, const PixelType pixelType, const std::pair<int, int>& imageSize);
    void doneAddingImages(const std::string& updatedImageDescription);

    std::pair<std::vector<std::uint16_t>, PixelType> readImage(size_t imageIndex);

    const std::string& getFilePath() const { return _filePath; }

    void flush() { _outputFile.flush(); }

private:
    std::vector<std::uint8_t> _getBigTiffHeader() const;
    std::vector<std::uint8_t> _constructIFD(const std::vector<std::uint16_t>& image, const PixelType pixelType, const std::pair<int, int>& imageSize,
        const std::uint64_t ifdOffsetInFile, bool isFirstIFD,
        std::uint64_t& stringTagOffsetInFile, std::uint64_t& ifdOffsetsTagOffsetInFile) const;
    void _writeTag(std::uint8_t*& bufferPtr, int tagID, std::uint64_t count, std::uint64_t value) const;
    void _writeStringTag(std::uint8_t*& bufferPtr, int tagID) const;
    void _writeImageDescriptionAndIFDOffsets(const std::string& imageDescription);
    void _writeImageDescription(const std::string& imageDescription);
    void _writeIFDOffsets(const std::vector<std::uint64_t>& ifdOffsets);
    template <typename T> void _storeInBuffer(std::uint8_t*& bufferPtr, T value) const {
        void *valuePtr = reinterpret_cast<void*>(&value);
        memcpy(bufferPtr, valuePtr, sizeof(T));
        bufferPtr += sizeof(T);
    }

    std::fstream _outputFile;
    std::string _filePath;
    std::string _initialImageDescription;
    bool _wroteAtLeastOneIFD;
    std::vector<std::uint64_t> _ifdOffsets;
    std::vector<std::uint64_t> _dataOffsets;
    std::vector<PixelType> _dataPixelFormats;
    std::vector<std::pair<int, int>> _imageSizes;
    std::uint64_t _lastIFDPointerOffset;
    std::uint64_t _imageDescriptionTagOffset;
    std::uint64_t _ifdOffsetsTagOffset;
};

#endif
