/*
 * Copyright 2021-2026, Dario Casalinuovo.
 * Distributed under the terms of the GPL License.
 */

#include "DrmHWInterface.h"

#include "DrmBuffer.h"

#include <algorithm>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>

#include <Autolock.h>

#include "modeset.h"


int DrmHWInterface::fFd = -1;

extern "C" void seat_enable_cb(struct libseat* seat, void* data)
{
	DrmHWInterface* hw = static_cast<DrmHWInterface*>(data);
	hw->_OnSessionEnable();

}

extern "C" void seat_disable_cb(struct libseat* seat, void* data)
{
	DrmHWInterface* hw = static_cast<DrmHWInterface*>(data);

	hw->_OnSessionDisable();

}

static struct libseat_seat_listener seat_listener = {
	.enable_seat = seat_enable_cb,
	.disable_seat = seat_disable_cb,
};


DrmHWInterface::DrmHWInterface()
	:
	HWInterface(),
	fFrontBuffer(NULL),
	fBackBuffer(NULL),
	fEventStream(NULL),
	fSeat(NULL),
	fDeviceId(-1),
	fSessionActive(false),
	fInitialized(false),
	fRunning(false),
	fSessionGeneration(0),
	fEventThread(-1),
	fSessionLock("drm session lock"),
	fSessionSem(create_sem(0, "drm session sem")),
	fSeatLock("drm seat lock"),
	fCrtcId(0)
{
	fSeat = libseat_open_seat(&seat_listener, this);
	if (!fSeat) {
		fprintf(stderr, "Failed to open libseat session\n");
		return;
	}

	printf("libseat opened, fSeat=%p, seat_fd=%d\n", (void*)fSeat,
		libseat_get_fd(fSeat));

	while (!fSessionActive) {
		int ret = libseat_dispatch(fSeat, -1);
		if (ret < 0)
			break;
	}

	if (!fSessionActive) {
		libseat_close_seat(fSeat);
		fSeat = NULL;
		return;
	}

	fRunning = true;
	fEventThread = spawn_thread(_EventThreadEntry, "drm event thread",
		B_NORMAL_PRIORITY, this);
	if (fEventThread >= 0)
		resume_thread(fEventThread);
}


void
DrmHWInterface::_OnSessionEnable()
{
	printf("Session enabled\n");

	if (fInitialized) {
		{
			BAutolock _(fSessionLock);
			fSessionActive = true;
			fSessionGeneration++;
		}

		release_sem(fSessionSem);

		_RestoreDisplay();

		if (fEventStream)
			fEventStream->Resume();
		return;
	}

	char path[B_PATH_NAME_LENGTH];
	for (int i = 0; i <= 9; ++i) {
		snprintf(path, sizeof(path), "/dev/dri/card%d", i);
		fDeviceId = libseat_open_device(fSeat, path, &fFd);
		if (fDeviceId >= 0)
			break;
	}

	if (fFd < 0) {
		fprintf(stderr, "Failed to open DRM device via libseat\n");
		return;
	}

	int ret = modeset_prepare(fFd);

	if (ret) {
		libseat_close_device(fSeat, fDeviceId);
		return;
	}

	struct modeset_dev *dev = get_dev();
	if (!dev) {
		fprintf(stderr, "no modeset device available\n");
		return;
	}

	dev->saved_crtc = drmModeGetCrtc(fFd, dev->crtc);
	fCrtcId = dev->crtc;

	fFrontBuffer = new DrmBuffer(fFd, dev->width, dev->height);
	fBackBuffer = new DrmBuffer(fFd, dev->width, dev->height);

	if (fFrontBuffer->InitCheck() != B_OK) {
		fprintf(stderr, "cannot initialize front buffer\n");
		delete fFrontBuffer;
		delete fBackBuffer;
		fFrontBuffer = NULL;
		fBackBuffer = NULL;
		return;
	}

	if (fBackBuffer->InitCheck() != B_OK) {
		fprintf(stderr, "cannot initialize back buffer, "
			"falling back to single-buffered\n");
		delete fBackBuffer;
		fBackBuffer = NULL;
	}

	ret = drmModeSetCrtc(fFd, fCrtcId, fFrontBuffer->GetFbId(), 0, 0,
				 &dev->conn, 1, &dev->mode);
	if (ret) {
		fprintf(stderr, "cannot set CRTC for connector %u (%d): %m\n",
			dev->conn, errno);
	}

	fEventStream = new LibInputEventStream(get_dev()->width, get_dev()->height, fSeat);
	fEventStream->SetSeatLock(&fSeatLock);

	fDisplayMode.virtual_width = get_dev()->width;
	fDisplayMode.virtual_height = get_dev()->height;
	fDisplayMode.space = B_RGB32;

	{
		BAutolock _(fSessionLock);
		fSessionActive = true;
		fInitialized = true;
	}
	release_sem(fSessionSem);
}


