// From https://github.com/NVIDIAGameWorks/NVIDIAImageScaling/blob/main/samples/DX11/include/DXUtilities.h

// The MIT License(MIT)
//
// Copyright(c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files(the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <log.h>

namespace graphics::shader
{
    using namespace motion_compensation_layer::log;

    void CompileShader(const std::filesystem::path& shaderFile,
                       const char* entryPoint,
                       ID3DBlob** blob,
                       const D3D_SHADER_MACRO* defines = nullptr,
                       ID3DInclude* includes = nullptr,
                       const char* target = "cs_5_0");

    void CompileShader(const void* data,
                       size_t size,
                       const char* entryPoint,
                       ID3DBlob** blob,
                       const D3D_SHADER_MACRO* defines = nullptr,
                       ID3DInclude* includes = nullptr,
                       const char* target = "cs_5_0");

    inline void CompileShader(std::string_view code, const char* entryPoint, ID3DBlob** blob, const char* target)
    {
        CompileShader(code.data(), code.size(), entryPoint, blob, nullptr, nullptr, target);
    }
} // namespace utility::shader
