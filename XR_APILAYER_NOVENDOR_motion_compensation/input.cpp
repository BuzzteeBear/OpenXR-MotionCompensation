// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "input.h"
#include "layer.h"
#include "feedback.h"
#include "utility.h"
#include <log.h>

using namespace openxr_api_layer;
using namespace log;
using namespace Feedback;

namespace Input
{
    bool KeyboardInput::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "KeyboardInput::Init");
        bool success = true;
        const std::set<Cfg> activities{
            Cfg::KeyActivate,     Cfg::KeyCalibrate,  Cfg::KeyTransInc,     Cfg::KeyTransDec,   Cfg::KeyRotInc,
            Cfg::KeyRotDec,       Cfg::KeyOffForward, Cfg::KeyOffBack,      Cfg::KeyOffUp,      Cfg::KeyOffDown,
            Cfg::KeyOffRight,     Cfg::KeyOffLeft,    Cfg::KeyRotRight,     Cfg::KeyRotLeft,    Cfg::KeyOverlay,
            Cfg::KeyCache,        Cfg::KeyModifier,   Cfg::KeyFastModifier, Cfg::KeySaveConfig, Cfg::KeySaveConfigApp,
            Cfg::KeyReloadConfig, Cfg::KeyLogTracker, Cfg::KeyLogProfile};
        const std::set<int> modifiers{VK_CONTROL, VK_SHIFT, VK_MENU};
        std::set<int> fastModifiers{};
        GetConfig()->GetShortcut(Cfg::KeyFastModifier, fastModifiers);
        std::string errors;

        auto keySetToString = [](const std::set<int>& keys) {
            std::string out;
            for (const int key : keys)
            {
                out += (out.empty() ? "[" : ", ") + std::to_string(key);
            }
            out += "]";
            return out;
        };

        for (const Cfg& activity : activities)
        {
            std::set<int> shortcut{};
            if (GetConfig()->GetShortcut(activity, shortcut))
            {
                // modifiers not included in the shortcut are put in the exclusion list
                std::set<int> modifiersNotSet;
                for (const int& modifier : modifiers)
                {
                    if (shortcut.contains(modifier) || Cfg::KeyFastModifier == activity ||
                        (m_FastActivities.contains(activity) && fastModifiers.contains(modifier)))
                    {
                        // don't exclude keys included in shortcut (and potential fast modifiers for corresponding
                        // activities)
                        continue;
                    }
                    modifiersNotSet.insert(modifier);
                }
                m_ShortCuts[activity] = {shortcut, modifiersNotSet};

                TraceLoggingWriteTagged(local,
                                        "KeyboardInput::Init",
                                        TLArg(static_cast<int>(activity), "Activity"),
                                        TLArg(keySetToString(shortcut).c_str(), "Keys"),
                                        TLArg(keySetToString(modifiersNotSet).c_str(), "Excluded Modifiers"));
            }
            else
            {
                success = false;
            }
        }
        TraceLoggingWriteStop(local, "KeyboardInput::Init");
        return success;
    }

    void InputHandler::HandleKeyboardInput(const XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::HandleKeyboardInput", TLArg(time, "Time"));

        bool isRepeat{false};
        const bool fast = m_Input.GetKeyState(Cfg::KeyFastModifier, isRepeat);

        TraceLoggingWriteTagged(local, "KeyboardInput::Init", TLArg(fast, "Fast"));

        if (m_Input.GetKeyState(Cfg::KeyActivate, isRepeat) && !isRepeat)
        {
            ToggleActive(time);
        }
        if (m_Input.GetKeyState(Cfg::KeyCalibrate, isRepeat) && !isRepeat)
        {
            Recalibrate(time);
        }
        if (m_Input.GetKeyState(Cfg::KeyTransInc, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(true, true, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyTransDec, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(true, false, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotInc, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(false, true, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotDec, isRepeat))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(false, false, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffForward, isRepeat))
        {
            ChangeOffset(Direction::Fwd, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffBack, isRepeat))
        {
            ChangeOffset(Direction::Back, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffUp, isRepeat))
        {
            ChangeOffset(Direction::Up, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffDown, isRepeat))
        {
            ChangeOffset(Direction::Down, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffRight, isRepeat))
        {
            ChangeOffset(Direction::Right, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOffLeft, isRepeat))
        {
            ChangeOffset(Direction::Left, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotRight, isRepeat))
        {
            ChangeOffset(Direction::RotRight, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyRotLeft, isRepeat))
        {
            ChangeOffset(Direction::RotLeft, fast);
        }
        if (m_Input.GetKeyState(Cfg::KeyOverlay, isRepeat) && !isRepeat)
        {
            ToggleOverlay();
        }
        if (m_Input.GetKeyState(Cfg::KeyCache, isRepeat) && !isRepeat)
        {
            ToggleCache();
        }
        if (m_Input.GetKeyState(Cfg::KeyModifier, isRepeat) && !isRepeat)
        {
            ToggleModifier();
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
        if (m_Input.GetKeyState(Cfg::KeyLogProfile, isRepeat) && !isRepeat)
        {
            m_Layer->LogCurrentInteractionProfile();
        }
        if (m_Input.GetKeyState(Cfg::KeyLogTracker, isRepeat) && !isRepeat)
        {
            m_Layer->m_Tracker->LogCurrentTrackerPoses(m_Layer->m_Session, time, m_Layer->m_Activated);
        }
        TraceLoggingWriteStop(local, "InputHandler::HandleKeyboardInput");
    }

    bool KeyboardInput::GetKeyState(const Cfg key, bool& isRepeat)
    {
        const auto it = m_ShortCuts.find(key);
        if (it == m_ShortCuts.end())
        {
            openxr_api_layer::log::ErrorLog("%s(%d): unable to find key", __FUNCTION__, key);
            return false;
        }
        return UpdateKeyState(it->second.first, it->second.second, isRepeat, Cfg::KeyFastModifier == key);
    }

    bool KeyboardInput::UpdateKeyState(const std::set<int>& vkKeySet,
                                       const std::set<int>& vkExclusionSet,
                                       bool& isRepeat,
                                       bool isModifier)
    {
        if (vkKeySet.empty())
        {
            return false;
        }
        const auto isPressed =
            vkKeySet.size() > 0 &&
            std::ranges::all_of(vkKeySet, [](const int vk) { return GetAsyncKeyState(vk) < 0; }) &&
            std::ranges::none_of(vkExclusionSet, [](const int vk) { return GetAsyncKeyState(vk) < 0; });
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
        return isPressed && (!prevState.first || isRepeat || isModifier);
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
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleActive", TLArg(time, "Time"));

        // handle debug test rotation
        if (m_Layer->m_TestRotation)
        {
            m_Layer->m_TestRotStart = time;
            m_Layer->m_Activated = !m_Layer->m_Activated;
            Log("test rotation motion compensation % s", m_Layer->m_Activated ? "activated" : "deactivated");
            TraceLoggingWriteStop(local, "InputHandler::ToggleActive");
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
            ErrorLog("layer initialization failed or incomplete!");
        }
        Log("motion compensation %s",
            oldState != m_Layer->m_Activated ? (m_Layer->m_Activated ? "activated" : "deactivated")
            : m_Layer->m_Activated           ? "kept active"
                                             : "could not be activated");
        if (oldState != m_Layer->m_Activated)
        {
            AudioOut::Execute(m_Layer->m_Activated ? Event::Activated : Event::Deactivated);
        }
        else if (!m_Layer->m_Activated)
        {
            AudioOut::Execute(Event::Error);
        }

        TraceLoggingWriteStop(local, "InputHandler::ToggleActive", TLArg(m_Layer->m_Activated, "Activated"));
    }

    void InputHandler::Recalibrate(XrTime time) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::Recalibrate", TLArg(time, "Time"));

        bool success = true;
        if (m_Layer->m_TestRotation)
        {
            m_Layer->m_TestRotStart = time;
            Log("test rotation motion compensation recalibrated");
            TraceLoggingWriteStop(local, "InputHandler::Recalibrate", TLArg(success, "Success"));
            return;
        }

        std::string trackerType;
        if (!m_Layer->m_Activated)
        {
            // trigger interaction suggestion and action set attachment if necessary
            m_Layer->LazyInit(time);
        }
        success = m_Layer->m_Tracker->ResetReferencePose(m_Layer->m_Session, time);
        if (success)
        {
            AudioOut::Execute(Event::Calibrated);
        }
        else
        {
            // failed to update reference pose -> deactivate mc
            if (m_Layer->m_Activated)
            {
                ErrorLog("motion compensation deactivated because tracker reference pose could not be calibrated");
                m_Layer->m_Activated = false;
            }
            AudioOut::Execute(Event::Error);
        }
        TraceLoggingWriteStop(local, "InputHandler::Recalibrate", TLArg(success, "Success"));
    }

    void InputHandler::ToggleOverlay() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleOverlay");

        if (!m_Layer->m_OverlayEnabled)
        {
            AudioOut::Execute(Event::Error);
            ErrorLog("overlay is deactivated in config file and cannot be activated");
            TraceLoggingWriteStop(local, "InputHandler::ToggleOverlay");
            return;
        }
        bool success = m_Layer->m_Overlay->ToggleOverlay();

        TraceLoggingWriteStop(local, "InputHandler::ToggleOverlay", TLArg(success, "Success"));
    }

    void InputHandler::ToggleCache() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleCache");

        m_Layer->m_UseEyeCache = !m_Layer->m_UseEyeCache;
        GetConfig()->SetValue(Cfg::CacheUseEye, m_Layer->m_UseEyeCache);
        AudioOut::Execute(m_Layer->m_UseEyeCache ? Event::EyeCached : Event::EyeCalculated);

        TraceLoggingWriteStop(local, "InputHandler::ToggleCache", TLArg(m_Layer->m_UseEyeCache, "Eye Cache"));
    }

    void InputHandler::ToggleModifier() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleModifier");

        const bool active = m_Layer->ToggleModifierActive();
        GetConfig()->SetValue(Cfg::FactorEnabled, active);
        AudioOut::Execute(active ? Event::ModifierOn : Event::ModifierOff);

        TraceLoggingWriteStop(local, "InputHandler::ToggleCache", TLArg(active, "Activated"));
    }

    void InputHandler::ChangeOffset(const Direction dir, const bool fast) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "InputHandler::ChangeOffset",
                               TLArg(static_cast<int>(dir), "Direction"),
                               TLArg(fast, "Fast"));

        bool success;

        if (m_Layer->m_VirtualTrackerUsed)
        {
            if (auto* tracker = reinterpret_cast<Tracker::VirtualTracker*>(m_Layer->m_Tracker))
            {
                if (Direction::RotLeft != dir && Direction::RotRight != dir)
                {
                    const float amount = fast ? 0.1f : 0.01f;
                    const XrVector3f direction{Direction::Left == dir    ? amount
                                               : Direction::Right == dir ? -amount
                                                                         : 0.0f,
                                               Direction::Up == dir     ? amount
                                               : Direction::Down == dir ? -amount
                                                                        : 0.0f,
                                               Direction::Fwd == dir    ? amount
                                               : Direction::Back == dir ? -amount
                                                                        : 0.0f};
                    success = tracker->ChangeOffset(direction);
                }
                else
                {
                    const float amount = fast ? utility::angleToRadian * 10.0f : utility::angleToRadian;
                    success = tracker->ChangeRotation(Direction::RotRight == dir ? -amount : amount);
                }
            }
            else
            {
                ErrorLog("unable to cast tracker to VirtualTracker pointer");
                success = false;
            }
        }
        else
        {
            ErrorLog("unable to modify offset, wrong type of tracker");
            success = false;
        }

        AudioOut::Execute(!success                    ? Event::Error
                          : Direction::Up == dir      ? Event::Up
                          : Direction::Down == dir    ? Event::Down
                          : Direction::Fwd == dir     ? Event::Forward
                          : Direction::Back == dir    ? Event::Back
                          : Direction::Left == dir    ? Event::Left
                          : Direction::Right == dir   ? Event::Right
                          : Direction::RotLeft == dir ? Event::RotLeft
                                                      : Event::RotRight);

        TraceLoggingWriteStop(local, "InputHandler::ChangeOffset", TLArg(success, "Success"));
    }

    void InputHandler::ReloadConfig() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ReloadConfig");

        m_Layer->m_Tracker->m_Calibrated = false;
        m_Layer->m_Activated = false;
        bool success = GetConfig()->Init(m_Layer->m_Application);
        if (success)
        {
            GetConfig()->GetBool(Cfg::LogVerbose, logVerbose);
            GetConfig()->GetBool(Cfg::TestRotation, m_Layer->m_TestRotation);
            GetConfig()->GetBool(Cfg::CacheUseEye, m_Layer->m_UseEyeCache);
            GetConfig()->GetBool(Cfg::LegacyMode, m_Layer->m_LegacyMode);
            Log("legacy mode is %s", m_Layer->m_LegacyMode ? "activated" : "off");
            m_Layer->m_AutoActivator =
                std::make_unique<utility::AutoActivator>(utility::AutoActivator(m_Layer->m_Input));
            m_Layer->m_HmdModifier = std::make_unique<Modifier::HmdModifier>();
            m_Layer->m_VirtualTrackerUsed = GetConfig()->IsVirtualTracker();
            Tracker::GetTracker(&m_Layer->m_Tracker);
            if (!m_Layer->m_Tracker->Init())
            {
                success = false;
            }
            if (m_Layer->m_Overlay)
            {
                m_Layer->m_Overlay->SetMarkerSize();
            }
        }
        AudioOut::Execute(!success ? Event::Error : Event::Load);

        TraceLoggingWriteStop(local, "InputHandler::ReloadConfig", TLArg(success, "Success"));
    }

    void InputHandler::SaveConfig(XrTime time, bool forApp) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::SaveConfig", TLArg(time, "Time"), TLArg(forApp, "AppSpecific"));

        if (m_Layer->m_VirtualTrackerUsed)
        {
            if (const auto tracker = reinterpret_cast<Tracker::VirtualTracker*>(m_Layer->m_Tracker))
            {
                tracker->SaveReferencePose(time);
            }
            else
            {
                ErrorLog("unable to cast tracker to VirtualTracker pointer");
            }
        }
        GetConfig()->WriteConfig(forApp);

        TraceLoggingWriteStop(local, "InputHandler::SaveConfig");
    }

    std::string ButtonPath::GetSubPath(const std::string& profile, int index)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "ButtonPath::GetSubPath",
                               TLArg(profile.c_str(), "Profile"),
                               TLArg(index, "Index"));

        std::string path;
        if (const auto buttons = m_Mapping.find(profile); buttons != m_Mapping.end() && buttons->second.size() > index)
        {
            path = buttons->second.at(index);
        }
        else
        {
            ErrorLog("%s: no button mapping (%d) found for profile: %s", __FUNCTION__, index, profile.c_str());
        }
        TraceLoggingWriteStop(local, "ButtonPath::GetSubPath", TLArg(path.c_str(), "Path"));

        return path;
    }

} // namespace Input