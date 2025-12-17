#include "utils/json_parser.h"
#include <regex>
#include <string>

std::string parseJSONField(const std::string& json, const std::string& field) {
    std::regex pattern("\"" + field + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str();
    }
    return "";
}

int parseJSONInt(const std::string& json, const std::string& field, int default_val) {
    std::regex pattern("\"" + field + "\"\\s*:\\s*(\\d+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}

