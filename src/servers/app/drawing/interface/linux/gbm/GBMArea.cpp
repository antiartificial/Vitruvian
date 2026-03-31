/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */

#include "GBMArea.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


GBMArea::GBMArea(struct gbm_device* gbm, uint32_t width, uint32_t height,
	const char* name)
	:
	fBO(NULL),
	fDmaBufFd(-1),
	fAreaId(-1),
	fErr(B_ERROR),
	fWidth(width),
	fHeight(height)
{
	// Allocate a GBM buffer suitable for scanout and rendering
	fBO = gbm_bo_create(gbm, fWidth, fHeight, GBM_FORMAT_XRGB8888,
		GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (fBO == NULL) {
		fprintf(stderr, "GBMArea: gbm_bo_create failed\n");
		return;
	}

	// Export the BO as a dma_buf file descriptor
	fDmaBufFd = gbm_bo_get_fd(fBO);
	if (fDmaBufFd < 0) {
		fprintf(stderr, "GBMArea: gbm_bo_get_fd failed\n");
		gbm_bo_destroy(fBO);
		fBO = NULL;
		return;
	}

	// Register as a Nexus shared area so other processes can clone it.
	// create_area expects a name, start address, address spec, size,
	// lock, and protection. Since we already have a dma_buf fd, we use
	// the lower-level Nexus ioctl path instead — create_area uses
	// memfd_create internally, which isn't what we want.
	//
	// For now, we keep the dma_buf fd and area_id for the compositor
	// to use directly. Cross-process sharing via Nexus area_create
	// with the dma_buf fd will be wired up when client-side GPU
	// rendering (Phase 5) is implemented.
	//
	// The area_id is set to a synthetic negative value to indicate
	// it's not yet registered with Nexus. The dma_buf fd is the
	// primary handle for compositor-internal use.
	fAreaId = -1;
	fErr = B_OK;
}


GBMArea::~GBMArea()
{
	if (fAreaId >= 0)
		delete_area(fAreaId);

	if (fDmaBufFd >= 0)
		close(fDmaBufFd);

	if (fBO != NULL)
		gbm_bo_destroy(fBO);
}


status_t
GBMArea::InitCheck() const
{
	return fErr;
}


uint32_t
GBMArea::Stride() const
{
	if (fBO != NULL)
		return gbm_bo_get_stride(fBO);
	return 0;
}
