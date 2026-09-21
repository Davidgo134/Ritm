#include <cmath>
#include <cstdio>
#include <cstring>

#include "ritm/engine.hpp"
#include "ritm/io.hpp"

static int g_fail = 0;
#define CHECK(c)                                                          \
    do {                                                                  \
        if (!(c)) {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);      \
            ++g_fail;                                                     \
        }                                                                 \
    } while (0)

using Bytes = std::vector<uint8_t>;

static void w16(Bytes& b, unsigned v) {
    b.push_back(v & 255);
    b.push_back((v >> 8) & 255);
}
static void w32(Bytes& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 255);
}
static void evByte(Bytes& b, int id, unsigned v) {
    b.push_back(id);
    b.push_back(v);
}
static void evWord(Bytes& b, int id, unsigned v) {
    b.push_back(id);
    w16(b, v);
}
static void evDword(Bytes& b, int id, uint32_t v) {
    b.push_back(id);
    w32(b, v);
}
static void evData(Bytes& b, int id, const Bytes& d) {
    b.push_back(id);
    size_t n = d.size();
    do {
        uint8_t c = n & 0x7F;
        n >>= 7;
        if (n) c |= 0x80;
        b.push_back(c);
    } while (n);
    b.insert(b.end(), d.begin(), d.end());
}
static Bytes utf16(const std::string& s) {
    Bytes o;
    for (char c : s) {
        o.push_back(uint8_t(c));
        o.push_back(0);
    }
    o.push_back(0);
    o.push_back(0);
    return o;
}
static Bytes ascii(const std::string& s) {
    Bytes o(s.begin(), s.end());
    o.push_back(0);
    return o;
}
static void note(Bytes& b, uint32_t pos, unsigned ch, uint32_t len, unsigned key, unsigned vel) {
    w32(b, pos);
    w16(b, 0);
    w16(b, ch);
    w32(b, len);
    w16(b, key);
    w16(b, 0);
    b.push_back(0);
    b.push_back(0);
    b.push_back(0);
    b.push_back(0);
    b.push_back(64);
    b.push_back(vel);
    b.push_back(128);
    b.push_back(128);
}
static void plItem(Bytes& b, uint32_t pos, unsigned idx, uint32_t len, unsigned track) {
    w32(b, pos);
    w16(b, 20480);
    w16(b, idx);
    w32(b, len);
    w16(b, 499 - track);
    for (int i = 0; i < 18; ++i) b.push_back(0);
}

static Bytes makeFlp() {
    Bytes ev;
    evData(ev, 199, ascii("20.8.4.2576"));
    evDword(ev, 156, 140000);
    evWord(ev, 64, 0);
    evData(ev, 192, utf16("Kick"));
    evData(ev, 196, utf16("kick.wav"));
    evWord(ev, 65, 1);
    evData(ev, 193, utf16("Pattern 1"));
    Bytes notes;
    note(notes, 0, 0, 96, 60, 100);
    note(notes, 96, 0, 48, 64, 90);
    evData(ev, 224, notes);
    Bytes pl;
    plItem(pl, 0, 20480 + 1, 384, 0);
    evData(ev, 233, pl);
    evByte(ev, 10, 1);

    Bytes f = {'F', 'L', 'h', 'd'};
    w32(f, 6);
    w16(f, 0);
    w16(f, 1);
    w16(f, 96);
    f.push_back('F');
    f.push_back('L');
    f.push_back('d');
    f.push_back('t');
    w32(f, uint32_t(ev.size()));
    f.insert(f.end(), ev.begin(), ev.end());
    return f;
}

static void testFlp() {
    ritm::Project p;
    std::string err;
    CHECK(ritm::loadFlp(makeFlp(), p, err));
    CHECK(p.format == "flp");
    CHECK(std::fabs(p.bpm - 140.0) < 1e-6);
    CHECK(p.ppq == 96);
    CHECK(p.appVersion == "20.8.4.2576");
    CHECK(p.channels.size() == 1);
    CHECK(p.channels[0].name == "Kick");
    CHECK(p.channels[0].sample == "kick.wav");
    CHECK(p.patterns.size() == 1);
    CHECK(p.patterns[0].name == "Pattern 1");
    CHECK(p.patterns[0].notes.size() == 2);
    CHECK(p.patterns[0].notes[1].key == 64);
    CHECK(p.patterns[0].notes[1].pos == 96);
    CHECK(p.patterns[0].notes[1].len == 48);
    CHECK(p.patterns[0].notes[0].vel == 100);
    CHECK(p.playlist.size() == 1);
    CHECK(p.playlist[0].pattern == 1);
    CHECK(p.playlist[0].len == 384);
    CHECK(p.playlist[0].track == 0);

    Bytes bad = {'X', 'X'};
    CHECK(!ritm::loadFlp(bad, p, err));
    Bytes trunc = makeFlp();
    trunc.resize(trunc.size() - 20);
    CHECK(ritm::loadFlp(trunc, p, err));
}

