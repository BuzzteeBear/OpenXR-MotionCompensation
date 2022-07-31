# OpenXR Motion Compensation

Version: 0.0.1

This package contains the binary and scripts needed for OpenXR motion compensation [API layer](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#api-layers). 

Limitations:

- The motion compensation API Layer is made for Windows 64-bit only.

DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks.

## Installation
The following steps are necessary to deploy the API layer on your System:

### Extract archive
Copy the files contained in this archive to a directory of your choice. The directory should not have any special write access restrictions since the binary creates a configuration file for each application it is loaded from (see [Configuration](#configuration) section below).

### Execute install script
- Right click on the file `Install-OpenXR-MotionCompensation.ps1` and select the option *Run with PowerShell*
- Confirm the UAC dialog
- The will create a registry entry making it possible for to load the api layer on OpenXR initialization

### Optional: Confirm correct installation
You can use the application [OpenXR Explorer](https://github.com/maluoi/openxr-explorer/releases) to verify the correct installation:
- Install OpenXR Explorer
- Connect your headset
- Start your corresponding VR runtime application (e.g. SteamVR, Oculus App, Mixed Reality Portal, Varjo Base, PiTool, etc.)
- Start OpenXR explorer
- Search for the section `xrEnumerateInstanceExtensionProperties` (should be in the middle column at the bottom by default)
- Check if the entry `XR_APILAYER_NOVENDOR_motion_compensation` with version `v1` exists

### Uninstallation
To remove the OpenXr Motion Compensation layer: 
- execute the script `Uninstall-OpenXR-MotionCompensation.ps1` as described in the [Execute install script](#execute-install-script) section above
- optional: delete the files in the directory you chose for installation

## Configuration
The archive contains the default configuration file `OpenXR-MotionCompensation.ini`. You can make changes to that file to configure options you want to be the same for all OpenXR applications.

Upon starting an OpenXR application with the API layer active for the first time, a configuration file named after the application is created in the installation directory. You can use it to copy (partial) sections from the default configuration file whenever you want to make changes only for that application specifically.

### Use of the Configuration file
What you can modify in a configuration file:
- the tracker to use for motion compensation
- the strength and the number of filtering stages for both translational and rotational filters
- keyboard inputs (e.g. to activate/deactivate or recalibrate motion compensation during runtime)

### Sections in configuration file
- `tracker`: use either the left or the right motion controller as reference tracker. Valid options for the key `side` are `left` and `right`. The key `type` has no funcionality yet
- `translational_filter` and `rotational_filter`: modify the filtering magnitude (key `strength `with valid options between `0.0` and `1.0`) number of filtering stages (key `order`with valid options: `1`,`2`,`3`)  
- `shortcuts`: `activate` to turn motion compensation on or off. `center` to reset the neutral reference pose of the tracker. Other keys are not funtional yet. See [List of keyboard bindings](appendix:_list-of-keyboard-bindings) for valid options.

## Running your application
- make sure your using OpenXR as runtime in the application you wish to use motion compensation in
- start application
- center the in-app view
- activate the motion controller you configured and mount it on your motion rig
- bring your motion rig to neutral position
- press the `activate` shortcut you configured (`CTRL` + `HOME` by default). This implicitly sets the neutral reference pose for the tracker
- if necessary you can recalibrate the tracker by pressing the `center` shortcut (`CRTL` + `END` by default) while the motion rig is in neutral position 
- you can increase the filter strength of translational and rotational filter using the following shortcuts:
    * `translation_increase` (`CTRL` + `+` by default)
    * `translation_decrease` (`CTRL` + `-` by default)
    * `rotation_increase` (`CTRL` + `0` by default)
    * `translation_decrease` (`CTRL` + `9` by default)
- after modifying filter strength you can save your changes to the application specific configuration file using the `save_config` shortcut (`CTRL` + `SHIFT` + `S` by default). If you want to use these values globally for all applicattions you can copy them to the default configuration file `OpenXR-MotionCompensation.ini`.
- after modifying the config file(s) can use the `reload_config` shortcut to use the new values. For technical reasons motion compensation is automatically deactivated and the reference tracker pose is invalidated upon configuration reload. Controller side cannot be changed during runtime in the current version.

### Notes
Upon activating a shortcut you get audible feedback, using either the confirmation, warning or error sound configured in your windows system control, depending on the successful execution of the action you activated.

If you recenter the in-app view during a session the reference pose is reset by default. Therefore you should only do that while your motion rig is in neutral position. It is possible (depending on the application) that this automatic recalibration is not triggered, causing the view and reference pose to be out of sync and leading to erroneous motion compensation. You should do the following steps to get this corrected again:
- deactivate motion compensation by pressing the `activate` shortcut
- bring your motion rig to neutral position
- press the `center` shortcut 
- reactivate motion compensation by pressing the `activate` shortcut

If the motion controller cannot be tracked for whatever reason when activating motion compensation or recalibrating the tracker pose, the API layer is unable to set the reference pose and motion compensation is (or stays) deactivated.

## Logging
The motion compensation layers logs rudimentary information and errors in a text file loacated at `...\Users\<Your_Username>\AppData\XR_APILAYER_NOVENDOR_motion_compensation.log`. After unexpected behaviour or a crash you can check that file for abormalities or error reports.

If you encounter repeatable bugs or crashes you can use the Windows Performance Recorder Profile (WPRP) tracelogging in `scripts\Tracing.wprp` to create a more detailed protocol.

[Tracelogging](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-portal) can become very useful to investigate user issues.

To capture a trace for the API layer:

- start the OpenXr application
- Open a command line prompt or powershell in administrator mode and in a folder where you have write permissions
- Begin recording a trace with the command: `wpr -start path\to\Tracing.wprp -filemode`
- Leave that command prompt open
- Reproduce the crash/issue
- Back to the command prompt, finish the recording with: `wpr -stop arbirtary_name_of_file.etl`
- These files are highly compressible!

You can send the trace file to the developer or use an application such as [Tabnalysis](https://apps.microsoft.com/store/detail/tabnalysis/9NQLK2M4RP4J?hl=en-id&gl=ID) to inspect the content yourself.

## Debugging
For debugging reasons you can check, if the motion compensation functionality generally works on your system without using tracker input from the motion controllers at all. To do so, you have to set `testrotation` in the `debug` section to `1` and (re)start your OpenXR application of choice. Then you should be able to see the world rotating around you after pressing the activation shortcut.

*Beware that this can be a nauseating experience because your eyes suggest that your head is turning in the virtual world, while your inner ear tells your brain otherwise. You can stop motion compensation at any time by pressing the activation shortcut again!* 

## Appendix: List of keyboard bindings
To combine multiple keys for a single shortcut they need to be separated by '+' with no spaces in between the key descriptors.

List of supported shortcut key names:

 - `SHIFT`: shift key
 - `CTRL`: ctrl key
 - `ALT`: alt key
 - `LSHIFT`: left shift key
 - `RSHIFT`: right shift key
 - `LCTRL`: left ctrl key
 - `RCTRL`: right ctrl key
 - `LALT`: left alt key
 - `RALT`: right alt key
 - `0` - `9`: numerical key
 - `A` - `Z`: alphbetical key
 - `BACKQUOTE`: `~ key (US)
 - `TAB`: tabulator key
 - `CAPS`: caps lock key
 - `PLUS`: + key (any country)
 - `MINUS`: - key (any country)
 - `OPENBRACKET`: [{ key (US)
 - `CLOSEBRACKET`: ]} key (US)
 - `SEMICOLON`: ;: key (US)
 - `QUOTE`: '" key (US)
 - `BACKSLASH`: \\| key (US)
 - `COMMA`: , key (any country
 - `PERIOD`: . key (any country)
 - `SLASH`: /? key (US)
 - `BACK`:  backspace key
 - `CLR`: clr key
 - `RETURN`: return key
 - `ESC`: esc key
 - `SPACE`: space key
 - `LEFT`: cursor left key
 - `UP`: cursor up key
 - `RIGHT`: cursor right key
 - `DOWN`: cursor down key
 - `INS`: ins key
 - `DEL`: del key
 - `HOME`: home key
 - `END`: end key
 - `PGUP`: page up key
 - `PGDN`: page down key
 - `NUM0`: 0 key on NUM
 - `NUM1`: 1 key on NUM 
 - `NUM2`: 2 key on NUM
 - `NUM3`: 3 key on NUM
 - `NUM4`: 4 key on NUM
 - `NUM5`: 5 key on NUM
 - `NUM6`: 6 key on NUM
 - `NUM7`: 7 key on NUM
 - `NUM8`: 8 key on NUM
 - `NUM9`: 9 key on NUM
 - `NUMLOCK`: numlock key
 - `NUMDIVIDE`: / key on NUM
 - `NUMMULTIPLY`: * key on NUM
 - `NUMSUBTRACT`: - key on NUM
 - `NUMADD`: + key on NUM
 - `NUMDECIMAL`: . key on NUM
 - `NUMSEPARATOR`: separator key on NUM
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
 - `PRTSC`: print screen key
 - `SCROLL`: scroll lock key
 - `PAUSE`: pause key
 - `SELECT`: select key
 - `PRINT`: print key
 - `HELP`: help key
 - `EXEC`: execute key
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
 - `GAMEPAD_START`: start button on gamepad
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
