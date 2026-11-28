#include <ResourceManager/ShaderCompiler.h>

#include <ResourceManager/Resources.h>

#include <shaderc/shaderc.hpp>

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <string>
#include <print>

namespace VKRE {

    static Slang::ComPtr<slang::IGlobalSession> sSlangSession;

    static ShaderStage ToShaderStage(SlangStage stage) {
        switch (stage) {
            case SLANG_STAGE_VERTEX:    return ShaderStage::Vertex;
            case SLANG_STAGE_FRAGMENT:  return ShaderStage::Fragment;
            case SLANG_STAGE_COMPUTE:   return ShaderStage::Compute;
            default:                    return ShaderStage::None;
        }
    }

    static std::string BlobToString(slang::IBlob* blob) {
        if (!blob || blob->getBufferSize() == 0) return {};
        return std::string(static_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize());
    }

    void ShaderCompiler::Init() {
        if (sSlangSession)
            return;

        if (SLANG_FAILED(slang::createGlobalSession(sSlangSession.writeRef()))) {
            std::println("ShaderCompiler::Init slang failed to initialise global session");
            abort();
        }
    }

    void ShaderCompiler::Shutdown() {
        if (!sSlangSession)
            return;

        slang::shutdown();
        sSlangSession = nullptr;
    }

