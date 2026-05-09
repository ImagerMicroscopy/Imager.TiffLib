#include "BasicTIFFWriter.h"

BasicTIFFWriter::BasicTIFFWriter(const std::string & filePath) :
    _tiffWriter(filePath, "IMS Basic Image")
{}

BasicTIFFWriter::~BasicTIFFWriter() {
	finalize();
}

void BasicTIFFWriter::addNewImage(const std::vector<std::uint8_t>& image, const LNBTIFF::PixelFormat pixelFormat,
								  const std::pair<int, int>& size) {
	_tiffWriter.writeImage(image, pixelFormat, size);
}

void BasicTIFFWriter::finalize() {
	_tiffWriter.doneAddingImages("IMS Basic Image");
}

BasicTIFFReader::BasicTIFFReader(const std::string& filePath) :
	_tiffFile(filePath)
{

}

BasicTIFFReader::~BasicTIFFReader() {

}

std::uint64_t BasicTIFFReader::nImages() {
    return _tiffFile.nImages();
}

BasicTIFFReader::ReadImage BasicTIFFReader::readImage(size_t imageIdx) {
    std::uint64_t imageLength, imageWidth, nBytesInImage;
	LNBTIFF::PixelFormat pixelFormat;
	_tiffFile.getImageDimensions(imageIdx, imageLength, imageWidth, pixelFormat, nBytesInImage);

	ReadImage readImage;
	readImage.size = {imageWidth, imageLength};
	readImage.data.resize(nBytesInImage);
	_tiffFile.loadImageData(imageIdx, readImage.data.data(), readImage.data.size());

	return readImage;
}
