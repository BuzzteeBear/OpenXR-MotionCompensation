// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "config.h"

namespace motion_compensation_layer
{
    class OpenXrLayer;
}

namespace Input
{
    class KeyboardInput
    {
      public:
        bool Init();
        bool GetKeyState(Cfg key, bool& isRepeat);

      private:
        bool UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat);

        std::map<Cfg, std::set<int>> m_ShortCuts;
        std::map<std::set<int>, std::pair<bool, std::chrono::steady_clock::time_point>> m_KeyStates;
        const std::chrono::milliseconds m_KeyRepeatDelay = 300ms;
    };
    class InputHandler
    {
      private:
        enum class Direction
        {
            Fwd,
            Back,
            Up,
            Down,
            Left,
            Right,
            RotRight,
            RotLeft
        };
      public:
        explicit InputHandler(motion_compensation_layer::OpenXrLayer* layer);
        bool Init();
        void ToggleActive(XrTime time) const;
        void Recalibrate(XrTime time) const;
        void ToggleOverlay() const;
        void ToggleCache() const;
        void ChangeOffset(::Input::InputHandler::Direction dir) const;
        void ReloadConfig() const;
        void SaveConfig(XrTime time, bool forApp) const;
        void ToggleCorDebug(XrTime time) const;
        void HandleKeyboardInput(XrTime time);

      private:
        motion_compensation_layer::OpenXrLayer* m_Layer;
        KeyboardInput m_Input;
    };
} // namespace Input
