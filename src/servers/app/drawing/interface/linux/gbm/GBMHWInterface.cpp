/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */

#include "GBMHWInterface.h"

#include "GBMBuffer.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <Autolock.h>


extern "C" void
gbm_seat_enable_cb(struct libseat* seat, void* data)
{
	GBMHWInterface* hw = static_cast<GBMHWInterface*>(data);
	hw->_OnSessionEnable();
}


extern "C" void
gbm_seat_disable_cb(struct libseat* seat, void* data)
{
	GBMHWInterface* hw = static_cast<GBMHWInterface*>(data);
	hw->_OnSessionDisable();
}


static struct libseat_seat_listener sGbmSeatListener = {
	.enable_seat = gbm_seat_enable_cb,
	.disable_seat = gbm_seat_disable_cb,
};


GBMHWInterface::GBMHWInterface()
	:
	HWInterface(),
	fDrmFd(-1),
	fGbmDevice(NULL),
	fFrontBuffer(NULL),
	fBackBuffer(NULL),
	fConnectorId(0),
	fCrtcId(0),
	fSavedCrtc(NULL),
	fEventStream(NULL),
	fSeat(NULL),
	fDeviceId(-1),
	fSessionActive(false),
	fInitialized(false),
	fRunning(false),
	fEventThread(-1),
	fSessionLock("gbm session lock"),
	fSessionSem(create_sem(0, "gbm session sem")),
	fSeatLock("gbm seat lock")
{
	memset(&fMode, 0, sizeof(fMode));
	memset(&fDisplayMode, 0, sizeof(fDisplayMode));

	fSeat = libseat_open_seat(&sGbmSeatListener, this);
	if (!fSeat) {
		fprintf(stderr, "GBMHWInterface: failed to open libseat session\n");
		return;
	}

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
	fEventThread = spawn_thread(_EventThreadEntry, "gbm event thread",
		B_NORMAL_PRIORITY, this);
	if (fEventThread >= 0)
		resume_thread(fEventThread);
}


GBMHWInterface::~GBMHWInterface()
{
	fRunning = false;
	release_sem(fSessionSem);

	if (fEventThread >= 0) {
		status_t exitValue;
		wait_for_thread(fEventThread, &exitValue);
	}

	delete_sem(fSessionSem);

	delete fBackBuffer;
	delete fFrontBuffer;
	delete fEventStream;

	if (fSavedCrtc != NULL) {
		if (fDrmFd >= 0) {
			drmModeSetCrtc(fDrmFd, fSavedCrtc->crtc_id,
				fSavedCrtc->buffer_id, fSavedCrtc->x, fSavedCrtc->y,
				&fConnectorId, 1, &fSavedCrtc->mode);
		}
		drmModeFreeCrtc(fSavedCrtc);
	}

	if (fGbmDevice != NULL)
		gbm_device_destroy(fGbmDevice);

	if (fSeat != NULL && fDeviceId >= 0)
		libseat_close_device(fSeat, fDeviceId);

	if (fSeat != NULL)
		libseat_close_seat(fSeat);

	if (fDrmFd >= 0)
		close(fDrmFd);
}


/*static*/ bool
GBMHWInterface::IsAvailable()
{
	// Quick probe: can we open a DRM render node and create a GBM device?
	for (int i = 0; i <= 9; i++) {
		char path[64];
		snprintf(path, sizeof(path), "/dev/dri/card%d", i);

		int fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;

		struct gbm_device* gbm = gbm_create_device(fd);
		if (gbm != NULL) {
			gbm_device_destroy(gbm);
			close(fd);
			return true;
		}
		close(fd);
	}
	return false;
}


