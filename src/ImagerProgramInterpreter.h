#ifndef MEASUREMENTIMAGESTORAGE_IMAGERPROGRAMINTERPRETER_H
#define MEASUREMENTIMAGESTORAGE_IMAGERPROGRAMINTERPRETER_H

#include <map>
#include <memory>

#include "json.hpp"

using AcquisitionName = std::string;

class ProgramElement;

std::shared_ptr<ProgramElement> ParseImagerProgramElement(const nlohmann::json &json);

void CalculateDetectionIndicesAndStagePositionNames(std::shared_ptr<ProgramElement> encodedProgram,
                                                    std::map<AcquisitionName, std::vector<std::int64_t>>& detectionIndices,
                                                    std::vector<std::string>& stagePositionNames);

#endif //MEASUREMENTIMAGESTORAGE_IMAGERPROGRAMINTERPRETER_H