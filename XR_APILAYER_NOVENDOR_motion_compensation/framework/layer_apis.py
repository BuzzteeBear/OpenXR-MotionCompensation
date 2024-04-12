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
	"xrAcquireSwapchainImage",
	"xrReleaseSwapchainImage",
    "xrGetCurrentInteractionProfile",
    "xrAttachSessionActionSets",
    "xrSuggestInteractionProfileBindings",
    "xrSyncActions",
    "xrCreateReferenceSpace",
    "xrCreateActionSpace",
    "xrLocateSpace",
    "xrLocateViews",
    "xrWaitFrame",
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
    "xrGetActionStatePose",
    "xrGetActionStateBoolean",
    "xrApplyHapticFeedback",
    "xrEnumerateSwapchainImages",
	"xrConvertWin32PerformanceCounterToTimeKHR",
    "xrDestroyAction",
    "xrDestroyActionSet",
    "xrDestroySpace"
]

# The list of OpenXR extensions our layer will either override or use.
extensions = [
	"XR_EXT_hp_mixed_reality_controller",
	"XR_KHR_win32_convert_performance_counter_time"
]
