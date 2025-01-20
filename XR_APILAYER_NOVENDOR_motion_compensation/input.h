// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "config.h"
#include "utility.h"

namespace openxr_api_layer
{
    class OpenXrLayer;
}

namespace input
{
    class CorEstimatorCmd
    {
      public:
        bool Init();
        bool Read();
        void ConfirmStart();
        void ConfirmStop();
        void ConfirmSetCor();
        void ConfirmReset();
        void Failure();

        bool m_Controller{false}, m_Start{false}, m_Stop{false}, m_Error{false}, m_Reset{false}, m_SetCor{false};
        utility::DofValue m_CurrentDof{};

      private:
        void WriteFlag(const int flag, bool active);

        utility::Mmf m_Mmf{};
    };

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
        std::chrono::milliseconds m_KeyRepeatDelay{std::chrono::milliseconds(300)};
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
        void LockRefPose() const;
        void ReleaseRefPose() const;
        void ToggleOverlay() const;
        void TogglePassthrough() const;
        void ToggleCrosshair() const;
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

    

    class InteractionPaths
    {
      public:
        std::set<std::string> GetProfiles();
        std::string GetSubPath(const std::string& profile, int index);

      private:
        const std::vector<std::string> m_StandardButtons{"x", "y", "a", "b"};

        std::map<std::string, std::vector<std::string>> m_Mapping{
            {"/interaction_profiles/khr/simple_controller", {"select", "menu", "select", "menu"}},
            {"/interaction_profiles/bytedance/pico_neo3_controller", m_StandardButtons},
            {"/interaction_profiles/bytedance/pico4_controller", m_StandardButtons},
            {"/interaction_profiles/hp/mixed_reality_controller", m_StandardButtons},
            {"/interaction_profiles/htc/vive_controller", {"trackpad", "menu", "trackpad", "menu"}},
            {"/interaction_profiles/htc/vive_cosmos_controller", m_StandardButtons},
            {"/interaction_profiles/htc/vive_focus3_controller", m_StandardButtons},
            {"/interaction_profiles/microsoft/motion_controller", {"trackpad", "thumbstick", "trackpad", "thumbstick"}},
            {"/interaction_profiles/oculus/touch_controller", m_StandardButtons},
            {"/interaction_profiles/oculus/touch_pro_controller", m_StandardButtons},
            {"/interaction_profiles/oculus/touch_plus_controller", m_StandardButtons},
            {"/interaction_profiles/samsung/odyssey_controller", {"trackpad", "thumbstick", "trackpad", "thumbstick"}},
            {"/interaction_profiles/valve/index_controller", {"a", "b", "a", "b"}},
            {"/interaction_profiles/facebook/touch_controller_pro", m_StandardButtons}};
    };
} // namespace input
