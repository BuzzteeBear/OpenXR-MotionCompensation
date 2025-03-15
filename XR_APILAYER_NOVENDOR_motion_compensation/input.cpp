// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "input.h"
#include "layer.h"
#include "output.h"
#include <log.h>
#include <util.h>
#include <ranges>

using namespace openxr_api_layer;
using namespace log;
using namespace output;

namespace input
{
    static std::string ToString(const ActivityBit& bit)
    {
        uint64_t input = static_cast<uint64_t>(bit);
        int significance = 0;
        while (input >>= 1)
        {
            ++significance;
        }
        return std::format("{}", significance);
    }

    bool CorEstimatorCmd::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "CorEstimatorCmd::Init");

        m_Mmf.SetWriteable(sizeof(int));
        m_Mmf.SetName("Local\\OXRMC_CorEstimatorCmd");
        int zero = 0;
        bool success = m_Mmf.Write(&zero, sizeof(int));

        TraceLoggingWriteStop(local, "CorEstimatorCmd::Init", TLArg(success, "Success"));
        return success; 
    }

    bool CorEstimatorCmd::Read()
    {
        int cmd;
        if (!m_Mmf.Read(&cmd,sizeof(int),0))
        {
            if (!m_Error)
            {
                ErrorLog("%s: unable to read from mmf: Local\\OXRMC_CorEstimatorCmd", __FUNCTION__);
                m_Error = true;
            }
            return false;
        }
        m_Error = false;
        m_PoseType = cmd & 7;
        m_Controller = cmd & static_cast<int>(CorEstimatorFlags::controller);
        m_Start = cmd & static_cast<int>(CorEstimatorFlags::start);
        m_Stop = cmd & static_cast<int>(CorEstimatorFlags::stop);
        m_Reset = cmd & static_cast<int>(CorEstimatorFlags::reset);
        return true;
    }

    void CorEstimatorCmd::ConfirmStart()
    {
        WriteFlag(static_cast<int>(CorEstimatorFlags::confirm), true);
        WriteFlag(static_cast<int>(CorEstimatorFlags::start), false);
    }

    void CorEstimatorCmd::ConfirmStop()
    {
        WriteFlag(static_cast<int>(CorEstimatorFlags::stop), false);
    }

    void CorEstimatorCmd::ConfirmReset()
    {
        WriteFlag(static_cast<int>(CorEstimatorFlags::reset), false);
    }

    void CorEstimatorCmd::Failure()
    {
        WriteFlag(static_cast<int>(CorEstimatorFlags::failure), true);
        WriteFlag(static_cast<int>(CorEstimatorFlags::start), false);
    }


    void CorEstimatorCmd::WriteFlag(const int flag, bool active)
    {
        int cmd;
        if (!m_Mmf.Read(&cmd, sizeof(int), 0))
        {
            if (!m_Error)
            {
                ErrorLog("%s (%d / %d): unable to read from mmf: Local\\OXRMC_CorEstimatorCmd", __FUNCTION__, flag, active);
            }
            m_Error = true;
            return;
        }
        cmd = active ? cmd | flag : cmd & ~flag;
        if (!m_Mmf.Write(&cmd, sizeof(int)))
        {
            if (!m_Error)
            {
                ErrorLog("%s (%d / %d): unable to write to mmf: Local\\OXRMC_CorEstimatorCmd", __FUNCTION__, flag, active);
            }
            m_Error = true;
            return; 
        }
        m_Error = false;
    }

    bool CorEstimatorResult::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "CorEstimatorResult::Init");

        std::tuple<int32_t, XrPosef, float> nullData{};
        m_Mmf.SetWriteable(sizeof(nullData));
        m_Mmf.SetName("Local\\OXRMC_CorEstimatorResult");
        bool success = m_Mmf.Write(&nullData, sizeof(nullData));

        TraceLoggingWriteStop(local, "CorEstimatorResult::Init", TLArg(success, "Success"));
        return success; 
    }

    std::optional<CorResult> CorEstimatorResult::ReadResult()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "CorEstimatorResult::ReadResult");

        CorResult data{};
        if (!m_Mmf.Read(&data, sizeof(data), 0) || !data.resultType)
        {
            TraceLoggingWriteStop(local, "CorEstimatorResult::ReadResult", TLArg(data.resultType, "ResultType"));
            return {};
        }
        CorResult nullData{};
        m_Mmf.Write(&nullData, sizeof(nullData));
        DebugLog("CorEstimatorResult::ReadResult: %d / %s / %f",
                 data.resultType,
                 xr::ToString(XrPosef()).c_str(),
                 data.radius);      
        return data;
    }

    bool MmfInput::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "MmfInput::Init");

        ActivityFlags nullData{};
        m_Mmf.SetWriteable(sizeof(nullData));
        m_Mmf.SetName("Local\\OXRMC_ActivityInput");
        bool success = m_Mmf.Write(&nullData, sizeof(nullData));
        if (!success)
        {
            ErrorLog("%s: unable to create activity mmf", __FUNCTION__);
        }

        TraceLoggingWriteStop(local, "MmfInput::Init", TLArg(success, "Success"));
        return success;
    }

    bool MmfInput::ReadMmf()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "MmfInput::ReadMmf");

        bool success{false};
        ActivityFlags data{};
        if (m_Mmf.Read(&data, sizeof(data), 0))
        {
            m_Flags = data;
            success = true;
        }
        else
        {
            m_Flags = {};
        }
        TraceLoggingWriteStop(local,
                              "MmfInput::ReadMmf",
                              TLArg(true, "Success"),
                              TLArg(data.trigger, "Trigger"),
                              TLArg(data.confirm, "Confirm"));
        return success;
    }

    bool MmfInput::GetTrigger(ActivityBit bit)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "MmfInput::GetTrigger", TLArg(ToString(bit).c_str(), "Bit"));
        if (!m_Flags.has_value())
        {
            TraceLoggingWriteStop(local, "MmfInput::GetTrigger", TLArg(false, "Success"));
            return false;
        }
        bool confirmed = m_Flags.value().confirm & static_cast<uint64_t>(bit);
        bool triggered = m_Flags.value().trigger & static_cast<uint64_t>(bit);

        if (triggered)
        {
            if (confirmed)
            {
                // already triggered
                triggered = false;
            }
            else
            {
                // set confirm bit
                m_Flags.value().confirm |= static_cast<int64_t>(bit);
                DebugLog("Trigger bit was set: %s", ToString(bit).c_str());
            }
        }
        else if (confirmed)
        {
            // clear confirm bit
            m_Flags.value().confirm &= ~static_cast<int64_t>(bit);
            DebugLog("Confirm bit cleared: %s", ToString(bit).c_str());
        }
        TraceLoggingWriteStop(local,
                              "MmfInput::GetTrigger",
                              TLArg(triggered, "Triggered"),
                              TLArg(confirmed, "Confirmed"));
        return triggered;
    }

    bool MmfInput::WriteConfirm()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "MmfInput::WriteConfirm");
        if (!m_Flags.has_value())
        {
            TraceLoggingWriteStop(local,
                                  "MmfInput::WriteConfirm",
                                  TLArg(false, "Success"));
            return false;
        }
        bool success = m_Mmf.Write(&m_Flags.value().confirm, sizeof(uint64_t), sizeof(uint64_t));
        TraceLoggingWriteStop(local,
                              "MmfInput::WriteConfirm",
                              TLArg(success, "Success"),
                              TLArg(m_Flags.value_or(ActivityFlags{}).trigger, "Trigger"),
                              TLArg(m_Flags.value_or(ActivityFlags{}).confirm, "Confirm"));
        return success;
    }


    bool KeyboardInput::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "KeyboardInput::Init");
        bool success = true;
        const std::set<Cfg> activities{
            Cfg::KeyActivate,     Cfg::KeyCalibrate,  Cfg::KeyLockRefPose,   Cfg::KeyReleaseRefPose,
            Cfg::KeyTransInc,     Cfg::KeyTransDec,   Cfg::KeyRotInc,        Cfg::KeyRotDec,
            Cfg::KeyStabilizer,   Cfg::KeyStabInc,    Cfg::KeyStabDec,       Cfg::KeyOffForward,
            Cfg::KeyOffBack,      Cfg::KeyOffUp,      Cfg::KeyOffDown,       Cfg::KeyOffRight,
            Cfg::KeyOffLeft,      Cfg::KeyRotRight,   Cfg::KeyRotLeft,       Cfg::KeyOverlay,
            Cfg::KeyPassthrough,  Cfg::KeyCrosshair,  Cfg::KeyCache,         Cfg::KeyModifier,
            Cfg::KeyFastModifier, Cfg::KeySaveConfig, Cfg::KeySaveConfigApp, Cfg::KeyReloadConfig,
            Cfg::KeyVerbose,      Cfg::KeyRecorder,   Cfg::KeyLogTracker,    Cfg::KeyLogProfile};
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

    void InputHandler::HandleInput(const XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::HandleInput", TLArg(time, "Time"));

        bool isRepeat{false};
        const bool fast = m_Keyboard.GetKeyState(Cfg::KeyFastModifier, isRepeat);

        m_Mmf->ReadMmf();

        TraceLoggingWriteTagged(local, "KeyboardInput::HandleInput", TLArg(fast, "Fast"));

        if ((m_Keyboard.GetKeyState(Cfg::KeyActivate, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::Activate))
        {
            ToggleActive(time);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyCalibrate, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::Calibrate))
        {
            Recalibrate(time);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyLockRefPose, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::LockRefPose))
        {
            LockRefPose();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyReleaseRefPose, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::ReleaseRefPose))
        {
            ReleaseRefPose();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyTransInc, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::FilterTranslationIncrease))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(true, true, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyTransDec, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::FilterTranslationDecrease))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(true, false, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyRotInc, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::FilterRotationIncrease))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(false, true, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyRotDec, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::FilterRotationDecrease))
        {
            m_Layer->m_Tracker->ModifyFilterStrength(false, false, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyStabilizer, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::StabilizerToggle))
        {
            m_Layer->m_Tracker->ToggleStabilizer();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyStabInc, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::StabilizerIncrease))
        {
            m_Layer->m_Tracker->ModifyStabilizer(true, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyStabDec, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::StabilizerDecrease))
        {
            m_Layer->m_Tracker->ModifyStabilizer(false, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOffForward, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetForward))
        {
            ChangeOffset(Direction::Fwd, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOffBack, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetBack))
        {
            ChangeOffset(Direction::Back, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOffUp, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetUp))
        {
            ChangeOffset(Direction::Up, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOffDown, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetDown))
        {
            ChangeOffset(Direction::Down, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOffRight, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetRight))
        {
            ChangeOffset(Direction::Right, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOffLeft, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetLeft))
        {
            ChangeOffset(Direction::Left, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyRotRight, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetRotateRight))
        {
            ChangeOffset(Direction::RotRight, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyRotLeft, isRepeat)) || m_Mmf->GetTrigger(ActivityBit::OffsetRotateLeft))
        {
            ChangeOffset(Direction::RotLeft, fast);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyOverlay, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::OverlayToggle))
        {
            ToggleOverlay();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyPassthrough, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::PassthroughToggle))
        {
            TogglePassthrough();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyCrosshair, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::CrosshairToggle))
        {
            ToggleCrosshair();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyCache, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::EyeCacheToggle))
        {
            ToggleCache();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyModifier, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::ModifierToggle))
        {
            ToggleModifier();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeySaveConfig, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::SaveConfig))
        {
            SaveConfig(time, false);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeySaveConfigApp, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::SaveConfigPerApp))
        {
            SaveConfig(time, true);
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyReloadConfig, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::ReloadConfig))
        {
            ReloadConfig();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyVerbose, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::VerboseLoggingToggle))
        {
            ToggleVerbose();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyRecorder, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::RecorderToggle))
        {
            m_Layer->ToggleRecorderActive();
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyLogProfile, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::LogProfile))
        {
            m_Layer->LogCurrentInteractionProfileAndSource("HandleKeyboardInput");
        }
        if ((m_Keyboard.GetKeyState(Cfg::KeyLogTracker, isRepeat) && !isRepeat) || m_Mmf->GetTrigger(ActivityBit::LogTracker))
        {
            m_Layer->m_Tracker->LogCurrentTrackerPoses(m_Layer->m_Session, time, m_Layer->m_Activated);
        }

        m_Mmf->WriteConfirm();

        TraceLoggingWriteStop(local, "InputHandler::HandleInput");
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

    InputHandler::InputHandler(OpenXrLayer* layer) : m_Layer(layer), m_Mmf(std::make_unique<MmfInput>()) {}

    bool InputHandler::Init()
    {
        return m_Keyboard.Init() && m_Mmf->Init();
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
            EventSink::Execute(m_Layer->m_Activated ? Event::Activated : Event::Deactivated);
            TraceLoggingWriteStop(local, "InputHandler::ToggleActive");
            return;
        }

        // perform last-minute initialization before activation
        const bool lazySuccess = m_Layer->m_Activated || m_Layer->LazyInit(time);

        const bool oldState = m_Layer->m_Activated;
        if (m_Layer->m_Initialized && lazySuccess)
        {
            // if tracker is not calibrated, activate only after successful calibration
            m_Layer->m_Activated = m_Layer->m_Tracker->m_Calibrated
                                       ? !m_Layer->m_Activated
                                       : m_Layer->m_Tracker->ResetReferencePose(m_Layer->m_Session, time);
        }
        else
        {
            ErrorLog("%s: layer initialization failed or incomplete!", __FUNCTION__);
        }
        Log("motion compensation %s",
            oldState != m_Layer->m_Activated ? (m_Layer->m_Activated ? "activated" : "deactivated")
            : m_Layer->m_Activated           ? "kept active"
                                             : "could not be activated");
        if (oldState != m_Layer->m_Activated)
        {
            EventSink::Execute(m_Layer->m_Activated ? Event::Activated : Event::Deactivated);
        }
        else if (!m_Layer->m_Activated)
        {
            EventSink::Execute(Event::Critical);
        }

        TraceLoggingWriteStop(local, "InputHandler::ToggleActive", TLArg(m_Layer->m_Activated, "Activated"));
    }

    void InputHandler::Recalibrate(XrTime time) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::Recalibrate", TLArg(time, "Time"));

        if (m_Layer->m_TestRotation)
        {
            m_Layer->m_TestRotStart = time;
            Log("test rotation motion compensation recalibrated");
            EventSink::Execute(Event::Calibrated);
            TraceLoggingWriteStop(local, "InputHandler::Recalibrate", TLArg(true, "Success"));
            return;
        }

        bool success = false;
        // trigger interaction suggestion and action set attachment if necessary
        if (m_Layer->m_Activated || m_Layer->LazyInit(time))
        {
            success = m_Layer->m_Tracker->ResetReferencePose(m_Layer->m_Session, time);
            m_Layer->SetCalibratedHmdPose(time);
        }
        if (!success)
        {
            // failed to update reference pose -> deactivate mc
            if (m_Layer->m_Activated)
            {
                ErrorLog("%s: motion compensation deactivated because tracker reference pose could not be calibrated",
                         __FUNCTION__);
                m_Layer->m_Activated = false;
            }
            EventSink::Execute(Event::Critical);
        }
        TraceLoggingWriteStop(local, "InputHandler::Recalibrate", TLArg(success, "Success"));
    }

    void InputHandler::LockRefPose() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::LockRefPose");

        if (!m_Layer->m_Tracker->m_Calibrated)
        {
            ErrorLog("%s: reference pose needs to ba calibrated before it can be locked", __FUNCTION__);
            EventSink::Execute(Event::Error);
            TraceLoggingWriteStop(local, "InputHandler::LockRefPose", TLArg(false, "Calibrated"));
            return;
        }
        m_Layer->m_Tracker->SaveReferencePose();
        bool success = GetConfig()->WriteRefPoseValues();
        if (success)
        {
            m_Layer->m_Tracker->m_LoadPoseFromFile = true;
            success = GetConfig()->SetRefPoseFromFile(true);
        }
        EventSink::Execute(success ? Event::RefPoseLocked : Event::Error);

        TraceLoggingWriteStop(local, "InputHandler::LockRefPose");
    }

    void InputHandler::ReleaseRefPose() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::LockRefPose");

        m_Layer->m_Tracker->m_LoadPoseFromFile = false;
        bool success = GetConfig()->SetRefPoseFromFile(false);
        EventSink::Execute(success ? Event::RefPoseReleased : Event::Error);

        TraceLoggingWriteStop(local, "InputHandler::LockRefPose");
    }

    void InputHandler::ToggleOverlay() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleOverlay");

        if (!m_Layer->m_Overlay)
        {
            EventSink::Execute(Event::Error);
            ErrorLog("%s: overlay is deactivated in config file and cannot be activated", __FUNCTION__);
            TraceLoggingWriteStop(local, "InputHandler::ToggleOverlay");
            return;
        }
        bool success = m_Layer->m_Overlay->ToggleOverlay();

        TraceLoggingWriteStop(local, "InputHandler::ToggleOverlay", TLArg(success, "Success"));
    }

    void InputHandler::TogglePassthrough() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::TogglePassthrough");

        if (!m_Layer->m_Overlay)
        {
            EventSink::Execute(Event::Error);
            ErrorLog("%s: overlay is deactivated in config file so passthrough mode cannot be activated", __FUNCTION__);
            TraceLoggingWriteStop(local, "InputHandler::TogglePassthrough");
            return;
        }
        bool success = m_Layer->m_Overlay->TogglePassthrough();

        TraceLoggingWriteStop(local, "InputHandler::TogglePassthrough", TLArg(success, "Success"));
    }

    void InputHandler::ToggleCrosshair() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleCrosshair");

        if (!m_Layer->m_Overlay)
        {
            EventSink::Execute(Event::Error);
            ErrorLog("%s: overlay is deactivated in config file so crosshair overlay cannot be activated", __FUNCTION__);
            TraceLoggingWriteStop(local, "InputHandler::ToggleCrosshair");
            return;
        }
        bool success = m_Layer->m_Overlay->ToggleCrosshair();

        TraceLoggingWriteStop(local, "InputHandler::ToggleCrosshair", TLArg(success, "Success"));
    }

    void InputHandler::ToggleCache() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleCache");

        m_Layer->m_UseEyeCache = !m_Layer->m_UseEyeCache;
        GetConfig()->SetValue(Cfg::CacheUseEye, m_Layer->m_UseEyeCache);
        Log("%s is used for reconstruction of eye positions", m_Layer->m_UseEyeCache ? "caching" : "calculation");
        EventSink::Execute(m_Layer->m_UseEyeCache ? Event::EyeCached : Event::EyeCalculated);

        TraceLoggingWriteStop(local, "InputHandler::ToggleCache", TLArg(m_Layer->m_UseEyeCache, "Eye Cache"));
    }

    void InputHandler::ToggleModifier() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleModifier");

        const bool active = m_Layer->ToggleModifierActive();
        GetConfig()->SetValue(Cfg::FactorEnabled, active);
        EventSink::Execute(active ? Event::ModifierOn : Event::ModifierOff);

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
        if (Direction::RotLeft != dir && Direction::RotRight != dir)
        {
            const float amount = fast ? 0.1f : 0.01f;
            const XrVector3f direction{Direction::Left == dir    ? -amount
                                       : Direction::Right == dir ? amount
                                                                 : 0.0f,
                                       Direction::Up == dir     ? amount
                                       : Direction::Down == dir ? -amount
                                                                : 0.0f,
                                       Direction::Fwd == dir    ? -amount
                                       : Direction::Back == dir ? amount
                                                                : 0.0f};
            success = m_Layer->m_Tracker->ChangeOffset(direction);
        }
        else
        {
            const float amount = fast ? utility::angleToRadian * 10.0f : utility::angleToRadian;
            success = m_Layer->m_Tracker->ChangeRotation(Direction::RotRight == dir ? -amount : amount);
        }

        EventSink::Execute(!success                    ? Event::Error
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

        m_Layer->m_Tracker->InvalidateCalibration(true);
        m_Layer->m_Activated = false;
        bool success = GetConfig()->Init(m_Layer->m_Application);
        if (success)
        {
            GetConfig()->GetBool(Cfg::LogVerbose, logVerbose);
            GetConfig()->GetBool(Cfg::TestRotation, m_Layer->m_TestRotation);
            GetConfig()->GetBool(Cfg::CacheUseEye, m_Layer->m_UseEyeCache);
            Log("%s is used for reconstruction of eye positions", m_Layer->m_UseEyeCache ? "caching" : "calculation");
            GetConfig()->GetBool(Cfg::LegacyMode, m_Layer->m_LegacyMode);
            Log("legacy mode is %s", m_Layer->m_LegacyMode ? "activated" : "off");
            m_Layer->m_AutoActivator =
                std::make_unique<utility::AutoActivator>(utility::AutoActivator(m_Layer->m_Input));
            m_Layer->m_HmdModifier = std::make_unique<modifier::HmdModifier>();
            m_Layer->m_VirtualTrackerUsed = GetConfig()->IsVirtualTracker();
            m_Layer->m_Tracker = tracker::GetTracker();
            if (!m_Layer->m_Tracker->Init())
            {
                success = false;
            }
            if (m_Layer->m_Overlay)
            {
                m_Layer->m_Overlay->ResetMarker();
                m_Layer->m_Overlay->ResetCrosshair();
            }
            m_Layer->m_CorEstimator->Init();
        }
        EventSink::Execute(!success ? Event::Critical : Event::Load);

        TraceLoggingWriteStop(local, "InputHandler::ReloadConfig", TLArg(success, "Success"));
    }

    void InputHandler::SaveConfig(XrTime time, bool forApp) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::SaveConfig", TLArg(time, "Time"), TLArg(forApp, "AppSpecific"));
        
        GetConfig()->WriteConfig(forApp);

        TraceLoggingWriteStop(local, "InputHandler::SaveConfig");
    }

    void InputHandler::ToggleVerbose()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "InputHandler::ToggleVerbose");

        logVerbose = !logVerbose;
        Log("verbose logging %s\n", logVerbose ? "activated" : "off");
        EventSink::Execute(logVerbose ? Event::VerboseOn : Event::VerboseOff);

        TraceLoggingWriteStop(local, "InputHandler::ToggleVerbose", TLArg(logVerbose, "LogVerbose"));
    }

    std::set<std::string> InteractionPaths::GetProfiles()
    {
        std::set<std::string> profiles;
        for (const auto& profile : m_Mapping | std::views::keys)
        {
            profiles.insert(profile);
        }
        return profiles;
    }

    std::string InteractionPaths::GetSubPath(const std::string& profile, int index)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "InteractionPaths::GetSubPath",
                               TLArg(profile.c_str(), "Profile"),
                               TLArg(index, "Index"));
        if ("left" != GetConfig()->GetControllerSide())
        {
            index += 2;
        }

        std::string path;
        if (const auto buttons = m_Mapping.find(profile);
            buttons != m_Mapping.end() && static_cast<int>(buttons->second.size()) > index)
        {
            path = buttons->second.at(index);
        }
        else
        {
            ErrorLog("%s: no button mapping (%d) found for profile: %s", __FUNCTION__, index, profile.c_str());
        }
        path.insert(0, "input/");
        path += "/click";
        TraceLoggingWriteStop(local, "InteractionPaths::GetSubPath", TLArg(path.c_str(), "Path"));

        return path;
    }

} // namespace input