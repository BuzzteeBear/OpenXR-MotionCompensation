# OpenXR Motion Compensation

**DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks!**

Version: 0.2.8

**This document contains instructions on how to use OpenXR motion compensation [API layer](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#api-layers).**

## Purpose of OpenXR Motion Compensation 

When using a motion rig in combination with a VR headset (hmd) he movement of the rig causes the in-game camera to change along with your position in the real world. In simulations for example you're basically feel being pushed around inside the cockpit when the motion rig moves.  
Motion compensation reduces or ideally removes that effect by locking the in-game world to the pose of the motion rig.
This software aims to provide an API layer for motion compensation to be used with applications and hmds supporting the OpenXR standard.  
To be able to do that, the software needs to be informed on the motion rig movement / position. This can be achieved using a tracker, which is either a physical object attached to the motion rig and tracked by the VR runtime (e.g. a motion controller or a vive puck) or a virtual tracker using data from the motion software driving the motion rig. 


Limitations:

- The motion compensation API Layer is made for Windows 64-bit only.
- The software (obviously) only works with VR/AR applications using an OpenXR runtime implementation.

## Contact
Feel free to join our [Discord community](https://discord.gg/BVWugph5XF) or to send an e-mail to [**oxrmc@mailbox.org**](mailto:oxrmc@mailbox.org) for feedback and assistance.

You can find the [source code](https://github.com/BuzzteeBear/OpenXR-MotionCompensation) and the [latest release](https://github.com/BuzzteeBear/OpenXR-MotionCompensation/releases) or report issues on github.

If you are (or know someone) willing and able to support the software development (mostly C++, maybe some GUI stuff later on) side of the project, feel free to contact **@BuzzteeBear** on the Discord server to ask about ways to contribute.

Donations to the project are very welcome and can be made via [Paypal](https://www.paypal.com/donate/?hosted_button_id=Q64DT2ADFCBU8).  
On explicit user request you can also sponsor the project via [GitHub Sponsors](https://github.com/sponsors/BuzzteeBear?o=esb)

## Install

### Run installer executable
Just double click the installation executable called `Install_OpenXR-MotionCompensation_<current_version>.exe` and follow the instructions.
A few hints regarding the installation process:
- If you're upgrading from a version prior to 0.2.0, it is recommended to target the installation directory already existing. This will allow the installer to transfer your existing configuration files into the `appdata/local/OpenXR-MotionCompensation` directory that is used from version 0.2.0 onward.
- Using a sub directory of `program files` as installation target is recommended, especially for compatibility with WMR based headsets.
- Although the installation needs administrative privileges make sure to run the installation executable using the windows account you're using to launch your games/Open XR applications. This enables the installer to put the configuration file(s) into the correct appdata directory.
- If something goes wrong on installation and you don't know what or why, you can check the log file `Setup Log <yyyy-mm-dd xxx>.txt` that is created in the `%TEMP%` folder.

### Conflict with other OpenXR API layers
There may be issues with other OpenXR API layers that are installed on your system. For the most part they can be solved by using the correct order of installation (because that implicitly determines the order in which the layers are loaded).  
According to user feedback following constraints seem to be working:
- **XRNeckSaver** needs to be installed before OXRMC.
- **OpenKneeBoard** needs to be installed before OXRMC. 
  - but it is (or at least was at some point) putting its registry key in `...HKEY_CURRENT_USER/...` while OXRMC uses `...HKEY_LOCAL_MACHINE/...` . So if you're having trouble changing the loading order, try moving the key for OpenKneeboard from `Computer\HKEY_CURRENT_USER\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit` to `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit`.
- if you install one of the above with after OXRMC, you can just run the OXRMC installer afterwards to modify loading order.

### Optional: Confirm correct installation

You can use the application [OpenXR Explorer](https://github.com/maluoi/openxr-explorer/releases) to verify the correct installation:
- Install OpenXR Explorer
- Connect your headset
- Start your corresponding VR runtime application (e.g. SteamVR, Oculus App, Mixed Reality Portal, Varjo Base, PiTool, etc.)
- Start OpenXR explorer
- Search for the section `xrEnumerateApiLayerProperties` (should be in the middle column at the bottom by default)
- Check if the entry `XR_APILAYER_NOVENDOR_motion_compensation` with version `v1` exists

### Update

To get OXRMC updated, download and run the latest installation executable from [Github](https://github.com/BuzzteeBear/OpenXR-MotionCompensation/releases). If you want to change the installation directory you have to uninstall the previous version first.

### Uninstall

To remove the OpenXR-MotionCompensation layer just use windows settings/control panel as you would do with any other application. During de-installation you can choose to delete your configuration and log files in the appdata directory or to keep them for later use. 
- If something goes wrong on installation and you don't know what or why, you can check the log file `Uninstall Log <yyyy-mm-dd xxx>.txt` that is created in the `%TEMP%` folder.

## Configuration

Configuration files can be found at `...\Users\*<Your_Username>*\AppData\Local\OpenXR-MotionCompensation\OpenXR-MotionCompensation.log`.
After initial installation this directory contains the default configuration file `OpenXR-MotionCompensation.ini`. You can make changes to that file to configure options you want to be the same for all OpenXR applications.
Upon starting an OpenXR application with the API layer active for the first time, a configuration file named after the application is created in the same directory. You can use it to copy (partial) sections from the default configuration file whenever you want to make changes only for that application specifically.

### Use of the Configuration file

What you can modify in a configuration file:
- the tracker to use for motion compensation
- the strength and the number of filtering stages for both translational and rotational filters
- keyboard inputs (e.g. to activate/deactivate or re-calibrate motion compensation during runtime)  
**Note that all keys and values in the configuration file(s) are case sensitive. That means all [keyboard shortcuts](#list-of-keyboard-bindings) must only contain capital letters, numbers and/or underscores**

### Sections in configuration file
- `startup`: You can modify oxrmc's behavior on application start, e.g. disable a specific feature by setting the corresponding key to 0. 
  - `enabled`: you can disable all functionality globally or for a single application. Note that you cannot enable a single application if oxrmc is disabled globally in the default config file. Modifying this setting requires an application restart.
  - `physical_enabled`: initialization of physical tracker (motion controller or vive tracker) on startup can be skipped (e.g. if you're using a virtual tracker). Modifying this setting requires an application restart.
  - `overlay_enabled`: enable initialization of the graphical overlay (for example if it's not required because of using a physical tracker or if the position of the center of rotation is successfully setup and `use_cor_pos` = 1 is used). Changing this value requires the VR session to be restarted.
  - `physical_early_init`: initialize physical tracker as soon instead of as late as possible. May be required in native OpenXR games / sims that do not support motion controllers input (e.g. iRacing). May avoid conflicts with other OpenXR layers (e.g. eye tracking in OpenXR toolkit). Modifying this setting requires an application restart.
  - `upside_down`: turn in-game coordinate system upside down by rotating it 180 degrees around the 'forward' axis. Necessary for correct orientation of virtual tracker and motion compensation in some games (automatically set for iRacing). Changing this value requires the VR session to be restarted.
  - `activate`: automatically activate motion compensation on application start and configuration reload.
  - `activate_delay`: delay auto-activation by specified number of seconds. The required time for successful activation may vary, depending on application and tracker type used.
  - `activate_countdown`: enable audible countdown for the last 10 seconds before auto-activation. This is supposed to allow getting to neutral position and timely centering of in-game view.
  - `compensate_controllers`: enable motion compensation for motion controllers (that are not used as reference trackers). Note that enabling this feature will disable cor manipulation via motion controller. Changing this value requires the application top be restarted.
- `tracker`: 
  - The following tracker `type` keys are available:
    - `controller`: use either the left or the right motion controller as reference tracker. Valid options for the key `side` are `left` and `right` (**Note that changing the side or switching between motion controller and vive tracker requires a restart of the vr session**)
    - `vive`: use a vive tracker as reference for motion compensation. The key `side` has to match the role assigned to the tracker. Valid options for that are:
      - `handheld_object` - which hand (left, right, any) doesn't matter. Having more than one active vive tracker assigned to that role may lead to conflicts, though.
      - `left_foot`
      - `right_foot`
      - `left_shoulder`
      - `right_shoulder`
      - `left_elbow`
      - `right_elbow`
      - `left_knee`
      - `right_knee`
      - `waist`
      - `chest`
      - `camera`
      - `keyboard`.
    - `srs`: use the virtual tracker data provided by SRS motion software when using a Witmotion (or similar?) sensor on the motion rig.
    - `flypt` use the virtual tracker data provided by FlyPT Mover.
    - `yaw`: use the virtual tracker data provided by Yaw VR and Yaw 2. Either while using SRS or Game Engine.
  - the keys `offset_...`, `use_cor_pos` and `cor_...` are used to handle the configuration of the center of rotation (cor) for all available virtual trackers.
    - offset values are meant to be modified to specify how far away the cor is in terms of up/down, forward/backward left/right, and up/down direction relative to your headset. The yaw angle defines a counterclockwise rotation of the forward vector after positioning of the cor on calibration.
    - `use_cor_pos` can be enabled to reuse the exact cor position within vr playspace for the next sessions, independent of offset values and hmd position at calibration time.
    - values starting with `cor_` are not meant for manual editing in the config file but are instead populated on saving the current configuration.  
  - `marker_size` sets the size of the cor / reference tracker marker displayed in the overlay. The value corresponds to the length of one double cone in cm.
  - `connection_timeout` sets the time (in seconds) the tracker needs to be unresponsive before motion compensation is automatically deactivated. Setting a negative value disables automatic deactivation.
  - `connection_check` is only relevant for virtual trackers and determines the period (in seconds) for checking whether the memory mapped file used for data input is actually still actively used. Setting a negative value disables the check
- `translational_filter` and `rotational_filter`: set the filtering magnitude (key `strength` with valid options between **0.0** and **1.0**) number of filtering stages (key `order`with valid options: **1, 2, 3**).  
  The key `vertical_factor` is applied to translational filter strength in vertical/heave direction only (Note that the filter strength is multiplied by the factor and the resulting product of strength * vertical_factor is clamped internally between 0.0 and 1.0).
- `cache`: you can modify the cache used for reverting the motion corrected pose on frame submission:
  - `use_eye_cache` - choose between calculating eye poses (0 = default) or use cached eye poses (1, was default up until version 0.1.4). Either one might work better with some games or hmds if you encounter jitter with mc activated. You can also modify this setting (and subsequently save it to config file) during runtime with the corresponding shortcut below.
  - `tolerance` - modify the time values are kept in cache for before deletion. This may affect eye calculation as well as cached eye positions.
- `shortcuts`: can be used to configure shortcuts for different commands (See [List of keyboard bindings](#list-of-keyboard-bindings) for valid values):
  - `activate`- turn motion compensation on or off. Note that this implicitly triggers the calibration action (`center`) if that hasn't been executed before.
  - `center` - re-calibrate the neutral reference pose of the tracker
  - `translation_increase`, `translation_decrease` - modify the strength of the translational filter. Changes made during runtime can be saved by using a save command (see below).
  - `rotation_increase`, `rotation_decrease` = see above, but for rotational filter
  - `offset_forward`, `offset_back`, `offset_up`, `offset_down`, `offset_right`, `offset_left` - move the center of rotation (cor) for a virtual tracker. The directions are aligned with the forward vector set with the `center` command. Changes made during runtime can be saved by using a save command (see below).
  - `rotate_right`, `rotate_left` - rotate the aforementioned forward vector around the gravitational (yaw-)axis. Note that these changes cannot be saved. Therefore changing the offset position AFTER rotating manually and saving the offset values will result in the cor being a different offset position after reloading those saved values.
  - `toggle_overlay` - (de-)activate graphical overlay displaying the reference tracker position(s) (See [Graphical overlay](#graphical-overlay) for details).
  - `toggle_cache` - change between calculated and cached eye positions.
  - `fast_modifier` - press key(s) in addition to a filter or cor manipulation shortcut to increase amount of change per keypress/repetition. Filter modification will be sped up by factor 5 while cor manipulation will move/rotate 10 instead of 1 cm/degree
  - `save_config` -  write current filter strength and cor offsets to global config file 
  - `save_config_app` -  write current filter strength and cor offsets to application specific config file. Note that values in this file will precedent values in the global config file. 
  - `reload_config` - read in and apply configuration for current app from config files. For technical reasons motion compensation is automatically deactivated and the reference tracker pose is invalidated upon configuration reload.
  - Note that there are some immutable keyboard shortcuts:
    - `ctrl + shift + alt + i`: logs your current interaction profile, which can be useful when debugging issues with a physical tracker.
    - `ctrl + shift + alt + t`: logs the current pose of the reference tracker, can also be used for the purpose of troubleshooting. 
- `debug`: 
  - `log_verbose` - enables debug level entries in log file. Note that activating this option may have a negative impact on performance.
  - `testrotation` - for debugging reasons you can check, if the motion compensation functionality generally works on your system without using tracker input from the motion controllers at all by setting this value to `1` and reloading the configuration. You should be able to see the world rotating around you after pressing the activation shortcut.  
**Beware that this can be a nauseating experience because your eyes suggest that your head is turning in the virtual world, while your inner ear tells your brain otherwise. You can stop motion compensation at any time by pressing the activation shortcut again!** 

## Using a virtual tracker

To use a virtual tracker (as opposed to a physical device) set parameter `tracker_type` according to the motion software that is providing the data for motion compensation on your system:
- `yaw`: Yaw Game Engine (or Sim Racing Studio when using rotational data provided by Yaw VR or Yaw 2)
- `srs`: Sim Racing Studio, using a Witmotion sensor
- `flypt`: FlyPT Mover

You can find video tutorials on virtual tracker setup by [MotionXP](https://www.youtube.com/watch?v=116TVKMO9p8) and [SimHanger](https://www.youtube.com/watch?v=NT-kpJwzJzw) on YouTube.

### Calibrating virtual tracker
To enable OXRMC to correlate translation and rotation of the rig to the virtual space correctly when using a virtual tracker, you have to provide the information where the center of rotation (cor) of your motion rig is positioned and which way is forward. This can be done with the following steps:

0. Calculate, measure or estimate the distance between your headset and the center of rotation of your motion rig in forward/backward, up/down and left/right direction (I was told most 6 dof rigs rotate around the bottom of the seat but your mileage may vary). If you're using a witmotion sensor, please keep in mind that cor position does not (necessarily) coincide with mounting position of the sensor.
1. Enter the offset values in the config file
2. Start the OpenXR application of your choice
3. Bring your motion rig in neutral position
3. Sit in your rig 
4. put your headset on and face forward (~ direction surge). Potential rotation of the hmd on roll and pitch axis is ignored for the calculation
5. issue the calibration command by activating the `center` shortcut. You can also do this implicitly by activating motion compensation if you haven't (re)calibrated since last loading of the configuration.
- You can use the tracker marker of the graphical overlay and keyboard shortcuts (or the left motion controller, see further below) to adjust the cor position in-game. Make sure to calibrate the tracker first, because the marker tracker just rests at vr play-space origin beforehand. For in-game changes to survive application restart, you have to manually save the configuration.
- If you're unable to locate the cor of your rig, try out the method described in the [according troubleshooting section](#virtual-tracker)
- You may have to invert some of the rotations/translations on output side to get them compensated properly. **For new users it's strongly recommended to use some artificial telemetry (joystick input, sine wave generator, etc.) and testing one degree of freedom at at time**
- If you're using YawVR Game Engine you can also use the parameters `Head Distance` and `Height` in its Motion Compensation tab to specify the offset of the cor. Head distance is basically equal to `offset_forward` in the configration file. But note that the height parameter is measured upwards from the bottom of your play-space, so you'll need to have that setup correctly in order to use that feature.

### Saving and reloading the cor location
Once you're satisfied with the current setting, the current position and orientation of the cor can be saved to the (global = `ctrl + shift + s` or app-specific = `ctrl + shift + a`) config file. You can subsequently set the config key `use_cor_pos` to `1`. This causes the cor position to be loaded from the config file when calibrating instead of being determined using the hmd position and the offset values.  
Applications using OpenComposite usually operate in a different VR play-space than titles supporting native OpenXR. That's why cor position needs to be saved once for all native games and once for all games using OpenComposite.  
**Note that this functionality may not work with all HMD vendors. Setting up the play-space in the VR runtime of your hmd (before first use) might help to get this working correctly. Rumor has it that some HMDs need to be started/initialized at the exact same location for the play-space coordinates to be consistent in between uses.**

### Adjusting cor location using a motion controller
You can use (only) the left motion controller to move the cor position in virtual space. The virtual tracker has to be calibrated and motion compensation needs to be deactivated. Make sure to activate the graphical overlay (`ctrl + d` by default) to see the cor marker in game. 
- press and hold the trigger button to 'grab' and move the cor marker, this way you can make it reach positions that are obstructed in the real world.
- while pressing the trigger:
  - moving the controller left/right, up/down, or forward/backward is pushing the cor marker in the same direction
  - rotating the controller on the yaw axis is adjusting the cor marker accordingly. Rotation of the motion controller on pitch or roll axis are ignored.
- press the menu button (or button 'a' on a valve index controller) to have the cor location snap to the current controller position. While this button is pressed, you cannot move the cor using the trigger.
- in order for the adjusted position to persist for upcoming sessions, save the configuration.

## Running your application
1. make sure your using OpenXR as runtime in the application you wish to use motion compensation in
2. start application
3. center the in-app view
4. activate the motion controller you configured and mount it on your motion rig
5. bring your motion rig to neutral position
6. Reset the in-game view if necessary
7. press the `activate` shortcut (**CTRL** + **INSERT** by default). This implicitly sets the neutral reference pose for the tracker
- if necessary you can re-calibrate the tracker by pressing the `center` shortcut (**CTRL** + **DEL** by default) while the motion rig is in neutral position 
- you can increase or decrease the filter strength of translational and rotational filters
- you can modify the cor offset when currently using a virtual tracker
- after modifying filter strength or cor offset for virtual tracker you can save your changes to the default configuration file 
- after modifying the config file(s) manually you can use the `reload_config` shortcut (**CTRL** + **SHIFT** + **L** by default) to restart the OXRMC software with the new values. 

### Graphical overlay
You can enable/disable the overlay using the `toggle_overlay` shortcut. It displays a marker in your headset view for:
- the current neutral position of the reference tracker. **Note that the position of the marker does not represent the tracker/cor position before tracker calibration**
  - for a virtual tracker the neutral position corresponds to the center of rotation currently configured. The marker uses the following color coding:
    - blue points upwards
    - green points forward
    - red points to the right
    - if blue and red are pointing in the opposite direction, try setting `upside_down` to 1 in the `startup` section of the config file of the corresponding application (or check if it is set to 1 inadvertently). 
  - for a physical tracker the orientation of the marker is depending on the runtime implementation
- the current tracker position, if mc is currently active. This marker uses:
  - cyan instead of blue
  - yellow instead of green
  - magenta instead of red
 
### Connection Loss
OXRMC can detect wether a reference tracker isn't available anymore if: 
- for a physical tracker: the runtime lost tracking of a motion controller / vive tracker 
- for a virtual tracker: the memory mapped file providing data for a virtual tracker is removed by windows due to inactivity of the sender  

After detecting a loss of connection a configurable timeout period is used (`connection_timeout`), allowing two possible outcomes:
- the connection is reestablished within the timeout period: motion compensation is continued (and the next potential connection loss resets the timeout period)
- the connection stays lost and motion compensation is automatically deactivated. At that point you get an audible warning about connection loss. 
  - If you try to reactivate and the tracker is available again, motion compensation is resumed (without the need for tracker re-calibration) 
  - Otherwise, the error feedback is repeated and motion compensation stays deactivated. When using a virtual tracker and having connection problems, you can use the MMF Reader app (see below) to cross check existence and current output values of the memory mapped file used for data exchange.

## Troubleshooting
Upon activating any shortcut you get audible feedback, corresponding to the performed action (or an error, if something went wrong).
If you're getting 'error' or no feedback at all, check for error entries (search for keyword 'error') in the log file at **...\Users\<Your_Username>\AppData\Local\OpenXR-MotionCompensation\OpenXR-MotionCompensation.log**. 

### Re-centering in-game view
If you recenter the in-app view during a session the reference pose is reset by default. Therefore you should only do that while your motion rig is in neutral position. It is possible (depending on the application) that this automatic recalibration is not triggered, causing the view and reference pose to be out of sync and leading to erroneous motion compensation. You should do the following steps to get this corrected again:
1. deactivate motion compensation by pressing the `activate` shortcut
2. bring your motion rig to neutral position. Face forward if you using a virtual tracker
3. re-calibrate by pressing the `center` shortcut
4. reactivate motion compensation by pressing the `activate` shortcut

### Virtual tracker
When using a **virtual tracker** and the audible feedback says 'motion compensation activated' but you don't get motion compensation as you would expect
Use the [MmfReader App](#mmf-reader) to make sure oxrmc is actually receiving data from the motion software.
- check center of rotation position
- activate graphical overlay
- verify position and orientation of the marker
If don't have a clue where the cor of your motion rig is supposed to be, you can try this procedure, that should work for most motion rig setups (you can watch a [video of a similar procedure at YouTube](https://youtu.be/mIIlIlV-B_4)):
1. Find a way to feed your motion software with artificial rotational telemetry (e.g. Joystick mode in the Setup section of SRS, a sine wave generator for FlyPT Mover or Gamepad / DirectInput plugin for YawVR Game Engine.  
2. Calibrate your cor (ctrl + del by default) as described in [here](#calibrate-virtual-tracker) and activate motion compensation
3. Find the right height
   1. start rolling fully to the right while keeping the head still (in reference to the seat) and check if your in-game position is moving. 
   2. if your view is moved to the right, lower your cor position (ctrl + page down), if it's moving left lift it up (ctrl + page up)
   3. rinse & repeat until your in-game position does not move when rolling
4. Set the forward distance
   1. start pitching fully backward do the same as for roll, but this time check if you're moved up or down in game
   2. if your position is rising move the cor backwards, if it's lowering move it forward
   3. rinse & repeat until your in-game position does not move when rolling
5. Save your configuration (ctrl + shift + s), once you got your cor dialed in. That causes the new offset values as well as the current cor position to be written into the config file for later use. 

### Physical tracker
- Make sure the tracker/controller doesn't go into standby mode
- Place a lighthouse based tracker to have line of sight to as many base-stations as possible
- If you're experiencing tracking issues on strong vibrations (e.g. transducer) on the rig, try to find a better mounting spot or tune vibrations down. 

You can always request help on the [Discord server](https://discord.gg/BVWugph5XF)
- provide as much **information** as possible about your setup and the issue you're having, including:
  - log file
  - tracker type
  - hmd
  - game(s)
  - using OpenComposite or native OpenXR

## Additional Notes

- If the motion controller cannot be tracked for whatever reason (or if the memory mapped file containing the motion data for a virtual tracker cannot be found or accessed) when activating motion compensation or recalibrating the tracker pose, the API layer is unable to set the reference pose and motion compensation is (or stays) deactivated.

### MMF Reader
The software package includes a small app called MMF Reader which allows you to display the content of the memory mapped file used for virtual trackers. Just execute it from windows start menu or use the executable in the installation directory and select the kind of tracker you're using from the dropdown menu. 
- If the memory mapped file does not exist and therefore no values can be read, all the values are displaying an `X`. 
- Otherwise the current values are displayed using arc degree as unit for rotations and meter for translations.

### Logging
The motion compensation layers logs rudimentary information and errors in a text file located at **...\Users\<Your_Username>\AppData\Local\OpenXR-MotionCompensation\OpenXR-MotionCompensation.log**. After unexpected behavior or a crash you can check that file for abnormalities or error reports.

If you encounter repeatable bugs or crashes you can use the Windows Performance Recorder Profile (WPRP) trace-logging in combination with the configuration contained within `scripts\Trace_OpenXR-MotionCompensation.wprp` to create a more detailed protocol.

[Trace-logging](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-portal) can become very useful to investigate user issues.

To capture a trace for the API layer:

- start the OpenXR application
- Open a command line prompt or powershell in administrator mode and in a folder where you have write permissions
- Begin recording a trace with the command: `wpr -start "C:\Program Files\OpenXR-MotionCompensation\Trace_OpenXR-MotionCompensation.wprp" -filemode`
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
`CTRL` | control key
`ALT` | alt key
`LSHIFT` | left shift key
`RSHIFT` | right shift key
`LCTRL` | left control key
`RCTRL` | right control key
`LALT` | left alt key
`RALT` | right alt key
`0`- `9` | numerical key
`A`- `Z` | alphabetical key
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
`CLR` | clear key
`RETURN` | return key
`ESC` | escape key
`SPACE` | space key
`LEFT` | cursor left key
`UP` | cursor up key
`RIGHT` | cursor right key
`DOWN` | cursor down key
`INS` | insert key
`DEL` | delete key
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
