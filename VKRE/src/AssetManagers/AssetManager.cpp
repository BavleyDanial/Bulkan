#include <AssetsManagers/AssetManager.h>

#include <algorithm>
#include <bit>
#include <functional>
#include <span>

namespace VKRE {

    static std::string NormalisePath(const std::filesystem::path& path) {
        return path.lexically_normal().generic_string();
    }

    // TODO: Simple for now: just a hash of the path. Will be replaced by a real asset ID.
    static AssetID MakeAssetID(const std::string& normalisedPath) {
        return std::hash<std::string>{}(normalisedPath);
    }

    static uint32_t GetMipCount(uint32_t width, uint32_t height, bool generateMips) {
        return generateMips ? static_cast<uint32_t>(std::bit_width(std::max(width, height))) : 1;
    }

    AssetManager::AssetManager(const AssetManagerSettings& settings, RHIRenderer& RHIRenderer)
        :mRHIRenderer(RHIRenderer) {
        assert(!sInstance && "AssetManager has been initiated already");
        sInstance = this;

        InitStore<MeshAsset>(settings.MaxMeshes);
        InitStore<ShaderAsset>(settings.MaxShaders);
        InitStore<Texture2DAsset>(settings.MaxTextures2D);
        InitStore<TextureCubeAsset>(settings.MaxTexturesCube);
    }

    AssetManager::~AssetManager() {
        FreePendingUnloads();

        auto checkLeaks = [](auto& store, const char* assetType) {
            uint32_t leaked = 0;
            store.Slots.ForEach([&](auto, auto& slot) {
                if (slot.RefCount > 0)
                    leaked++;
            });

            if (leaked > 0)
                std::println("AssetManager::~AssetManager: {} asset(s) of type {} were leaked during shutdown", leaked, assetType);
        };

        checkLeaks(GetStore<MeshAsset>(), "MeshAsset");
        checkLeaks(GetStore<ShaderAsset>(), "ShaderAsset");
        checkLeaks(GetStore<Texture2DAsset>(), "Texture2DAsset");
        checkLeaks(GetStore<TextureCubeAsset>(), "TextureCubeAsset");

        UnloadAll<MeshAsset>();
        UnloadAll<ShaderAsset>();
        UnloadAll<Texture2DAsset>();
        UnloadAll<TextureCubeAsset>();

        sInstance = nullptr;
    }

    template<typename T>
    void AssetManager::InitStore(uint32_t capacity) {
        AssetStore<T>& store = GetStore<T>();
        store.Slots.Init(capacity);
        store.CachedIDs.reserve(capacity);
    }

    template<typename T>
    ResourceHandle<T> AssetManager::FindAndAddRef(AssetID id) {
        AssetStore<T>& store = GetStore<T>();
        auto it = store.CachedIDs.find(id);
        if (it == store.CachedIDs.end())
            return ResourceHandle<T>::Null();

        AssetSlot<T>* slot = store.Slots.Get(it->second);
        assert(slot && "AssetManager::FindAndAddRef CachedIDs points at a dead slot");
        slot->RefCount++;
        return it->second;
    }

    template<typename T>
    ResourceHandle<T> AssetManager::Insert(AssetID id, T&& data) {
        AssetStore<T>& store = GetStore<T>();
        ResourceHandle<T> handle = store.Slots.Allocate();
        if (handle.IsNull()) {
            std::println("AssetManager::Insert store is full, dropping {}", data.Path);
            DestroyData(data, true);
            return ResourceHandle<T>::Null();
        }

        AssetSlot<T>* slot = store.Slots.Get(handle);
        slot->Data = std::move(data);
        slot->ID = id;
        slot->RefCount = 1;
        store.CachedIDs[id] = handle;
        return handle;
    }

    template<typename T>
    void AssetManager::ProcessUnloads() {
        AssetStore<T>& store = GetStore<T>();

        std::vector<ResourceHandle<T>> pending;
        pending.swap(store.PendingUnload);

        for (ResourceHandle<T> handle : pending) {
            AssetSlot<T>* slot = store.Slots.Get(handle);
            if (!slot || slot->RefCount > 0)
                continue;

            DestroyData(slot->Data, true);
            store.CachedIDs.erase(slot->ID);
            store.Slots.Free(handle);
        }
    }

    template<typename T>
    void AssetManager::UnloadAll() {
        AssetStore<T>& store = GetStore<T>();
        store.Slots.ForEach([&](ResourceHandle<T>, AssetSlot<T>& slot) {
            DestroyData(slot.Data, false);
        });
        store.CachedIDs.clear();
        store.PendingUnload.clear();
    }

