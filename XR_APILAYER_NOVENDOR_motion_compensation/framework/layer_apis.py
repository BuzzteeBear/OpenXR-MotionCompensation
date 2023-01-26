# The list of OpenXR functions our layer will override.
override_functions = [
    "xrGetSystem",
    "xrPollEvent",
    "xrCreateSession",
    "xrBeginSession",
    "xrEndSession",
    "xrDestroySession",
    "xrCreateSwapchain",
    "xrDestroySwapchain",
    "xrWaitSwapchainImage",
    "xrWaitSwapchainImage",
    "xrAcquireSwapchainImage",
    "xrReleaseSwapchainImage",
    "xrGetCurrentInteractionProfile",
    "xrAttachSessionActionSets",
    "xrSuggestInteractionProfileBindings",
    "xrSyncActions",
    "xrCreateReferenceSpace",
    "xrLocateSpace",
    "xrLocateViews",
    "xrBeginFrame",
    "xrEndFrame"
]

# The list of OpenXR functions our layer will use from the runtime.
# Might repeat entries from override_functions above.
requested_functions = [
    "xrGetInstanceProperties",
    "xrGetSystemProperties",
    "xrStringToPath",
    "xrPathToString",
    "xrSuggestInteractionProfileBindings",
    "xrCreateAction",
    "xrCreateActionSet",
    "xrCreateActionSpace",
    "xrGetActionStatePose",
    "xrEnumerateSwapchainFormats",
    "xrEnumerateSwapchainImages",
    "xrDestroyAction",
    "xrDestroyActionSet",
    "xrDestroySpace"
]

# The list of OpenXR extensions our layer will either override or use.
extensions = ["XR_EXT_hp_mixed_reality_controller"]
