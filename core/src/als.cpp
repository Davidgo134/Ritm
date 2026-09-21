#include <cstdlib>
#include <memory>
#include <utility>

#include "ritm/io.hpp"

namespace ritm {
namespace {

struct Xml {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<std::unique_ptr<Xml>> kids;

    const Xml* child(const std::string& n) const {
        for (auto& k : kids)
            if (k->name == n) return k.get();
        return nullptr;
    }
    std::string attr(const std::string& n) const {
        for (auto& a : attrs)
            if (a.first == n) return a.second;
        return {};
    }
    const Xml* path(std::initializer_list<const char*> names) const {
        const Xml* cur = this;
        for (auto* n : names) {
            cur = cur ? cur->child(n) : nullptr;
            if (!cur) return nullptr;
        }
        return cur;
    }
    std::string val(std::initializer_list<const char*> names) const {
        const Xml* x = path(names);
        return x ? x->attr("Value") : std::string();
    }
    void findAll(const std::string& n, std::vector<const Xml*>& out) const {
        for (auto& k : kids) {
            if (k->name == n) out.push_back(k.get());
            k->findAll(n, out);
        }
    }
    const Xml* findFirst(const std::string& n) const {
        for (auto& k : kids) {
            if (k->name == n) return k.get();
            if (auto* r = k->findFirst(n)) return r;
        }
        return nullptr;
    }
};

std::string unescape(const std::string& s) {
    if (s.find('&') == std::string::npos) return s;
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            size_t e = s.find(';', i);
            if (e != std::string::npos) {
                std::string t = s.substr(i + 1, e - i - 1);
                if (t == "amp") o += '&';
                else if (t == "lt") o += '<';
                else if (t == "gt") o += '>';
                else if (t == "quot") o += '"';
                else if (t == "apos") o += '\'';
                else if (!t.empty() && t[0] == '#') {
                    long c = t.size() > 1 && (t[1] == 'x' || t[1] == 'X') ? std::strtol(t.c_str() + 2, nullptr, 16)
                                                                          : std::strtol(t.c_str() + 1, nullptr, 10);
                    if (c > 0 && c < 128) o += char(c);
                    else o += '?';
                } else o += s.substr(i, e - i + 1);
                i = e;
                continue;
            }
        }
        o += s[i];
    }
    return o;
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

std::unique_ptr<Xml> parseXml(const std::string& s, std::string& err) {
    auto root = std::make_unique<Xml>();
    std::vector<Xml*> stack{root.get()};
    size_t i = 0, n = s.size();
    while (i < n) {
        size_t lt = s.find('<', i);
        if (lt == std::string::npos) break;
        i = lt;
        if (s.compare(i, 4, "<!--") == 0) {
            size_t e = s.find("-->", i + 4);
            if (e == std::string::npos) break;
            i = e + 3;
            continue;
        }
        if (s.compare(i, 2, "<?") == 0) {
            size_t e = s.find("?>", i + 2);
            if (e == std::string::npos) break;
            i = e + 2;
            continue;
        }
        if (s.compare(i, 2, "<!") == 0) {
            size_t e = s.find('>', i);
            if (e == std::string::npos) break;
            i = e + 1;
            continue;
        }
        if (s.compare(i, 2, "</") == 0) {
            size_t e = s.find('>', i);
            if (e == std::string::npos) break;
            if (stack.size() > 1) stack.pop_back();
            i = e + 1;
            continue;
        }
        ++i;
        size_t ns = i;
        while (i < n && !isSpace(s[i]) && s[i] != '/' && s[i] != '>') ++i;
        auto node = std::make_unique<Xml>();
        node->name = s.substr(ns, i - ns);
        bool selfClose = false;
        while (i < n) {
            while (i < n && isSpace(s[i])) ++i;
            if (i >= n) break;
            if (s[i] == '/') {
                selfClose = true;
                ++i;
                continue;
            }
            if (s[i] == '>') {
                ++i;
                break;
            }
            size_t as = i;
            while (i < n && s[i] != '=' && !isSpace(s[i]) && s[i] != '>' && s[i] != '/') ++i;
            std::string an = s.substr(as, i - as);
            while (i < n && isSpace(s[i])) ++i;
            std::string av;
            if (i < n && s[i] == '=') {
                ++i;
                while (i < n && isSpace(s[i])) ++i;
                if (i < n && (s[i] == '"' || s[i] == '\'')) {
                    char q = s[i++];
                    size_t vs = i;
                    while (i < n && s[i] != q) ++i;
                    av = unescape(s.substr(vs, i - vs));
                    if (i < n) ++i;
                }
            }
            if (!an.empty()) node->attrs.emplace_back(std::move(an), std::move(av));
        }
        Xml* raw = node.get();
        stack.back()->kids.push_back(std::move(node));
        if (!selfClose) {
            stack.push_back(raw);
            if (stack.size() > 512) {
                err = "xml nesting too deep";
                return nullptr;
            }
        }
    }
    return root;
}

double num(const std::string& s, double def = 0) { return s.empty() ? def : std::atof(s.c_str()); }

std::string trackName(const Xml& t) {
    std::string n = t.val({"Name", "EffectiveName"});
    if (n.empty()) n = t.val({"Name", "UserName"});
    return n;
}

}  // namespace

