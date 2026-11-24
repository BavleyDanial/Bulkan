#pragma once

#include <cstdint>
#include <functional>

namespace VKRE {

    template<typename Tag>
    struct ResourceHandle {
        static constexpr uint32_t INVALID_GEN = 0;

        uint32_t Index = 0;
        uint32_t Generation = INVALID_GEN;

        bool IsNull() const { return Generation == INVALID_GEN; }
        explicit operator bool() const { return !IsNull(); }
        bool operator==(const ResourceHandle&) const = default;

        static ResourceHandle Null() { return {}; }
    };

    // NOTE: Add any future GPU resources here
    using GPUBufferHandle = ResourceHandle<struct GPUBufferTag>;
    using Texture2DHandle = ResourceHandle<struct Texture2DTag>;
    using TextureCubeHandle = ResourceHandle<struct TextureCubeTag>;

}

namespace std {

    template<typename Tag>
    struct hash<VKRE::ResourceHandle<Tag>> {
        size_t operator()(const VKRE::ResourceHandle<Tag>& handle) const noexcept {
            uint64_t packed = (static_cast<uint64_t>(handle.Generation) << 32) | handle.Index;
            return std::hash<uint64_t>{}(packed);
        }
    };

}
