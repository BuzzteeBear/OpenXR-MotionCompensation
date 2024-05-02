if (Test-Path "pimax-openxr.json") {
    $JsonPath = Join-Path "$PSScriptRoot" "XR_APILAYER_NOVENDOR_motion_compensation.json"
} elseif (Test-Path "pimax-openxr-32.json") {
	$JsonPath = Join-Path "$PSScriptRoot" "XR_APILAYER_NOVENDOR_motion_compensation-32.json"
} else {
	Exit
}

Start-Process -FilePath powershell.exe -Verb RunAs -Wait -ArgumentList @"
	& {
		Remove-ItemProperty -Path HKLM:\Software\Khronos\OpenXR\1\ApiLayers\Implicit -Name '$jsonPath' -Force | Out-Null
	}
"@
