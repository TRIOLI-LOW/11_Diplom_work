#pragma once 
#include <vector>
#include <string>
#include <unordered_map>

#include "link.h"

std::string getHtmlContent(const Link& link);
std::string cleanHtml(const std::string& html);
std::unordered_map<std::string, int> countWordFrequency(const std::string& text);
void saveToDatabase(const std::string& url, const std::unordered_map<std::string, int>& wordFrequency);
void initializeDatabase();
std::vector<Link> extractLinks(const std::string& html, const std::string& baseHost, ProtocolType protocol);