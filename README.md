# OpenXR Motion Compensation

This repository contains the source project for a basic [OpenXR API layer](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#api-layers) for motion compensation. 
It is based on the [OpenXR API layer template](https://github.com/mbucchia/OpenXR-Layer-Template) by Matthieu Bucchianeri. 
More about [OpenXR](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html) and the [OpenXR Loader](https://www.khronos.org/registry/OpenXR/specs/1.0/loader.html).

Prerequisites:

- Visual Studio 2019 or above;
- NuGet package manager (installed via Visual Studio Installer);
- Python 3 interpreter (installed via Visual Studio Installer or externally available in your PATH).

Limitations:

- The API Layer is made for Windows 64-bit only.

DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks.

## Customization

### Make sure to pull all 3 Git submodules dependencies:

- `external/OpenXR-SDK-Source`: This repository contains some critical definition files for generating and building the API layer.
- `external/OpenXR-SDK`: This repository contains the official Khronos SDK (pre-built), with the OpenXR header files needed for OpenXR development.
- `external/OpenXR-MixedReality`: This repository contains (among other things) a collection of utilities to conveniently write OpenXR code.

### Use the developer's install/uninstall scripts under `script\[Un]install-layer.ps1`.

You will need to use these scripts to enable/disable the API layer from within your output folder (eg: run `bin\x64\Debug\Install-Layer.ps1` to activate the debug version of the layer during development).

WARNING: Keep track of the content of your registry under `HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit`. You don't want to have multiple copies of the API layer active at once!

### Use the Windows Performance Recorder Profile (WPRP) tracelogging in `scripts\Tracing.wprp`.

[Tracelogging](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-portal) can become very useful for debugging locally and to investigate user issues. Update the GUID associate with your traces:

To capture a trace for the API layer:

- Open a command line prompt or powershell in administrator mode and in a folder where you have write permissions
- Begin recording a trace with the command: `wpr -start path\to\Tracing.wprp -filemode`
- Leave that command prompt open
- Reproduce the crash/issue
- Back to the command prompt, finish the recording with: `wpr -stop output.etl`
- These files are highly compressible!

Use an application such as [Tabnalysis](https://apps.microsoft.com/store/detail/tabnalysis/9NQLK2M4RP4J?hl=en-id&gl=ID) to inspect the content of the trace file.

### Customize the layer code

NOTE: Because an OpenXR API layer is tied to a particular instance, you may retrieve the `XrInstance` handle at any time by invoking `OpenXrApi::GetXrInstance()`.

### Declare OpenXR functions to override in `XR_APILAYER_NOVENDOR_template\framework\layer_apis.py`

Update the definitions as follows:

- `override_functions`: the list of OpenXR functions to intercept and to populate a "chain" for. To override the implementation of each function, simply implement a virtual method with the same name and prototype in the `OpenXrLayer` class in `XR_APILAYER_NOVENDOR_template\layer.cpp`. To call the real OpenXR implementation for the function, simply invoke the corresponding method in the `OpenXrApi` base class.
- `requested_functions`: the list of OpenXR functinos that the API layer may use. This list is used to create wrappers to the real OpenXR implementation without overriding the function. To invoke a function, simply call the corresponding method in the `OpenXrApi` base class.
- `extensions`: if any of the function declared in the lists above is not part of the OpenXR core spec, you must specify the corresponding extensions to search in (eg: `XR_KHR_D3D11_enable`).

### (Optional) Require OpenXR extensions in `XR_APILAYER_NOVENDOR_template\framework\dispatch.cpp`

If the API layer requires a specific OpenXR extension, update the `implicitExtensions` vector in `XR_APILAYER_NOVENDOR_template\framework\dispatch.cpp` to list the extensions to request.

WARNING: Not all OpenXR runtimes may support all extensions. It is recommended to query the list of supported extensions from the OpenXR runtime and only add to the `implicitExtensions` the extensions that are advertised by the platform. You can use the following code to query the list of extensions:
```
    CHECK_XRCMD(apiLayerInfo->nextInfo->nextGetInstanceProcAddr(
        XR_NULL_HANDLE,
        "xrEnumerateInstanceExtensionProperties`:
        reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateInstanceExtensionProperties)));

    uint32_t extensionsCount = 0;
    CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionsCount, nullptr));
    std::vector<XrExtensionProperties> extensions(extensionsCount, {XR_TYPE_EXTENSION_PROPERTIES});
    CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(
        nullptr, extensionsCount, &extensionsCount, extensions.data()));
