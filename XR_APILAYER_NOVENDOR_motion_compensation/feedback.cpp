// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "feedback.h"
#include "utility.h"
#include "resource.h"
#include "layer.h"
#include <log.h>
#include <playsoundapi.h>

using namespace motion_compensation_layer::log;
namespace Feedback
{
    bool AudioOut::Init()
    {
         // TODO: preload sounds to avoid file system access
        m_SoundResources[Event::Error] = ERROR_WAV;
        m_SoundResources[Event::Load] = LOADED_WAV;
        m_SoundResources[Event::Save] = SAVED_WAV;
        m_SoundResources[Event::Activated] = ACTIVATED_WAV;
        m_SoundResources[Event::Deactivated] = DEACTIVATED_WAV;
        m_SoundResources[Event::Calibrated] = CALIBRATED_WAV;
        m_SoundResources[Event::Plus] = PLUS_WAV;
        m_SoundResources[Event::Minus] = MINUS_WAV;
        m_SoundResources[Event::Max] = MAX_WAV;
        m_SoundResources[Event::Min] = MIN_WAV;
        m_SoundResources[Event::Up] = UP_WAV;
        m_SoundResources[Event::Down] = DOWN_WAV;
        m_SoundResources[Event::Forward] = FORWARD_WAV;
        m_SoundResources[Event::Back] = BACK_WAV;
        m_SoundResources[Event::Left] = LEFT_WAV;
        m_SoundResources[Event::Right] = RIGHT_WAV;
        m_SoundResources[Event::RotLeft] = ROT_LEFT_WAV;
        m_SoundResources[Event::RotRight] = ROT_RIGHT_WAV;
        m_SoundResources[Event::DebugOn] = DEBUG_ON_WAV;
        m_SoundResources[Event::DebugOff] = DEBUG_OFF_WAV;

        return true;
    }

    void AudioOut::Execute(Event feedback)
    {  
        auto soundResource = m_SoundResources.find(feedback);
        if (m_SoundResources.end() != soundResource)
        {

            if (!PlaySound(NULL, 0, 0) || !PlaySound(MAKEINTRESOURCE(soundResource->second),
                                                     motion_compensation_layer::dllModule,
                                                     SND_RESOURCE | SND_ASYNC))

            {
                ErrorLog("%s: unable to play sound (%d : % d): %s\n",
                         __FUNCTION__,
                         soundResource->first,
                         soundResource->second,
                         utility::LastErrorMsg(GetLastError()).c_str());
            }
        }
        else
        {
            ErrorLog("%s: unknown feedback identifier: %d\n", __FUNCTION__, feedback);
        }
    }
} // namespace Feedback

std::unique_ptr<Feedback::AudioOut> g_AudioOut = nullptr;

Feedback::AudioOut* GetAudioOut()
{
    if (!g_AudioOut)
    {
        g_AudioOut = std::make_unique<Feedback::AudioOut>();
    }
    return g_AudioOut.get();
}