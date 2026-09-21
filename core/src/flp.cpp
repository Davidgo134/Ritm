#include <cstdlib>
#include <cstring>

#include "ritm/io.hpp"

namespace ritm {
namespace {

struct Reader {
    const uint8_t* p;
    size_t n;
    size_t i = 0;
    bool ok(size_t k) const { return i + k <= n; }
    uint8_t u8() { return p[i++]; }
    uint16_t u16() {
        uint16_t v = uint16_t(p[i] | (p[i + 1] << 8));
        i += 2;
        return v;
    }
    uint32_t u32() {
        uint32_t v = uint32_t(p[i]) | (uint32_t(p[i + 1]) << 8) | (uint32_t(p[i + 2]) << 16) |
                     (uint32_t(p[i + 3]) << 24);
        i += 4;
        return v;
    }
};

uint16_t rd16(const uint8_t* d) { return uint16_t(d[0] | (d[1] << 8)); }
uint32_t rd32(const uint8_t* d) {
    return uint32_t(d[0]) | (uint32_t(d[1]) << 8) | (uint32_t(d[2]) << 16) | (uint32_t(d[3]) << 24);
}

void putUtf8(std::string& out, uint32_t c) {
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xC0 | (c >> 6));
        out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += char(0xE0 | (c >> 12));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xF0 | (c >> 18));
        out += char(0x80 | ((c >> 12) & 0x3F));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
}

std::string decodeText(const uint8_t* d, size_t n, bool utf16) {
    std::string out;
    if (utf16) {
        for (size_t k = 0; k + 1 < n; k += 2) {
            uint32_t c = rd16(d + k);
            if (c == 0) break;
            if (c >= 0xD800 && c < 0xDC00 && k + 3 < n) {
                uint32_t lo = rd16(d + k + 2);
                if (lo >= 0xDC00 && lo < 0xE000) {
                    c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                    k += 2;
                }
            }
            putUtf8(out, c);
        }
    } else {
        for (size_t k = 0; k < n && d[k]; ++k) out += char(d[k]);
    }
    return out;
}

enum : int {
    EvNewChan = 64,
    EvNewPat = 65,
    EvTempoWord = 66,
    EvTempo = 156,
    EvChanName = 192,
    EvPatName = 193,
    EvTitle = 194,
    EvSample = 196,
    EvVersion = 199,
    EvDefPlugin = 201,
    EvPluginName = 203,
    EvPatNotes = 224,
    EvPlaylist = 233,
};

constexpr int kPatternBase = 20480;
constexpr size_t kNoteSize = 24;
constexpr size_t kPlaylistItemSize = 32;

}  // namespace

bool loadFlp(const std::vector<uint8_t>& data, Project& p, std::string& err) {
    Reader r{data.data(), data.size()};
    if (!r.ok(14) || std::memcmp(data.data(), "FLhd", 4) != 0) {
        err = "not an FLP file";
        return false;
    }
    r.i = 4;
    uint32_t hlen = r.u32();
    if (hlen < 6 || !r.ok(hlen)) {
        err = "bad FLP header";
        return false;
    }
    r.u16();  // format
    r.u16();  // channel count
    int ppq = r.u16();
    r.i = 8 + hlen;
    if (!r.ok(8) || std::memcmp(data.data() + r.i, "FLdt", 4) != 0) {
        err = "missing FLdt chunk";
        return false;
    }
    r.i += 4;
    uint32_t dlen = r.u32();
    size_t end = r.i + dlen;
    if (end > data.size()) end = data.size();

    p = Project{};
    p.format = "flp";
    p.ppq = ppq > 0 ? ppq : 96;

    bool utf16 = true;
    int curChan = -1;
    int curPat = -1;

    while (r.i < end) {
        int id = r.u8();
        size_t len = 0;
        if (id < 64) {
            len = 1;
        } else if (id < 128) {
            len = 2;
        } else if (id < 192) {
            len = 4;
        } else {
            unsigned shift = 0;
            while (r.i < end) {
                uint8_t b = r.u8();
                len |= size_t(b & 0x7F) << shift;
                shift += 7;
                if (!(b & 0x80)) break;
                if (shift > 28) break;
            }
        }
        if (r.i + len > end) {
            p.warnings.push_back("truncated event stream");
            break;
        }
        const uint8_t* d = data.data() + r.i;
        r.i += len;

        switch (id) {
            case EvVersion: {
                std::string v = decodeText(d, len, false);
                p.appVersion = v;
                int major = std::atoi(v.c_str());
                utf16 = major >= 12 || major == 0;
                break;
            }
            case EvNewChan:
                curChan = rd16(d);
                p.ensureChannel(curChan);
                break;
            case EvNewPat:
                curPat = rd16(d);
                p.ensurePattern(curPat);
                break;
            case EvTempoWord:
                p.bpm = rd16(d);
                break;
            case EvTempo:
                p.bpm = rd32(d) / 1000.0;
                break;
            case EvChanName:
                if (curChan >= 0) p.ensureChannel(curChan).name = decodeText(d, len, utf16);
                break;
            case EvPatName:
                if (curPat >= 0) p.ensurePattern(curPat).name = decodeText(d, len, utf16);
                break;
            case EvTitle:
                p.title = decodeText(d, len, utf16);
                break;
            case EvSample:
                if (curChan >= 0) p.ensureChannel(curChan).sample = decodeText(d, len, utf16);
                break;
            case EvDefPlugin:
            case EvPluginName:
                if (curChan >= 0) {
                    auto& c = p.ensureChannel(curChan);
                    if (id == EvPluginName || c.plugin.empty()) c.plugin = decodeText(d, len, utf16);
                }
                break;
            case EvPatNotes: {
                if (curPat < 0) break;
                auto& pat = p.ensurePattern(curPat);
                for (size_t o = 0; o + kNoteSize <= len; o += kNoteSize) {
                    const uint8_t* n = d + o;
                    Note note;
                    note.pos = rd32(n);
                    note.channel = rd16(n + 6);
                    note.len = rd32(n + 8);
                    note.key = rd16(n + 12);
                    note.pan = int8_t(int(n[20]) - 64);
                    note.vel = n[21];
                    pat.notes.push_back(note);
                }
                break;
            }
            case EvPlaylist: {
                for (size_t o = 0; o + kPlaylistItemSize <= len; o += kPlaylistItemSize) {
                    const uint8_t* n = d + o;
                    PlaylistItem it;
                    it.pos = rd32(n);
                    int base = rd16(n + 4);
                    int idx = rd16(n + 6);
                    it.len = rd32(n + 8);
                    int rv = rd16(n + 12);
                    it.track = 499 - rv;
                    if (idx > base && base == kPatternBase) {
                        it.pattern = idx - base;
                    } else {
                        it.channel = idx;
                    }
                    p.playlist.push_back(it);
                }
                break;
            }
            default:
                break;
        }
    }
    return true;
}

}  // namespace ritm
