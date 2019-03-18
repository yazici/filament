/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMENT_DRIVER_VULKANDISPOSER_H
#define TNT_FILAMENT_DRIVER_VULKANDISPOSER_H

#include "../DriverBase.h"

#include <tsl/robin_map.h>
#include <tsl/robin_set.h>

#include <functional>
#include <memory>
#include <vector>

namespace filament {
namespace driver {

// Because vkDestroy* calls are synchronous, resources in currently-executing command buffers
// must have their destruction deferred, so we provide a simple ref-counting mechanism for this.

template<typename KEY>
class Disposer {
    struct Disposable {
        size_t refcount;
        std::function<void()> destructor;
    };

    tsl::robin_map<KEY, std::unique_ptr<Disposable>> mDisposables;
    std::vector<std::unique_ptr<Disposable>> mGraveyard;

public:
    using Set = tsl::robin_set<KEY>;

    void createDisposable(KEY resource, std::function<void()> destructor) noexcept {
        mDisposables[resource].reset(new Disposable { 1, destructor });
    }

    void addReference(KEY resource) noexcept {
        ++mDisposables[resource]->refcount;
    }

    void removeReference(KEY resource) noexcept {
        if (--mDisposables[resource]->refcount == 0) {
            mGraveyard.emplace_back(std::move(mDisposables[resource]));
            mDisposables.erase(resource);
        }
    }
    void acquire(KEY resource, Set& resources) noexcept {
        auto iter = resources.find(resource);
        if (iter == resources.end()) {
            resources.insert(resource);
            addReference(resource);
        }
    }

    void release(Set& resources) {
        for (auto resource : resources) {
            removeReference(resource);
        }
        resources.clear();
    }

    void gc() noexcept {
        for (auto& ptr : mGraveyard) {
            ptr->destructor();
        }
        mGraveyard.clear();
    }
};

using VulkanDisposer = Disposer<const HwBase*>;

} // namespace filament
} // namespace driver

#endif // TNT_FILAMENT_DRIVER_VULKANDISPOSER_H
