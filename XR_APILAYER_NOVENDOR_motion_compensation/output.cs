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
        RefPoseReleased,
        CrosshairOn,
        CrosshairOff,
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

	public enum StatusFlags
	{
		initialized = 1 << 0,
		calibrated = 1 << 1,
		activated = 1 << 2,
		critical = 1 << 3,
		error = 1 << 4,
		connection = 1 << 5,
		modified = 1 << 6,
	};

	public enum PoseType
	{
		Cor = -1,
		None,
		Sway,
		Surge,
		Heave,
		Yaw,
		Roll,
		Pitch,
		Hmd
	};

	public enum CorEstimatorFlags
	{
		controller = 1 << 3,
		start = 1 << 4,
		confirm = 1 << 5,
		stop = 1 << 6,
		failure = 1 << 7,
		reset = 1 << 8,
	};
}
