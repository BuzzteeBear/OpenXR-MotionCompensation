# OpenXR Motion Compensation

Version: 0.1.0

**Please open this file with a browser or another application able to display [markdown files](https://www.markdownguide.org/getting-started) correctly.**

This package contains the binary and scripts needed for OpenXR motion compensation [API layer](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#api-layers). 

Limitations:

- The motion compensation API Layer is made for Windows 64-bit only.

**DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks!**

## Installation

The following steps are necessary to deploy the API layer on your System:

### Extract archive

Copy the files contained in this archive to a subdirectory (name it as you like) in your **Program Files** location. The directory should not have any special write access restrictions since the binary creates a configuration file for each application it is loaded from (see [Configuration](#configuration) section below).

### Execute install script

- Right click on the file `Install-OpenXR-MotionCompensation.ps1` and select the option **Run with PowerShell**
- Confirm the UAC dialog
- The will create a registry entry making it possible for to load the api layer on OpenXR initialization

### Optional: Confirm correct installation

You can use the application [OpenXR Explorer](https://github.com/maluoi/openxr-explorer/releases) to verify the correct installation:
- Install OpenXR Explorer
- Connect your headset
- Start your corresponding VR runtime application (e.g. SteamVR, Oculus App, Mixed Reality Portal, Varjo Base, PiTool, etc.)
- Start OpenXR explorer
- Search for the section `xrEnumerateApiLayerProperties` (should be in the middle column at the bottom by default)
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
- `translational_filter` and `rotational_filter`: set the filtering magnitude (key `strength` with valid options between **0.0** and **1.0**) number of filtering stages (key `order`with valid options: **1, 2, 3**)  
- `shortcuts`: Can be used to ocnfigure shortcuts for different commands (See [List of keyboard bindings](appendix:_list-of-keyboard-bindings) for valid values):
  - `activate`- turn motion compensation on or off. Note that this implicitly triggers the `center` action if that hasn't been executed before.
  - `center` - recalibrate the neutral reference pose of the tracker
  - `translation_increase`, `translation_decrease` - modify the strength of the translational filter. Changes made during runtime can be saved by using a save command (see below)
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

**Note that lacking a motion rig at the moment I was unable to verify that the signum of the positional and rotational values provided by SRS and FlyPT Mover is set correctly. Any feedback on this matter is highly appreciated!**

### Calibrate virtual tracker
To enable OXRMC to correlate translation and rotation of the rig to the virtual space correctly when using a virtual tracker, you have to provide the information where the center of rotation (cor) of your motion rig is positioned and which way is forward. This is done with the following steps:

0. Calculate, measure or estimate the distance between your headset and the center of rotation of your motion rig in forward/backward, up/down and left/right direction (I was told most 6 dof rigs rotate around the bottom of the seat but your mileage may vary).
1. Enter the offset values in the config file
2. Start the OpenXR application of your choice
3. Bring your motion rig in neutral position
3. Sit in your rig 
4. put your headset on and face forward (~ direction surge). Potential rotation of the hmd on roll and pitch angle is ignored for the calculation
5. issue the `center` command py activated the correspnding shortcut. You can also do this implicitly by activating motion compensation if you haven't (re)centered since last loading of the configuration.

Unfortunately it's currently only possible to save the offset towards the hmd position but not the exact position of the cor between game sessions. So you have to recalibrate the cor everytime you start the game

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

## Addtional Notes
- Upon activating a shortcut you get audible feedback, using either the confirmation, warning or error sound configured in your windows system control, depending on wether the execution of the action was successful.

- If you recenter the in-app view during a session the reference pose is reset by default. Therefore you should only do that while your motion rig is in neutral position. It is possible (depending on the application) that this automatic recalibration is not triggered, causing the view and reference pose to be out of sync and leading to erroneous motion compensation. You should do the following steps to get this corrected again:
  1. deactivate motion compensation by pressing the `activate` shortcut
  2. bring your motion rig to neutral position. Face forward if yout using a virtual tracker
  3. press the `center` shortcut
  4. reactivate motion compensation by pressing the `activate` shortcut

- If the motion controller cannot be tracked for whatever reason (or if the memory mapped file containing the motion data for a virtual tracker cannot be found or accessed) when activating motion compensation or recalibrating the tracker pose, the API layer is unable to set the reference pose and motion compensation is (or stays) deactivated.

## Logging
The motion compensation layers logs rudimentary information and errors in a text file located at **...\Users\<Your_Username>\AppData\XR_APILAYER_NOVENDOR_motion_compensation.log**. After unexpected behaviour or a crash you can check that file for abormalities or error reports.

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
- `0`- `9`: numerical key
- `A`- `Z`: alphbetical key
- `BACKQUOTE`: `~ key (US)
- `TAB`: tabulator key
- `CAPS`: caps lock key
- `PLUS`: + key (any country)
- `MINUS`:- key (any country)
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
- `NUMSUBTRACT`:- key on NUM
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


## Contact
Feel free to join our Discord community at https://discord.gg/BVWugph5XF for feedback and assistance.

To actively support the project you can apply to become an **@Alpha Tester** on the Discord server to get access to early development builds of the software, provide feedback, and report bugs and crashes.

If you are (or know someone) willing and able to support the software development (mostly c++, maybe some GUI stuff later on) side of the project feel free to contact **@BuzzteeBear** on the Discord server about ways to contribute.

Donations to the project are very welcome and can be made at https://www.paypal.com/donate/?hosted_button_id=Q64DT2ADFCBU8 
