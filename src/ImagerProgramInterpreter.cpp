#include "ImagerProgramInterpreter.h"

#include <ranges>

enum class ElementType {
    LoopType,
    StageLoopType,
    TerminalType,
    DetectionType
};

class ProgramElement {
public:
    ProgramElement() {}
    virtual ~ProgramElement() {}

    virtual ElementType getType() const = 0;
    virtual size_t getLoopCount() const {throw std::logic_error("ProgramElement::getLoopCount");}
    virtual const std::vector<std::shared_ptr<ProgramElement>>& getChildren() { throw std::logic_error("ProgramElement::getChildren"); }
};

class ProgramElement_Loop : public ProgramElement {
public:
    ProgramElement_Loop(size_t nTimes, const std::vector<std::shared_ptr<ProgramElement>>& children) : _nTimes(nTimes), _children(children) {}
    ~ProgramElement_Loop() {}

    ElementType getType() const override {return ElementType::LoopType;}

    size_t getLoopCount() const override { return _nTimes; }
    const std::vector<std::shared_ptr<ProgramElement>>& getChildren() override { return _children; }

private:
    size_t _nTimes;
    std::vector<std::shared_ptr<ProgramElement>> _children;
};

class ProgramElement_StageLoop : public ProgramElement {
public:
    ProgramElement_StageLoop(const std::vector<std::string>& stagePositionNames, const std::vector<std::shared_ptr<ProgramElement>>& children) :
        _stagePositionNames(stagePositionNames),
        _children(children)
    {}
    ~ProgramElement_StageLoop() {}

    ElementType getType() const override {return ElementType::StageLoopType;}

    size_t getLoopCount() const override { return _stagePositionNames.size(); }
    const std::vector<std::string>& getStagePositionNames() const { return _stagePositionNames; }
    const std::vector<std::shared_ptr<ProgramElement>>& getChildren() override { return _children; }

private:
    std::vector<std::string> _stagePositionNames;
    std::vector<std::shared_ptr<ProgramElement>> _children;
};

class ProgramElement_Terminal : public ProgramElement {
public:
    ProgramElement_Terminal() {}
    ~ProgramElement_Terminal() {}

    ElementType getType() const override {return ElementType::TerminalType;}
};

class ProgramElement_Detection : public ProgramElement {
public:
    ProgramElement_Detection(const std::vector<std::string>& acquiredAcqNames) : _acquiredAcqNames(acquiredAcqNames) {}
    ~ProgramElement_Detection() {}

    ElementType getType() const override {return ElementType::DetectionType;}

    const std::vector<std::string>& getAcquiredAcqNames() const { return _acquiredAcqNames; }
private:
    std::vector<std::string> _acquiredAcqNames;
};


std::shared_ptr<ProgramElement> ParseImagerProgramElement(const nlohmann::json &json) {
    std::string elementType = json["elementtype"].get<std::string>();

    std::vector<std::string> terminalTypes({"irradiation", "wait", "executerobotprogram"});
    std::vector<std::string> loopTypes({"dotimes", "timelapse", "relativestageloop", "stageloop"});

    if (std::ranges::find(terminalTypes, elementType) != terminalTypes.end()) {
        return std::make_shared<ProgramElement_Terminal>();
    }

    if (std::ranges::find(loopTypes, elementType) != loopTypes.end()) {
        std::vector<std::shared_ptr<ProgramElement>> children;
        nlohmann::json elements = json["elements"];
        for (const auto& child : elements) {
            children.push_back(ParseImagerProgramElement(child));
        }

        if (("dotimes" == elementType) || ("timelapse" == elementType)) {
            size_t nTimes = json["ntotal"].get<size_t>();
            return std::make_shared<ProgramElement_Loop>(nTimes, children);
        }
        if ("relativestageloop" == elementType) {
            nlohmann::json params = json["params"];
            nlohmann::json additionalplanesx = params["additionalplanesx"].get<size_t>();
            nlohmann::json additionalplanesy = params["additionalplanesy"].get<size_t>();
            nlohmann::json additionalplanesz = params["additionalplanesz"].get<size_t>();
            size_t nNegX = additionalplanesx[0].get<size_t>();
            size_t nPosX = additionalplanesx[1].get<size_t>();
            size_t nNegY = additionalplanesy[0].get<size_t>();
            size_t nPosY = additionalplanesy[1].get<size_t>();
            size_t nNegZ = additionalplanesz[0].get<size_t>();
            size_t nPosZ = additionalplanesz[1].get<size_t>();
            size_t loopCount = (nNegX + nPosX + 1) * (nNegY + nPosY + 1) * (nNegZ + nPosZ + 1);
            return std::make_shared<ProgramElement_Loop>(loopCount, children);
        }
        if ("stageloop" == elementType) {
            std::vector<std::string> posNames;
            nlohmann::json positions = json["positions"];
            for (const auto& pos : positions) {
                posNames.push_back(pos["name"].get<std::string>());
            }
            return std::make_shared<ProgramElement_StageLoop>(posNames, children);
        }
    }

    if ("detection" == elementType) {
        nlohmann::json detectionNames = json["detectionnames"];
        std::vector<std::string> acqNames;
        for (const auto& detection : detectionNames) {
            acqNames.push_back(detection.get<std::string>());
        }
        return std::make_shared<ProgramElement_Detection>(acqNames);
    }

    throw std::runtime_error("unrecognized element type during parsing");
}

