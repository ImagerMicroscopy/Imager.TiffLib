#include "BasicTIFFWriter.h"

BasicTIFFWriter::BasicTIFFWriter(const std::string & filePath) :
    _tiffWriter(filePath, "IMS Basic Image")
{}

BasicTIFFWriter::~BasicTIFFWriter() {
	finalize();
}

void BasicTIFFWriter::addNewImage(const std::vector<std::uint16_t>& image, const PixelType pixelType, const std::pair<int, int>& size) {
	_tiffWriter.writeImage(image, pixelType, size);
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
	TagType pixelType;
	_tiffFile.getImageDimensions(imageIdx, imageLength, imageWidth, pixelType, nBytesInImage);
	if (pixelType != TIFF_SHORT) {
		throw std::runtime_error("Not a 16-bit or double image");
	}
	size_t nPixels = nBytesInImage / sizeof(std::uint16_t);

	ReadImage readImage;
	readImage.size = {imageWidth, imageLength};
	readImage.data.resize(nPixels);
	_tiffFile.loadImageData(imageIdx, reinterpret_cast<std::uint8_t*>(readImage.data.data()), readImage.data.size() * sizeof(std::uint16_t));

	return readImage;
}
