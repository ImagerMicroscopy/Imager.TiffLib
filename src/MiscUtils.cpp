#include "MiscUtils.h"

#include "json.hpp"

std::vector<std::string> ChannelNamesFromImagerProgram(const std::string& program) {
	nlohmann::json json(program);
	return json["defineddetections"].get<std::vector<std::string>>();
}
