/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */
#ifndef GBM_AREA_H
#define GBM_AREA_H

#include <OS.h>
#include <gbm.h>


/*
 * GBMArea wraps a GBM buffer object as a Nexus shared memory area,
 * enabling zero-copy buffer sharing between app_server and client
 * processes.
 *
 * The flow:
 *   1. app_server creates a GBMArea (allocates gbm_bo, exports dma_buf fd,
 *      registers as a named Nexus area)
 *   2. The area_id is sent to the client over a BMessage/port
 *   3. The client calls clone_area() to get a mapped fd into its own
 *      address space for CPU rendering, or imports the dma_buf fd
 *      directly for GPU rendering via EGL
 *
 * GBMArea owns the GBM BO and the Nexus area registration. When destroyed,
 * it cleans up both.
 */
class GBMArea {
public:
								GBMArea(struct gbm_device* gbm,
									uint32_t width, uint32_t height,
									const char* name);
								~GBMArea();

			status_t			InitCheck() const;

			area_id				AreaId() const { return fAreaId; }
			struct gbm_bo*		GetBO() const { return fBO; }
			int					DmaBufFd() const { return fDmaBufFd; }

			uint32_t			Width() const { return fWidth; }
			uint32_t			Height() const { return fHeight; }
			uint32_t			Stride() const;

private:
			struct gbm_bo*		fBO;
			int					fDmaBufFd;
			area_id				fAreaId;
			status_t			fErr;

			uint32_t			fWidth;
			uint32_t			fHeight;
};

#endif