static void testAls() {
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Ableton MajorVersion=\"5\" Creator=\"Ableton Live 11.0\">\n"
        "<LiveSet>\n"
        "<MasterTrack><DeviceChain><Mixer><Tempo><Manual Value=\"128\" /></Tempo></Mixer></DeviceChain></MasterTrack>\n"
        "<Tracks>\n"
        "<MidiTrack Id=\"1\"><Name><EffectiveName Value=\"Lead &amp; Pad\" /></Name>\n"
        "<DeviceChain><MainSequencer><ClipTimeable><ArrangerAutomation><Events>\n"
        "<MidiClip Id=\"0\" Time=\"0\"><CurrentStart Value=\"4\" /><CurrentEnd Value=\"8\" /><Name Value=\"Clip A\" />\n"
        "<Notes><KeyTracks><KeyTrack Id=\"0\"><Notes>\n"
        "<MidiNoteEvent Time=\"0\" Duration=\"1\" Velocity=\"100\" IsEnabled=\"true\" />\n"
        "<MidiNoteEvent Time=\"1.5\" Duration=\"0.5\" Velocity=\"80\" IsEnabled=\"true\" />\n"
        "</Notes><MidiKey Value=\"60\" /></KeyTrack></KeyTracks></Notes>\n"
        "</MidiClip></Events></ArrangerAutomation></ClipTimeable></MainSequencer></DeviceChain></MidiTrack>\n"
        "<AudioTrack Id=\"2\"><Name><EffectiveName Value=\"Drums\" /></Name>\n"
        "<DeviceChain><MainSequencer><ClipTimeable><ArrangerAutomation><Events>\n"
        "<AudioClip Id=\"0\"><CurrentStart Value=\"0\" /><CurrentEnd Value=\"16\" /><Name Value=\"Loop\" />\n"
        "<SampleRef><FileRef><Path Value=\"/x/loop.wav\" /></FileRef></SampleRef></AudioClip>\n"
        "</Events></ArrangerAutomation></ClipTimeable></MainSequencer></DeviceChain></AudioTrack>\n"
        "</Tracks></LiveSet></Ableton>\n";
    Bytes gz;
    CHECK(ritm::gzip(xml, gz));
    ritm::Project p;
    std::string err;
    CHECK(ritm::loadAls(gz, p, err));
    CHECK(p.format == "als");
    CHECK(std::fabs(p.bpm - 128.0) < 1e-6);
    CHECK(p.trackNames.size() == 2);
    CHECK(p.trackNames[0] == "Lead & Pad");
    CHECK(p.patterns.size() == 1);
    CHECK(p.patterns[0].name == "Clip A");
    CHECK(p.patterns[0].notes.size() == 2);
    CHECK(p.patterns[0].notes[1].pos == uint32_t(1.5 * p.ppq));
    CHECK(p.patterns[0].notes[0].key == 60);
    CHECK(p.playlist.size() == 2);
    CHECK(p.playlist[0].pos == uint32_t(4 * p.ppq));
    CHECK(p.playlist[0].len == uint32_t(4 * p.ppq));
    CHECK(p.playlist[1].channel >= 10000);
    CHECK(p.findChannel(p.playlist[1].channel)->sample == "/x/loop.wav");
}

static void testEngine() {
    ritm::Project p;
    p.bpm = 120;
    p.ppq = 96;
    auto& pat = p.ensurePattern(1);
    ritm::Note n;
    n.pos = 0;
    n.len = 96;
    n.key = 69;
    n.vel = 110;
    pat.notes.push_back(n);
    ritm::PlaylistItem it;
    it.pattern = 1;
    it.len = 384;
    p.playlist.push_back(it);

    auto tl = std::make_shared<ritm::Timeline>(ritm::Timeline::fromProject(p));
    CHECK(tl->events.size() == 1);
    ritm::Engine e(48000);
    e.setTimeline(tl);
    std::vector<float> buf(2 * 48000);
    e.play();
    e.process(buf.data(), 48000);
    float head = 0, tail = 0;
    for (int i = 4800; i < 24000; ++i) head = std::max(head, std::fabs(buf[2 * i]));
    for (int i = 40000; i < 48000; ++i) tail = std::max(tail, std::fabs(buf[2 * i]));
    CHECK(head > 0.02f);
    CHECK(tail < 1e-4f);
    CHECK(e.positionTicks() > 0);
    e.stop();
    CHECK(!e.playing());
}

int main() {
    testFlp();
    testAls();
    testEngine();
    if (g_fail) {
        std::printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
