// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "input.h"
#include "layer.h"
#include "feedback.h"
#include <log.h>

namespace Input
{
    using namespace motion_compensation_layer;
    using namespace log;
    using namespace Feedback;


    bool KeyboardInput::Init()
    {
        bool success = true;
        const std::set<Cfg> activities{
            Cfg::KeyActivate, Cfg::KeyCenter,     Cfg::KeyTransInc,      Cfg::KeyTransDec,     Cfg::KeyRotInc,
            Cfg::KeyRotDec,   Cfg::KeyOffForward, Cfg::KeyOffBack,       Cfg::KeyOffUp,        Cfg::KeyOffDown,
            Cfg::KeyOffRight, Cfg::KeyOffLeft,    Cfg::KeyRotRight,      Cfg::KeyRotLeft,      Cfg::KeyOverlay,
            Cfg::KeyCache,    Cfg::KeySaveConfig, Cfg::KeySaveConfigApp, Cfg::KeyReloadConfig, Cfg::KeyDebugCor};
        std::string errors;

        // undocumented / uncofigurable shoprtcuts:
        m_ShortCuts[Cfg::InteractionProfile] = {VK_CONTROL, VK_SHIFT, VK_MENU, 0x49}; // ctrl+shift+alt+i
        m_ShortCuts[Cfg::CurrentTrackerPose] = {VK_CONTROL, VK_SHIFT, VK_MENU, 0x54}; // ctrl+shift+alt+t

        for (const Cfg& activity : activities)
        {
            std::set<int> shortcut;
            if (GetConfig()->GetShortcut(activity, shortcut))
            {
                m_ShortCuts[activity] = shortcut;
            }
            else
            {
                success = false;
            }
        }
        return success;
    }

