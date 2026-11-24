#pragma once

#include <Renderer/RHIRenderer.h>

#include <ResourceManager/ResourceHandles.h>
#include <ResourceManager/ResourcePool.h>

#include <AssetsManagers/MeshImporter.h>
#include <AssetsManagers/ShaderImporter.h>
#include <AssetsManagers/TextureImporter.h>

#include <Assets/Mesh.h>
#include <Assets/Shader.h>
#include <Assets/Texture.h>
//#include <Assets/Materials.h>

#include <array>
#include <cassert>
#include <filesystem>
#include <print>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace VKRE {

    // TODO: Make this a real ID later on
    using AssetID = uint64_t;

    // TODO: Move this to somewhere that can be edited by the user or an editor or something
    struct AssetManagerSettings {
        uint32_t MaxMeshes = 1024;
        uint32_t MaxShaders = 512;
        uint32_t MaxTextures2D = 8192;
        uint32_t MaxTexturesCube = 512;
    };

    template<typename T>
    struct AssetSlot {
        T Data{};
        AssetID ID = 0;
        uint32_t RefCount = 0; // TODO: Change to atomic if in the future we need that
    };

    template<typename T>
    struct AssetStore {
        using AssetType = T;
        ResourcePool<T, AssetSlot<T>> Slots;
        std::unordered_map<AssetID, ResourceHandle<T>> CachedIDs;
        std::vector<ResourceHandle<T>> PendingUnload;
    };

    class AssetManager {
    public:
        AssetManager(const AssetManagerSettings& settings, RHIRenderer& RHIRenderer);
        ~AssetManager();

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        static ResourceHandle<MeshAsset> LoadMesh(const std::filesystem::path& path, const MeshImportOptions& options = {});
        static ResourceHandle<ShaderAsset> LoadShader(const std::filesystem::path& path, const ShaderImportOptions& options = {});
        static ResourceHandle<Texture2DAsset> LoadTexture2D(const std::filesystem::path& path, const TextureImportOptions& options = {});
        static ResourceHandle<TextureCubeAsset> LoadTextureCube(const std::filesystem::path& path, const TextureImportOptions& options = {});
        static ResourceHandle<TextureCubeAsset> LoadTextureCube(const std::array<std::filesystem::path, 6>& paths, const TextureImportOptions& options = {});

        template<typename T> static void AddRef(ResourceHandle<T> handle);
        template<typename T> static void Release(ResourceHandle<T> handle);
        template<typename T> static const T* Get(ResourceHandle<T> handle);

        static uint32_t GetTextureIndex(ResourceHandle<Texture2DAsset> handle);
        static uint32_t GetTextureIndex(ResourceHandle<TextureCubeAsset> handle);

    private:
        ResourceHandle<MeshAsset> LoadMeshImpl(const std::filesystem::path& path, const MeshImportOptions& options);
        ResourceHandle<ShaderAsset> LoadShaderImpl(const std::filesystem::path& path, const ShaderImportOptions& options);
        ResourceHandle<Texture2DAsset> LoadTexture2DImpl(const std::filesystem::path& path, const TextureImportOptions& options);
        ResourceHandle<TextureCubeAsset> LoadTextureCubeImpl(const std::filesystem::path& path, const TextureImportOptions& options);
        ResourceHandle<TextureCubeAsset> LoadTextureCubeImpl(const std::array<std::filesystem::path, 6>& paths, const TextureImportOptions& options);
        ResourceHandle<TextureCubeAsset> FillTextureCubeData(AssetID id, const TextureCubeImportResults& importedTexture, const std::string& pathStr, const TextureImportOptions& options);

        template<typename T> AssetStore<T>& GetStore();
        template<typename T> void InitStore(uint32_t capacity);

        template<typename T> ResourceHandle<T> FindAndAddRef(AssetID id);
        template<typename T> ResourceHandle<T> Insert(AssetID id, T&& data);
        template<typename T> void ProcessUnloads();
        template<typename T> void UnloadAll();

        void DestroyData(MeshAsset& mesh, bool releaseDependencies);
        void DestroyData(ShaderAsset& shader, bool releaseDependencies);
        void DestroyData(Texture2DAsset& texture, bool releaseDependencies);
        void DestroyData(TextureCubeAsset& texture, bool releaseDependencies);

        void FreePendingUnloads();

    private:
        std::tuple<
            AssetStore<MeshAsset>,
            AssetStore<ShaderAsset>,
            AssetStore<Texture2DAsset>,
            AssetStore<TextureCubeAsset>
        > mStores;

        inline static AssetManager* sInstance = nullptr;
        RHIRenderer& mRHIRenderer;

        friend class Application;
    };

    template<typename T, typename Tuple>
    inline constexpr bool TupleHasType = false;

    template<typename T, typename... Ts>
    inline constexpr bool TupleHasType<T, std::tuple<Ts...>> = (std::is_same_v<T, Ts> || ...);

    template<typename T>
    AssetStore<T>& AssetManager::GetStore() {
        static_assert(TupleHasType<AssetStore<T>, decltype(mStores)>, "AssetManager::GetStore no store exists for this asset type");
        return std::get<AssetStore<T>>(mStores);
    }

    template<typename T>
    void AssetManager::AddRef(ResourceHandle<T> handle) {
        assert(sInstance && "AssetManager::AddRef AssetManager hasn't been initialised");
        AssetSlot<T>* slot = sInstance->GetStore<T>().Slots.Get(handle);
        if (!slot) {
            if (handle) std::println("AssetManager::AddRef stale handle (index={})", handle.Index);
            return;
        }
        slot->RefCount++;
    }

    template<typename T>
    void AssetManager::Release(ResourceHandle<T> handle) {
        assert(sInstance && "AssetManager::Release AssetManager hasn't been initialised");
        AssetStore<T>& store = sInstance->GetStore<T>();
        AssetSlot<T>* slot = store.Slots.Get(handle);
        if (!slot) {
            if (handle) std::println("AssetManager::Release stale handle (index={})", handle.Index);
            return;
        }

        assert(slot->RefCount > 0 && "AssetManager::Release was called more times than Load/AddRef");
        if (slot->RefCount > 0 && --slot->RefCount == 0)
            store.PendingUnload.push_back(handle);
    }

    template<typename T>
    const T* AssetManager::Get(ResourceHandle<T> handle) {
        assert(sInstance && "AssetManager::Get AssetManager hasn't been initialised");
        const AssetSlot<T>* slot = sInstance->GetStore<T>().Slots.Get(handle);
        return slot ? &slot->Data : nullptr;
    }

}
