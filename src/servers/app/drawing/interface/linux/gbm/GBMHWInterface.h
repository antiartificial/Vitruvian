/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */
#ifndef GBM_HW_INTERFACE_H
#define GBM_HW_INTERFACE_H

#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
extern "C" {
#include <libseat.h>
}

#include <OS.h>
#include <Locker.h>

#include "HWInterface.h"
#include "LibInputEventStream.h"

class GLCompositor;
class GBMBuffer;

class GBMHWInterface : public HWInterface {
public:
								GBMHWInterface();
	virtual						~GBMHWInterface();

	virtual	status_t			Initialize();
	virtual	status_t			Shutdown();

	virtual	EventStream*		CreateEventStream();

	virtual	status_t			SetMode(const display_mode& mode);
	virtual	void				GetMode(display_mode* mode);
	virtual	status_t			GetPreferredMode(display_mode* mode);

	virtual status_t			GetDeviceInfo(accelerant_device_info* info);
	virtual status_t			GetFrameBufferConfig(
									frame_buffer_config& config);

	virtual status_t			GetModeList(display_mode** _modeList,
									uint32* _count);
	virtual status_t			GetPixelClockLimits(display_mode* mode,
									uint32* _low, uint32* _high);
	virtual status_t			GetTimingConstraints(
									display_timing_constraints* constraints);
	virtual status_t			ProposeMode(display_mode* candidate,
									const display_mode* low,
									const display_mode* high);

	virtual sem_id				RetraceSemaphore();
	virtual status_t			WaitForRetrace(
									bigtime_t timeout = B_INFINITE_TIMEOUT);

	virtual status_t			SetDPMSMode(uint32 state);
	virtual uint32				DPMSMode();
	virtual uint32				DPMSCapabilities();

	virtual status_t			SetBrightness(float brightness);
	virtual status_t			GetBrightness(float* brightness);

	virtual	RenderingBuffer*	FrontBuffer() const;
	virtual	RenderingBuffer*	BackBuffer() const;
	virtual	bool				IsDoubleBuffered() const;

	virtual	status_t			CopyBackToFront(const BRect& frame);

			void				_OnSessionEnable();
			void				_OnSessionDisable();

			static bool			IsAvailable();

private:
	static	int32				_EventThreadEntry(void* data);
			void				_EventThreadMain();
			void				_RestoreDisplay();

			status_t			_SetupDrmResources();
			status_t			_CreateBuffers();
			status_t			_InitCompositor();
			uint32_t			_AddFB(struct gbm_bo* bo);
			status_t			_CompositorFlip();
			status_t			_LegacyFlip();

			int					fDrmFd;
			struct gbm_device*	fGbmDevice;

			GBMBuffer*			fFrontBuffer;
			GBMBuffer*			fBackBuffer;

			GLCompositor*		fCompositor;
			struct gbm_bo*		fCurrentBO;
			uint32_t			fCurrentFbId;

			// DRM KMS state
			uint32_t			fConnectorId;
			uint32_t			fCrtcId;
			drmModeModeInfo		fMode;
			drmModeCrtc*		fSavedCrtc;

			display_mode		fDisplayMode;

			LibInputEventStream* fEventStream;

			struct libseat*		fSeat;
			int					fDeviceId;
			volatile bool		fSessionActive;
			bool				fInitialized;
			volatile bool		fRunning;

			thread_id			fEventThread;
			BLocker				fSessionLock;
			sem_id				fSessionSem;
			BLocker				fSeatLock;
};

#endif
