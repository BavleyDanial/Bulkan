#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace VKRE {

    struct ShaderAsset {
        std::string Name;
        std::string Path;
        std::string EntryPoint = "main";

        std::vector<uint32_t> VertexSpirV;
        std::vector<uint32_t> FragmentSpirV;
        std::vector<uint32_t> ComputeSpirV;
    };

}
