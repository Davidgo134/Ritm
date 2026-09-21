#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "ritm/project.hpp"

namespace ritm {

bool readFile(const std::string& path, std::vector<uint8_t>& out);
bool gunzip(const std::vector<uint8_t>& in, std::string& out, std::string& err);
bool gzip(const std::string& in, std::vector<uint8_t>& out);

bool loadFlp(const std::vector<uint8_t>& data, Project& p, std::string& err);
bool loadAls(const std::vector<uint8_t>& data, Project& p, std::string& err);
bool loadProject(const std::string& path, Project& p, std::string& err);

}  // namespace ritm