void
GBMHWInterface::_OnSessionEnable()
{
	if (fInitialized) {
		{
			BAutolock _(fSessionLock);
			fSessionActive = true;
		}
		release_sem(fSessionSem);
		_RestoreDisplay();

		if (fEventStream)
			fEventStream->Resume();
		return;
	}

	// Open the first available DRM device via libseat
	char path[64];
	for (int i = 0; i <= 9; ++i) {
		snprintf(path, sizeof(path), "/dev/dri/card%d", i);
		fDeviceId = libseat_open_device(fSeat, path, &fDrmFd);
		if (fDeviceId >= 0)
			break;
	}

	if (fDrmFd < 0) {
		fprintf(stderr, "GBMHWInterface: failed to open DRM device\n");
		return;
	}

	// Create GBM device
	fGbmDevice = gbm_create_device(fDrmFd);
	if (fGbmDevice == NULL) {
		fprintf(stderr, "GBMHWInterface: gbm_create_device failed\n");
		libseat_close_device(fSeat, fDeviceId);
		fDrmFd = -1;
		return;
	}

	// Find connector, CRTC, and preferred mode
	if (_SetupDrmResources() != B_OK) {
		fprintf(stderr, "GBMHWInterface: DRM resource setup failed\n");
		gbm_device_destroy(fGbmDevice);
		fGbmDevice = NULL;
		libseat_close_device(fSeat, fDeviceId);
		fDrmFd = -1;
		return;
	}

	// Save current CRTC state for restore on exit
	fSavedCrtc = drmModeGetCrtc(fDrmFd, fCrtcId);

	// Create front and back GBM buffers
	if (_CreateBuffers() != B_OK) {
		fprintf(stderr, "GBMHWInterface: buffer creation failed\n");
		if (fSavedCrtc != NULL) {
			drmModeFreeCrtc(fSavedCrtc);
			fSavedCrtc = NULL;
		}
		gbm_device_destroy(fGbmDevice);
		fGbmDevice = NULL;
		libseat_close_device(fSeat, fDeviceId);
		fDrmFd = -1;
		return;
	}

	// Set the CRTC to scan out the front buffer
	int ret = drmModeSetCrtc(fDrmFd, fCrtcId, fFrontBuffer->GetFbId(),
		0, 0, &fConnectorId, 1, &fMode);
	if (ret != 0) {
		fprintf(stderr, "GBMHWInterface: drmModeSetCrtc failed (%d): %m\n",
			errno);
	}

	fEventStream = new LibInputEventStream(fMode.hdisplay, fMode.vdisplay,
		fSeat);
	fEventStream->SetSeatLock(&fSeatLock);

	fDisplayMode.virtual_width = fMode.hdisplay;
	fDisplayMode.virtual_height = fMode.vdisplay;
	fDisplayMode.space = B_RGB32;

	{
		BAutolock _(fSessionLock);
		fSessionActive = true;
		fInitialized = true;
	}
	release_sem(fSessionSem);

	printf("GBMHWInterface: initialized %ux%u via GBM\n",
		fMode.hdisplay, fMode.vdisplay);
}


status_t
GBMHWInterface::_SetupDrmResources()
{
	drmModeRes* res = drmModeGetResources(fDrmFd);
	if (res == NULL) {
		fprintf(stderr, "GBMHWInterface: drmModeGetResources failed\n");
		return B_ERROR;
	}

	// Find the first connected connector with modes
	drmModeConnector* conn = NULL;
	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(fDrmFd, res->connectors[i]);
		if (conn == NULL)
			continue;
		if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
			break;
		drmModeFreeConnector(conn);
		conn = NULL;
	}

	if (conn == NULL) {
		fprintf(stderr, "GBMHWInterface: no connected display found\n");
		drmModeFreeResources(res);
		return B_ERROR;
	}

	fConnectorId = conn->connector_id;

	// Use the first (preferred/highest resolution) mode
	memcpy(&fMode, &conn->modes[0], sizeof(fMode));
	fprintf(stderr, "GBMHWInterface: selected mode %ux%u@%uHz\n",
		fMode.hdisplay, fMode.vdisplay, fMode.vrefresh);

	// Find a CRTC for this connector
	drmModeEncoder* enc = NULL;
	if (conn->encoder_id)
		enc = drmModeGetEncoder(fDrmFd, conn->encoder_id);

	if (enc != NULL && enc->crtc_id != 0) {
		fCrtcId = enc->crtc_id;
		drmModeFreeEncoder(enc);
	} else {
		if (enc != NULL)
			drmModeFreeEncoder(enc);

		// Try all encoders
		bool found = false;
		for (int i = 0; i < conn->count_encoders && !found; i++) {
			enc = drmModeGetEncoder(fDrmFd, conn->encoders[i]);
			if (enc == NULL)
				continue;

			for (int j = 0; j < res->count_crtcs; j++) {
				if (enc->possible_crtcs & (1u << j)) {
					fCrtcId = res->crtcs[j];
					found = true;
					break;
				}
			}
			drmModeFreeEncoder(enc);
		}

		if (!found) {
			fprintf(stderr, "GBMHWInterface: no suitable CRTC found\n");
			drmModeFreeConnector(conn);
			drmModeFreeResources(res);
			return B_ERROR;
		}
	}

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	return B_OK;
}


