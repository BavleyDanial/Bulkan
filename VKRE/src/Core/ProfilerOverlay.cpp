#include <Core/ProfilerOverlay.h>
#include <Core/Profiler.h>

#include <imgui.h>

namespace VKRE {

    void ProfilerOverlay::OnUIRender() {
        if (ImGui::Begin("Profiler")) {
#ifdef VKRE_PROFILING_ENABLED
        const auto samples = Profiler::GetLastFrameSamples();
        const auto markers = Profiler::GetMarkerStats();
        const float frameMs = Profiler::GetLastFrameTimeMs();

        ImGui::Text("Frame %.3f ms (%.0f FPS)  |  CPU time only", frameMs, frameMs > 0.0f ? 1000.0f / frameMs : 0.0f);

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                          ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("##ProfilerSamples", 6, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Marker", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Last");
            ImGui::TableSetupColumn("Avg");
            ImGui::TableSetupColumn("Max");
            ImGui::TableSetupColumn("% frame");
            ImGui::TableSetupColumn("Calls");
            ImGui::TableHeadersRow();

            const float indent = ImGui::GetStyle().IndentSpacing * 0.5f;

            for (const ProfilerSample& sample : samples) {
                const ProfilerMarkerStats& marker = markers[sample.Marker];

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sample.Depth * indent);
                ImGui::TextUnformatted(marker.Name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", sample.ElapsedTimeMs);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", marker.AverageMs);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", marker.MaxMs);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%5.1f", frameMs > 0.0f ? 100.0f * sample.ElapsedTimeMs / frameMs : 0.0f);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%u", marker.CallsLastFrame);
            }

            ImGui::EndTable();
        }
#else
        ImGui::TextUnformatted("Profiling is compiled out (VKRE_PROFILING_ENABLED is not defined).");
#endif
        }

        ImGui::End();
    }
}
