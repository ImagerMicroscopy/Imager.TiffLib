#include "BasicTIFFWriter.h"

BasicTIFFWriter::BasicTIFFWriter(const std::string & filePath) :
    _tiffWriter(filePath, "IMS Basic Image")
{}

BasicTIFFWriter::~BasicTIFFWriter() {
	finalize();
}

void BasicTIFFWriter::addNewImage(const std::vector<std::uint16_t>& image, const LNBTIFF::PixelFormat pixelType,
								  const std::pair<int, int>& size) {
	if (pixelType != LNBTIFF::Mono16) {
		throw std::runtime_error("only Mono16 supported in BasicTIFFWriter for now");
	}

	std::vector<std::uint8_t> imageData(image.size() * sizeof(std::uint16_t));
	memcpy(imageData.data(), image.data(), imageData.size());

	_tiffWriter.writeImage(imageData, pixelType, size);
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
	if (pixelFormat != LNBTIFF::Mono16) {
		throw std::runtime_error("Not a 16-bit image");
	}
	size_t nPixels = nBytesInImage / sizeof(std::uint16_t);

	ReadImage readImage;
	readImage.size = {imageWidth, imageLength};
	readImage.data.resize(nPixels);
	_tiffFile.loadImageData(imageIdx, reinterpret_cast<std::uint8_t*>(readImage.data.data()), readImage.data.size() * sizeof(std::uint16_t));

	return readImage;
}
