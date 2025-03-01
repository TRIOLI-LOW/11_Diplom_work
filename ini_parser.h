
#pragma once

#include <string>
#include <map>
#include <sstream>
#include <stdexcept>

class IniParser {
private:
    std::map<std::string, std::map<std::string, std::string>> data;
    static std::string trim(const std::string& str);
public:
    IniParser(const std::string& filename);

    template<typename T>
    T getValue(const std::string& section, const std::string& key);
};

// Определение шаблонного метода должно быть **в этом же заголовочном файле**:
template<typename T>
T IniParser::getValue(const std::string& section, const std::string& key) {
    if (data.count(section) && data[section].count(key)) {
        std::stringstream ss(data[section][key]);
        T value;
        ss >> value;
        if (ss.fail()) {
            throw std::runtime_error("Failed to convert ini value: " + section + "." + key);
        }
        return value;
    }
    throw std::runtime_error("Key not found in ini file: " + section + "." + key);
}