bool loadAls(const std::vector<uint8_t>& data, Project& p, std::string& err) {
    std::string xml;
    if (!gunzip(data, xml, err)) return false;
    auto doc = parseXml(xml, err);
    if (!doc) return false;
    const Xml* live = doc->path({"Ableton", "LiveSet"});
    if (!live) {
        err = "not an Ableton Live set";
        return false;
    }

    p = Project{};
    p.format = "als";
    p.ppq = 960;
    if (const Xml* ab = doc->child("Ableton")) p.appVersion = ab->attr("Creator");

    if (const Xml* master = live->child("MasterTrack")) {
        if (const Xml* t = master->findFirst("Tempo")) {
            double b = num(t->val({"Manual"}));
            if (b > 0) p.bpm = b;
        }
    }

    const Xml* tracks = live->child("Tracks");
    if (!tracks) return true;
    int trackIndex = 0;
    int nextPattern = 1;
    int nextAudioChannel = 10000;
    for (auto& tk : tracks->kids) {
        bool midi = tk->name == "MidiTrack";
        bool audio = tk->name == "AudioTrack";
        if (!midi && !audio) continue;
        std::string tname = trackName(*tk);
        p.trackNames.push_back(tname);
        int chId = trackIndex;
        if (midi) p.ensureChannel(chId).name = tname;

        std::vector<const Xml*> arr;
        tk->findAll("ArrangerAutomation", arr);
        for (const Xml* a : arr) {
            const Xml* events = a->child("Events");
            if (!events) continue;
            for (auto& ev : events->kids) {
                double start = num(ev->val({"CurrentStart"}));
                double end = num(ev->val({"CurrentEnd"}));
                if (ev->name == "MidiClip") {
                    Pattern& pat = p.ensurePattern(nextPattern);
                    pat.name = ev->val({"Name"});
                    std::vector<const Xml*> keyTracks;
                    ev->findAll("KeyTrack", keyTracks);
                    for (const Xml* kt : keyTracks) {
                        int key = int(num(kt->val({"MidiKey"})));
                        const Xml* notes = kt->child("Notes");
                        if (!notes) continue;
                        for (auto& ne : notes->kids) {
                            if (ne->name != "MidiNoteEvent") continue;
                            if (ne->attr("IsEnabled") == "false") continue;
                            Note nt;
                            nt.pos = uint32_t(num(ne->attr("Time")) * p.ppq + 0.5);
                            nt.len = uint32_t(num(ne->attr("Duration")) * p.ppq + 0.5);
                            nt.key = uint16_t(key);
                            nt.vel = uint8_t(num(ne->attr("Velocity"), 100));
                            nt.channel = uint16_t(chId);
                            pat.notes.push_back(nt);
                        }
                    }
                    PlaylistItem it;
                    it.pos = uint32_t(start * p.ppq + 0.5);
                    it.len = end > start ? uint32_t((end - start) * p.ppq + 0.5) : 0;
                    it.track = trackIndex;
                    it.pattern = nextPattern;
                    p.playlist.push_back(it);
                    ++nextPattern;
                } else if (ev->name == "AudioClip") {
                    Channel& c = p.ensureChannel(nextAudioChannel);
                    c.name = ev->val({"Name"});
                    std::string path = ev->val({"SampleRef", "FileRef", "Path"});
                    if (path.empty()) path = ev->val({"SampleRef", "FileRef", "RelativePath"});
                    c.sample = path;
                    PlaylistItem it;
                    it.pos = uint32_t(start * p.ppq + 0.5);
                    it.len = end > start ? uint32_t((end - start) * p.ppq + 0.5) : 0;
                    it.track = trackIndex;
                    it.channel = nextAudioChannel;
                    p.playlist.push_back(it);
                    ++nextAudioChannel;
                }
            }
        }
        ++trackIndex;
    }
    return true;
}

}  // namespace ritm
