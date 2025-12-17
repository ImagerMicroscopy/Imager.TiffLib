#include "StorageWrapperClass.h"

#include <algorithm>
#include <stdexcept>

size_t StorageWrapperClass::indexForAcquisitionName(const std::string& acqName) const {
	auto it = std::find(getAcquisitionNames().cbegin(), getAcquisitionNames().cend(), acqName);
	if (it == getAcquisitionNames().cend()) {
		throw std::runtime_error("unknown acquisition name");
	} else {
		return (it - getAcquisitionNames().cbegin());
	}
}
