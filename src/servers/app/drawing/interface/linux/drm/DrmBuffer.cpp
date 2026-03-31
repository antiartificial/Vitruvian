/*
 * Copyright 2021-2026, Dario Casalinuovo.
 * Distributed under the terms of the GPL License.
 */

#include "DrmBuffer.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>


DrmBuffer::DrmBuffer(int fd, uint32_t width, uint32_t height)
	:
	fFd(fd),
	fErr(B_ERROR),
	fColorSpace(B_RGB32),
	fWidth(width),
	fHeight(height),
	fStride(0),
	fSize(0),
	fHandle(0),
	fFbId(0),
	fMap(NULL)
{
	struct drm_mode_create_dumb creq;
	memset(&creq, 0, sizeof(creq));
	creq.width = fWidth;
	creq.height = fHeight;
	creq.bpp = 32;

	if (drmIoctl(fFd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		fprintf(stderr, "DrmBuffer: cannot create dumb buffer (%d): %m\n",
			errno);
		return;
	}

	fStride = creq.pitch;
	fSize = creq.size;
	fHandle = creq.handle;

	if (drmModeAddFB(fFd, fWidth, fHeight, 24, 32, fStride,
			fHandle, &fFbId) != 0) {
		fprintf(stderr, "DrmBuffer: cannot create framebuffer (%d): %m\n",
			errno);
		goto err_destroy;
	}

	struct drm_mode_map_dumb mreq;
	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = fHandle;

	if (drmIoctl(fFd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) != 0) {
		fprintf(stderr, "DrmBuffer: cannot map dumb buffer (%d): %m\n",
			errno);
		goto err_fb;
	}

	fMap = (uint8_t*)mmap(0, fSize, PROT_READ | PROT_WRITE, MAP_SHARED,
		fFd, mreq.offset);
	if (fMap == MAP_FAILED) {
		fprintf(stderr, "DrmBuffer: cannot mmap dumb buffer (%d): %m\n",
			errno);
		fMap = NULL;
		goto err_fb;
	}

	memset(fMap, 0, fSize);
	fErr = B_OK;
	return;

err_fb:
	drmModeRmFB(fFd, fFbId);
	fFbId = 0;
err_destroy:
	struct drm_mode_destroy_dumb dreq;
	memset(&dreq, 0, sizeof(dreq));
	dreq.handle = fHandle;
	drmIoctl(fFd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	fHandle = 0;
}


DrmBuffer::~DrmBuffer()
{
	CALLED();

	if (fMap != NULL)
		munmap(fMap, fSize);

	if (fFbId != 0)
		drmModeRmFB(fFd, fFbId);

	if (fHandle != 0) {
		struct drm_mode_destroy_dumb dreq;
		memset(&dreq, 0, sizeof(dreq));
		dreq.handle = fHandle;
		drmIoctl(fFd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}
}


status_t
DrmBuffer::InitCheck() const
{
	CALLED();
	return fErr;
}


color_space
DrmBuffer::ColorSpace() const
{
	CALLED();
	return fColorSpace;
}


void*
DrmBuffer::Bits() const
{
	CALLED();
	return (void*)fMap;
}


uint32
DrmBuffer::BytesPerRow() const
{
	CALLED();
	return fStride;
}


uint32
DrmBuffer::Width() const
{
	CALLED();
	return fWidth;
}


uint32
DrmBuffer::Height() const
{
	CALLED();
	return fHeight;
}


uint32_t
DrmBuffer::GetFbId() const
{
	return fFbId;
}