status_t
GBMHWInterface::_CreateBuffers()
{
	fFrontBuffer = new GBMBuffer(fGbmDevice, fMode.hdisplay, fMode.vdisplay);
	if (fFrontBuffer->InitCheck() != B_OK) {
		fprintf(stderr, "GBMHWInterface: front buffer init failed\n");
		delete fFrontBuffer;
		fFrontBuffer = NULL;
		return B_ERROR;
	}

	uint32_t frontFb = _AddFB(fFrontBuffer->GetBO());
	if (frontFb == 0) {
		delete fFrontBuffer;
		fFrontBuffer = NULL;
		return B_ERROR;
	}
	fFrontBuffer->SetFbId(frontFb);

	fBackBuffer = new GBMBuffer(fGbmDevice, fMode.hdisplay, fMode.vdisplay);
	if (fBackBuffer->InitCheck() != B_OK) {
		fprintf(stderr, "GBMHWInterface: back buffer init failed, "
			"falling back to single-buffered\n");
		delete fBackBuffer;
		fBackBuffer = NULL;
		return B_OK;
	}

	uint32_t backFb = _AddFB(fBackBuffer->GetBO());
	if (backFb == 0) {
		fprintf(stderr, "GBMHWInterface: back buffer AddFB failed, "
			"falling back to single-buffered\n");
		delete fBackBuffer;
		fBackBuffer = NULL;
		return B_OK;
	}
	fBackBuffer->SetFbId(backFb);

	return B_OK;
}


uint32_t
GBMHWInterface::_AddFB(struct gbm_bo* bo)
{
	uint32_t handle = gbm_bo_get_handle(bo).u32;
	uint32_t stride = gbm_bo_get_stride(bo);
	uint32_t width = gbm_bo_get_width(bo);
	uint32_t height = gbm_bo_get_height(bo);

	uint32_t fbId = 0;
	int ret = drmModeAddFB(fDrmFd, width, height, 24, 32, stride,
		handle, &fbId);
	if (ret != 0) {
		fprintf(stderr, "GBMHWInterface: drmModeAddFB failed (%d): %m\n",
			errno);
		return 0;
	}
	return fbId;
}


void
GBMHWInterface::_OnSessionDisable()
{
	if (fEventStream)
		fEventStream->Suspend();

	{
		BAutolock _(fSessionLock);
		fSessionActive = false;
	}

	libseat_disable_seat(fSeat);
}


void
GBMHWInterface::_RestoreDisplay()
{
	if (fDrmFd < 0 || fFrontBuffer == NULL)
		return;

	drmModeSetCrtc(fDrmFd, fCrtcId, fFrontBuffer->GetFbId(), 0, 0,
		&fConnectorId, 1, &fMode);
}


/*static*/ int32
GBMHWInterface::_EventThreadEntry(void* data)
{
	static_cast<GBMHWInterface*>(data)->_EventThreadMain();
	return 0;
}


void
GBMHWInterface::_EventThreadMain()
{
	while (fRunning) {
		int seat_fd;
		{
			BAutolock _(fSeatLock);
			seat_fd = fSeat ? libseat_get_fd(fSeat) : -1;
		}

		if (seat_fd < 0) {
			snooze(100000);
			continue;
		}

		struct pollfd pfd;
		pfd.fd = seat_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		int ret = poll(&pfd, 1, 100);
		if (ret > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
			BAutolock _(fSeatLock);
			libseat_dispatch(fSeat, 0);
		}
	}
}