void
DrmHWInterface::_OnSessionDisable()
{
	printf("Session disabled\n");

	if (fEventStream)
		fEventStream->Suspend();

	{
		BAutolock _(fSessionLock);
		fSessionActive = false;
		fSessionGeneration++;
	}

	libseat_disable_seat(fSeat);
}


void
DrmHWInterface::_RestoreDisplay()
{
	if (fFd < 0 || fFrontBuffer == NULL)
		return;

	struct modeset_dev *dev = get_dev();
	if (!dev)
		return;

	drmModeSetCrtc(fFd, fCrtcId, fFrontBuffer->GetFbId(), 0, 0,
		&dev->conn, 1, &dev->mode);
}


/*static*/ int32
DrmHWInterface::_EventThreadEntry(void* data)
{
	static_cast<DrmHWInterface*>(data)->_EventThreadMain();
	return 0;
}


void
DrmHWInterface::_EventThreadMain()
{
	int seatErrorCount = 0;

	while (fRunning) {
		int seat_fd;
		{
			BAutolock _(fSeatLock);
			seat_fd = fSeat ? libseat_get_fd(fSeat) : -1;
		}

		if (seat_fd < 0) {
			seatErrorCount++;
			snooze(100000);
			continue;
		}
		seatErrorCount = 0;

		bool active;
		{
			BAutolock _(fSessionLock);
			active = fSessionActive;
		}

		struct pollfd pfd;
		pfd.fd = seat_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		int ret = poll(&pfd, 1, 100);
		if (ret > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
			BAutolock _(fSeatLock);
			int dret = libseat_dispatch(fSeat, 0);
			if (dret < 0 && !active)
				printf("libseat_dispatch: error\n");
		}
	}
}


DrmHWInterface::~DrmHWInterface()
{
	CALLED();

	fRunning = false;
	release_sem(fSessionSem);

	if (fEventThread >= 0) {
		status_t exitValue;
		wait_for_thread(fEventThread, &exitValue);
	}

	delete_sem(fSessionSem);

	if (fSeat && fDeviceId >= 0)
		libseat_close_device(fSeat, fDeviceId);

	if (fSeat)
		libseat_close_seat(fSeat);

	modeset_cleanup(fFd);

	delete fBackBuffer;
	delete fFrontBuffer;
	delete fEventStream;
}


status_t
DrmHWInterface::Initialize()
{
	status_t ret = HWInterface::Initialize();
	if (ret != B_OK)
		return ret;

	if (fFrontBuffer == NULL)
		return B_ERROR;

	ret = fFrontBuffer->InitCheck();
	if (ret != B_OK)
		return ret;

	return B_OK;
}


EventStream*
DrmHWInterface::CreateEventStream()
{
	return fEventStream;
}


status_t
DrmHWInterface::Shutdown()
{
	CALLED();
	return B_OK;
}