    void AssetManager::FreePendingUnloads() {
        ProcessUnloads<MeshAsset>();
        ProcessUnloads<ShaderAsset>();
        ProcessUnloads<Texture2DAsset>();
        ProcessUnloads<TextureCubeAsset>();
    }

    void AssetManager::DestroyData(MeshAsset& mesh, bool releaseDependencies) {
        mRHIRenderer.Destroy(mesh.VertexBuffer);
        mRHIRenderer.Destroy(mesh.IndexBuffer);

        if (releaseDependencies) {
            for (ResourceHandle<Texture2DAsset> texture : mesh.Textures)
                Release(texture);
        }
    }

    void AssetManager::DestroyData(ShaderAsset&, bool) {
        // CPU only, nothing on the GPU.
    }

    void AssetManager::DestroyData(Texture2DAsset& texture, bool) {
        mRHIRenderer.Destroy(texture.Texture);
    }

    void AssetManager::DestroyData(TextureCubeAsset& texture, bool) {
        mRHIRenderer.Destroy(texture.Texture);
    }

    uint32_t AssetManager::GetTextureIndex(ResourceHandle<Texture2DAsset> handle) {
        const Texture2DAsset* texture = Get(handle);
        return texture ? sInstance->mRHIRenderer.GetBindlessIndex(texture->Texture) : 0;
    }

    uint32_t AssetManager::GetTextureIndex(ResourceHandle<TextureCubeAsset> handle) {
        const TextureCubeAsset* texture = Get(handle);
        return texture ? sInstance->mRHIRenderer.GetBindlessIndex(texture->Texture) : 0;
    }

    ResourceHandle<MeshAsset> AssetManager::LoadMesh(const std::filesystem::path& path, const MeshImportOptions& options) {
        assert(sInstance && "AssetManager hasn't been initiated");
        return sInstance->LoadMeshImpl(path, options);
    }

    ResourceHandle<MeshAsset> AssetManager::LoadMeshImpl(const std::filesystem::path& path, const MeshImportOptions& /*options*/) {
        std::string pathStr = NormalisePath(path);
        AssetID id = MakeAssetID(pathStr);
        ResourceHandle<MeshAsset> cached = FindAndAddRef<MeshAsset>(id);
        if (!cached.IsNull())
            return cached;

        std::optional<MeshImportResults> importedMeshResults = MeshImporter::LoadFromFile(path);
        if (!importedMeshResults.has_value()) {
            std::println("AssetManager::LoadMesh failed to import {}", pathStr);
            return ResourceHandle<MeshAsset>::Null();
        }

        MeshImportResults& importedMesh = importedMeshResults.value();

        MeshAsset meshAsset;
        meshAsset.Name = importedMesh.Name;
        meshAsset.Path = pathStr;
        meshAsset.SubMeshes = std::move(importedMesh.SubMeshes);
        meshAsset.Nodes = std::move(importedMesh.Nodes);
        meshAsset.NodeSubMeshIndices = std::move(importedMesh.NodeSubMeshIndices);
        meshAsset.NodeNames = std::move(importedMesh.NodeNames);

        std::filesystem::path meshDir = path.parent_path();
        for (const std::string& texPath : importedMesh.TexturePaths)
            meshAsset.Textures.push_back(LoadTexture2DImpl(meshDir / texPath, { .GenerateMipMaps = true }));

        GPUBufferDesc vbDesc{};
        vbDesc.DebugName = importedMesh.Name + "_VBO";
        vbDesc.Size = static_cast<uint64_t>(importedMesh.Vertices.size()) * sizeof(Vertex);
        vbDesc.Usage = GPUBufferUsage::Vertex | GPUBufferUsage::TransferDst;
        vbDesc.HostVisible = false;

        GPUBufferDesc ibDesc{};
        ibDesc.DebugName = importedMesh.Name + "_IBO";
        ibDesc.Size = static_cast<uint64_t>(importedMesh.Indices.size()) * sizeof(uint32_t);
        ibDesc.Usage = GPUBufferUsage::Index | GPUBufferUsage::TransferDst;
        ibDesc.HostVisible = false;

        meshAsset.VertexBuffer = mRHIRenderer.CreateBuffer(vbDesc);
        meshAsset.IndexBuffer = mRHIRenderer.CreateBuffer(ibDesc);
        if (meshAsset.VertexBuffer.IsNull() || meshAsset.IndexBuffer.IsNull()) {
            std::println("AssetManager::LoadMesh failed to create buffers for {}", pathStr);
            DestroyData(meshAsset, true);
            return ResourceHandle<MeshAsset>::Null();
        }

        mRHIRenderer.UploadBuffer(meshAsset.VertexBuffer, std::as_bytes(std::span(importedMesh.Vertices)));
        mRHIRenderer.UploadBuffer(meshAsset.IndexBuffer, std::as_bytes(std::span(importedMesh.Indices)));
        meshAsset.VertexBufferAddress = mRHIRenderer.GetBufferDeviceAddress(meshAsset.VertexBuffer);

        return Insert<MeshAsset>(id, std::move(meshAsset));
    }