status_t
GBMHWInterface::Initialize()
{
	status_t ret = HWInterface::Initialize();
	if (ret != B_OK)
		return ret;

	if (fFrontBuffer == NULL)
		return B_ERROR;

	return fFrontBuffer->InitCheck();
}


EventStream*
GBMHWInterface::CreateEventStream()
{
	return fEventStream;
}


status_t
GBMHWInterface::Shutdown()
{
	return B_OK;
}


status_t
GBMHWInterface::SetMode(const display_mode& mode)
{
	return B_OK;
}


void
GBMHWInterface::GetMode(display_mode* mode)
{
	*mode = fDisplayMode;
}


status_t
GBMHWInterface::GetPreferredMode(display_mode* mode)
{
	*mode = fDisplayMode;
	return B_OK;
}


status_t
GBMHWInterface::GetDeviceInfo(accelerant_device_info* info)
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::GetFrameBufferConfig(frame_buffer_config& config)
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::GetModeList(display_mode** _modeList, uint32* _count)
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::GetPixelClockLimits(display_mode* mode, uint32* _low,
	uint32* _high)
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::GetTimingConstraints(display_timing_constraints* constraints)
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::ProposeMode(display_mode* candidate,
	const display_mode* low, const display_mode* high)
{
	return B_UNSUPPORTED;
}


sem_id
GBMHWInterface::RetraceSemaphore()
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::WaitForRetrace(bigtime_t timeout)
{
	if (fDrmFd < 0 || fBackBuffer == NULL)
		return B_UNSUPPORTED;

	struct drm_wait_vblank wait;
	memset(&wait, 0, sizeof(wait));
	wait.request.type = DRM_VBLANK_RELATIVE;
	wait.request.sequence = 1;

	if (ioctl(fDrmFd, DRM_IOCTL_WAIT_VBLANK, &wait) < 0)
		return B_ERROR;

	return B_OK;
}


status_t
GBMHWInterface::SetDPMSMode(uint32 state)
{
	return B_UNSUPPORTED;
}


uint32
GBMHWInterface::DPMSMode()
{
	return B_UNSUPPORTED;
}


uint32
GBMHWInterface::DPMSCapabilities()
{
	return 0;
}


status_t
GBMHWInterface::SetBrightness(float brightness)
{
	return B_UNSUPPORTED;
}


status_t
GBMHWInterface::GetBrightness(float* brightness)
{
	return B_UNSUPPORTED;
}


RenderingBuffer*
GBMHWInterface::FrontBuffer() const
{
	return fFrontBuffer;
}


RenderingBuffer*
GBMHWInterface::BackBuffer() const
{
	return fBackBuffer;
}


bool
GBMHWInterface::IsDoubleBuffered() const
{
	return fBackBuffer != NULL;
}


status_t
GBMHWInterface::CopyBackToFront(const BRect& frame)
{
	if (fBackBuffer == NULL)
		return B_UNSUPPORTED;

	int ret = drmModePageFlip(fDrmFd, fCrtcId, fBackBuffer->GetFbId(),
		DRM_MODE_PAGE_FLIP_EVENT, this);
	if (ret != 0) {
		fprintf(stderr, "GBMHWInterface: page flip failed (%d): %m\n",
			errno);
		return B_ERROR;
	}

	// Wait for page flip completion before swapping pointers
	drmEventContext evctx;
	memset(&evctx, 0, sizeof(evctx));
	evctx.version = 2;
	evctx.page_flip_handler
		= [](int, unsigned int, unsigned int, unsigned int, void*) {};

	struct pollfd pfd;
	pfd.fd = fDrmFd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	while (true) {
		int pr = poll(&pfd, 1, 1000);
		if (pr > 0) {
			drmHandleEvent(fDrmFd, &evctx);
			break;
		} else if (pr == 0) {
			fprintf(stderr, "GBMHWInterface: page flip timeout\n");
			break;
		} else if (errno != EINTR) {
			break;
		}
	}

	std::swap(fFrontBuffer, fBackBuffer);
	return B_OK;
}
