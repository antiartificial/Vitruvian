/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */
#ifndef GL_COMPOSITOR_H
#define GL_COMPOSITOR_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <gbm.h>


/*
 * GLCompositor manages an EGL context on a GBM device and provides
 * GPU-accelerated compositing of window layers.
 *
 * In Phase 4, the compositor renders textured quads for each window
 * layer (back to front) using GLES2. Window backing stores are
 * imported as EGLImages from dma_buf fds.
 *
 * For the initial implementation, the compositor provides a simple
 * blit path: given a CPU-rendered buffer (from AGG), upload it as
 * a GL texture and render a fullscreen quad. This replaces the
 * memcpy/page-flip path with a GPU-composited one.
 */
class GLCompositor {
public:
								GLCompositor();
								~GLCompositor();

			status_t			Init(struct gbm_device* gbm,
									uint32_t width, uint32_t height);
			void				Shutdown();

			bool				IsInitialized() const { return fInitialized; }

			// Blit a CPU buffer (XRGB8888) to the screen via GPU texture upload
			status_t			BlitBuffer(const void* bits, uint32_t width,
									uint32_t height, uint32_t stride);

			// Get the current front buffer BO after eglSwapBuffers
			struct gbm_bo*		LockFrontBuffer();
			void				ReleaseFrontBuffer(struct gbm_bo* bo);

private:
			status_t			_CreateShaders();
			void				_DestroyShaders();

			EGLDisplay			fDisplay;
			EGLContext			fContext;
			EGLSurface			fSurface;
			struct gbm_surface*	fGbmSurface;

			GLuint				fProgram;
			GLuint				fVBO;
			GLuint				fTexture;

			GLint				fAttrPosition;
			GLint				fAttrTexCoord;
			GLint				fUniformTexture;

			uint32_t			fWidth;
			uint32_t			fHeight;
			bool				fInitialized;
};

#endif
