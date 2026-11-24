#pragma once

#include "Layer.h"

#include <vector>
#include <memory>
#include <cstdint>

namespace VKRE {

    class LayersStack {
    public:
        LayersStack() = default;
        ~LayersStack();

        void Clear();

        void PushLayer(std::unique_ptr<Layer> layer);
        void PushOverlay(std::unique_ptr<Layer> overlay);
        std::unique_ptr<Layer> PopLayer(Layer* layer);
        std::unique_ptr<Layer> PopOverlay(Layer* overlay);

        std::vector<std::unique_ptr<Layer>>::iterator begin() { return mLayers.begin(); }
        std::vector<std::unique_ptr<Layer>>::iterator end() { return mLayers.end(); }
        std::vector<std::unique_ptr<Layer>>::reverse_iterator rbegin() { return mLayers.rbegin(); }
        std::vector<std::unique_ptr<Layer>>::reverse_iterator rend() { return mLayers.rend(); }

        std::vector<std::unique_ptr<Layer>>::const_iterator begin() const { return mLayers.begin(); }
        std::vector<std::unique_ptr<Layer>>::const_iterator end() const { return mLayers.end(); }
        std::vector<std::unique_ptr<Layer>>::const_reverse_iterator rbegin() const { return mLayers.rbegin(); }
        std::vector<std::unique_ptr<Layer>>::const_reverse_iterator rend() const { return mLayers.rend(); }

    private:
        std::vector<std::unique_ptr<Layer>> mLayers;
        uint32_t mLayerInsertIndex = 0;
    };

}
