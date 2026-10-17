#include <Core/Profiler.h>

#include <array>
#include <chrono>
#include <print>
#include <algorithm>

namespace VKRE {

    // TODO: Maybe make a Timer or Clock system
    namespace {
        using Clock = std::chrono::steady_clock;
        inline int64_t Now() { return Clock::now().time_since_epoch().count(); }
        inline float TicksToMs(int64_t ticks) {
            return std::chrono::duration<float, std::milli>(Clock::duration(ticks)).count();
        }

        struct ProfilerState {
            std::array<ProfilerSample, Profiler::MaxSamplesPerFrame> Samples;
            std::array<int64_t, Profiler::MaxSamplesPerFrame> SampleStarts;

            uint32_t SampleCount;
            uint32_t Depth;
            uint32_t FrameSample;

            std::array<ProfilerSample, Profiler::MaxSamplesPerFrame> LastSamples;
            uint32_t LastSampleCount;

            std::array<ProfilerMarkerStats, Profiler::MaxMarkers> Stats;
            std::array<std::array<float, Profiler::HistoryLength>, Profiler::MaxMarkers> History;
            std::array<double, Profiler::MaxMarkers> HistorySum;
            std::array<float, Profiler::MaxMarkers> FrameMs;
            std::array<uint32_t, Profiler::MaxMarkers> FrameCalls;
            uint16_t MarkerCount;
            uint16_t FrameMarker;

            uint32_t HistoryHead;
            uint32_t HistoryFilled;
        };
    }

    static ProfilerState sProfilerState{};

    uint16_t Profiler::RegisterMarker(const char* name) {
        if (sProfilerState.MarkerCount >= MaxMarkers) {
            std::println("[Profiler] MaxMarkers ({}) reached, ignoring marker '{}'", MaxMarkers, name);
            return INVALID_MARKER;
        }

        const uint16_t marker = sProfilerState.MarkerCount++;
        sProfilerState.Stats[marker].Name = name;
        return marker;
    }

    void Profiler::BeginFrame() {
        static const uint16_t sFrameMarker = RegisterMarker("Frame");
        sProfilerState.FrameMarker = sFrameMarker;

        sProfilerState.SampleCount = 0;
        sProfilerState.Depth = 0;
        sProfilerState.FrameSample = BeginSample(sFrameMarker);
    }

    void Profiler::EndFrame() {
        EndSample(sProfilerState.FrameSample);

        if (sProfilerState.Depth != 0) {
            std::println("[Profiler] {} sample(s) still open at EndFrame", sProfilerState.Depth);
            sProfilerState.Depth = 0;
        }

        std::copy_n(sProfilerState.Samples.begin(), sProfilerState.SampleCount, sProfilerState.LastSamples.begin());
        sProfilerState.LastSampleCount = sProfilerState.SampleCount;

        const uint16_t markerCount = sProfilerState.MarkerCount;
        std::fill_n(sProfilerState.FrameMs.begin(), markerCount, 0.0f);
        std::fill_n(sProfilerState.FrameCalls.begin(), markerCount, 0u);

        for (uint32_t i = 0; i < sProfilerState.SampleCount; i++) {
            const ProfilerSample& sample = sProfilerState.Samples[i];
            sProfilerState.FrameMs[sample.Marker] += sample.ElapsedTimeMs;
            sProfilerState.FrameCalls[sample.Marker]++;
        }

        const uint32_t head = sProfilerState.HistoryHead;
        const uint32_t filled = std::min(sProfilerState.HistoryFilled + 1, HistoryLength);

        for (uint16_t marker = 0; marker < markerCount; marker++) {
            auto& history = sProfilerState.History[marker];
            const float ms = sProfilerState.FrameMs[marker];

            sProfilerState.HistorySum[marker] += static_cast<double>(ms) - static_cast<double>(history[head]);
            history[head] = ms;

            ProfilerMarkerStats& stats = sProfilerState.Stats[marker];
            stats.LastMs = ms;
            stats.AverageMs = static_cast<float>(sProfilerState.HistorySum[marker] / filled);
            stats.MaxMs = *std::max_element(history.begin(), history.begin() + filled);
            stats.CallsLastFrame = sProfilerState.FrameCalls[marker];
        }

        sProfilerState.HistoryHead = (head + 1) % HistoryLength;
        sProfilerState.HistoryFilled = filled;
    }

    uint32_t Profiler::BeginSample(uint16_t marker) {
        const uint32_t depth = sProfilerState.Depth++;
        if (marker == INVALID_MARKER || sProfilerState.SampleCount >= MaxSamplesPerFrame)
            return INVALID_SAMPLE;

        const uint32_t sample = sProfilerState.SampleCount++;
        sProfilerState.Samples[sample] = { .Marker = marker, .Depth = static_cast<uint16_t>(depth), .ElapsedTimeMs = 0.0f };
        sProfilerState.SampleStarts[sample] = Now();
        return sample;
    }

    void Profiler::EndSample(uint32_t sample) {
        const int64_t end = Now();

        if (sProfilerState.Depth > 0)
            sProfilerState.Depth--;
        if (sample == INVALID_SAMPLE)
            return;

        sProfilerState.Samples[sample].ElapsedTimeMs = TicksToMs(end - sProfilerState.SampleStarts[sample]);
    }

    std::span<const ProfilerSample> Profiler::GetLastFrameSamples() {
        return { sProfilerState.LastSamples.data(), sProfilerState.LastSampleCount };
    }

    std::span<const ProfilerMarkerStats> Profiler::GetMarkerStats() {
        return { sProfilerState.Stats.data(), sProfilerState.MarkerCount };
    }

    float Profiler::GetLastFrameTimeMs() {
        return sProfilerState.LastSampleCount > 0 ? sProfilerState.LastSamples[0].ElapsedTimeMs : 0.0f;
    }

}