    ResourceHandle<ShaderAsset> AssetManager::LoadShader(const std::filesystem::path& path, const ShaderImportOptions& options) {
        assert(sInstance && "AssetManager hasn't been initiated");
        return sInstance->LoadShaderImpl(path, options);
    }

    ResourceHandle<ShaderAsset> AssetManager::LoadShaderImpl(const std::filesystem::path& path, const ShaderImportOptions& options) {
        std::string pathStr = NormalisePath(path);
        AssetID id = MakeAssetID(pathStr);
        ResourceHandle<ShaderAsset> cached = FindAndAddRef<ShaderAsset>(id);
        if (!cached.IsNull())
            return cached;

        std::optional<ShaderImportResults> importedShaderResults = ShaderImporter::LoadFromFile(path, options);
        if (!importedShaderResults.has_value() || !importedShaderResults->IsValid()) {
            std::println("AssetManager::LoadShader failed to import {}", pathStr);
            return ResourceHandle<ShaderAsset>::Null();
        }

        ShaderImportResults& importedShader = importedShaderResults.value();

        ShaderAsset shaderAsset;
        shaderAsset.Name = importedShader.Name;
        shaderAsset.Path = pathStr;
        shaderAsset.EntryPoint = options.EntryPoint;
        shaderAsset.VertexSpirV = std::move(importedShader.VertexSpirV);
        shaderAsset.FragmentSpirV = std::move(importedShader.FragmentSpirV);
        shaderAsset.ComputeSpirV = std::move(importedShader.ComputeSpirV);

        return Insert<ShaderAsset>(id, std::move(shaderAsset));
    }

    ResourceHandle<Texture2DAsset> AssetManager::LoadTexture2D(const std::filesystem::path& path, const TextureImportOptions& options) {
        assert(sInstance && "AssetManager hasn't been initiated");
        return sInstance->LoadTexture2DImpl(path, options);
    }

    ResourceHandle<Texture2DAsset> AssetManager::LoadTexture2DImpl(const std::filesystem::path& path, const TextureImportOptions& options) {
        std::string pathStr = NormalisePath(path);
        AssetID id = MakeAssetID(pathStr);
        ResourceHandle<Texture2DAsset> cached = FindAndAddRef<Texture2DAsset>(id);
        if (!cached.IsNull())
            return cached;

        std::optional<Texture2DImportResults> importedTextureResults = TextureImporter::LoadTexture2DFromFile(path, options);
        if (!importedTextureResults.has_value()) {
            std::println("AssetManager::LoadTexture2D failed to import {}", pathStr);
            return ResourceHandle<Texture2DAsset>::Null();
        }

        Texture2DImportResults& importedTexture = importedTextureResults.value();

        Texture2DAsset textureAsset;
        textureAsset.Name = importedTexture.Name;
        textureAsset.Path = pathStr;
        textureAsset.Width = importedTexture.Width;
        textureAsset.Height = importedTexture.Height;
        textureAsset.Format = importedTexture.Format;
        textureAsset.MipLevels = GetMipCount(importedTexture.Width, importedTexture.Height, options.GenerateMipMaps);

        TextureDesc desc{};
        desc.DebugName = importedTexture.Name;
        desc.Width = importedTexture.Width;
        desc.Height = importedTexture.Height;
        desc.Format = importedTexture.Format;
        desc.Usage = TextureUsage::Sampled | TextureUsage::TransferSrc | TextureUsage::TransferDst;
        desc.MipLevels = textureAsset.MipLevels;

        textureAsset.Texture = mRHIRenderer.CreateTexture2D(desc);
        if (textureAsset.Texture.IsNull()) {
            std::println("AssetManager::LoadTexture2D failed to create texture for {}", pathStr);
            return ResourceHandle<Texture2DAsset>::Null();
        }

        mRHIRenderer.UploadTexture2D(textureAsset.Texture, importedTexture.Data);
        return Insert<Texture2DAsset>(id, std::move(textureAsset));
    }

