#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUT2INTERNAL_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUT2INTERNAL_HPP

#include <chrono>
#include "prx/common/StderrLog.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

#include "SDL.h"
#include "SceTypes.hpp"
#include "AudioOut2PadMix.hpp"

bool AudioOut2TraceEnabled();

double AudioOut2TraceSeconds();

#define AUDIOOUT2_TRACE(...) \
    do { \
        if (AudioOut2TraceEnabled()) aps5::LogErr( "[audioout2] " __VA_ARGS__); \
    } while (0)

static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80260502);
static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE = static_cast<int>(0x80260503);

static constexpr int SCE_AUDIO_OUT2_ERROR_QUEUE_FULL = static_cast<int>(0x80260507);

static constexpr std::uint32_t AUDIO_OUT2_SAMPLE_RATE = 48000;

static constexpr std::uint32_t AUDIO_OUT2_DEFAULT_GRAIN = 256;
static constexpr std::uint32_t AUDIO_OUT2_OUTPUT_CHANNELS = 2;
static constexpr std::uint32_t AUDIO_OUT2_PORT_CHANNELS_MAX = 8;

struct AudioOut2Context;

struct AudioOut2Port {
    bool used = false;
    AudioOut2Context* context = nullptr;
    std::uint16_t type = 0;
    std::uint32_t dataFormat = 0;
    std::uint32_t samplingFreq = 0;
    std::uint32_t flags = 0;
    std::uint32_t channels = 0;
    bool int16 = false;
    const void* data = nullptr;
    float volume[AUDIO_OUT2_PORT_CHANNELS_MAX] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    std::uint64_t dataSets = 0;
    std::uint64_t attributeTraces = 0;
};

struct AudioOut2Context {
    std::mutex lock;
    std::uint32_t grain = AUDIO_OUT2_DEFAULT_GRAIN;
    std::uint32_t queueDepth = 1;

    std::uint32_t queued = 0;
    std::chrono::steady_clock::time_point playHead;
    SDL_AudioDeviceID device = 0;

    std::vector<float> mix;
    SDL_AudioDeviceID padDevice = 0;
    std::chrono::steady_clock::time_point nextPadProbe;
    AudioOut2PadLayout padLayout;
    std::vector<float> padMix;
    std::vector<float> padFrames;

    std::uint64_t pushes = 0;
    std::uint64_t blockingPushes = 0;
    std::uint64_t fullRejects = 0;
    std::uint64_t advances = 0;
    std::uint64_t queueLevelPolls = 0;
    std::uint64_t primes = 0;
    std::uint64_t dropped = 0;
    std::uint64_t summaryPushes = 0;
    std::uint64_t summaryAdvances = 0;
    std::uint64_t summaryPolls = 0;

    float summaryPeak = 0.0f;
    std::chrono::steady_clock::time_point summaryStart;
};

std::uint32_t AudioOut2MixPorts(const AudioOut2Context& context, float* out, float* padOut, std::uint32_t frames);
bool AudioOut2HasPadPorts(const AudioOut2Context& context);

void AudioOut2ReleasePorts(const AudioOut2Context& context);

#endif
