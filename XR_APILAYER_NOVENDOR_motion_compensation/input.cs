// Copyright(c) 2024 Sebastian Veith
#pragma once

namespace input
{
	public enum ActivityBit : UInt64
	{
		Activate = 1 << 0,
		Calibrate = 1 << 1,
		LockRefPose = 1 << 2,
		ReleaseRefPose = 1 << 3,
		FilterTranslationIncrease = 1 << 4,
		FilterTranslationDecrease = 1 << 5,
		FilterRotationIncrease = 1 << 6,
		FilterRotationDecrease = 1 << 7,
		StabilizerToggle = 1 << 8,
		StabilizerIncrease = 1 << 9,
		StabilizerDecrease = 1 << 10,
		OffsetForward = 1 << 11,
		OffsetBack = 1 << 12,
		OffsetUp = 1 << 13,
		OffsetDown = 1 << 14,
		OffsetRight = 1 << 15,
		OffsetLeft = 1 << 16,
		OffsetRotateRight = 1 << 17,
		OffsetRotateLeft = 1 << 18,
		OverlayToggle = 1 << 19,
		PassthroughToggle = 1 << 20,
		CrosshairToggle = 1 << 21,
		EyeCacheToggle = 1 << 22,
		ModifierToggle = 1 << 23,
		SaveConfig = 1 << 24,
		SaveConfigPerApp = 1 << 25,
		ReloadConfig = 1 << 26,
		VerboseLoggingToggle = 1 << 27,
		RecorderToggle = 1 << 28,
		LogTracker = 1 << 29,
		LogProfile = 1 << 30,
	};

	public struct ActivityFlags
	{
		public UInt64 trigger;
		public UInt64 confirm;
	};
}
