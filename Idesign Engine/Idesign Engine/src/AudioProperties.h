///////////////////////////////////////////////////////////////////////////////
///
///	@file  : AudioProperties.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _AUDIO_PROPERTIES_H_
#define _AUDIO_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    struct AudioProperties
    {
        std::string guid;
        std::string customName;
        std::string filePath;
        std::string mode;
        std::string soundType;

        //  Runtime properties
        float volume = 1.0f;           // Range: 0.0 to 1.0
        bool loop = false;             // Loop playback
        bool isPlaying = false;        // Playback status
        float duration = 0.0f;         // Duration in seconds

        // Optional: internal ID or handle for audio system
        int audioID = -1;
    };

    inline constexpr std::array g_AudioPropertiesPropsArray =
    {
        PROP_STRING(guid, AudioProperties),
        PROP_STRING(customName, AudioProperties),
        PROP_STRING(filePath, AudioProperties),
        PROP_STRING(mode, AudioProperties),
        PROP_STRING(soundType, AudioProperties),
    };

    inline constexpr prop_list g_AudioPropertiesProps{ "AudioProperties", g_AudioPropertiesPropsArray };
    inline global_registration g_AudioPropertiesReg(&g_AudioPropertiesProps);
}
#endif // !_AUDIO_PROPERTIES_H_