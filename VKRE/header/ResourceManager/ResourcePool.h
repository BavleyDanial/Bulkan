#pragma once

#include "ResourceHandles.h"

#include <vector>
#include <cassert>
#include <memory>
#include <concepts>

namespace VKRE {

    template<typename Tag, typename T>
    class ResourcePool {
    public:
        void Init(uint32_t capacity) {
            assert(mCapacity == 0 && "ResourcePool::Init called more than once");
            assert(capacity > 0 && "ResourcePool::Init capacity must be > 0");
            mCapacity = capacity;
            mItems = std::make_unique<T[]>(capacity);
            mGenerations = std::make_unique<uint32_t[]>(capacity);
            mFreeList.reserve(capacity);
            for (uint32_t i = 0; i < capacity; i++)
                mFreeList.push_back((capacity - i) - 1);
        }

        ResourceHandle<Tag> Allocate() {
            if (mFreeList.empty())
                return ResourceHandle<Tag>::Null();

            uint32_t index = mFreeList.back();
            mFreeList.pop_back();
            uint32_t gen = ++mGenerations[index]; // even (free) -> odd (alive)
            assert((gen & 1u) && "ResourcePool::Allocate slot is free after allocation, possible double-recycle bug");
            mLiveCount++;
            return { index, gen };
        }

        bool Retire(ResourceHandle<Tag> handle) {
            if (!IsValid(handle))
                return false;
            mGenerations[handle.Index]++; // odd (alive) -> even (free)
            mLiveCount--;
            return true;
        }

        void Recycle(uint32_t index) {
            assert(index < mCapacity && (mGenerations[index] & 1u) == 0 && "ResourcePool::Recycle was called on a live slot");
            mItems[index] = T{};
            // A generation of 0 here means it wrapped around: retire the slot for good.
            if (mGenerations[index] != 0)
                mFreeList.push_back(index);
        }

        void Free(ResourceHandle<Tag> handle) {
            if (Retire(handle))
                Recycle(handle.Index);
        }

        bool IsValid(ResourceHandle<Tag> handle) const {
            if (handle.IsNull() || handle.Index >= mCapacity) return false;
            return mGenerations[handle.Index] == handle.Generation;
        }

        T* Get(ResourceHandle<Tag> handle) {
            return IsValid(handle) ? &mItems[handle.Index] : nullptr;
        }

        const T* Get(ResourceHandle<Tag> handle) const {
            return IsValid(handle) ? &mItems[handle.Index] : nullptr;
        }

        T& At(uint32_t index) {
            return mItems[index];
        }

        const T& At(uint32_t index) const {
            return mItems[index];
        }

        template<typename Fn>
        requires std::invocable<Fn, ResourceHandle<Tag>, T&>
        void ForEach(Fn&& fn) {
            for (uint32_t i = 0; i < mCapacity; i++) {
                if (mGenerations[i] & 1u)
                    fn(ResourceHandle<Tag>{ i, mGenerations[i] }, mItems[i]);
            }
        }

        uint32_t GetLiveCount() const { return mLiveCount; }
        uint32_t GetCapacity() const { return mCapacity; }
        uint32_t GetFreeCount() const { return static_cast<uint32_t>(mFreeList.size()); }

    private:
        std::unique_ptr<T[]> mItems;
        std::unique_ptr<uint32_t[]> mGenerations; // odd (alive), even (free)
        std::vector<uint32_t> mFreeList;

        uint32_t mCapacity = 0;
        uint32_t mLiveCount = 0;
    };

}
