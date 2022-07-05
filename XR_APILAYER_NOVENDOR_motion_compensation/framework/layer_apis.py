# The list of OpenXR functions our layer will override.
override_functions = [
    "xrGetSystem",
    "xrCreateSession",
    "xrCreateReferenceSpace",
    "xrLocateSpace",
    "xrLocateViews",
    "xrAttachSessionActionSets",
    "xrSuggestInteractionProfileBindings"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetInstanceProperties",
    "xrGetSystemProperties",
    "xrEnumerateInstanceExtensionProperties",
    "xrEnumerateViewConfigurationViews",
    "xrCreateAction",
    "xrCreateActionSet",
    "xrStringToPath",
    "xrPathToString",
    "xrSuggestInteractionProfileBindings",
    "xrCreateActionSpace",
    "xrSyncActions",
    "xrGetActionStatePose",
    "xrDestroyActionSet",
    "xrDestroyAction",
    "xrDestroySpace"
]

# The list of OpenXR extensions our layer will either override or use.
extensions = ["XR_EXT_hp_mixed_reality_controller"]
