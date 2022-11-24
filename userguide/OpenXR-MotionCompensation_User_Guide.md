# OpenXR Motion Compensation

**DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks!**

Version: 0.2.0

**This document contains instructions on how to use OpenXR motion compensation [API layer](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#api-layers).**

Limitations:

- The motion compensation API Layer is made for Windows 64-bit only.
- The software (obviously) only works with VR/AR applications using an OpenXR runtime implementation.



## Contact
Feel free to join our [Discord community](https://discord.gg/BVWugph5XF) for feedback and assistance.

You can find the [source code](https://github.com/BuzzteeBear/OpenXR-MotionCompensation) and the [latest release](https://github.com/BuzzteeBear/OpenXR-MotionCompensation/releases) or report issues at github.com.

If you are (or know someone) willing and able to support the software development (mostly C++, maybe some GUI stuff later on) side of the project, feel free to contact **@BuzzteeBear** on the Discord server to ask about ways to contribute.

Donations to the project are very welcome and can be made via [Paypal](https://www.paypal.com/donate/?hosted_button_id=Q64DT2ADFCBU8). 

## Installation

### Execute installer executable
Just execute the installation executable called `Install OpenXR-MotionCompensation <current_version>.exe` and follow the instructions.
A few hints concerning the installation process:
- If you're upgrading from a version prior to 0.2.0, it is recommended to target the installation directory already existing. This will allow the installer to transfer your existing configuration files into the `appdata/local/OpenXR-MotionCompensation` directory that is used from version 0.2.0 onwards.
- Using a subdirectory of `program files` as installation target is recommended, especially for compatibility with WMR based headsets.
- Although the installation needs adminstrative privileges make sure to run the installation executable using the windows account you're using to launch your games/Open XR applications. This enables the installer to put the configuraton file(s) into the correct appdata directory.
- If somethimng goes wrong on installation and you don't know what or why, you can check the log file `Setup Log <yyyy-mm-dd xxx>.txt` that is created in the `%TEMP%` folder.

### Conflict with other OpenXR API layers
There may be issues with other OpenXR API layers that are installed on your system. For the most part they can be solved by using the correct order of installation (because that implicitly determines the order in which the layers are loaded).  
According to user feedback following constraints seem to be working:
- **XRNeckSaver** needs to be installed before OXRMC.
- **OpenKneeBoard** needs to be installed before OXRMC. 
  - but it is (or at least was at some point) putting its registry key in `...HKEY_CURRENT_USER/...` while OXRMC uses `...HKEY_LOCAL_MACHINE/...` . So if you're having trouble changing the loading order, try moving the key for OpenKneeboard from `Computer\HKEY_CURRENT_USER\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit` to `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit`.
- **OpenXR Toolkit** does it's own magic on installation and doesn't care if it's installed before or after OXRMC.

### Optional: Confirm correct installation

You can use the application [OpenXR Explorer](https://github.com/maluoi/openxr-explorer/releases) to verify the correct installation:
- Install OpenXR Explorer
- Connect your headset
- Start your corresponding VR runtime application (e.g. SteamVR, Oculus App, Mixed Reality Portal, Varjo Base, PiTool, etc.)
- Start OpenXR explorer
- Search for the section `xrEnumerateApiLayerProperties` (should be in the middle column at the bottom by default)
- Check if the entry `XR_APILAYER_NOVENDOR_motion_compensation` with version `v1` exists

### Update

To use an updated version of OXRMC it is sufficient to download and run the latest installation executable. If you want to change the installation directory you will have to uninstall the previous version first.

### Uninstallation

To remove the OpenXR-MotionCompensation layer just use windows settings/control panel as you would do witgh any other application. During uninstallation you can choose to delete your configuration and log files in the appdata directory or to keep them for later use. 
- If somethimng goes wrong on installation and you don't know what or why, you can check the log file `Uninstall Log <yyyy-mm-dd xxx>.txt` that is created in the `%TEMP%` folder.

## Configuration

Configuration files can be found at `...\Users\*<Your_Username>*\AppData\Local\OpenXR-MotionCompensation\OpenXR-MotionCompensation.log`.
After intial installtion this directory contains the default configuration file `OpenXR-MotionCompensation.ini`. You can make changes to that file to configure options you want to be the same for all OpenXR applications.
Upon starting an OpenXR application with the API layer active for the first time, a configuration file named after the application is created in the same directory. You can use it to copy (partial) sections from the default configuration file whenever you want to make changes only for that application specifically.

### Use of the Configuration file

What you can modify in a configuration file:
- the tracker to use for motion compensation
- the strength and the number of filtering stages for both translational and rotational filters
- keyboard inputs (e.g. to activate/deactivate or recalibrate motion compensation during runtime)  
**Note that all keys and values in the configuration file(s) are case sensitive. That means all [keyboard shortcuts](#list-of-keyboard-bindings) must only contain capital letters, numbers and/or underscores**

### Sections in configuration file

- `tracker`: The following tracker `type` keys are available:
  - `controller`: use either the left or the right motion controller as reference tracker. Valid options for the key `side` are `left` and `right` (**Note that changing side requires a restart of the vr session**)
  - `srs`: use the virtual tradcker data provided by SRS motion software when using a Witmotion (or similar?) sensor on the motion rig.
  - `flypt` use the virtual tracker data provided by FlyPT Mover.
  - `yaw`: use the virtual tracker data provided by Yaw VR and Yaw 2. Either while using SRS or Game Engine.
  - the keys `offset_...`, `use_cor_pos` and `cor_...` are used to handle the configuration of the center of rotation (cor) for all available virtual trackers.
- `translational_filter` and `rotational_filter`: set the filtering magnitude (key `strength` with valid options between **0.0** and **1.0**) number of filtering stages (key `order`with valid options: **1, 2, 3**).  
- `shortcuts`: can be used to ocnfigure shortcuts for different commands (See [List of keyboard bindings](#list-of-keyboard-bindings) for valid values):
  - `activate`- turn motion compensation on or off. Note that this implicitly triggers the calibration action (`center`) if that hasn't been executed before.
  - `center` - recalibrate the neutral reference pose of the tracker
  - `translation_increase`, `translation_decrease` - modify the strength of the translational filter. Changes made during runtime can be saved by using a save command (see below).
  - `rotation_increase`, `rotation_decrease` = see above, but for rotational filter
  - `offset_forward`, `offset_back`, `offset_up`, `offset_down`, `offset_right`, `offset_left` - move the center of rotation (cor) for a virtual tracker. The directions are aligned with the forward vector set with the `center` command. Changes made during runtime can be saved by using a save command (see below)
  - `rotate_right`, `rotate_left` - rotate the aforementioned forward vector aroung the gravitational (yaw-)axis. Note that these changes cannot be saved. Therefore changing the offset position AFTER rotating manually and saving the offset values will result in the cor being a different offset position after relaoding those saved values
  - `cor_debug_mode` - since OXRMC is lacking a visual feedback of the positon of the cor, you can activate this mode to be able to rotate the virtual world around the cor. Translations of the motion tracker are ignored in this mode.  
  **Beware that this can be a nauseating experience because your eyes suggest that your moving inside the virtual world, while your inner ear tells your brain otherwise. You can stop motion compensation at any time by pressing the activation shortcut again!** 
  - `save_config` -  write current filter strength and cor offsets to global config file 
  - `save_config_app` -  write current filter strength and cor offsets to application specific config file. Note that values in this file will precedent values in the global config file. 
  - `reload_config` - read in and apply configuration for current app from config files. For technical reasons motion compensation is automatically deactivated and the reference tracker pose is invalidated upon configuration reload.

- `debug`: For debugging reasons you can check, if the motion compensation functionality generally works on your system without using tracker input from the motion controllers at all by setting `testrotation` value to `1` and reloading the configuration. You should be able to see the world rotating around you after pressing the activation shortcut.  
**Beware that this can be a nauseating experience because your eyes suggest that your head is turning in the virtual world, while your inner ear tells your brain otherwise. You can stop motion compensation at any time by pressing the activation shortcut again!** 

## Using a virtual tracker

To use a virtual tracker set parameter `tracker_type` according to the motion software that is providing the data for motion compensation on your system:
- `yaw`: Yaw Game Engine (or Sim Racing Studio when using rotational data provided by Yaw VR or Yaw 2)
- `srs`: Sim Racing Studio (experimental)
- `flypt`: FlyPT Mover (experimental)

### Calibrate virtual tracker
To enable OXRMC to correlate translation and rotation of the rig to the virtual space correctly when using a virtual tracker, you have to provide the information where the center of rotation (cor) of your motion rig is positioned and which way is forward. This is done with the following steps:

0. Calculate, measure or estimate the distance between your headset and the center of rotation of your motion rig in forward/backward, up/down and left/right direction (I was told most 6 dof rigs rotate around the bottom of the seat but your mileage may vary).
1. Enter the offset values in the config file
2. Start the OpenXR application of your choice
3. Bring your motion rig in neutral position
3. Sit in your rig 
4. put your headset on and face forward (~ direction surge). Potential rotation of the hmd on roll and pitch angle is ignored for the calculation
5. issue the calibration command by activating the `center` shortcut. You can also do this implicitly by activating motion compensation if you haven't (re)calibrated since last loading of the configuration.

- If you're using YawVR Game Engine you can also use the parameters `Head Distance` and `Height` in its Motion Compensation tab to specify the offset of the cor. Head distance is basically equal to `offset_forward` in the configration file. But note that the height parameter is measured upwards from the bottom of your playspace, so you'll need to have that setup correctly in order to use that feature.

### Saving and the cor location (experimental)
The current position and orientation of the cor is part of the configuration and can be saved to the (global or app-specific) config file. When your satisfied with the current setting you can set the config key `use_cor_pos` to `1`. This causes the cor position to be loaded from the config file when calibrating instead of being determined using the hmd position and the offset values.  
**Note that this functionality is still expermiental and may not work with all HMD vendors. Setting up the playspace in the VR runtime configuration of your hmd might help to get this orking correctly. Rumor has it that some HMDs need to be started/initialized at the exact same location for the playspace coordinates to be consistent in between uses.**  
Feedback on success or failure of this functionality using different VR systems is expicitly welcome and can be left on the [discord server](#contact) of the project.

## Running your application
1. make sure your using OpenXR as runtime in the application you wish to use motion compensation in
2. start application
3. center the in-app view
4. activate the motion controller you configured and mount it on your motion rig
5. bring your motion rig to neutral position
6. Reset the ingame view if necesary
7. press the `activate` shortcut (**CTRL** + **INSERT** by default). This implicitly sets the neutral reference pose for the tracker
- if necessary you can recalibrate the tracker by pressing the `center` shortcut (**CTRL** + **DEL** by default) while the motion rig is in neutral position 
- you can increase or decrease the filter strength of translational and rotational filters
- you can modify the cor offset when currently using a virtual tracker
- after modifying filter strength or cor offset for virtual tracker you can save your changes to the default configuration file 
- after modifying the config file(s) manually you can use the `reload_config` shortcut (**CTRL** + **SHIFT** + **L** by default) to restart the OXRMC software with the new values. 

## Additional Notes
- Upon activating any shortcut you get audible feedback, corresponding to the performed action (or an error, if something went wrong).

- If you recenter the in-app view during a session the reference pose is reset by default. Therefore you should only do that while your motion rig is in neutral position. It is possible (depending on the application) that this automatic recalibration is not triggered, causing the view and reference pose to be out of sync and leading to erroneous motion compensation. You should do the following steps to get this corrected again:
  1. deactivate motion compensation by pressing the `activate` shortcut
  2. bring your motion rig to neutral position. Face forward if yout using a virtual tracker
  3. recalibrate by pressing the `center` shortcut
  4. reactivate motion compensation by pressing the `activate` shortcut

- If the motion controller cannot be tracked for whatever reason (or if the memory mapped file containing the motion data for a virtual tracker cannot be found or accessed) when activating motion compensation or recalibrating the tracker pose, the API layer is unable to set the reference pose and motion compensation is (or stays) deactivated.

## Logging
The motion compensation layers logs rudimentary information and errors in a text file located at **...\Users\<Your_Username>\AppData\Local\OpenXR-MotionCompensation\OpenXR-MotionCompensation.log**. After unexpected behaviour or a crash you can check that file for abormalities or error reports.

If you encounter repeatable bugs or crashes you can use the Windows Performance Recorder Profile (WPRP) tracelogging in combination with the configuration contained within `scripts\Trace_OpenXR-MotionCompensation.wprp` to create a more detailed protocol.

[Tracelogging](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-portal) can become very useful to investigate user issues.

To capture a trace for the API layer:

- start the OpenXr application
- Open a command line prompt or powershell in administrator mode and in a folder where you have write permissions
- Begin recording a trace with the command: `wpr -start path\to\Trace_OpenXR-MotionCompensation.wprp -filemode`
- Leave that command prompt open
- Reproduce the crash/issue
- Back to the command prompt, finish the recording with: `wpr -stop arbitrary_name_of_file.etl`
- These files are highly compressible!

You can send the trace file to the developer or use an application such as [Tabnalysis](https://apps.microsoft.com/store/detail/tabnalysis/9NQLK2M4RP4J?hl=en-id&gl=ID) to inspect the content yourself.


## List of keyboard bindings
To combine multiple keys for a single shortcut they need to be separated by '+' with no spaces in between the key descriptors.

List of supported shortcut key names:

Name | Key 
:---|:---
`SHIFT` | shift key
`CTRL` | ctrl key
`ALT` | alt key
`LSHIFT` | left shift key
`RSHIFT` | right shift key
`LCTRL` | left ctrl key
`RCTRL` | right ctrl key
`LALT` | left alt key
`RALT` | right alt key
`0`- `9` | numerical key
`A`- `Z` | alphbetical key
`BACKQUOTE` | \`~ key (US)|
`TAB` | tabulator key
`CAPS` | caps lock key
`PLUS` | + key (any country)
`MINUS` | - key (any country)
`OPENBRACKET` | [{ key (US)
`CLOSEBRACKET` | ]} key (US)
`SEMICOLON` | ;: key (US)
`QUOTE` | '" key (US)
`BACKSLASH` | \\\| key (US)
`COMMA` | , key (any country)
`PERIOD` | . key (any country)
`SLASH` | /? key (US)
`BACK` |  backspace key
`CLR` | clr key
`RETURN` | return key
`ESC` | esc key
`SPACE` | space key
`LEFT` | cursor left key
`UP` | cursor up key
`RIGHT` | cursor right key
`DOWN` | cursor down key
`INS` | ins key
`DEL` | del key
`HOME` | home key
`END` | end key
`PGUP` | page up key
`PGDN` | page down key
`NUM0` | 0 key on NUM
`NUM1` | 1 key on NUM 
`NUM2` | 2 key on NUM
`NUM3` | 3 key on NUM
`NUM4` | 4 key on NUM
`NUM5` | 5 key on NUM
`NUM6` | 6 key on NUM
`NUM7` | 7 key on NUM
`NUM8` | 8 key on NUM
`NUM9` | 9 key on NUM
`NUMLOCK` | numlock key
`NUMDIVIDE` | / key on NUM
`NUMMULTIPLY` | * key on NUM
`NUMSUBTRACT` | - key on NUM
`NUMADD` | + key on NUM
`NUMDECIMAL` | . key on NUM
`NUMSEPARATOR` | separator key on NUM
`F1` | F1 key
`F2` | F2 key
`F3` | F3 key
`F4` | F4 key
`F5` | F5 key
`F6` | F6 key
`F7` | F7 key
`F8` | F8 key
`F9` | F9 key
`F10` | F10 key
`F11` | F11 key
`F12` | F12 key
`PRTSC` | print screen key
`SCROLL` | scroll lock key
`PAUSE` | pause key
`SELECT` | select key
`PRINT` | print key
`HELP` | help key
`EXEC` | execute key
`GAMEPAD_A` | A button on gamepad
`GAMEPAD_B` | B button on gamepad
`GAMEPAD_X` | X button on gamepad
`GAMEPAD_Y` | Y button on gamepad
`GAMEPAD_RIGHT_SHOULDER` | right shoulder button on gamepad
`GAMEPAD_LEFT_SHOULDER` | left shoulder button on gamepad
`GAMEPAD_LEFT_TRIGGER` | left trigger button on gamepad
`GAMEPAD_RIGHT_TRIGGER` | right trigger button on gamepad
`GAMEPAD_DPAD_UP` | digital pad up on gamepad
`GAMEPAD_DPAD_DOWN` | digital pad down on gamepad
`GAMEPAD_DPAD_LEFT` | digital pad left on gamepad
`GAMEPAD_DPAD_RIGHT` | digital pad right on gamepad
`GAMEPAD_START` | start button on gamepad
`GAMEPAD_VIEW` | view button on gamepad
`GAMEPAD_LEFT_THUMBSTICK_BUTTON` | left thumbstick pressed on gamepad
`GAMEPAD_RIGHT_THUMBSTICK_BUTTON` | right thumbstick pressed on gamepad
`GAMEPAD_LEFT_THUMBSTICK_UP` | left thumbstick up on gamepad
`GAMEPAD_LEFT_THUMBSTICK_DOWN` | left thumbstick down on gamepad
`GAMEPAD_LEFT_THUMBSTICK_RIGHT` | left thumbstick left on gamepad
`GAMEPAD_LEFT_THUMBSTICK_LEFT` | left thumbstick right on gamepad
`GAMEPAD_RIGHT_THUMBSTICK_UP` | right thumbstick up on gamepad
`GAMEPAD_RIGHT_THUMBSTICK_DOWN` | right thumbstick down on gamepad
`GAMEPAD_RIGHT_THUMBSTICK_RIGHT` | right thumbstick left on gamepad
`GAMEPAD_RIGHT_THUMBSTICK_LEFT` | right thumbstick right on gamepad
