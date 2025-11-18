#include "pch.h"
#include "AudioLoader.h"
#include "AudioSystem.h"

namespace Framework
{
    bool loadAudio(AudioMetadata& audioProps)
    {
        // Check if AudioSystem is available
        if (!g_AudioSystem)
        {
            std::cerr << "[AudioLoader] AudioSystem not initialized!" << std::endl;
            return false;
        }

        // Use the path field which contains the event path
        FMOD::Studio::EventDescription* eventDesc = g_AudioSystem->LoadEvent(audioProps.path);
        if (!eventDesc)
        {
            std::cerr << "[AudioLoader] Failed to load sound: " << audioProps.name
                << " with path: " << audioProps.path << std::endl;
            return false;
        }

        // Check if event is 3D
        bool is3D = false;
        eventDesc->is3D(&is3D);

        std::cout << "[AudioLoader] Loaded " << audioProps.name
            << " (3D: " << (is3D ? "Yes" : "No") << ")" << std::endl;

        // Get duration in milliseconds
        int lengthMs = 0;
        FMOD_RESULT res = eventDesc->getLength(&lengthMs);
        if (res == FMOD_OK && lengthMs > 0)
        {
            audioProps.duration = static_cast<float>(lengthMs) / 1000.0f;
        }
        else
        {
            audioProps.duration = 0.0f;
        }

        return true;
    }
}