// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "resource.h"

namespace Feedback
{
    enum class Event
    {
        Error = -1,
        Load,
        Save,
        Activated,
        Deactivated,
        Calibrated,
        Plus,
        Minus,
        Max,
        Min,
        Up,
        Down,
        Forward,
        Back,
        Left,
        Right,
        RotLeft,
        RotRight,
        DebugOn,
        DebugOff,
        ConnectionLost,
        EyeCached,
        EyeCalculated,
        OverlayOn,
        OverlayOff,
        ModifierOn,
        ModifierOff,
        CalibrationLost,
        VerboseOn,
        VerboseOff
    };

    class AudioOut
    {
      public:
        static void Execute(Event event);
        static void CountDown(int seconds);
      private:
        inline static const std::map<Event, int> m_SoundResources{{Event::Error, ERROR_WAV},
                                                                  {Event::Load, LOADED_WAV},
                                                                  {Event::Save, SAVED_WAV},
                                                                  {Event::Activated, ACTIVATED_WAV},
                                                                  {Event::Deactivated, DEACTIVATED_WAV},
                                                                  {Event::Calibrated, CALIBRATED_WAV},
                                                                  {Event::Plus, PLUS_WAV},
                                                                  {Event::Minus, MINUS_WAV},
                                                                  {Event::Max, MAX_WAV},
                                                                  {Event::Min, MIN_WAV},
                                                                  {Event::Up, UP_WAV},
                                                                  {Event::Down, DOWN_WAV},
                                                                  {Event::Forward, FORWARD_WAV},
                                                                  {Event::Back, BACK_WAV},
                                                                  {Event::Left, LEFT_WAV},
                                                                  {Event::Right, RIGHT_WAV},
                                                                  {Event::RotLeft, ROT_LEFT_WAV},
                                                                  {Event::RotRight, ROT_RIGHT_WAV},
                                                                  {Event::DebugOn, DEBUG_ON_WAV},
                                                                  {Event::DebugOff, DEBUG_OFF_WAV},
                                                                  {Event::ConnectionLost, CONNECTION_LOST_WAV},
                                                                  {Event::EyeCached, EYE_CACHED_WAV},
                                                                  {Event::EyeCalculated, EYE_CALCULATION_WAV},
                                                                  {Event::OverlayOn, OVERLAY_ON_WAV},
                                                                  {Event::OverlayOff, OVERLAY_OFF_WAV},
                                                                  {Event::ModifierOn, MODIFIER_ON_WAV},
                                                                  {Event::ModifierOff, MODIFIER_OFF_WAV},
                                                                  {Event::CalibrationLost, CALIBRATION_LOST_WAV},
                                                                  {Event::VerboseOn, VERBOSE_ON_WAV},
                                                                  {Event::VerboseOff, VERBOSE_OFF_WAV}};

    };

} // namespace Feedback
