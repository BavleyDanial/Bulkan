#include <AssetsManagers/ShaderImporter.h>
#include <ResourceManager/ShaderCompiler.h>

#include <format>
#include <fstream>
#include <print>
#include <sstream>

#include <optional>

namespace VKRE {

    std::optional<ShaderImportResults> ShaderImporter::LoadFromFile(
        const std::filesystem::path& path,
        const ShaderImportOptions& options) {

        std::optional<std::string> source = ReadFile(path);
        if (!source.has_value()) return std::nullopt;

        ShaderImportResults results{};
        results.Name = path.filename().string();

        ShaderCompileOptions compileOptions{};
        compileOptions.Optimise = options.Optimize;
        compileOptions.GenerateDebugInfo = options.GenerateDebugInfo;

        ShaderCompileResult compResult = ShaderCompiler::Compile(source.value(), path, compileOptions);
        if (compResult.Failed()) {
            std::println("ShaderImporter::LoadFromFile failed to compile shader ({})", results.Name);
            return std::nullopt;
        }

        for (auto& compiled : compResult.Stages) {
            std::vector<uint32_t>* target = nullptr;
            switch (compiled.Stage) {
                case ShaderStage::Vertex:   target = &results.VertexSpirV; break;
                case ShaderStage::Fragment: target = &results.FragmentSpirV; break;
                case ShaderStage::Compute:  target = &results.ComputeSpirV; break;
                default: break;
            }

            if (!target) continue;

            if (!target->empty()) {
                std::println("ShaderImporter::LoadFromFile '{}' has more than one entry point for stage '{}', ignoring '{}'",
                    results.Name, static_cast<uint32_t>(compiled.Stage), compiled.EntryPoint);
                continue;
            }

            *target = std::move(compiled.ByteCode);
        }

        if (!results.IsValid()) {
            std::println("ShaderImporter::LoadFromFile no valid stages found (path={})", path.string());
            return std::nullopt;
        }

        return results;
    }

    std::optional<std::string> ShaderImporter::ReadFile(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            std::println("ShaderCompilation::LoadFromFile File not found '{}'", path.string());
            return std::nullopt;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            std::println("ShaderCompilation::LoadFromFile Could not open file '{}'", path.string());
            return std::nullopt;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

}
