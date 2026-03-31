/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */

#include "GBMBuffer.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>


GBMBuffer::GBMBuffer(struct gbm_device* gbm, uint32_t width, uint32_t height)
	:
	fBO(NULL),
	fErr(B_ERROR),
	fWidth(width),
	fHeight(height),
	fStride(0),
	fFbId(0),
	fMapData(NULL),
	fMap(NULL)
{
	// Allocate a GBM buffer object with linear tiling so the CPU can
	// write directly into it via mmap. This lets AGG render into the
	// buffer without any GPU involvement.
	fBO = gbm_bo_create(gbm, fWidth, fHeight, GBM_FORMAT_XRGB8888,
		GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
	if (fBO == NULL) {
		// Some drivers don't support LINEAR; fall back to default tiling
		// and rely on gbm_bo_map to handle the tiling translation.
		fBO = gbm_bo_create(gbm, fWidth, fHeight, GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_WRITE);
		if (fBO == NULL) {
			fprintf(stderr, "GBMBuffer: gbm_bo_create failed\n");
			return;
		}
	}

	fStride = gbm_bo_get_stride(fBO);

	// Map the buffer for CPU access
	uint32_t mapStride = 0;
	fMap = gbm_bo_map(fBO, 0, 0, fWidth, fHeight,
		GBM_BO_TRANSFER_READ_WRITE, &mapStride, &fMapData);
	if (fMap == NULL) {
		fprintf(stderr, "GBMBuffer: gbm_bo_map failed\n");
		gbm_bo_destroy(fBO);
		fBO = NULL;
		return;
	}

	// Use the mapped stride (may differ from the BO stride on tiled surfaces)
	fStride = mapStride;

	memset(fMap, 0, fStride * fHeight);
	fErr = B_OK;
}


GBMBuffer::~GBMBuffer()
{
	if (fMapData != NULL && fBO != NULL)
		gbm_bo_unmap(fBO, fMapData);

	if (fBO != NULL)
		gbm_bo_destroy(fBO);
}


status_t
GBMBuffer::InitCheck() const
{
	return fErr;
}


color_space
GBMBuffer::ColorSpace() const
{
	return B_RGB32;
}


void*
GBMBuffer::Bits() const
{
	return fMap;
}


uint32
GBMBuffer::BytesPerRow() const
{
	return fStride;
}


uint32
GBMBuffer::Width() const
{
	return fWidth;
}


uint32
GBMBuffer::Height() const
{
	return fHeight;
}
