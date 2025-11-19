#include "pch.h" // Keep this if your project uses PCH
#include "EventManager.h"
#include <iostream>

namespace Framework {

    void EventManager::Subscribe(const std::string& eventName, EventCallback callback) {
        mListeners[eventName].push_back(callback);
    }

    void EventManager::Fire(const std::string& eventName, void* payload) {
        if (mListeners.find(eventName) != mListeners.end()) {
            for (auto& callback : mListeners[eventName]) {
                callback(payload);
            }
        }
    }

    void EventManager::Reset() {
        mListeners.clear();
    }
}