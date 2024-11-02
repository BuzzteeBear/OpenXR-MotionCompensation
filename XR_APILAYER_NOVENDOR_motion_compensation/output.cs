// Copyright(c) 2024 Sebastian Veith
#pragma once

namespace output
{
	public enum Event
	{
		Silence = 0,
		Error,
		Critical,
		Initialized,
		Load,
		Save,
		Activated,
		Deactivated,
        Calibrated,
        Restored,
        Plus,
		Minus,
		Max,
		Min,
		Up,
		Down,
		Forward,
		Back,
		Left,
		Right,
		RotLeft,
		RotRight,
		DebugOn,
		DebugOff,
		ConnectionLost,
		EyeCached,
		EyeCalculated,
		OverlayOn,
		OverlayOff,
		ModifierOn,
		ModifierOff,
		CalibrationLost,
		VerboseOn,
		VerboseOff,
		RecorderOn,
		RecorderOff,
		StabilizerOn,
		StabilizerOff,
		PassthroughOn,
		PassthroughOff,
        RefPoseLocked,
        RefPoseReleased
    };

	public struct EventData
	{
		public Int32 id;
		public Int64 eventTime;
	};

	public record struct Status
	{
		public bool initialized;
		public bool calibrated;
		public bool activated;
		public bool critical;
		public bool error;
		public bool connectionLost;
		public bool modified;
	};

	enum StatusFlags
	{
		initialized = 0x1,
		calibrated = 0x2,
		activated = 0x4,
		critical = 0x8,
		error = 0x10,
		connection = 0x20,
		modified = 0x40
	};
}
