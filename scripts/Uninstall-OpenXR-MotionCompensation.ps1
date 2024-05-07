if (Test-Path "XR_APILAYER_NOVENDOR_motion_compensation.json") {
    $RegistryPath = "HKLM:\Software\Khronos\OpenXR\1\ApiLayers\Implicit"
    $JsonPath = Join-Path "$PSScriptRoot" "XR_APILAYER_NOVENDOR_motion_compensation.json"
} elseif (Test-Path "XR_APILAYER_NOVENDOR_motion_compensation_32.json") {
	$RegistryPath = "HKLM:\Software\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit"
	$JsonPath = Join-Path "$PSScriptRoot" "XR_APILAYER_NOVENDOR_motion_compensation_32.json"
} else {
	Exit
}

Start-Process -FilePath powershell.exe -Verb RunAs -Wait -ArgumentList @"
	& {
		Remove-ItemProperty -Path $RegistryPath -Name '$jsonPath' -Force | Out-Null
	}
"@
