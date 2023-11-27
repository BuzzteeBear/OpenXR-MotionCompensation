// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "feedback.h"
#include "utility.h"
#include "resource.h"
#include "layer.h"
#include <log.h>
#include <playsoundapi.h>

using namespace openxr_api_layer::log;
namespace Feedback
{
    void AudioOut::Execute(const Event event)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "AudioOut::Execute", TLArg(static_cast<int>(event), "Event"));

        const auto soundResource = m_SoundResources.find(event);
        if (m_SoundResources.end() != soundResource)
        {
            

            if (PlaySound(nullptr, nullptr, 0) && PlaySound(MAKEINTRESOURCE(soundResource->second),
                                                            openxr_api_layer::dllModule,
                                                            SND_RESOURCE | SND_ASYNC))
            {
                TraceLoggingWriteTagged(local, "AudioOut::Execute", TLArg(soundResource->second, "Resource"));
            }
            else
            {
                ErrorLog("%s: unable to play sound (%d : % d): %s",
                         __FUNCTION__,
                         soundResource->first,
                         soundResource->second,
                         utility::LastErrorMsg().c_str());
            }
        }
        else
        {
            ErrorLog("%s: unknown event identifier: %d", __FUNCTION__, event);
        }
        TraceLoggingWriteStop(local, "AudioOut::Execute");
    }

    void AudioOut::CountDown(const int seconds)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "AudioOut::CountDown", TLArg(seconds, "Seconds"));

        if (seconds > 0 && seconds <= 10 &&
            (PlaySound(nullptr, nullptr, 0) && PlaySound(MAKEINTRESOURCE(COUNT0_WAV + static_cast<uint64_t>(seconds)),
                                                         openxr_api_layer::dllModule,
                                                         SND_RESOURCE | SND_ASYNC)))
        {
            {
                TraceLoggingWriteTagged(local,
                                  "AudioOut::CountDown",
                                  TLArg(seconds, "Seconds"),
                                  TLArg(COUNT0_WAV + seconds, "Resource"));
            }
        }
        else
        {
            ErrorLog("%s: unable to play sound (%d : % d): %s",
                     __FUNCTION__,
                     -seconds,
                     COUNT0_WAV + seconds,
                     utility::LastErrorMsg().c_str());
        }
        TraceLoggingWriteStop(local, "AudioOut::CountDown");
    }
} // namespace Feedback