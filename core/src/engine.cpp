#include "ritm/engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ritm {

Timeline Timeline::fromProject(const Project& p) {
    Timeline t;
    t.bpm = p.bpm;
    t.ppq = p.ppq;
    auto addPattern = [&](const Pattern& pat, double base, double limit) {
        for (const auto& n : pat.notes) {
            if (limit > 0 && n.pos >= limit) continue;
            Event e;
            e.tick = base + n.pos;
            e.len = n.len ? n.len : p.ppq / 4;
            e.channel = n.channel;
            e.key = n.key;
            e.vel = std::min(1.0f, n.vel / 127.0f);
            t.events.push_back(e);
            t.lengthTicks = std::max(t.lengthTicks, e.tick + e.len);
        }
    };
    bool any = false;
    for (const auto& it : p.playlist) {
        if (it.pattern < 0) continue;
        for (const auto& pat : p.patterns)
            if (pat.id == it.pattern) {
                addPattern(pat, it.pos, it.len);
                any = true;
            }
    }
    if (!any && !p.patterns.empty()) addPattern(p.patterns.front(), 0, 0);
    std::stable_sort(t.events.begin(), t.events.end(),
                     [](const Event& a, const Event& b) { return a.tick < b.tick; });
    return t;
}

Engine::Engine(int sampleRate) : sr_(sampleRate) {}

void Engine::setTimeline(std::shared_ptr<const Timeline> t) {
    std::lock_guard<std::mutex> g(m_);
    tl_ = std::move(t);
    if (tl_) bpm_.store(tl_->bpm);
    seekTo_.store(0);
    seekPending_.store(true);
}

void Engine::setBpm(double bpm) { bpm_.store(std::max(10.0, std::min(999.0, bpm))); }
void Engine::setLoop(bool on) { loop_.store(on); }
void Engine::play() { playing_.store(true); }
void Engine::stop() {
    playing_.store(false);
    seekTo_.store(0);
    seekPending_.store(true);
}
void Engine::seekTicks(double tick) {
    seekTo_.store(std::max(0.0, tick));
    seekPending_.store(true);
}

void Engine::trigger(const Event& e, double samplesPerTick) {
    Voice* v = nullptr;
    for (auto& c : voices_)
        if (!c.active) {
            v = &c;
            break;
        }
    if (!v) {
        v = &voices_[0];
        for (auto& c : voices_)
            if (c.env < v->env) v = &c;
    }
    v->active = true;
    v->releasing = false;
    v->channel = e.channel;
    v->key = e.key;
    v->phase = 0;
    v->env = 0;
    v->lp = 0;
    v->vel = e.vel;
    float freq = 440.0f * std::pow(2.0f, (e.key - 69) / 12.0f);
    v->inc = freq / float(sr_);
    v->remaining = long(e.len * samplesPerTick);
    if (v->remaining < 1) v->remaining = 1;
}

void Engine::process(float* out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * size_t(frames));
    std::unique_lock<std::mutex> lk(m_, std::try_to_lock);
    if (!lk.owns_lock() || !tl_) return;
    const Timeline& tl = *tl_;

    if (seekPending_.exchange(false)) {
        pos_.store(seekTo_.load());
        next_ = 0;
        while (next_ < tl.events.size() && tl.events[next_].tick < pos_.load()) ++next_;
        for (auto& v : voices_) v.active = false;
    }

    const double bpm = bpm_.load();
    const double ticksPerSample = bpm * tl.ppq / (60.0 * sr_);
    const double samplesPerTick = 1.0 / ticksPerSample;
    const bool playing = playing_.load();
    double pos = pos_.load();
    const float attack = 1.0f / (0.005f * sr_);
    const float release = 1.0f / (0.08f * sr_);
    const float lpCoef = 1.0f - std::exp(-2.0f * 3.14159265f * 5000.0f / float(sr_));

    for (int i = 0; i < frames; ++i) {
        if (playing) {
            while (next_ < tl.events.size() && tl.events[next_].tick <= pos) {
                trigger(tl.events[next_], samplesPerTick);
                ++next_;
            }
            pos += ticksPerSample;
            if (tl.lengthTicks > 0 && pos >= tl.lengthTicks + tl.ppq) {
                if (loop_.load()) {
                    pos = 0;
                    next_ = 0;
                } else {
                    playing_.store(false);
                }
            }
        }
        float mix = 0;
        for (auto& v : voices_) {
            if (!v.active) continue;
            if (v.releasing) {
                v.env -= release;
                if (v.env <= 0) {
                    v.active = false;
                    continue;
                }
            } else {
                v.env = std::min(1.0f, v.env + attack);
                if (--v.remaining <= 0) v.releasing = true;
            }
            v.phase += v.inc;
            if (v.phase >= 1.0f) v.phase -= 1.0f;
            float saw = 2.0f * v.phase - 1.0f;
            float sine = std::sin(6.2831853f * v.phase);
            float s = 0.6f * saw + 0.4f * sine;
            v.lp += lpCoef * (s - v.lp);
            mix += v.lp * v.env * v.vel * 0.2f;
        }
        mix = std::tanh(mix);
        out[2 * i] = mix;
        out[2 * i + 1] = mix;
    }
    pos_.store(pos);
}

}  // namespace ritm