    // TODO: this is AI slop that I wanted to temporarily just test out slang, remove/edit code once ready
    ShaderCompileResult ShaderCompiler::Compile(const std::string& source, const std::filesystem::path& path, const ShaderCompileOptions& options) {
        const std::string debugName = path.filename().string();

        if (!sSlangSession) {
            std::println("ShaderCompiler::Compile called before ShaderCompiler::Init ('{}')", debugName);
            return { {}, "ShaderCompiler not initialised" };
        }

        // ---- Compiler options -------------------------------------------------------------------
        std::vector<slang::CompilerOptionEntry> compilerOptions;
        auto addIntOption = [&](slang::CompilerOptionName name, int32_t value) {
            slang::CompilerOptionEntry entry{};
            entry.name = name;
            entry.value.kind = slang::CompilerOptionValueKind::Int;
            entry.value.intValue0 = value;
            compilerOptions.push_back(entry);
        };

        // Direct SPIR-V emission is already Slang's default; set explicitly so a default change
        // can't silently route through GLSL.
        addIntOption(slang::CompilerOptionName::EmitSpirvMethod, SLANG_EMIT_SPIRV_DIRECTLY);
        addIntOption(slang::CompilerOptionName::Optimization, options.Optimise ? SLANG_OPTIMIZATION_LEVEL_HIGH : SLANG_OPTIMIZATION_LEVEL_NONE);
        if (options.GenerateDebugInfo)
            addIntOption(slang::CompilerOptionName::DebugInformation, SLANG_DEBUG_INFO_LEVEL_STANDARD);

        // ---- Target -------------------------------------------------------------------------------
        slang::TargetDesc target{};
        target.format = SLANG_SPIRV;
        target.profile = sSlangSession->findProfile("spirv_1_6"); // TODO: Make this auto sync with VulkanContext's version

        // ---- Search paths: the file's own folder first, then the caller's ----------------------------
        std::vector<std::string> searchPathStorage;
        searchPathStorage.push_back(path.parent_path().empty() ? std::string(".") : path.parent_path().generic_string());
        for (const auto& includePath : options.IncludePaths)
            searchPathStorage.push_back(includePath.generic_string());

        std::vector<const char*> searchPaths;
        for (const auto& searchPath : searchPathStorage)
            searchPaths.push_back(searchPath.c_str());

        // ---- Defines: "NAME" or "NAME=VALUE" --------------------------------------------------------
        std::vector<std::pair<std::string, std::string>> defineStorage;
        for (const auto& define : options.Defines) {
            auto pos = define.find('=');
            if (pos != std::string::npos) {
                defineStorage.emplace_back(define.substr(0, pos), define.substr(pos + 1));
            } else {
                defineStorage.emplace_back(define, "1");
            }
        }

        std::vector<slang::PreprocessorMacroDesc> macros;
        for (const auto& [name, value] : defineStorage)
            macros.push_back({ name.c_str(), value.c_str() });

        // ---- Session ------------------------------------------------------------------------------
        // A fresh session per compile: a session caches every module it imports and has no way to
        // drop one, so a shared session would keep serving stale imports after an edit. Its settings
        // (defines, search paths) are also fixed at creation, while options vary per call.
        slang::SessionDesc sessionDesc{};
        sessionDesc.targets = &target;
        sessionDesc.targetCount = 1;
        // GLM is column-major. With this, mul(M, v) in Slang == M * v in GLSL, no transposes.
        // Slang's own default is ROW_MAJOR, so this line is load-bearing.
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
        sessionDesc.searchPaths = searchPaths.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());
        sessionDesc.preprocessorMacros = macros.empty() ? nullptr : macros.data();
        sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());
        sessionDesc.compilerOptionEntries = compilerOptions.data();
        sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(compilerOptions.size());

        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(sSlangSession->createSession(sessionDesc, session.writeRef()))) {
            std::println("ShaderCompiler::Compile Failed to create a Slang session for '{}'", debugName);
            return { {}, "Failed to create Slang session" };
        }

        // Collects warnings and errors from every step below.
        std::string diagnosticsText;
        auto collect = [&](slang::IBlob* blob) { diagnosticsText += BlobToString(blob); };

        auto fail = [&](const char* step) -> ShaderCompileResult {
            std::println("ShaderCompiler::Compile Failed to {} '{}'\n{}", step, debugName, diagnosticsText);
            return { {}, diagnosticsText };
        };

        // ---- Parse + check ----------------------------------------------------------------------------
        // The module is owned by the session, do not release it.
        const std::string moduleName = path.stem().string();
        const std::string modulePath = path.generic_string();

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* module = session->loadModuleFromSourceString(moduleName.c_str(), modulePath.c_str(), source.c_str(), diagnostics.writeRef());
        collect(diagnostics);
        if (!module)
            return fail("compile");

        // ---- Every [shader("...")] entry point --------------------------------------------------------
        const SlangInt32 entryPointCount = module->getDefinedEntryPointCount();
        if (entryPointCount == 0) {
            diagnosticsText += "No [shader(\"...\")] entry points found\n";
            return fail("find entry points in");
        }

        std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints(static_cast<size_t>(entryPointCount));
        std::vector<slang::IComponentType*> components;
        components.push_back(module);
        for (SlangInt32 i = 0; i < entryPointCount; ++i) {
            module->getDefinedEntryPoint(i, entryPoints[i].writeRef());
            components.push_back(entryPoints[i]);
        }

        // ---- Link ---------------------------------------------------------------------------------
        Slang::ComPtr<slang::IComponentType> program;
        diagnostics = nullptr;
        if (SLANG_FAILED(session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), program.writeRef(), diagnostics.writeRef()))) {
            collect(diagnostics);
            return fail("compose");
        }
        collect(diagnostics);

        Slang::ComPtr<slang::IComponentType> linked;
        diagnostics = nullptr;
        if (SLANG_FAILED(program->link(linked.writeRef(), diagnostics.writeRef()))) {
            collect(diagnostics);
            return fail("link");
        }
        collect(diagnostics);

        // ---- SPIR-V per entry point ------------------------------------------------------------------
        // Layout entry point i is the i-th entry point added to the composite above. Slang names each
        // SPIR-V entry point "main" whatever its source name, which is what the pipelines expect.
        slang::ProgramLayout* layout = linked->getLayout();

        ShaderCompileResult result{};
        for (SlangInt32 i = 0; i < entryPointCount; ++i) {
            slang::EntryPointReflection* reflection = layout->getEntryPointByIndex(static_cast<SlangUInt>(i));
            const ShaderStage stage = ToShaderStage(reflection->getStage());
            const char* entryName = reflection->getName();

            if (stage == ShaderStage::None) {
                std::println("ShaderCompiler::Compile '{}': entry point '{}' has a stage the engine doesn't support yet, skipped", debugName, entryName);
                continue;
            }

            Slang::ComPtr<slang::IBlob> code;
            diagnostics = nullptr;
            if (SLANG_FAILED(linked->getEntryPointCode(i, 0, code.writeRef(), diagnostics.writeRef())) || !code) {
                collect(diagnostics);
                std::println("ShaderCompiler::Compile Failed to generate SPIR-V for '{}' in '{}'\n{}", entryName, debugName, diagnosticsText);
                result.ErrorMsg += diagnosticsText;
                continue;
            }
            collect(diagnostics);

            ShaderCompiledStage& compiled = result.Stages.emplace_back();
            compiled.Stage = stage;
            compiled.EntryPoint = entryName;
            compiled.ByteCode.resize(code->getBufferSize() / sizeof(uint32_t));
            std::memcpy(compiled.ByteCode.data(), code->getBufferPointer(), compiled.ByteCode.size() * sizeof(uint32_t));
        }

        if (result.Succeeded() && !diagnosticsText.empty())
            std::println("ShaderCompiler::Compile warnings in '{}'\n{}", debugName, diagnosticsText);

        return result;
    }
}
