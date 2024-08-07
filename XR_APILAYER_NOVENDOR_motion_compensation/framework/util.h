// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "pch.h"

namespace xr {

    static inline std::string ToString(XrVersion version) {
        return std::format("{}.{}.{}", XR_VERSION_MAJOR(version), XR_VERSION_MINOR(version), XR_VERSION_PATCH(version));
    }

    static inline std::string ToString(const XrPosef& pose) {
        return std::format("p: ({:.3f}, {:.3f}, {:.3f}), o:({:.3f}, {:.3f}, {:.3f}, {:.3f})",
                           pose.position.x,
                           pose.position.y,
                           pose.position.z,
                           pose.orientation.x,
                           pose.orientation.y,
                           pose.orientation.z,
                           pose.orientation.w);
    }

    static inline std::string ToString(const utility::Dof& dof)
    {
        return std::format("sway: {:f}, surge: {:f}, heave: {:f}, yaw: {:f}, roll: {:f}, pitch: {:f}",
                           dof.data[utility::sway],
                           dof.data[utility::surge],
                           dof.data[utility::heave],
                           dof.data[utility::yaw],
                           dof.data[utility::roll],
                           dof.data[utility::pitch]);
    }

    static inline std::string ToString(const XrVector3f& vec)
    {
        return std::format("({:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z);
    }

    static inline std::string ToString(const XrQuaternionf& quat)
    {
        return std::format("({:.3f}, {:.3f}, {:.3f}, {:.3f})", quat.x, quat.y, quat.z, quat.w);
    }

    static inline std::string ToString(const XrFovf& fov) {
        return std::format(
            "(l:{:.3f}, r:{:.3f}, u:{:.3f}, d:{:.3f})", fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown);
    }

    static inline std::string ToString(const math::NearFar& nearFar)
    {
        return std::format("(n:{:.3f}, f:{:.3f})", nearFar.Near, nearFar.Far);
    }

    static inline std::string ToString(const XrRect2Di& rect) {
        return std::format("x:{}, y:{} w:{} h:{}", rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
    }

    static inline std::string ToString(const XrRect2Df& rect) {
        return std::format("x:{}, y:{} w:{} h:{}", rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
    }

    static XrPosef Normalize(const XrPosef& pose)
    {
        // normalize quaternion to counteract accumulated calculation error
        XrQuaternionf normalized;
        math::StoreXrQuaternion(&normalized, DirectX::XMQuaternionNormalize(math::LoadXrQuaternion(pose.orientation)));
        return XrPosef{normalized, pose.position};
    }
} // namespace xr