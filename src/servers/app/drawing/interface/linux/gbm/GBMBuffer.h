/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */
#ifndef GBM_BUFFER_H
#define GBM_BUFFER_H

#include <gbm.h>

#include "RenderingBuffer.h"


class GBMBuffer : public RenderingBuffer {
public:
								GBMBuffer(struct gbm_device* gbm,
									uint32_t width, uint32_t height);
	virtual						~GBMBuffer();

	virtual	status_t			InitCheck() const;

	virtual	color_space			ColorSpace() const;
	virtual	void*				Bits() const;
	virtual	uint32				BytesPerRow() const;
	virtual	uint32				Width() const;
	virtual	uint32				Height() const;

			struct gbm_bo*		GetBO() const { return fBO; }
			uint32_t			GetFbId() const { return fFbId; }

			// Set the DRM framebuffer id after AddFB
			void				SetFbId(uint32_t fbId) { fFbId = fbId; }

private:
			struct gbm_bo*		fBO;
			status_t			fErr;

			uint32_t			fWidth;
			uint32_t			fHeight;
			uint32_t			fStride;
			uint32_t			fFbId;

			void*				fMapData;
			void*				fMap;
};

#endif