status_t
DrmHWInterface::SetMode(const display_mode& mode)
{
	CALLED();
	return B_OK;
}


void
DrmHWInterface::GetMode(display_mode* mode)
{
	CALLED();
	*mode = fDisplayMode;
}


status_t
DrmHWInterface::GetPreferredMode(display_mode* mode)
{
	CALLED();
	*mode = fDisplayMode;
	return B_OK;
}


status_t
DrmHWInterface::GetDeviceInfo(accelerant_device_info* info)
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::GetFrameBufferConfig(frame_buffer_config& config)
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::GetModeList(display_mode** _modeList, uint32* _count)
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::GetPixelClockLimits(display_mode* mode, uint32* _low, uint32* _high)
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::GetTimingConstraints(display_timing_constraints* constraints)
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::ProposeMode(display_mode* candidate,
	const display_mode* low, const display_mode* high)
{
	CALLED();
	return B_UNSUPPORTED;
}


sem_id
DrmHWInterface::RetraceSemaphore()
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::WaitForRetrace(bigtime_t timeout)
{
	CALLED();

	if (fFd < 0 || fBackBuffer == NULL)
		return B_UNSUPPORTED;

	// TODO we should check if the session is active to avoid having
	// someone stuck on this.

	// Wait for the next vblank event using DRM_IOCTL_WAIT_VBLANK.
	struct drm_wait_vblank wait;
	memset(&wait, 0, sizeof(wait));
	wait.request.type = DRM_VBLANK_RELATIVE;
	wait.request.sequence = 1;

	if (ioctl(fFd, DRM_IOCTL_WAIT_VBLANK, &wait) < 0)
		return B_ERROR;

	return B_OK;
}


status_t
DrmHWInterface::SetDPMSMode(uint32 state)
{
	CALLED();
	return B_UNSUPPORTED;
}


uint32
DrmHWInterface::DPMSMode()
{
	CALLED();
	return B_UNSUPPORTED;
}


uint32
DrmHWInterface::DPMSCapabilities()
{
	CALLED();
	return 0;
}


status_t
DrmHWInterface::SetBrightness(float brightness)
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
DrmHWInterface::GetBrightness(float* brightness)
{
	CALLED();
	return B_UNSUPPORTED;
}


RenderingBuffer*
DrmHWInterface::FrontBuffer() const
{
	CALLED();
	return fFrontBuffer;
}


RenderingBuffer*
DrmHWInterface::BackBuffer() const
{
	CALLED();
	return fBackBuffer;
}


bool
DrmHWInterface::IsDoubleBuffered() const
{
	return fBackBuffer != NULL;
}


status_t
DrmHWInterface::CopyBackToFront(const BRect& frame)
{
	if (fBackBuffer == NULL)
		return B_UNSUPPORTED;

	int ret = drmModePageFlip(fFd, fCrtcId, fBackBuffer->GetFbId(),
		DRM_MODE_PAGE_FLIP_EVENT, this);
	if (ret != 0) {
		fprintf(stderr, "page flip failed (%d): %m\n", errno);
		return B_ERROR;
	}

	// Wait for the page flip to complete before swapping pointers.
	// Without this, the next flip would get EBUSY and drawing into
	// the "back" buffer could corrupt the buffer still being scanned out.
	drmEventContext evctx;
	memset(&evctx, 0, sizeof(evctx));
	evctx.version = 2;
	evctx.page_flip_handler
		= [](int, unsigned int, unsigned int, unsigned int, void*) {};

	struct pollfd pfd;
	pfd.fd = fFd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	while (true) {
		int pr = poll(&pfd, 1, 1000);
		if (pr > 0) {
			drmHandleEvent(fFd, &evctx);
			break;
		} else if (pr == 0) {
			fprintf(stderr, "page flip completion timeout\n");
			break;
		} else if (errno != EINTR) {
			break;
		}
	}

	std::swap(fFrontBuffer, fBackBuffer);
	return B_OK;
}