    void InputHandler::HandleKeyboardInput(const XrTime time)
    {
        bool isRepeat{false};
        if (m_Input.GetKeyState(Cfg::KeyActivate, isRepeat) && !isRepeat)
        {
            ToggleActive(time);
        }
        if (m_Input.GetKeyState(Cfg::KeyCenter, isRepeat) && !isRepeat)
        {
            Recalibrate(time);
        }
        if (m_Input.GetKeyState(Cfg::KeyTransInc, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(true, true);
        }
        if (m_Input.GetKeyState(Cfg::KeyTransDec, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(true, false);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotInc, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(false, true);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotDec, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(false, false);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffForward, isRepeat))
        {
            ChangeOffset(Direction::Fwd);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffBack, isRepeat))
        {
            ChangeOffset(Direction::Back);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffUp, isRepeat))
        {
            ChangeOffset(Direction::Up);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffDown, isRepeat))
        {
            ChangeOffset(Direction::Down);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffRight, isRepeat))
        {
            ChangeOffset(Direction::Right);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffLeft, isRepeat))
        {
            ChangeOffset(Direction::Left);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotRight, isRepeat))
        {
            ChangeOffset(Direction::RotRight);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotLeft, isRepeat))
        {
            ChangeOffset(Direction::RotLeft);
        }
        if (m_Input.GetKeyState(Cfg::KeyOverlay, isRepeat) && !isRepeat)
        {
            ToggleOverlay();
        }
        if (m_Input.GetKeyState(Cfg::KeyCache, isRepeat) && !isRepeat)
        {
            ToggleCache();
        }
        if (m_Input.GetKeyState(Cfg::KeySaveConfig, isRepeat) && !isRepeat)
        {
            SaveConfig(time, false);
        }
        if (m_Input.GetKeyState(Cfg::KeySaveConfigApp, isRepeat) && !isRepeat)
        {
            SaveConfig(time, true);
        }
        if (m_Input.GetKeyState(Cfg::KeyReloadConfig, isRepeat) && !isRepeat)
        {
            ReloadConfig();
        }
        if (m_Input.GetKeyState(Cfg::KeyDebugCor, isRepeat) && !isRepeat)
        {
            ToggleCorDebug(time);
        }
        if (m_Input.GetKeyState(Cfg::InteractionProfile, isRepeat) && !isRepeat)
        {
            m_Layer->RequestCurrentInteractionProfile();
        }
        if (m_Input.GetKeyState(Cfg::CurrentTrackerPose, isRepeat) && !isRepeat)
        {
            m_Layer->m_Tracker->LogCurrentTrackerPoses(m_Layer->m_Session, time, m_Layer->m_Activated);
        }
    }

    bool KeyboardInput::GetKeyState(const Cfg key, bool& isRepeat)
    {
        const auto it = m_ShortCuts.find(key);
        if (it == m_ShortCuts.end())
        {
            motion_compensation_layer::log::ErrorLog("%s(%d): unable to find key\n", __FUNCTION__, key);
            return false;
        }
        return UpdateKeyState(it->second, isRepeat);
    }

    bool KeyboardInput::UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat)
    {
        const auto isPressed =
            vkKeySet.size() > 0 && std::ranges::all_of(vkKeySet, [](const int vk) { return GetAsyncKeyState(vk) < 0; });
        auto keyState = m_KeyStates.find(vkKeySet);
        const auto now = std::chrono::steady_clock::now();
        if (m_KeyStates.end() == keyState)
        {
            // remember keyState for next call
            keyState = m_KeyStates.insert({vkKeySet, {false, now}}).first;
        }
        const auto lastToggleTime = isPressed != keyState->second.first ? now : keyState->second.second;
        const auto prevState = std::exchange(keyState->second, {isPressed, lastToggleTime});
        isRepeat = isPressed && prevState.first && (now - prevState.second) > m_KeyRepeatDelay;
        if (isRepeat)
        {
            // reset toggle time for next repetition
            keyState->second.second = now;
        }
        return isPressed && (!prevState.first || isRepeat);
    }

    InputHandler::InputHandler(OpenXrLayer* layer)
    {
        m_Layer = layer;
    }

    bool InputHandler::Init()
    {
        return m_Input.Init();
    }

    void InputHandler::ToggleActive(XrTime time) const
    {
        // handle debug test rotation
        if (m_Layer->m_TestRotation)
        {
            m_Layer->m_TestRotStart = time;
            m_Layer->m_Activated = !m_Layer->m_Activated;
            Log("test rotation motion compensation % s\n", m_Layer->m_Activated ? "activated" : "deactivated");
            return;
        }

        // perform last-minute initialization before activation
        const bool lazySuccess = m_Layer->m_Activated || m_Layer->LazyInit(time);

        const bool oldState = m_Layer->m_Activated;
        if (m_Layer->m_Initialized && lazySuccess)
        {
            // if tracker is not initialized, activate only after successful init
            m_Layer->m_Activated = m_Layer->m_Tracker->m_Calibrated
                                       ? !m_Layer->m_Activated
                                       : m_Layer->m_Tracker->ResetReferencePose(m_Layer->m_Session, time);
        }
        else
        {
            ErrorLog("layer initialization failed or incomplete!\n");
        }
        Log("motion compensation %s\n",
            oldState != m_Layer->m_Activated ? (m_Layer->m_Activated ? "activated" : "deactivated")
            : m_Layer->m_Activated           ? "kept active"
                                             : "could not be activated");
        if (oldState != m_Layer->m_Activated)
        {
            GetAudioOut()->Execute(m_Layer->m_Activated ? Event::Activated : Event::Deactivated);
        }
        else if (!m_Layer->m_Activated)
        {
            GetAudioOut()->Execute(Event::Error);
        }
        TraceLoggingWrite(g_traceProvider,
                          "ToggleActive",
                          TLArg(m_Layer->m_Activated ? "Deactivated" : "Activated", "MotionCompensation"),
                          TLArg(time, "Time"));
    }

    void InputHandler::Recalibrate(XrTime time) const
    {
        if (m_Layer->m_TestRotation)
        {
            m_Layer->m_TestRotStart = time;
            Log("test rotation motion compensation recalibrated\n");
            return;
        }

        std::string trackerType;
        if (m_Layer->m_PhysicalEnabled && !m_Layer->m_Activated &&
            GetConfig()->GetString(Cfg::TrackerType, trackerType) &&
            ("controller" == trackerType || "vive" == trackerType))
        {
            // trigger interaction suggestion and action set attachment if necessary
            m_Layer->LazyInit(time);
        }

        if (m_Layer->m_Tracker->ResetReferencePose(m_Layer->m_Session, time))
        {
            GetAudioOut()->Execute(Event::Calibrated);
            TraceLoggingWrite(g_traceProvider, "Recalibrate", TLArg("Reset", "Tracker_Reference"), TLArg(time, "Time"));
        }
        else
        {
            // failed to update reference pose -> deactivate mc
            if (m_Layer->m_Activated)
            {
                ErrorLog("motion compensation deactivated because tracker reference pose cold not be reset\n");
            }
            m_Layer->m_Activated = false;
            GetAudioOut()->Execute(Event::Error);
            TraceLoggingWrite(g_traceProvider,
                              "Recalibrate",
                              TLArg("Deactivated_Reset", "MotionCompensation"),
                              TLArg(time, "Time"));
        }
    }

    void InputHandler::ToggleOverlay() const
    {
        if (!m_Layer->m_OverlayEnabled)
        {
            GetAudioOut()->Execute(Event::Error);
            ErrorLog("overlay is deactivated in config file and cannot be activated\n");
            return;
        }
        m_Layer->m_Overlay->ToggleOverlay();
    }

    void InputHandler::ToggleCache() const
    {
        m_Layer->m_UseEyeCache = !m_Layer->m_UseEyeCache;
        GetConfig()->SetValue(Cfg::CacheUseEye, m_Layer->m_UseEyeCache);
        GetAudioOut()->Execute(m_Layer->m_UseEyeCache ? Event::EyeCached : Event::EyeCalculated);
    }

    void InputHandler::ChangeOffset(const Direction dir) const
    {
        bool success = true;
        std::string trackerType;
        if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            if ("yaw" == trackerType || "srs" == trackerType || "flypt" == trackerType)
            {
                if (auto* tracker = reinterpret_cast<Tracker::VirtualTracker*>(m_Layer->m_Tracker))
                {
                    if (Direction::RotLeft != dir && Direction::RotRight != dir)
                    {
                        const XrVector3f direction{Direction::Left == dir    ? 0.01f
                                                   : Direction::Right == dir ? -0.01f
                                                                             : 0.0f,
                                                   Direction::Up == dir     ? 0.01f
                                                   : Direction::Down == dir ? -0.01f
                                                                            : 0.0f,
                                                   Direction::Fwd == dir    ? 0.01f
                                                   : Direction::Back == dir ? -0.01f
                                                                            : 0.0f};
                        success = tracker->ChangeOffset(direction);
                    }
                    else
                    {
                        success = tracker->ChangeRotation(Direction::RotRight == dir);
                    }
                }
                else
                {
                    ErrorLog("unable to cast tracker to VirtualTracker pointer\n");
                    success = false;
                }
            }
            else
            {
                ErrorLog("unable to modify offset, wrong type of tracker: %s\n", trackerType);
                success = false;
            }
        }
        else
        {
            success = false;
        }
        GetAudioOut()->Execute(!success                    ? Event::Error
                               : Direction::Up == dir      ? Event::Up
                               : Direction::Down == dir    ? Event::Down
                               : Direction::Fwd == dir     ? Event::Forward
                               : Direction::Back == dir    ? Event::Back
                               : Direction::Left == dir    ? Event::Left
                               : Direction::Right == dir   ? Event::Right
                               : Direction::RotLeft == dir ? Event::RotLeft
                                                           : Event::RotRight);
    }

    void InputHandler::ReloadConfig() const
    {
        m_Layer->m_Tracker->m_Calibrated = false;
        m_Layer->m_Activated = false;
        bool success = GetConfig()->Init(m_Layer->m_Application);
        if (success)
        {
            GetConfig()->GetBool(Cfg::TestRotation, m_Layer->m_TestRotation);
            GetConfig()->GetBool(Cfg::CacheUseEye, m_Layer->m_UseEyeCache);
            Tracker::GetTracker(&m_Layer->m_Tracker);
            if (!m_Layer->m_Tracker->Init())
            {
                success = false;
            }
        }
        GetAudioOut()->Execute(!success ? Event::Error : Event::Load);
    }

    void InputHandler::SaveConfig(XrTime time, bool forApp) const
    {
        std::string trackerType;
        if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            if ("yaw" == trackerType || "srs" == trackerType || "flypt" == trackerType)
            {
                Tracker::VirtualTracker* tracker = reinterpret_cast<Tracker::VirtualTracker*>(m_Layer->m_Tracker);
                if (tracker)
                {
                    tracker->SaveReferencePose(time);
                }
                else
                {
                    ErrorLog("unable to cast tracker to VirtualTracker pointer\n");
                }
            }
        }
        GetConfig()->WriteConfig(forApp);
    }

    void InputHandler::ToggleCorDebug(const XrTime time) const
    {
        bool success = true;
        std::string trackerType;
        if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            if ("yaw" == trackerType || "srs" == trackerType || "flypt" == trackerType)
            {
                if (auto* tracker = reinterpret_cast<Tracker::VirtualTracker*>(m_Layer->m_Tracker))
                {
                    success = tracker->ToggleDebugMode(m_Layer->m_Session, time);
                }
                else
                {
                    ErrorLog("unable to cast tracker to VirtualTracker pointer\n");
                    success = false;
                }
            }
            else
            {
                ErrorLog("unable to activate cor debug mode, wrong type of tracker: %s\n", trackerType);
                success = false;
            }
        }
        else
        {
            success = false;
        }
        if (!success)
        {
            GetAudioOut()->Execute(Event::Error);
        }
    }
} // namespace Input