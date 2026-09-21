#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ritm {

struct Note {
    uint32_t pos = 0;
    uint32_t len = 0;
    uint16_t key = 60;
    uint8_t vel = 100;
    int8_t pan = 0;
    uint16_t channel = 0;
};

struct Channel {
    int id = 0;
    std::string name;
    std::string sample;
    std::string plugin;
};

struct Pattern {
    int id = 0;
    std::string name;
    std::vector<Note> notes;
};

struct PlaylistItem {
    uint32_t pos = 0;
    uint32_t len = 0;
    int track = 0;
    int pattern = -1;
    int channel = -1;
};

struct Project {
    std::string format;
    std::string title;
    std::string appVersion;
    double bpm = 120.0;
    int ppq = 96;
    std::vector<Channel> channels;
    std::vector<Pattern> patterns;
    std::vector<PlaylistItem> playlist;
    std::vector<std::string> trackNames;
    std::vector<std::string> warnings;

    Pattern* findPattern(int id);
    Channel* findChannel(int id);
    Pattern& ensurePattern(int id);
    Channel& ensureChannel(int id);
};

}  // namespace ritm
