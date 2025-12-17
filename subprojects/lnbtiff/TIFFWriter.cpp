#include "TIFFWriter.h"

#include "TIFFDefs.h"

template <typename T>
bool Within(T a, T b, T c) {
    return ((a >= b) && (a <= c));
}

TIFFWriter::TIFFWriter(const std::string& filePath, const std::string& initialImageDescription) :
    _filePath(filePath)
    , _initialImageDescription(initialImageDescription)
    , _lastIFDPointerOffset(0)
    , _wroteAtLeastOneIFD(false)
    , _imageDescriptionTagOffset(0)
    , _ifdOffsetsTagOffset(0) {
    _outputFile.open(filePath.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (_outputFile.fail() != 0) {
        std::string error("Cannot create an output file at ");
        error += filePath;
        throw std::runtime_error(error);
    }

    // allow _outputFile to throw exceptions when writing data
    _outputFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    // write the necessary header
    std::vector<std::uint8_t> header = _getBigTiffHeader();
    _outputFile.write((char*)header.data(), header.size());
    _lastIFDPointerOffset = header.size() - sizeof(std::uint64_t);
}

TIFFWriter::~TIFFWriter() {
}

void TIFFWriter::writeImage(const std::vector<std::uint16_t>& image, const PixelType pixelType, const std::pair<int, int>& imageSize) {
    std::uint64_t ifdOffset = _outputFile.tellp();
    // ensure IFD starts on word boundary
    if ((ifdOffset % (std::uint64_t)2) != 0) {
        _outputFile.put(0);
        ifdOffset += 1;
    }

    std::vector<std::uint8_t> ifd;
    // if this is the first IFD then make space for the OME-XML string and data offsets tag.
    if (!_wroteAtLeastOneIFD) {
        ifd = _constructIFD(image, pixelType, imageSize, ifdOffset, true, _imageDescriptionTagOffset, _ifdOffsetsTagOffset);
    } else {
        std::uint64_t dummy;
        ifd = _constructIFD(image, pixelType, imageSize, ifdOffset, false, dummy, dummy);
    }

    _outputFile.write((char*)ifd.data(), ifd.size());
    _outputFile.write((char*)image.data(), image.size() * sizeof(std::uint16_t));

    // touch up previous IFD offset field so it points to this one.
    std::uint64_t newOffset = _outputFile.tellp();
    _outputFile.seekp(_lastIFDPointerOffset);
    _outputFile.write((char*)(&ifdOffset), sizeof(std::uint64_t));
    _outputFile.seekp(newOffset);

    _lastIFDPointerOffset = ifdOffset + ifd.size() - sizeof(std::uint64_t);
    _ifdOffsets.push_back(ifdOffset);
    _dataOffsets.push_back(ifdOffset + ifd.size());
    _dataPixelFormats.push_back(pixelType);
    _imageSizes.push_back(imageSize);

    if (!_wroteAtLeastOneIFD) {
        // write the initial image description
        _writeImageDescription(_initialImageDescription);
    }

    _wroteAtLeastOneIFD = true;
}

void TIFFWriter::doneAddingImages(const std::string& updatedImageDescription) {
    _writeImageDescriptionAndIFDOffsets(updatedImageDescription);
}

std::pair<std::vector<std::uint16_t>, PixelType> TIFFWriter::readImage(size_t imageIndex) {
    if (!Within(imageIndex, (size_t)0, _dataOffsets.size() - 1)) {
        throw std::runtime_error("TIFFWriter::readImage() but no image");
    }

    size_t nPixels = _imageSizes.at(imageIndex).first * _imageSizes.at(imageIndex).second;
    PixelType pixelType = _dataPixelFormats.at(imageIndex);
    size_t nBytes;
    if (pixelType == kInt16) {
        nBytes = nPixels * sizeof(std::uint16_t);
    } else if (pixelType == kFP64) {
        nBytes = nPixels * sizeof(double);
    } else {
        throw std::runtime_error("unknown pixel type in readImage");
    }

    std::pair<std::vector<std::uint16_t>, PixelType> image;
    image.first.resize(nBytes / sizeof(std::uint16_t));
    image.second = pixelType;
    std::ifstream inputStream(_filePath.c_str(), std::ios::binary | std::ios::in);
    inputStream.seekg(_dataOffsets.at(imageIndex));
    inputStream.read((char*)image.first.data(), nBytes);
    return image;
}

std::vector<std::uint8_t> TIFFWriter::_getBigTiffHeader() const {
    std::vector<std::uint8_t> header(16);
    std::uint8_t* bufPtr = header.data();
    _storeInBuffer<std::uint16_t>(bufPtr, 0x4949);          // byte order indication
    _storeInBuffer<std::uint16_t>(bufPtr, 0x002B);          // version number
    _storeInBuffer<std::uint16_t>(bufPtr, 0x0008);          // bytesize of offsets
    _storeInBuffer<std::uint16_t>(bufPtr, 0);               // always '0'
    _storeInBuffer<std::uint64_t>(bufPtr, 0);               // offset to first IFD
    return header;
}

std::vector<std::uint8_t> TIFFWriter::_constructIFD(const std::vector<std::uint16_t>& image, const PixelType pixelType, const std::pair<int, int>& imageSize,
    const std::uint64_t ifdOffsetInFile, bool isFirstIFD,
    std::uint64_t& stringTagOffsetInFile, std::uint64_t& ifdOffsetsTagOffsetInFile) const {
    int nTIFFTags = (isFirstIFD) ? 11 : 9;
    int nRows = imageSize.first;
    int nCols = imageSize.second;
    std::uint64_t ifdLength = (8 + nTIFFTags * 20 + 8);
    std::uint64_t dataOffsetInFile = ifdOffsetInFile + ifdLength;
    std::uint64_t dataLength = image.size() * sizeof(std::uint16_t);
    std::vector<std::uint8_t> ifd(ifdLength);
    std::uint8_t* bufferPtr = ifd.data();

    // write number of tiff tags
    _storeInBuffer<uint64_t>(bufferPtr, nTIFFTags);
    _writeTag(bufferPtr, TIFFTAG_IMAGEWIDTH, 1, nRows);
    _writeTag(bufferPtr, TIFFTAG_IMAGELENGTH, 1, nCols);
    if (pixelType == kInt16) {
        _writeTag(bufferPtr, TIFFTAG_BITSPERSAMPLE, 1, 16);
    } else if (pixelType == kFP64) {
        _writeTag(bufferPtr, TIFFTAG_BITSPERSAMPLE, 1, 64);
    }
    _writeTag(bufferPtr, TIFFTAG_COMPRESSION, 1, COMPRESSION_NONE);
    _writeTag(bufferPtr, TIFFTAG_PHOTOMETRIC, 1, PHOTOMETRIC_MINISBLACK);
    if (isFirstIFD) {
        stringTagOffsetInFile = ifdOffsetInFile + std::uint64_t(bufferPtr - ifd.data());
        _writeStringTag(bufferPtr, TIFFTAG_IMAGEDESCRIPTION);
        ifdOffsetsTagOffsetInFile = ifdOffsetInFile + std::uint64_t(bufferPtr - ifd.data());
        _writeTag(bufferPtr, LNB_TIFFTAG_IFDOFFSETS, 1, 0);
    }
    _writeTag(bufferPtr, TIFFTAG_STRIPOFFSETS, 1, dataOffsetInFile);
    _writeTag(bufferPtr, TIFFTAG_ROWSPERSTRIP, 1, nCols);
    _writeTag(bufferPtr, TIFFTAG_STRIPBYTECOUNTS, 1, dataLength);
    if (pixelType == kInt16) {
        _writeTag(bufferPtr, TIFFTAG_SAMPLEFORMAT, 1, SAMPLEFORMAT_UINT);
    } else if (pixelType == kFP64) {
        _writeTag(bufferPtr, TIFFTAG_SAMPLEFORMAT, 1, SAMPLEFORMAT_IEEEFP);
    }

    // offset to next IFD (zero for now)
    _storeInBuffer<uint64_t>(bufferPtr, 0);
    return ifd;
}

void TIFFWriter::_writeTag(std::uint8_t*& bufferPtr, int tagID, std::uint64_t count, std::uint64_t value) const {
    _storeInBuffer<uint16_t>(bufferPtr, tagID);
    _storeInBuffer<uint16_t>(bufferPtr, TIFF_LONG8);
    _storeInBuffer<uint64_t>(bufferPtr, count);
    _storeInBuffer<uint64_t>(bufferPtr, value);
}

void TIFFWriter::_writeStringTag(std::uint8_t*& bufferPtr, int tagID) const {
    _storeInBuffer<uint16_t>(bufferPtr, tagID);
    _storeInBuffer<uint16_t>(bufferPtr, TIFF_ASCII);
    _storeInBuffer<uint64_t>(bufferPtr, 0);
    _storeInBuffer<uint64_t>(bufferPtr, 0);
}

void TIFFWriter::_writeImageDescriptionAndIFDOffsets(const std::string& imageDescription) {
    if (!_wroteAtLeastOneIFD) {
        throw std::runtime_error("TIFFWriter::writeImageDescriptionAndIFDOffsets() but have no IFDs");
    }
    _writeImageDescription(imageDescription);
    _writeIFDOffsets(_ifdOffsets);
}

void TIFFWriter::_writeImageDescription(const std::string & imageDescription) {
    _outputFile.seekp(0, std::ios::end);
    std::uint64_t dataOffset = _outputFile.tellp();
    _outputFile.write(imageDescription.c_str(), imageDescription.size() + 1);

    std::uint64_t tiffCount = imageDescription.size() + 1;
    _outputFile.seekp(_imageDescriptionTagOffset + 4);
    _outputFile.write(reinterpret_cast<char*>(&tiffCount), sizeof(tiffCount));
    _outputFile.write(reinterpret_cast<char*>(&dataOffset), sizeof(dataOffset));
    _outputFile.seekp(0, std::ios::end);
}

void TIFFWriter::_writeIFDOffsets(const std::vector<std::uint64_t>& ifdOffsets) {
    _outputFile.seekp(0, std::ios::end);
    std::uint64_t dataOffset = _outputFile.tellp();
    std::uint64_t magic = 310117011014;
    _outputFile.write(reinterpret_cast<char*>(&magic), sizeof(magic));
    _outputFile.write(reinterpret_cast<const char*>(ifdOffsets.data()), ifdOffsets.size() * sizeof(std::uint64_t));

    std::uint64_t tiffCount = ifdOffsets.size() + 1;
    _outputFile.seekp(_ifdOffsetsTagOffset + 4);
    _outputFile.write(reinterpret_cast<char*>(&tiffCount), sizeof(tiffCount));
    _outputFile.write(reinterpret_cast<char*>(&dataOffset), sizeof(dataOffset));
    _outputFile.seekp(0, std::ios::end);
}
