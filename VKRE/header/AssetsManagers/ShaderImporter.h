#pragma once

#include <ResourceManager/Resources.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VKRE {

    struct ShaderImportOptions {
        bool Optimize = false;
        bool GenerateDebugInfo = true;
        std::string EntryPoint = "main";
    };

    struct ShaderImportResults {
        std::string Name;

        std::vector<uint32_t> VertexSpirV;
        std::vector<uint32_t> FragmentSpirV;
        std::vector<uint32_t> ComputeSpirV;

        bool IsValid() const { return !VertexSpirV.empty() || !FragmentSpirV.empty() || !ComputeSpirV.empty(); }
    };

    class ShaderImporter {
    public:
        static std::optional<ShaderImportResults> LoadFromFile(
            const std::filesystem::path& path,
            const ShaderImportOptions& options = {}
        );

    private:
        static std::optional<std::string> ReadFile(const std::filesystem::path& path);
    };

}
