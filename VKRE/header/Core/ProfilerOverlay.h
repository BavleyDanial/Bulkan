#pragma once

#include <Core/Layer.h>

namespace VKRE {

    class ProfilerOverlay : public Layer {
    public:
        ProfilerOverlay()
            :Layer("Profiler Overlay") {}

        void OnUIRender() override;
    };

}
