#include <cstdio>

#include "ritm/io.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: ritm-info <project>\n");
        return 2;
    }
    ritm::Project p;
    std::string err;
    if (!ritm::loadProject(argv[1], p, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    std::printf("format=%s app=%s bpm=%.3f ppq=%d\n", p.format.c_str(), p.appVersion.c_str(), p.bpm, p.ppq);
    std::printf("title=%s\n", p.title.c_str());
    std::printf("channels=%zu patterns=%zu playlist=%zu\n", p.channels.size(), p.patterns.size(),
                p.playlist.size());
    for (auto& c : p.channels)
        std::printf("  ch %d: %s | plugin=%s | sample=%s\n", c.id, c.name.c_str(), c.plugin.c_str(),
                    c.sample.c_str());
    for (auto& pat : p.patterns)
        std::printf("  pat %d: %s (%zu notes)\n", pat.id, pat.name.c_str(), pat.notes.size());
    for (auto& w : p.warnings) std::printf("warning: %s\n", w.c_str());
    return 0;
}
