/*
 * Copyright 2026, Vitruvian OS contributors.
 * Distributed under the terms of the GPL License.
 */

#include "GLCompositor.h"

#include <stdio.h>
#include <string.h>

#include <OS.h>


static const char* kVertexShader =
	"attribute vec2 aPosition;\n"
	"attribute vec2 aTexCoord;\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"    gl_Position = vec4(aPosition, 0.0, 1.0);\n"
	"    vTexCoord = aTexCoord;\n"
	"}\n";


static const char* kFragmentShader =
	"precision mediump float;\n"
	"varying vec2 vTexCoord;\n"
	"uniform sampler2D uTexture;\n"
	"void main() {\n"
	"    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
	"}\n";


// Fullscreen quad: position (x,y) + texcoord (u,v)
// Triangle strip: bottom-left, bottom-right, top-left, top-right
static const float kQuadVertices[] = {
	-1.0f, -1.0f,   0.0f, 1.0f,  // bottom-left  (texcoord flipped Y)
	 1.0f, -1.0f,   1.0f, 1.0f,  // bottom-right
	-1.0f,  1.0f,   0.0f, 0.0f,  // top-left
	 1.0f,  1.0f,   1.0f, 0.0f,  // top-right
};


static GLuint
CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "GLCompositor: shader compile error: %s\n", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}


GLCompositor::GLCompositor()
	:
	fDisplay(EGL_NO_DISPLAY),
	fContext(EGL_NO_CONTEXT),
	fSurface(EGL_NO_SURFACE),
	fGbmSurface(NULL),
	fProgram(0),
	fVBO(0),
	fTexture(0),
	fAttrPosition(-1),
	fAttrTexCoord(-1),
	fUniformTexture(-1),
	fWidth(0),
	fHeight(0),
	fInitialized(false)
{
}


GLCompositor::~GLCompositor()
{
	Shutdown();
}


