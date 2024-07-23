// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "config.h"

namespace openxr_api_layer
{
    class OpenXrLayer;
}

namespace input
{
    class KeyboardInput
    {
      public:
        bool Init();
        bool GetKeyState(Cfg key, bool& isRepeat);

      private:
        bool UpdateKeyState(const std::set<int>& vkKeySet,
                            const std::set<int>& vkExclusionSet,
                            bool& isRepeat,
                            bool isModifier);

        std::map<Cfg, std::pair<std::set<int>, std::set<int>>> m_ShortCuts;
        std::map<std::set<int>, std::pair<bool, std::chrono::steady_clock::time_point>> m_KeyStates;
        std::chrono::milliseconds m_KeyRepeatDelay{300ms};
        std::set<Cfg> m_FastActivities{Cfg::KeyTransInc,
                                       Cfg::KeyTransDec,
                                       Cfg::KeyRotInc,
                                       Cfg::KeyRotDec,
                                       Cfg::KeyStabilizer,
                                       Cfg::KeyStabInc,
                                       Cfg::KeyStabDec,
                                       Cfg::KeyOffForward,
                                       Cfg::KeyOffBack,
                                       Cfg::KeyOffUp,
                                       Cfg::KeyOffDown,
                                       Cfg::KeyOffRight,
                                       Cfg::KeyOffLeft,
                                       Cfg::KeyRotRight,
                                       Cfg::KeyRotLeft};
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
        explicit InputHandler(openxr_api_layer::OpenXrLayer* layer);
        bool Init();
        void HandleKeyboardInput(XrTime time);
        void ToggleActive(XrTime time) const;
        void Recalibrate(XrTime time) const;
        void ToggleOverlay() const;
        void ToggleCache() const;
        void ToggleModifier() const;
        void ChangeOffset(::input::InputHandler::Direction dir, bool fast) const;
        void ReloadConfig() const;
        void SaveConfig(XrTime time, bool forApp) const;
        static void ToggleVerbose();

      private:
        openxr_api_layer::OpenXrLayer* m_Layer;
        KeyboardInput m_Input;
    };

    class ButtonPath
    {
      public:
        std::string GetSubPath(const std::string& profile, int index);

      private:
        std::map<std::string, std::vector<std::string>> m_Mapping{
            {"/interaction_profiles/khr/simple_controller", {"input/select", "input/menu"}},
            {"/interaction_profiles/htc/vive_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/microsoft/motion_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/oculus/touch_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/oculus/go_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/valve/index_controller", {"input/trigger", "input/a/click"}},
            {"/interaction_profiles/hp/mixed_reality_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/samsung/odyssey_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/bytedance/pico_neo3_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/bytedance/pico4_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/facebook/touch_controller_pro", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/htc/vive_cosmos_controller", {"input/trigger", "input/menu"}},
            {"/interaction_profiles/htc/vive_focus3_controller", {"input/trigger", "input/menu"}}};
    };
} // namespace input
