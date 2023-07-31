// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include <log.h>
#include "utility.h"
#include "layer.h"
#include "config.h"
#include "feedback.h"

using namespace openxr_api_layer;
using namespace log;
using namespace xr::math;

namespace utility
{
    AutoActivator::AutoActivator(std::shared_ptr<Input::InputHandler> input)
    {
        m_Input = input;
        GetConfig()->GetBool(Cfg::AutoActive, m_Activate);
        GetConfig()->GetInt(Cfg::AutoActiveDelay, m_SecondsLeft);
        GetConfig()->GetBool(Cfg::AutoActiveCountdown, m_Countdown);

        Log("auto activation %s, delay: %d seconds, countdown %s\n",
            m_Activate ? "on" : "off",
            m_SecondsLeft,
            m_Countdown ? "on" : "off");
    }
    void AutoActivator::ActivateIfNecessary(const XrTime time)
    {
        if (m_Activate)
        {
            if (m_SecondsLeft <= 0)
            {
                m_Input->ToggleActive(time);
                m_Activate = false;
                return;
            }
            if (0 == m_ActivationTime)
            {
                m_ActivationTime = time + ++m_SecondsLeft * 1000000000ll;
            }
            const int currentlyLeft = static_cast<int>((m_ActivationTime - time) / 1000000000ll);

            if (m_Countdown && currentlyLeft < m_SecondsLeft)
            {
                Feedback::AudioOut::CountDown(currentlyLeft);
            }
            m_SecondsLeft = currentlyLeft;
        }
    }

    Mmf::Mmf()
    {
        float check;
        if (GetConfig()->GetFloat(Cfg::TrackerCheck, check) && check >= 0)
        {
            m_Check = static_cast<XrTime>(check * 1000000000.0);
            Log("mmf connection refresh interval is set to %.3f ms\n", check * 1000.0);
        }
        else
        {
            ErrorLog("%s: defaulting to mmf connection refresh interval of %.3f ms\n",
                     __FUNCTION__,
                     static_cast<double>(m_Check) / 1000000.0);
        }
    }

    Mmf::~Mmf()
    {
        Close();
    }

    void Mmf::SetName(const std::string& name)
    {
        m_Name = name;
    }

    bool Mmf::Open(const XrTime time)
    {
        std::unique_lock lock(m_MmfLock);
        m_FileHandle = OpenFileMapping(FILE_MAP_READ, FALSE, m_Name.c_str());

        if (m_FileHandle)
        {
            m_View = MapViewOfFile(m_FileHandle, FILE_MAP_READ, 0, 0, 0);
            if (m_View != nullptr)
            {
                m_LastRefresh = time;
                m_ConnectionLost = false;
            }
            else
            {
                ErrorLog("unable to map view of mmf %s: %s\n", m_Name.c_str(), LastErrorMsg().c_str());
                lock.unlock();
                Close();
                return false;
            }
        }
        else
        {
            if (!m_ConnectionLost)
            {
                ErrorLog("could not open file mapping object %s: %s\n", m_Name.c_str(), LastErrorMsg().c_str());
                m_ConnectionLost = true;
            }
            return false;
        }
        return true;
    }
    bool Mmf::Read(void* buffer, const size_t size, const XrTime time)
    {
        if (m_Check > 0 && time - m_LastRefresh > m_Check)
        {
            Close();
        }
        if (!m_View)
        {
            Open(time);
        }
        std::unique_lock lock(m_MmfLock);
        if (m_View)
        {
            try
            {
                memcpy(buffer, m_View, size);
            }
            catch (std::exception& e)
            {
                ErrorLog("%s: unable to read from mmf %s: %s\n", __FUNCTION__, m_Name.c_str(), e.what());
                // reset mmf connection
                Close();
                return false;
            }
            return true;
        }
        return false;
    }

    void Mmf::Close()
    {
        std::unique_lock lock(m_MmfLock);
        if (m_View)
        {
            UnmapViewOfFile(m_View);
        }
        m_View = nullptr;
        if (m_FileHandle)
        {
            CloseHandle(m_FileHandle);
        }
        m_FileHandle = nullptr;
    }

    namespace
    {
        using namespace DirectX;

        // Taken from
        // https://github.com/microsoft/OpenXR-MixedReality/blob/main/samples/SceneUnderstandingUwp/Scene_Placement.cpp
        bool XM_CALLCONV rayIntersectQuad(DirectX::FXMVECTOR rayPosition,
                                          DirectX::FXMVECTOR rayDirection,
                                          DirectX::FXMVECTOR v0,
                                          DirectX::FXMVECTOR v1,
                                          DirectX::FXMVECTOR v2,
                                          DirectX::FXMVECTOR v3,
                                          XrPosef* hitPose,
                                          float& distance)
        {
            // Not optimal. Should be possible to determine which triangle to test.
            bool hit = TriangleTests::Intersects(rayPosition, rayDirection, v0, v1, v2, distance);
            if (!hit)
            {
                hit = TriangleTests::Intersects(rayPosition, rayDirection, v3, v2, v0, distance);
            }
            if (hit && hitPose != nullptr)
            {
                FXMVECTOR hitPosition = XMVectorAdd(rayPosition, XMVectorScale(rayDirection, distance));
                FXMVECTOR plane = XMPlaneFromPoints(v0, v2, v1);

                // p' = p - (n . p + d) * n
                // Project the ray position onto the plane
                float t = XMVectorGetX(XMVector3Dot(plane, rayPosition)) + XMVectorGetW(plane);
                FXMVECTOR projPoint = XMVectorSubtract(rayPosition, XMVectorMultiply(XMVectorSet(t, t, t, 0), plane));

                // From the projected ray position, look towards the hit position and make the plane's normal "up"
                FXMVECTOR forward = XMVectorSubtract(hitPosition, projPoint);
                XMMATRIX virtualToGazeOrientation = XMMatrixLookToRH(hitPosition, forward, plane);
                xr::math::StoreXrPose(hitPose, XMMatrixInverse(nullptr, virtualToGazeOrientation));
            }
            return hit;
        }

    } // namespace

    std::string LastErrorMsg()
    {
        if (const DWORD error = GetLastError())
        {
            LPVOID buffer;
            if (const DWORD bufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                                   nullptr,
                                                   error,
                                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                                   reinterpret_cast<LPTSTR>(&buffer),
                                                   0,
                                                   nullptr))
            {
                const auto lpStr = static_cast<LPCSTR>(buffer);
                const std::string result(lpStr, lpStr + bufLen);
                LocalFree(buffer);
                return std::to_string(error) + " - " + result;
            }
        }
        return "0";
    }
} // namespace utility
