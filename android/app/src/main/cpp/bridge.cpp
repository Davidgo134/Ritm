#include <jni.h>
#include <oboe/Oboe.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>

#include "ritm/engine.hpp"
#include "ritm/io.hpp"

namespace {

std::shared_ptr<oboe::AudioStream> g_stream;
std::unique_ptr<ritm::Engine> g_engine;
std::atomic<ritm::Engine*> g_ep{nullptr};
std::shared_ptr<const ritm::Timeline> g_imported;

class Callback : public oboe::AudioStreamDataCallback {
public:
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* data, int32_t frames) override {
        auto* e = g_ep.load(std::memory_order_acquire);
        auto* out = static_cast<float*>(data);
        if (e) {
            e->process(out, frames);
        } else {
            std::memset(out, 0, sizeof(float) * 2 * size_t(frames));
        }
        return oboe::DataCallbackResult::Continue;
    }
};

}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL Java_dev_ritm_daw_Native_start(JNIEnv*, jobject) {
    if (g_stream) return JNI_TRUE;
    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setUsage(oboe::Usage::Media)
        ->setDataCallback(std::make_shared<Callback>());
    oboe::Result r = b.openStream(g_stream);
    if (r != oboe::Result::OK) {
        g_stream.reset();
        return JNI_FALSE;
    }
    g_engine = std::make_unique<ritm::Engine>(g_stream->getSampleRate());
    g_ep.store(g_engine.get(), std::memory_order_release);
    g_stream->setBufferSizeInFrames(g_stream->getFramesPerBurst() * 2);
    return g_stream->requestStart() == oboe::Result::OK ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_dev_ritm_daw_Native_shutdown(JNIEnv*, jobject) {
    if (g_stream) {
        g_stream->stop();
        g_stream->close();
        g_stream.reset();
    }
    g_ep.store(nullptr, std::memory_order_release);
    g_engine.reset();
}

JNIEXPORT void JNICALL Java_dev_ritm_daw_Native_setNotes(JNIEnv* env, jobject, jintArray notes, jdouble bpm,
                                                         jint ppq, jdouble loopTicks) {
    if (!g_engine) return;
    auto tl = std::make_shared<ritm::Timeline>();
    tl->bpm = bpm;
    tl->ppq = ppq;
    tl->loopTicks = loopTicks;
    jsize n = env->GetArrayLength(notes);
    std::vector<jint> buf(static_cast<size_t>(n));
    if (n > 0) env->GetIntArrayRegion(notes, 0, n, buf.data());
    for (jsize i = 0; i + 3 < n; i += 4) {
        ritm::Event e;
        e.tick = buf[size_t(i)];
        e.len = buf[size_t(i) + 1];
        e.key = buf[size_t(i) + 2];
        e.vel = std::min(1.0f, float(buf[size_t(i) + 3]) / 127.0f);
        tl->events.push_back(e);
        tl->lengthTicks = std::max(tl->lengthTicks, e.tick + e.len);
    }
    std::stable_sort(tl->events.begin(), tl->events.end(),
                     [](const ritm::Event& a, const ritm::Event& b) { return a.tick < b.tick; });
    g_engine->setTimeline(tl, true);
}

JNIEXPORT void JNICALL Java_dev_ritm_daw_Native_setBpm(JNIEnv*, jobject, jdouble bpm) {
    if (g_engine) g_engine->setBpm(bpm);
}

JNIEXPORT void JNICALL Java_dev_ritm_daw_Native_play(JNIEnv*, jobject) {
    if (g_engine) g_engine->play();
}

JNIEXPORT void JNICALL Java_dev_ritm_daw_Native_pause(JNIEnv*, jobject) {
    if (g_engine) g_engine->pause();
}

JNIEXPORT void JNICALL Java_dev_ritm_daw_Native_stopAndRewind(JNIEnv*, jobject) {
    if (g_engine) g_engine->stop();
}

JNIEXPORT jboolean JNICALL Java_dev_ritm_daw_Native_isPlaying(JNIEnv*, jobject) {
    return g_engine && g_engine->playing() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL Java_dev_ritm_daw_Native_position(JNIEnv*, jobject) {
    return g_engine ? g_engine->positionTicks() : 0.0;
}

JNIEXPORT jstring JNICALL Java_dev_ritm_daw_Native_importProject(JNIEnv* env, jobject, jstring path) {
    const char* c = env->GetStringUTFChars(path, nullptr);
    std::string p(c ? c : "");
    if (c) env->ReleaseStringUTFChars(path, c);
    ritm::Project proj;
    std::string err;
    if (!ritm::loadProject(p, proj, err)) {
        std::string msg = "ERR:";
        for (char ch : err)
            if (ch >= 32 && ch < 127) msg += ch;
        return env->NewStringUTF(msg.c_str());
    }
    g_imported = std::make_shared<ritm::Timeline>(ritm::Timeline::fromProject(proj));
    std::string info = "OK:" + proj.format + " channels=" + std::to_string(proj.channels.size()) +
                       " patterns=" + std::to_string(proj.patterns.size()) +
                       " notes=" + std::to_string(g_imported->events.size());
    return env->NewStringUTF(info.c_str());
}

JNIEXPORT jintArray JNICALL Java_dev_ritm_daw_Native_importedNotes(JNIEnv* env, jobject) {
    std::vector<jint> out;
    if (g_imported) {
        for (const auto& e : g_imported->events) {
            out.push_back(jint(e.tick));
            out.push_back(jint(e.len));
            out.push_back(jint(e.key));
            out.push_back(jint(e.vel * 127.0f + 0.5f));
        }
    }
    jintArray arr = env->NewIntArray(jsize(out.size()));
    if (!out.empty()) env->SetIntArrayRegion(arr, 0, jsize(out.size()), out.data());
    return arr;
}

JNIEXPORT jdouble JNICALL Java_dev_ritm_daw_Native_importedBpm(JNIEnv*, jobject) {
    return g_imported ? g_imported->bpm : 120.0;
}

JNIEXPORT jint JNICALL Java_dev_ritm_daw_Native_importedPpq(JNIEnv*, jobject) {
    return g_imported ? g_imported->ppq : 96;
}

}  // extern "C"
