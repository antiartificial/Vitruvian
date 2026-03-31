/*
 * Copyright 2021-2026, Dario Casalinuovo
 * All rights reserved. Distributed under the terms of the GPL license.
 */
#ifndef DRM_BUFFER_H
#define DRM_BUFFER_H

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "RenderingBuffer.h"


#if DEBUG
	#define CALLED() 			printf("CALLED %s\n",__PRETTY_FUNCTION__)
#else
  	#define CALLED() 			((void)0)
#endif


class DrmBuffer : public RenderingBuffer {
public:
								DrmBuffer(int fd, uint32_t width,
									uint32_t height);
	virtual						~DrmBuffer();

	virtual	status_t			InitCheck() const;

	virtual	color_space			ColorSpace() const;
	virtual	void*				Bits() const;
	virtual	uint32				BytesPerRow() const;
	virtual	uint32				Width() const;
	virtual	uint32				Height() const;

			uint32_t			GetFbId() const;

private:
			int					fFd;
			status_t			fErr;
			color_space			fColorSpace;

			uint32_t			fWidth;
			uint32_t			fHeight;
			uint32_t			fStride;
			uint32_t			fSize;
			uint32_t			fHandle;
			uint32_t			fFbId;
			uint8_t*			fMap;
};

#endif
