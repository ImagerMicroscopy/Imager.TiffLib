#ifndef BASICTIFFWRITER_H
#define BASICTIFFWRITER_H

#include <string>

#include "MiscUtils.h"
#include "TIFFFile.h"
#include "TIFFWriter.h"

class BasicTIFFWriter {
public:
	BasicTIFFWriter(const std::string& filePath);
	~BasicTIFFWriter();

	void addNewImage(const std::vector<std::uint16_t>& data, const LNBTIFF::PixelFormat pixelType, const std::pair<int, int>& size);

private:
	void finalize();

	LNBTIFF::TIFFWriter _tiffWriter;
};

class BasicTIFFReader {
public:
	class ReadImage {
	public:
		std::vector<uint16_t> data;
		std::pair<int, int> size;
	};

	BasicTIFFReader(const std::string& filePath);
	~BasicTIFFReader();

	std::uint64_t nImages();
	ReadImage readImage(size_t imageIdx);

private:
	LNBTIFF::TIFFFile _tiffFile;
};

#endif
