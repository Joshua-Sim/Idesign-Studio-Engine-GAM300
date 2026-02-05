///////////////////////////////////////////////////////////////////////////////
///
/// @file ButtonSystem.cpp
/// 
/// @brief Implementation of the ButtonSystem class, which manages interactive 
///        buttons within the game engine. This class provides functionality for the buttons
///  
/// @authors Joshua Sim Yue Chen
/// @copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#include "pch.h"
#include <iostream>
#include "Coordinator.h"
#include "ButtonSystem.h"
#include "TextureProperties.h"
#include "EngineState.h"
#include "TextProperties.h"

extern Framework::Coordinator ecsInterface;
namespace Framework {

    ButtonSystem::ButtonSystem() : m_inputHandler(nullptr) {}
    ButtonSystem::ButtonSystem(InputHandler* inputHandler) 
                            : m_inputHandler(inputHandler), 
                              m_isDraggingSlider(false), 
                              m_activeKnob(0) {}

    void ButtonSystem::SetInputHandler(InputHandler* handler)
    {
        m_inputHandler = handler;
    }

    void ButtonSystem::Initialize() {
        // Initialize buttons if needed
    }

    void ButtonSystem::Update(float deltaTime)
    {
        (void)deltaTime;
        for (auto entity : mEntities)
        {
            if (!ecsInterface.HasComponent<ButtonComponent>(entity)     ||
                !ecsInterface.HasComponent<TransformComponent>(entity)  ||
                !ecsInterface.HasComponent<TextureProperties>(entity))
                continue;

            auto& button = ecsInterface.GetComponent<ButtonComponent>(entity);
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entity);

            // interpret Transform as �center + full size� in design space
            // button.position = glm::vec2(tr.position.x - 0.5f * tr.scale.x,tr.position.y - 0.5f * tr.scale.y);
            button.position = glm::vec2(tr.position.x, tr.position.y);
            button.size = glm::vec2(tr.scale.x, tr.scale.y);
        }
    }

    // ---------------------------------------------------------------------
    // Click handling in design (1920x1080) coordinates
    // ---------------------------------------------------------------------
    void ButtonSystem::HandleClickUV(float u, float v)
    {
        float designX = u * DESIGN_WIDTH;
        float designY = v * DESIGN_HEIGHT;

        for (auto entity : mEntities)
        {
            if (!ecsInterface.HasComponent<ButtonComponent>(entity) ||
                !ecsInterface.HasComponent<TransformComponent>(entity) ||
                !ecsInterface.HasComponent<TextureProperties>(entity))
                continue;

            auto& button = ecsInterface.GetComponent<ButtonComponent>(entity);
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entity);
            auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);

            // Must be visible UI AND interactable
            if (!tex.isUI) continue;
            if (!tex.uiAlpha > 0.0f) continue;

            float left = tr.position.x - 0.5f * tr.scale.x;
            float right = tr.position.x + 0.5f * tr.scale.x;
            float bottom = tr.position.y - 0.5f * tr.scale.y;
            float top = tr.position.y + 0.5f * tr.scale.y;

            bool inside = (designX > left && designX < right && designY > bottom && designY < top);
            button.isHovered = inside;

            if (inside) button.TriggerClick(entity);
        }
    }

    void ButtonSystem::HandleSliderDrag(float u, float v)
    {
        float designX = u * DESIGN_WIDTH;
        float designY = v * DESIGN_HEIGHT;

        auto* input =     InputHandler::GetInstance();
        bool isPressed =  input->IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
        bool isHeld =     input->IsMouseButtonHeld(GLFW_MOUSE_BUTTON_LEFT);
        bool isReleased = input->IsMouseButtonReleased(GLFW_MOUSE_BUTTON_LEFT);

        /*--------------------------------------------------
            1) START DRAG � ONLY ON PRESS
        --------------------------------------------------*/
        if (!m_isDraggingSlider && isPressed)
        {
            struct KnobInfo { Entity knob; SliderType type; };
            std::vector<KnobInfo> knobs;

            auto gather = [&](SliderType type, const std::string& tag)
                {
                    auto ents = ecsInterface.GetEntitiesByTag(tag);
                    for (Entity e : ents)
                    {
                        if (ecsInterface.IsEntityValid(e) && ecsInterface.HasComponent<TransformComponent>(e))
                            knobs.push_back({ e, type });
                    }
                };

            gather(SliderType::Master, "sliderKnob_master");
            gather(SliderType::BGM, "sliderKnob_bgm");
            gather(SliderType::SFX, "sliderKnob_sfx");

            for (const auto& info : knobs)
            {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(info.knob);

                float left =   tr.position.x - 0.5f * tr.scale.x;
                float right =  tr.position.x + 0.5f * tr.scale.x;
                float bottom = tr.position.y - 0.5f * tr.scale.y;
                float top =    tr.position.y + 0.5f * tr.scale.y;

                if (designX >= left && designX <= right && designY >= bottom && designY <= top)
                {
                    m_isDraggingSlider = true;
                    m_activeKnob = info.knob;
                    m_activeSliderType = info.type;
                    break;
                }
            }
        }

        /*--------------------------------------------------
            2) DRAG � ONLY WHILE HELD
        --------------------------------------------------*/
        if (m_isDraggingSlider && isHeld)
        {
            std::string barTag;
            std::string textTag;
            float* lastVol = nullptr;

            switch (m_activeSliderType)
            {
                case SliderType::Master:
                    barTag = "sliderBar_master";
                    textTag = "volumeText_master";
                    lastVol = &m_lastMasterVol;
                    break;
                case SliderType::BGM:
                    barTag = "sliderBar_bgm";
                    textTag = "volumeText_bgm";
                    lastVol = &m_lastBGMVol;
                    break;
                case SliderType::SFX:
                    barTag = "sliderBar_sfx";
                    textTag = "volumeText_sfx";
                    lastVol = &m_lastSFXVol;
                    break;
                default: return;
            }

            auto bars = ecsInterface.GetEntitiesByTag(barTag);
            if (bars.empty()) return;

            Entity bar = bars.front();
            if (!ecsInterface.IsEntityValid(bar) || !ecsInterface.HasComponent<TransformComponent>(bar)) return;

            auto& barTr = ecsInterface.GetComponent<TransformComponent>(bar);
            auto& knobTr = ecsInterface.GetComponent<TransformComponent>(m_activeKnob);

            float barLeft = barTr.position.x - 0.5f * barTr.scale.x;
            float barRight = barTr.position.x + 0.5f * barTr.scale.x;

            knobTr.position.x = std::clamp(designX, barLeft, barRight);

            float volume = (knobTr.position.x - barLeft) / (barRight - barLeft);
            volume = std::clamp(volume, 0.0f, 1.0f);

            /* Update the fill bar visual */
            std::string fillBarTag;
            switch (m_activeSliderType)
            {
            case SliderType::Master: fillBarTag = "sliderFill_master"; break;
            case SliderType::BGM:    fillBarTag = "sliderFill_bgm";    break;
            case SliderType::SFX:    fillBarTag = "sliderFill_sfx";    break;
            default: break;
            }

            if (!fillBarTag.empty())
            {
                auto fillBars = ecsInterface.GetEntitiesByTag(fillBarTag);
                if (!fillBars.empty())
                {
                    Entity fillBar = fillBars.front();
                    if (ecsInterface.IsEntityValid(fillBar) &&
                        ecsInterface.HasComponent<TransformComponent>(fillBar))
                    {
                        auto& fillTr = ecsInterface.GetComponent<TransformComponent>(fillBar);
                        auto& barTr = ecsInterface.GetComponent<TransformComponent>(bar);

                        // Scale the fill bar width based on volume
                        fillTr.scale.x = barTr.scale.x * volume;

                        // Reposition to align with left edge of background bar
                        // (since scaling happens from center by default)
                        float barLeft = barTr.position.x - 0.5f * barTr.scale.x;
                        fillTr.position.x = barLeft + 0.5f * fillTr.scale.x;
                    }
                }
            }

            /* Apply audio */
            switch (m_activeSliderType)
            {
                case SliderType::Master: g_AudioSystem->SetMasterVolume(volume); break;
                case SliderType::BGM:    g_AudioSystem->SetBGMVolume(volume);    break;
                case SliderType::SFX:    g_AudioSystem->SetSFXVolume(volume);    break;
                default: break;
            }

            /* Update text + click sound only when value changes */
            if (lastVol && fabs(volume - *lastVol) > 0.01f)
            {
                auto texts = ecsInterface.GetEntitiesByTag(textTag);
                for (Entity textEnt : texts)
                {
                    if (ecsInterface.IsEntityValid(textEnt) && ecsInterface.HasComponent<TextProperties>(textEnt))
                    {
                        int percent = static_cast<int>(volume * 100.0f);
                        ecsInterface.GetComponent<TextProperties>(textEnt).text = std::to_string(percent) + "%";
                    }
                }

                if (fabs(volume - *lastVol) > 0.03f)
                {
                    g_AudioSystem->PlayAudio("event:/ButtonClick");
                }
                *lastVol = volume;
            }
        }

        /*--------------------------------------------------
            3) RELEASE � ALWAYS RESET
        --------------------------------------------------*/
        if (m_isDraggingSlider && isReleased)
        {
            m_isDraggingSlider = false;
            m_activeKnob = 0;
            m_activeSliderType = SliderType::None;
        }
    }
    
    void ButtonSystem::UpdateHoverUV(float u, float v)
    {
        float designX = u * DESIGN_WIDTH;
        float designY = v * DESIGN_HEIGHT;

        for (auto entity : mEntities)
        {
            if (!ecsInterface.HasComponent<ButtonComponent>(entity) ||
                !ecsInterface.HasComponent<TransformComponent>(entity) ||
                !ecsInterface.HasComponent<TextureProperties>(entity))
                continue;

            auto& button = ecsInterface.GetComponent<ButtonComponent>(entity);
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entity);
            auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);

            // only UI quads are treated as buttons
            if (!tex.isUI) continue;

            // derive bounds from Transform (center + full size)
            float left = tr.position.x - 0.5f * tr.scale.x;
            float right = tr.position.x + 0.5f * tr.scale.x;
            float bottom = tr.position.y - 0.5f * tr.scale.y;
            float top = tr.position.y + 0.5f * tr.scale.y;

            bool inside = (designX > left && designX < right && designY > bottom && designY < top);
            button.isHovered = inside;

            //DO NOT CHANGE ANY Z POSITIONING
            if (inside && button.isFrontButton == true)
            {
                tr.position.z = -100;
            }
            else
            {
                tr.position.z = 1;
            }
        }
    }
}