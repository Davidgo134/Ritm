#include "ritm/project.hpp"

namespace ritm {

Pattern* Project::findPattern(int id) {
    for (auto& p : patterns)
        if (p.id == id) return &p;
    return nullptr;
}

Channel* Project::findChannel(int id) {
    for (auto& c : channels)
        if (c.id == id) return &c;
    return nullptr;
}

Pattern& Project::ensurePattern(int id) {
    if (auto* p = findPattern(id)) return *p;
    patterns.emplace_back();
    patterns.back().id = id;
    return patterns.back();
}

Channel& Project::ensureChannel(int id) {
    if (auto* c = findChannel(id)) return *c;
    channels.emplace_back();
    channels.back().id = id;
    return channels.back();
}

}  // namespace ritm
