#pragma once

#include <ResourceManager/ResourceHandles.h>
#include <ResourceManager/Resources.h>
#include <string>

namespace VKRE {

    struct Texture2DAsset {
        std::string Name;
        std::string Path;

        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t MipLevels = 1;
        TextureFormat Format = TextureFormat::None;

        Texture2DHandle Texture;
    };

    struct TextureCubeAsset {
        std::string Name;
        std::string Path;

        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t MipLevels = 1;
        TextureFormat Format = TextureFormat::None;

        TextureCubeHandle Texture;
    };

}