void CalculateDetectionIndices_StagePositionNames_Worker(std::shared_ptr<ProgramElement> encodedProgram, size_t& currentDetIndex,
                                                         std::string& currentStagePositionName,
                                                         std::map<AcquisitionName, std::vector<std::int64_t>>& detIdxAccum,
                                                         std::vector<std::string>& stageAccum) {
    switch (encodedProgram->getType()) {
        case ElementType::TerminalType:
            break;
        case ElementType::LoopType:
            for (size_t i = 0; i < encodedProgram->getLoopCount(); ++i) {
                for (const auto& child : encodedProgram->getChildren()) {
                    CalculateDetectionIndices_StagePositionNames_Worker(child, currentDetIndex, currentStagePositionName, detIdxAccum, stageAccum);
                }
            }
            break;
        case ElementType::StageLoopType: {
            ProgramElement_StageLoop* stageLoop = dynamic_cast<ProgramElement_StageLoop*>(encodedProgram.get());
            std::string savedPositionName = currentStagePositionName;
            const std::vector<std::string> stagePositionNames = stageLoop->getStagePositionNames();
            for (const auto& posName : stagePositionNames) {
                currentStagePositionName = posName;
                for (const auto& child : encodedProgram->getChildren()) {
                    CalculateDetectionIndices_StagePositionNames_Worker(child, currentDetIndex, currentStagePositionName, detIdxAccum, stageAccum);
                }
            }
            currentStagePositionName = savedPositionName;
            break;
        }
        case ElementType::DetectionType: {
            ProgramElement_Detection* detection = dynamic_cast<ProgramElement_Detection*>(encodedProgram.get());
            const auto& acqNames = detection->getAcquiredAcqNames();
            for (const auto& acqName : acqNames) {
                detIdxAccum[acqName].push_back(currentDetIndex);
            }
            stageAccum.push_back(currentStagePositionName);
            currentDetIndex++;
            if (stageAccum.size() != currentDetIndex) {
                throw std::logic_error("mismatch between detection indices and stage position names");
            }
            break;
        }
        default:
            throw std::logic_error("unrecognized element type during parsing");
            break;
    }
}

void CalculateDetectionIndicesAndStagePositionNames(std::shared_ptr<ProgramElement> encodedProgram,
                                                    std::map<AcquisitionName, std::vector<std::int64_t>>& detectionIndices,
                                                    std::vector<std::string>& stagePositionNames) {
    detectionIndices.clear();
    stagePositionNames.clear();

    size_t currentDetIndex = 0;
    std::string currentStagePositionName;
    CalculateDetectionIndices_StagePositionNames_Worker(encodedProgram, currentDetIndex, currentStagePositionName, detectionIndices, stagePositionNames);
}
