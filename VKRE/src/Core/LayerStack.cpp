#include <Core/LayerStack.h>

#include <algorithm>

namespace VKRE {

    LayersStack::~LayersStack() {
        Clear();
    }

    void LayersStack::Clear() {
        for (auto it = mLayers.rbegin(); it != mLayers.rend(); it++)
            (*it)->OnDetach();
        mLayers.clear();
        mLayerInsertIndex = 0;
    }

    void LayersStack::PushLayer(std::unique_ptr<Layer> layer) {
        layer->OnAttach();
        mLayers.emplace(mLayers.begin() + mLayerInsertIndex, std::move(layer));
        mLayerInsertIndex++;
    }

    void LayersStack::PushOverlay(std::unique_ptr<Layer> overlay) {
        overlay->OnAttach();
        mLayers.emplace_back(std::move(overlay));
    }

    std::unique_ptr<Layer> LayersStack::PopLayer(Layer* layer) {
        auto end = mLayers.begin() + mLayerInsertIndex;
        auto it = std::find_if(mLayers.begin(), end, [layer](const auto& l) {
            return l.get() == layer;
        });

        if (it == end)
            return nullptr;

        (*it)->OnDetach();
        auto popped = std::move(*it);
        mLayers.erase(it);
        mLayerInsertIndex--;
        return popped;
    }

    std::unique_ptr<Layer> LayersStack::PopOverlay(Layer* overlay) {
        auto begin = mLayers.begin() + mLayerInsertIndex;
        auto it = std::find_if(begin, mLayers.end(), [overlay](const auto& l) {
            return l.get() == overlay;
        });

        if (it == mLayers.end())
            return nullptr;

        (*it)->OnDetach();
        auto popped = std::move(*it);
        mLayers.erase(it);
        return popped;
    }
}
