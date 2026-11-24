#include <ResourceManager/ShaderCompiler.h>

#include <ResourceManager/Resources.h>

#include <shaderc/shaderc.hpp>

#include <string>
#include <print>

namespace VKRE {

    static shaderc_shader_kind ToShadercKind(ShaderStage stage) {
        switch (stage) {
            case ShaderStage::Vertex:           return shaderc_glsl_vertex_shader;
            case ShaderStage::Fragment:         return shaderc_glsl_fragment_shader;
            case ShaderStage::Compute:          return shaderc_glsl_compute_shader;
            case ShaderStage::None:             return shaderc_glsl_infer_from_source;
        }
    }

    ShaderCompileResult ShaderCompiler::Compile(const std::string& source, ShaderStage stage, const std::string& debugName, const ShaderCompileOptions& options) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions shadercOptions;

        shadercOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3); // TODO: Make this auto sync with VulkanContext's version
        shadercOptions.SetTargetSpirv(shaderc_spirv_version_1_6);
        shadercOptions.SetSourceLanguage(shaderc_source_language_glsl); // TODO: make this customizable
        shadercOptions.SetOptimizationLevel(options.Optimise ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);

        if (options.GenerateDebugInfo)
            shadercOptions.SetGenerateDebugInfo();

        for (const auto& define : options.Defines) {
            auto pos = define.find('=');
            if (pos != std::string::npos) {
                shadercOptions.AddMacroDefinition(define.substr(0, pos), define.substr(pos + 1));
            } else {
                shadercOptions.AddMacroDefinition(define);
            }
        }

        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, ToShadercKind(stage), debugName.c_str(), shadercOptions);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::println("ShaderCompiler::CompileStage Failed to compile stage '{}' in '{}'\n{}", static_cast<uint32_t>(stage), debugName, result.GetErrorMessage());
            return { {}, result.GetErrorMessage() };
        }

        if (result.GetNumWarnings() > 0)
            std::println("ShaderCompiler::CompileStage warnings in stage '{}' in '{}' compiled with {} warning(s)", static_cast<uint32_t>(stage), debugName, result.GetErrorMessage());

        return { std::vector<uint32_t>(result.cbegin(), result.cend()), {} };
    }

}

