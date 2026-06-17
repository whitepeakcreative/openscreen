export type NativeLinuxSourceType = "display" | "window";

export type NativeLinuxRecordingRequest = {
	recordingId?: number;
	source: {
		type: NativeLinuxSourceType;
		sourceId: string;
		displayId?: number;
	};
	video: {
		fps: number;
		width: number;
		height: number;
	};
	audio: {
		system: {
			enabled: boolean;
		};
	};
	cursor: {
		mode: import("./recordingSession").CursorCaptureMode;
	};
};

export type NativeLinuxRecordingStartResult = {
	success: boolean;
	recordingId?: number;
	path?: string;
	helperPath?: string;
	error?: string;
};