```

### (Optional) Use general tracelogging patterns

Writing the calls to `TraceLoggingWrite()` in each function implemented by the layer may be tedious... You can look at the source code of the [PimaxXR runtime](https://github.com/mbucchia/Pimax-OpenXR/tree/main/pimax-openxr), which implements the tracelogging calls for all functions of the core OpenXR specification plus a handful of extensions. You may copy/paste the `TraceLoggingWrite()` calls from it.


### Use of the Configuration file
You can find the main configuration file named OpenXR-MotionCompensation.ini in the directory ...\OpenXR-MotionCompensation\configuration. This is copied to your output folder and can be used to modify:
- Tracker: the kind of tracker to use for motion compensation
- Filters: the strength and the number of filtering stages for both translational and rotational filtering
- Keyboard shortcuts: keyboard presses to activate/deactivate or modify motion compensation during runtime

When an OpenXR Application is launched with the motion compensation layer active, a *.ini file named after the application is created. This file can be used to apply application specific changes to the configuration by copying the corresponding file section(s) from the main configuration file.

To combine multiple keys for a sigle shortcut they need to be separated by '+' with no spaces in between the key names.

List of keyboard shortcut names that can be used:

 - `SHIFT`: shift key
 - `CONTROL`: ctrl key
 - `MENU`: alt key
 - `LSHIFT`: left shift key
 - `RSHIFT`: right shift key
 - `LCONTROL`: left ctrl key
 - `RCONTROL`: right ctrl key
 - `LMENU`: left alt key
 - `RMENU`: right alt key
 - `0` - `9`: numerical key
 - `A` - `Z`: alphbetical key
 - `SEMICOLON`: ;: key (US),
 - `PLUS`: + key (any country)
 - `COMMA`: , key (any country
 - `DASH`: - key (any country)
 - `PERIOD`: . key (any country)
 - `SLASH`: /? key (US)
 - `BACKQUOTE`: `~ key (US)
 - `OPENSQUARE`: [{ key (US)
 - `BACKSLASH`: \\| key (US)
 - `CLOSESQUARE`: ]} key (US)
 - `QUOTE`: '" key (US)
 - `BACK`:  backspace key
 - `TAB`: tabulator key
 - `CLEAR`: clr key
 - `RETURN`: return key
 - `PAUSE`: pause key
 - `CAPITAL`: caps lock key
 - `ESCAPE`: esc key
 - `SPACE`: space key
 - `LEFT`: cursor left key
 - `UP`: cursor up key
 - `RIGHT`: cursor right key
 - `DOWN`: cursor down key
 - `SELECT`: select key
 - `PRINT`: print key
 - `EXECUTE`: execute key
 - `SNAPSHOT`: print screen key
 - `PRIOR`: page up key
 - `NEXT`: page down key
 - `END`: end key
 - `HOME`: home key
 - `INSERT`: ins key
 - `DELETE`: del key
 - `HELP`: help key
 - `NUMPAD0`: 0 key on numpad
 - `NUMPAD1`: 1 key on numpad 
 - `NUMPAD2`: 2 key on numpad
 - `NUMPAD3`: 3 key on numpad
 - `NUMPAD4`: 4 key on numpad
 - `NUMPAD5`: 5 key on numpad
 - `NUMPAD6`: 6 key on numpad
 - `NUMPAD7`: 7 key on numpad
 - `NUMPAD8`: 8 key on numpad
 - `NUMPAD9`: 9 key on numpad
 - `NUMPADMULTIPLY`: * key on numpad
 - `NUMPADADD`: + key on numpad
 - `NUMPADSEPARATOR`: separator key on numpad
 - `NUMPADSUBTRACT`: - key on numpad
 - `NUMPADDECIMAL`: . key on numpad
 - `NUMPADDIVIDE`: / key on numpad
 - `F1`: F1 key
 - `F2`: F2 key
 - `F3`: F3 key
 - `F4`: F4 key
 - `F5`: F5 key
 - `F6`: F6 key
 - `F7`: F7 key
 - `F8`: F8 key
 - `F9`: F9 key
 - `F10`: F10 key
 - `F11`: F11 key
 - `F12`: F12 key
 - `NUMLOCK`: numlock key
 - `SCROLL`: scroll lock key
 - `GAMEPAD_A`: A button on gamepad
 - `GAMEPAD_B`: B button on gamepad
 - `GAMEPAD_X`: X button on gamepad
 - `GAMEPAD_Y`: Y button on gamepad
 - `GAMEPAD_RIGHT_SHOULDER`: right shoulder button on gamepad
 - `GAMEPAD_LEFT_SHOULDER`: left shoulder button on gamepad
 - `GAMEPAD_LEFT_TRIGGER`: left trigger button on gamepad
 - `GAMEPAD_RIGHT_TRIGGER`: right trigger button on gamepad
 - `GAMEPAD_DPAD_UP`: digital pad up on gamepad
 - `GAMEPAD_DPAD_DOWN`: digital pad down on gamepad
 - `GAMEPAD_DPAD_LEFT`: digital pad left on gamepad
 - `GAMEPAD_DPAD_RIGHT`: digital pad right on gamepad
 - `GAMEPAD_MENU`: start button on gamepad
 - `GAMEPAD_VIEW`: view button on gamepad
 - `GAMEPAD_LEFT_THUMBSTICK_BUTTON`: left thumbstick pressed on gamepad
 - `GAMEPAD_RIGHT_THUMBSTICK_BUTTON`: right thumbstick pressed on gamepad
 - `GAMEPAD_LEFT_THUMBSTICK_UP`: left thumbstick up on gamepad
 - `GAMEPAD_LEFT_THUMBSTICK_DOWN`: left thumbstick down on gamepad
 - `GAMEPAD_LEFT_THUMBSTICK_RIGHT`: left thumbstick left on gamepad
 - `GAMEPAD_LEFT_THUMBSTICK_LEFT`: left thumbstick right on gamepad
 - `GAMEPAD_RIGHT_THUMBSTICK_UP`: right thumbstick up on gamepad
 - `GAMEPAD_RIGHT_THUMBSTICK_DOWN`: right thumbstick down on gamepad
 - `GAMEPAD_RIGHT_THUMBSTICK_RIGHT`: right thumbstick left on gamepad
 - `GAMEPAD_RIGHT_THUMBSTICK_LEFT`: right thumbstick right on gamepad
