#pragma once

#include <cstdint>
#include <span>

namespace VKRE {

    struct ProfilerSample {
        uint16_t Marker = 0;
        uint16_t Depth = 0;
        float ElapsedTimeMs = 0.0f;
    };

    struct ProfilerMarkerStats {
        const char* Name = nullptr;
        float LastMs = 0.0f;
        float AverageMs = 0.0f;
        float MaxMs = 0.0f;
        uint32_t CallsLastFrame = 0;
    };

    class Profiler {
    public:
        static constexpr uint32_t MaxSamplesPerFrame = 1024;
        static constexpr uint16_t MaxMarkers = 256;
        static constexpr uint32_t HistoryLength = 128;

        static constexpr uint16_t INVALID_MARKER = UINT16_MAX;
        static constexpr uint32_t INVALID_SAMPLE = UINT32_MAX;

        static uint16_t RegisterMarker(const char* name);

        static void BeginFrame();
        static void EndFrame();

        static uint32_t BeginSample(uint16_t marker);
        static void EndSample(uint32_t sample);

        static std::span<const ProfilerSample> GetLastFrameSamples();
        static std::span<const ProfilerMarkerStats> GetMarkerStats();
        static float GetLastFrameTimeMs();
    };

    class ScopedProfiler {
    public:
        explicit ScopedProfiler(uint16_t marker)
            :mSample(Profiler::BeginSample(marker)) {}
        ~ScopedProfiler() { Profiler::EndSample(mSample); }

        ScopedProfiler(const ScopedProfiler&) = delete;
        ScopedProfiler& operator=(const ScopedProfiler&) = delete;
        ScopedProfiler(const ScopedProfiler&&) = delete;
        ScopedProfiler& operator=(const ScopedProfiler&&) = delete;

    private:
        uint32_t mSample;
    };

#define VKRE_CONCAT_IMPL(a, b) a##b
#define VKRE_CONCAT(a, b) VKRE_CONCAT_IMPL(a, b)

// TODO: Figure out how to add classes to the function timer
#ifdef VKRE_PROFILING_ENABLED
    #define VKRE_SCOPED_TIMER(name)                                                                                                 \
        static const uint16_t VKRE_CONCAT(sProfilerMarker, __LINE__) = ::VKRE::Profiler::RegisterMarker(name);                      \
        ::VKRE::ScopedProfiler VKRE_CONCAT(profilerTimer, __LINE__)(VKRE_CONCAT(sProfilerMarker, __LINE__))
    #define VKRE_FUNCTION_TIMER() VKRE_SCOPED_TIMER(__func__)
#else
    #define VKRE_SCOPED_TIMER(name) ((void)0)
    #define VKRE_FUNCTION_TIMER() ((void)0)
#endif

}