    ResourceHandle<TextureCubeAsset> AssetManager::LoadTextureCube(const std::filesystem::path& path, const TextureImportOptions& options) {
        assert(sInstance && "AssetManager hasn't been initiated");
        return sInstance->LoadTextureCubeImpl(path, options);
    }

    ResourceHandle<TextureCubeAsset> AssetManager::LoadTextureCube(const std::array<std::filesystem::path, 6>& paths, const TextureImportOptions& options) {
        assert(sInstance && "AssetManager hasn't been initiated");
        return sInstance->LoadTextureCubeImpl(paths, options);
    }

    ResourceHandle<TextureCubeAsset> AssetManager::LoadTextureCubeImpl(const std::filesystem::path& path, const TextureImportOptions& options) {
        std::string pathStr = NormalisePath(path);
        AssetID id = MakeAssetID(pathStr);
        ResourceHandle<TextureCubeAsset> cached = FindAndAddRef<TextureCubeAsset>(id);
        if (!cached.IsNull())
            return cached;

        std::optional<TextureCubeImportResults> importedTextureResults = TextureImporter::LoadTextureCubeFromHorizontalCross(path, options);
        if (!importedTextureResults.has_value()) {
            std::println("AssetManager::LoadTextureCube failed to import {}", pathStr);
            return ResourceHandle<TextureCubeAsset>::Null();
        }

        return FillTextureCubeData(id, importedTextureResults.value(), pathStr, options);
    }

    ResourceHandle<TextureCubeAsset> AssetManager::LoadTextureCubeImpl(const std::array<std::filesystem::path, 6>& paths, const TextureImportOptions& options) {
        // The six paths together are the asset's "path".
        std::string pathStr;
        for (const std::filesystem::path& path : paths)
            pathStr += NormalisePath(path) + "|";

        AssetID id = MakeAssetID(pathStr);
        ResourceHandle<TextureCubeAsset> cached = FindAndAddRef<TextureCubeAsset>(id);
        if (!cached.IsNull())
            return cached;

        std::optional<TextureCubeImportResults> importedTextureResults = TextureImporter::LoadTextureCubeFromFiles(paths, options);
        if (!importedTextureResults.has_value()) {
            std::println("AssetManager::LoadTextureCube failed to import {}", pathStr);
            return ResourceHandle<TextureCubeAsset>::Null();
        }

        return FillTextureCubeData(id, importedTextureResults.value(), pathStr, options);
    }

    ResourceHandle<TextureCubeAsset> AssetManager::FillTextureCubeData(AssetID id, const TextureCubeImportResults& importedTexture,
                                                                       const std::string& pathStr, const TextureImportOptions& options) {
        // TODO: The uploader can't build cube mip chains yet, so GenerateMipMaps is ignored for
        // cubes. Without this, mips 1+ would be sampled while their contents are undefined.
        if (options.GenerateMipMaps)
            std::println("AssetManager::LoadTextureCube mipmaps aren't supported for cubemaps yet, using 1 mip ({})", pathStr);

        TextureCubeAsset textureAsset;
        textureAsset.Name = importedTexture.Name;
        textureAsset.Path = pathStr;
        textureAsset.Width = importedTexture.Width;
        textureAsset.Height = importedTexture.Height;
        textureAsset.Format = importedTexture.Format;
        textureAsset.MipLevels = 1;

        TextureDesc desc{};
        desc.DebugName = importedTexture.Name;
        desc.Width = importedTexture.Width;
        desc.Height = importedTexture.Height;
        desc.Format = importedTexture.Format;
        desc.Usage = TextureUsage::Sampled | TextureUsage::TransferSrc | TextureUsage::TransferDst;
        desc.MipLevels = textureAsset.MipLevels;

        textureAsset.Texture = mRHIRenderer.CreateTextureCube(desc);
        if (textureAsset.Texture.IsNull()) {
            std::println("AssetManager::LoadTextureCube failed to create texture for {}", pathStr);
            return ResourceHandle<TextureCubeAsset>::Null();
        }

        CubeFaces faces;
        for (size_t i = 0; i < faces.size(); i++)
            faces[i] = importedTexture.Data[i];

        mRHIRenderer.UploadTextureCube(textureAsset.Texture, faces);
        return Insert<TextureCubeAsset>(id, std::move(textureAsset));
    }

}
