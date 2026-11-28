#pragma once

#include "Resources.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace VKRE {

    struct ShaderCompileOptions {
        bool Optimise = false;
        bool GenerateDebugInfo = true;

        std::vector<std::string> Defines;
        std::vector<std::filesystem::path> IncludePaths;
    };

    struct ShaderCompiledStage {
        ShaderStage Stage = ShaderStage::None;
        std::vector<uint32_t> ByteCode;
        std::string EntryPoint;
    };

    struct ShaderCompileResult {
        std::vector<ShaderCompiledStage> Stages;
        std::string ErrorMsg;

        bool Succeeded() const { return !Stages.empty(); }
        bool Failed() const { return Stages.empty(); }
    };

    class ShaderCompiler {
    public:
        static void Init();
        static void Shutdown();

        static ShaderCompileResult Compile(const std::string& source,
            const std::filesystem::path& path,
            const ShaderCompileOptions& options = {}
        );
    };

}
