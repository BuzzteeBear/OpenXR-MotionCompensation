# OpenXR Motion Compensation

This document is meant for developers willing to contribute. For just **using the software**, check out the [**user guide** here](https://github.com/BuzzteeBear/OpenXR-MotionCompensation/blob/main/userguide/OpenXR-MotionCompensation_User_Guide.md).

This repository contains the source project for a basic [OpenXR API layer](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#api-layers) for motion compensation. 
It is based on the [OpenXR API layer template](https://github.com/mbucchia/OpenXR-Layer-Template) by Matthieu Bucchianeri. 
More about [OpenXR](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html) and the [OpenXR Loader](https://www.khronos.org/registry/OpenXR/specs/1.0/loader.html).

## Purpose of this project

When using a motion rig in combination with a VR headset (hmd) he movement of the rig causes the in-game camera to change along with your position in the real world. In simulations for example you're basically feel being pushed around inside the cockpit when the motion rig moves.  
Motion compensation reduces or ideally removes that effect by locking the in-game world to the pose of the motion rig.
This software aims to provide an API layer for motion compensation to be used with applications and hmds supporting the OpenXR standard.  
To be able to do that, the software needs to be informed on the motion rig movement / position. This can be achieved using a tracker, which is either a physical object attached to the motion rig and tracked by the VR runtime (e.g. a motion controller or a vive puck) or a virtual tracker using data from the motion software driving the motion rig. 
 
## Building this project

Prerequisites:

- Visual Studio 2022 or above;
- NuGet package manager (installed via Visual Studio Installer);
- Python 3 interpreter (installed via Visual Studio Installer or externally available in your PATH).
- Inno Setup Compiler for creating an installation executable
- Git client with LFS support installed

Limitations:

- The API Layer is made for Windows 64-bit only.

DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks.

## Customization
The easiest way to get a uable project set up is to use the `Clone a repository` function of Visual Studio with the url of this github repository. Make sure the project path does not contain any whitespaces (or the call of the python script for source code generation will fail). Also ensure that the [Visual Studio git client can handle LFS](https://stackoverflow.com/a/47921547) for the external sdk dependencies below. 

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

WARNING: Not all OpenXR runtimes may support all extensions. The API layer will interrogate the OpenXR runtime (and upstream layers) and only request the extensions that are advertised by the platform. You can subsequently call `GetGrantedExtensions()` to retrieve the list of implicit extensions that were successfully requested.

### (Optional) Use general tracelogging patterns

Writing the calls to `TraceLoggingWrite()` in each function implemented by the layer may be tedious... You can look at the source code of the [PimaxXR runtime](https://github.com/mbucchia/Pimax-OpenXR/tree/main/pimax-openxr), which implements the tracelogging calls for all functions of the core OpenXR specification plus a handful of extensions. You may copy/paste the `TraceLoggingWrite()` calls from it.

## Additional Information

More details on how to use the OXRMC software can be found in the user guide `OpenXR-MotionCompensation_User_Guide.md`.