status_t
GLCompositor::Init(struct gbm_device* gbm, uint32_t width, uint32_t height)
{
	if (fInitialized)
		return B_OK;

	fWidth = width;
	fHeight = height;

	// Get EGL display from GBM device
	fDisplay = eglGetDisplay((EGLNativeDisplayType)gbm);
	if (fDisplay == EGL_NO_DISPLAY) {
		fprintf(stderr, "GLCompositor: eglGetDisplay failed\n");
		return B_ERROR;
	}

	EGLint major, minor;
	if (!eglInitialize(fDisplay, &major, &minor)) {
		fprintf(stderr, "GLCompositor: eglInitialize failed\n");
		fDisplay = EGL_NO_DISPLAY;
		return B_ERROR;
	}

	printf("GLCompositor: EGL %d.%d initialized\n", major, minor);

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "GLCompositor: eglBindAPI failed\n");
		eglTerminate(fDisplay);
		fDisplay = EGL_NO_DISPLAY;
		return B_ERROR;
	}

	// Choose config
	EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLConfig config;
	EGLint numConfigs;
	if (!eglChooseConfig(fDisplay, configAttribs, &config, 1, &numConfigs)
		|| numConfigs == 0) {
		fprintf(stderr, "GLCompositor: eglChooseConfig failed\n");
		eglTerminate(fDisplay);
		fDisplay = EGL_NO_DISPLAY;
		return B_ERROR;
	}

	// Create GBM surface for EGL
	fGbmSurface = gbm_surface_create(gbm, fWidth, fHeight,
		GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (fGbmSurface == NULL) {
		fprintf(stderr, "GLCompositor: gbm_surface_create failed\n");
		eglTerminate(fDisplay);
		fDisplay = EGL_NO_DISPLAY;
		return B_ERROR;
	}

	// Create EGL surface from GBM surface
	fSurface = eglCreateWindowSurface(fDisplay, config,
		(EGLNativeWindowType)fGbmSurface, NULL);
	if (fSurface == EGL_NO_SURFACE) {
		fprintf(stderr, "GLCompositor: eglCreateWindowSurface failed\n");
		gbm_surface_destroy(fGbmSurface);
		fGbmSurface = NULL;
		eglTerminate(fDisplay);
		fDisplay = EGL_NO_DISPLAY;
		return B_ERROR;
	}

	// Create context
	EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	fContext = eglCreateContext(fDisplay, config, EGL_NO_CONTEXT,
		contextAttribs);
	if (fContext == EGL_NO_CONTEXT) {
		fprintf(stderr, "GLCompositor: eglCreateContext failed\n");
		eglDestroySurface(fDisplay, fSurface);
		fSurface = EGL_NO_SURFACE;
		gbm_surface_destroy(fGbmSurface);
		fGbmSurface = NULL;
		eglTerminate(fDisplay);
		fDisplay = EGL_NO_DISPLAY;
		return B_ERROR;
	}

	if (!eglMakeCurrent(fDisplay, fSurface, fSurface, fContext)) {
		fprintf(stderr, "GLCompositor: eglMakeCurrent failed\n");
		Shutdown();
		return B_ERROR;
	}

	// Create shaders and GL resources
	status_t ret = _CreateShaders();
	if (ret != B_OK) {
		Shutdown();
		return ret;
	}

	// Create texture for CPU buffer upload
	glGenTextures(1, &fTexture);
	glBindTexture(GL_TEXTURE_2D, fTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glViewport(0, 0, fWidth, fHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	fInitialized = true;
	printf("GLCompositor: GLES2 compositor initialized (%ux%u)\n",
		fWidth, fHeight);

	return B_OK;
}


void
GLCompositor::Shutdown()
{
	if (fDisplay != EGL_NO_DISPLAY) {
		eglMakeCurrent(fDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
			EGL_NO_CONTEXT);
	}

	_DestroyShaders();

	if (fTexture != 0) {
		glDeleteTextures(1, &fTexture);
		fTexture = 0;
	}

	if (fContext != EGL_NO_CONTEXT) {
		eglDestroyContext(fDisplay, fContext);
		fContext = EGL_NO_CONTEXT;
	}

	if (fSurface != EGL_NO_SURFACE) {
		eglDestroySurface(fDisplay, fSurface);
		fSurface = EGL_NO_SURFACE;
	}

	if (fGbmSurface != NULL) {
		gbm_surface_destroy(fGbmSurface);
		fGbmSurface = NULL;
	}

	if (fDisplay != EGL_NO_DISPLAY) {
		eglTerminate(fDisplay);
		fDisplay = EGL_NO_DISPLAY;
	}

	fInitialized = false;
}


status_t
GLCompositor::_CreateShaders()
{
	GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
	if (vs == 0)
		return B_ERROR;

	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
	if (fs == 0) {
		glDeleteShader(vs);
		return B_ERROR;
	}

	fProgram = glCreateProgram();
	glAttachShader(fProgram, vs);
	glAttachShader(fProgram, fs);
	glLinkProgram(fProgram);

	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint status;
	glGetProgramiv(fProgram, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		char log[512];
		glGetProgramInfoLog(fProgram, sizeof(log), NULL, log);
		fprintf(stderr, "GLCompositor: program link error: %s\n", log);
		glDeleteProgram(fProgram);
		fProgram = 0;
		return B_ERROR;
	}

	fAttrPosition = glGetAttribLocation(fProgram, "aPosition");
	fAttrTexCoord = glGetAttribLocation(fProgram, "aTexCoord");
	fUniformTexture = glGetUniformLocation(fProgram, "uTexture");

	// Create VBO for the fullscreen quad
	glGenBuffers(1, &fVBO);
	glBindBuffer(GL_ARRAY_BUFFER, fVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices,
		GL_STATIC_DRAW);

	return B_OK;
}


void
GLCompositor::_DestroyShaders()
{
	if (fVBO != 0) {
		glDeleteBuffers(1, &fVBO);
		fVBO = 0;
	}

	if (fProgram != 0) {
		glDeleteProgram(fProgram);
		fProgram = 0;
	}
}


status_t
GLCompositor::BlitBuffer(const void* bits, uint32_t width, uint32_t height,
	uint32_t stride)
{
	if (!fInitialized || bits == NULL)
		return B_ERROR;

	eglMakeCurrent(fDisplay, fSurface, fSurface, fContext);

	// Upload the CPU-rendered buffer as a texture.
	// The buffer is XRGB8888 which maps to GL_BGRA_EXT / GL_UNSIGNED_BYTE
	// on most Mesa drivers. Fall back to GL_RGBA with swizzle if needed.
	glBindTexture(GL_TEXTURE_2D, fTexture);

	// Handle stride != width*4 by uploading row-by-row if needed
	if (stride == width * 4) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, bits);
	} else {
		// Allocate texture storage first, then upload rows
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		for (uint32_t y = 0; y < height; y++) {
			const uint8_t* row = (const uint8_t*)bits + y * stride;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1,
				GL_RGBA, GL_UNSIGNED_BYTE, row);
		}
	}

	// Draw fullscreen quad
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(fProgram);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fTexture);
	glUniform1i(fUniformTexture, 0);

	glBindBuffer(GL_ARRAY_BUFFER, fVBO);
	glEnableVertexAttribArray(fAttrPosition);
	glVertexAttribPointer(fAttrPosition, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(fAttrTexCoord);
	glVertexAttribPointer(fAttrTexCoord, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void*)(2 * sizeof(float)));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(fAttrPosition);
	glDisableVertexAttribArray(fAttrTexCoord);

	eglSwapBuffers(fDisplay, fSurface);

	return B_OK;
}


struct gbm_bo*
GLCompositor::LockFrontBuffer()
{
	if (fGbmSurface == NULL)
		return NULL;

	return gbm_surface_lock_front_buffer(fGbmSurface);
}


void
GLCompositor::ReleaseFrontBuffer(struct gbm_bo* bo)
{
	if (fGbmSurface != NULL && bo != NULL)
		gbm_surface_release_buffer(fGbmSurface, bo);
}
