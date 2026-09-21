#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "ritm/project.hpp"

namespace ritm {

struct Event {
    double tick = 0;
    double len = 0;
    int channel = 0;
    int key = 60;
    float vel = 0.8f;
};

struct Timeline {
    double bpm = 120;
    int ppq = 96;
    double lengthTicks = 0;
    std::vector<Event> events;

    static Timeline fromProject(const Project& p);
};

class Engine {
public:
    explicit Engine(int sampleRate = 48000);

    void setTimeline(std::shared_ptr<const Timeline> t);
    void setBpm(double bpm);
    void setLoop(bool on);
    void play();
    void stop();
    void seekTicks(double tick);
    bool playing() const { return playing_.load(); }
    double positionTicks() const { return pos_.load(); }

    // Interleaved stereo float output. Real-time safe: no allocation, no blocking.
    void process(float* out, int frames);

private:
    struct Voice {
        bool active = false;
        bool releasing = false;
        int channel = 0;
        int key = 0;
        float phase = 0;
        float inc = 0;
        float env = 0;
        float lp = 0;
        float vel = 0;
        long remaining = 0;
    };
    static constexpr int kVoices = 64;

    void trigger(const Event& e, double samplesPerTick);

    int sr_;
    std::mutex m_;
    std::shared_ptr<const Timeline> tl_;
    std::atomic<bool> playing_{false};
    std::atomic<bool> loop_{false};
    std::atomic<double> pos_{0};
    std::atomic<double> bpm_{120};
    std::atomic<bool> seekPending_{false};
    std::atomic<double> seekTo_{0};
    size_t next_ = 0;
    Voice voices_[kVoices];
};

}  // namespace ritm
