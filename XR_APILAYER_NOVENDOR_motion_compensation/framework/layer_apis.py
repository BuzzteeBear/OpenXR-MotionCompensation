# The list of OpenXR functions our layer will override.
override_functions = [
    "xrGetSystem",
    "xrCreateSession",
    "xrBeginSession",
    "xrAttachSessionActionSets",
    "xrSuggestInteractionProfileBindings",
    "xrCreateReferenceSpace",
    "xrLocateSpace",
    "xrLocateViews",
    "xrSyncActions",
    "xrEndFrame"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetInstanceProperties",
    "xrGetSystemProperties",
    "xrCreateAction",
    "xrCreateActionSet",
    "xrCreateActionSpace",
    "xrStringToPath",
    "xrPathToString",
    "xrSuggestInteractionProfileBindings",
    "xrGetActionStatePose",
    "xrDestroyAction",
    "xrDestroyActionSet",
    "xrDestroySpace"
]

# The list of OpenXR extensions our layer will either override or use.
extensions = ["XR_EXT_hp_mixed_reality_controller"]
