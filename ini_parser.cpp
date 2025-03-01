#include "ini_parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

IniParser::IniParser(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open ini file: " + filename);
    }

    std::string line, section;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;

        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
        }
        else {
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            data[section][key] = value;
        }
    }
}

std::string IniParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    return (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
}
