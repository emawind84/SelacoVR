/*
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
**---------------------------------------------------------------------------
** Copyright 2010-2020 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/ 
#ifdef WIN32
#include <Windows.h>
#endif

#include "gl_system.h"
#include "v_video.h"
#include "m_png.h"

#include "i_time.h"

#include "gl_interface.h"
#include "gl_framebuffer.h"
#include "gl_renderer.h"
#include "gl_renderbuffers.h"
#include "gl_samplers.h"
#include "hw_clock.h"
#include "hw_vrmodes.h"
#include "hw_skydome.h"
#include "hw_viewpointbuffer.h"
#include "hw_lightbuffer.h"
#include "gl_shaderprogram.h"
#include "gl_debug.h"
#include "r_videoscale.h"
#include "gl_buffers.h"
#include "gl_postprocessstate.h"
#include "v_draw.h"
#include "printf.h"
#include "gl_hwtexture.h"

#include "flatvertices.h"
#include "hw_cvars.h"

#include "filesystem.h"
#include "c_dispatch.h"

EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Bool, cl_capfps)
EXTERN_CVAR(Int, gl_pipeline_depth)
EXTERN_CVAR(Int, gl_max_transfer_threads)

void gl_LoadExtensions();
void gl_PrintStartupLog();

extern bool vid_hdr_active;


#ifdef WIN32
#include "hardware.h"
#include "win32glvideo.h"
#define gl_setAUXContext(a) static_cast<Win32GLVideo*>(Video)->setAuxContext(a)
#define gl_numAUXContexts() static_cast<Win32GLVideo*>(Video)->numAuxContexts()
#define gl_setNULLContext() static_cast<Win32GLVideo*>(Video)->setNULLContext()
#endif

#ifdef __POSIX_SDL_GL_SYSFB_H__
#include "hardware.h"
#define gl_setAUXContext(a) static_cast<OpenGLFrameBuffer*>(screen)->setAuxContext(a)
#define gl_numAUXContexts() static_cast<OpenGLFrameBuffer*>(screen)->numAuxContexts()
#define gl_setNULLContext() static_cast<OpenGLFrameBuffer*>(screen)->setNULLContext()
#endif


ADD_STAT(glloader)
{
	static int maxQueue = 0, maxSecondaryQueue = 0, queue, secQueue, total, collisions;
	static double minLoad = 0, maxLoad = 0, avgLoad = 0;

	auto sc = dynamic_cast<OpenGLRenderer::OpenGLFrameBuffer*>(screen);

	if (sc) {
		sc->GetBGQueueSize(queue, secQueue, collisions, maxQueue, maxSecondaryQueue, total);
		sc->GetBGStats(minLoad, maxLoad, avgLoad);

		FString out;
		out.AppendFormat(
			"[%d Threads] Queued: %3.3d - %3.3d  Collisions: %d\nMax: %3.3d Max Sec: %3.3d Tot: %d\n"
			"Min: %.3fms\n"
			"Max: %.3fms\n"
			"Avg: %.3fms\n",
			sc->GetNumThreads(), queue, secQueue, collisions, maxQueue, maxSecondaryQueue, total, minLoad, maxLoad, avgLoad
		);
		return out;
	}

	return "No OpenGL Device";
}

CCMD(gl_rstbgstats) {
	auto sc = dynamic_cast<OpenGLRenderer::OpenGLFrameBuffer*>(screen);
	if (sc) sc->ResetBGStats();
}


namespace OpenGLRenderer
{
	FGLRenderer *GLRenderer;

// @Cockatrice - Background Loader Stuff ===========================================
// =================================================================================
void GlTexLoadThread::bgproc() {
	gl_setAUXContext(auxContext);
	ResourceLoader2::bgproc();
	gl_setNULLContext();
}

void GlTexLoadThread::prepareLoad() {
	//gl_setAUXContext(auxContext);
}

bool GlTexLoadThread::loadResource(GlTexLoadIn & input, GlTexLoadOut & output) {
	FImageLoadParams* params = input.params;

	output.conversion = params->conversion;
	output.imgSource = input.imgSource;
	output.translation = params->translation;
	output.tex = input.tex;
	output.spi.generateSpi = input.spi.generateSpi;
	output.spi.notrimming = input.spi.notrimming;
	output.spi.shouldExpand = input.spi.shouldExpand;
	output.gtex = input.gtex;
	output.texUnit = input.texUnit;

	// Load pixels directly with the reader we copied on the main thread
	auto* src = input.imgSource;
	FBitmap pixels;

	bool indexed = false;	// TODO: Determine this properly
	bool mipmap = !indexed && input.allowMipmaps;
	bool gpu = src->IsGPUOnly();
	int exx = input.spi.shouldExpand && !gpu;
	int srcWidth = src->GetWidth();
	int srcHeight = src->GetHeight();
	int buffWidth = src->GetWidth() + 2 * exx;
	int buffHeight = src->GetHeight() + 2 * exx;

	

	if (exx && !gpu) {
		// This is incredibly wasteful, but necessary for now since we can't read the bitmap with an offset into a larger buffer
		// Read into a buffer and blit 
		FBitmap srcBitmap;
		srcBitmap.Create(srcWidth, srcHeight);
		output.isTranslucent = src->ReadPixels(params, &srcBitmap);
		pixels.Create(buffWidth, buffHeight);
		pixels.Blit(exx, exx, srcBitmap);

		// If we need sprite positioning info, generate it here and assign it in the main thread later
		if (input.spi.generateSpi) {
			FGameTexture::GenerateInitialSpriteData(output.spi.info, &srcBitmap, input.spi.shouldExpand, input.spi.notrimming);
		}
	}
	else {
		if (gpu) {
			int numMipLevels;
			size_t dataSize = 0, totalSize = 0;
			unsigned char* pixelData;
			output.isTranslucent = src->ReadCompressedPixels(params->reader, &pixelData, totalSize, dataSize, numMipLevels);
			output.tex->BackgroundCreateCompressedTexture(pixelData, (uint32_t)dataSize, (uint32_t)totalSize, buffWidth, buffHeight, input.texUnit, numMipLevels, "GlTexLoadThread::loadResource(Compressed)", !input.allowMipmaps);

			if (input.spi.generateSpi) {
				FGameTexture::GenerateEmptySpriteData(output.spi.info, buffWidth, buffHeight);
			}
		}
		else {
			pixels.Create(buffWidth, buffHeight);
			output.isTranslucent = src->ReadPixels(params, &pixels);

			if (input.spi.generateSpi) {
				FGameTexture::GenerateInitialSpriteData(output.spi.info, &pixels, input.spi.shouldExpand, input.spi.notrimming);
			}
		}
	}

	delete input.params;

	if(!gpu) 
		output.tex->BackgroundCreateTexture(pixels.GetPixels(), pixels.GetWidth(), pixels.GetHeight(), input.texUnit, mipmap, indexed, "GlTexLoadThread::loadResource()", !input.allowMipmaps);

	// TODO: Mark failed images as unloadable so they don't keep coming back to the queue
	return true;
}



// @Cockatrice - Background loader management =======================================
// ==================================================================================
void OpenGLFrameBuffer::GetBGQueueSize(int& current, int &secCurrent, int& collisions, int& max, int& maxSec, int& total) {
	max = maxSec = total = 0;
	current = primaryTexQueue.size();
	secCurrent = secondaryTexQueue.size();
	max = statMaxQueued;
	maxSec = statMaxQueuedSecondary;
	collisions = statCollisions;

	for (auto& tfr : bgTransferThreads) {
		total += tfr->statTotalLoaded();
	}
}

void OpenGLFrameBuffer::GetBGStats(double& min, double& max, double& avg) {
	min = 99999998;
	max = avg = 0;

	for (auto& tfr : bgTransferThreads) {
		min = std::min(tfr->statMinLoadTime(), min);
		max = std::max(tfr->statMaxLoadTime(), max);
		avg += tfr->statAvgLoadTime();
	}

	avg /= (double)bgTransferThreads.size();
}

void OpenGLFrameBuffer::ResetBGStats() {
	statMaxQueued = statMaxQueuedSecondary = 0;
	for (auto& tfr : bgTransferThreads) tfr->resetStats();
	statCollisions = 0;
}

void OpenGLFrameBuffer::PrequeueMaterial(FMaterial * mat, int translation)
{
	BackgroundCacheMaterial(mat, translation, true, true);
}


// @Cockatrice - Cache a texture material, intended for use outside of the main thread
bool OpenGLFrameBuffer::BackgroundCacheTextureMaterial(FGameTexture* tex, int translation, int scaleFlags, bool makeSPI) {
	if (!tex || !tex->isValid() || tex->GetID().GetIndex() == 0) return false;

	QueuedPatch qp = {
		tex, translation, scaleFlags, makeSPI
	};

	patchQueue.queue(qp);

	return true;
}


// @Cockatrice - Submit each texture in the material to the background loader
// Call from main thread only!
bool OpenGLFrameBuffer::BackgroundCacheMaterial(FMaterial* mat, int translation, bool makeSPI, bool secondary) {
	if (mat->Source()->GetUseType() == ETextureType::SWCanvas) return false;

	MaterialLayerInfo* layer;

	auto systex = static_cast<FHardwareTexture*>(mat->GetLayer(0, translation, &layer));
	auto remap = translation <= 0 || IsLuminosityTranslation(translation) ? nullptr : GPalette.TranslationToTable(translation);
	if (remap && remap->Inactive) remap = nullptr;

	// Submit each layer to the background loader
	int lump = layer->layerTexture->GetSourceLump();
	FResourceLump* rLump = lump >= 0 ? fileSystem.GetFileAt(lump) : nullptr;
	FImageLoadParams* params = nullptr;
	GlTexLoadSpi spi = {};
	bool shouldExpand = mat->sourcetex->ShouldExpandSprite() && (layer->scaleFlags & CTF_Expand);
	bool allowMipmaps = !mat->sourcetex->GetNoMipmaps();

	// If the texture is already submitted to the cache, find it and move it to the normal queue to reprioritize it
	if (rLump && !secondary && systex->GetState(0) == IHardwareTexture::HardwareState::CACHING) {
		GlTexLoadIn in;
		if (secondaryTexQueue.dequeueSearch(in, systex,
			[](void* a, GlTexLoadIn& b)
		{ return (FHardwareTexture*)a == b.tex; })) {
			systex->SetHardwareState(IHardwareTexture::HardwareState::LOADING, 0);
			primaryTexQueue.queue(in);
			return true;
		}
	}
	else if (rLump && systex->GetState(0) == IHardwareTexture::HardwareState::NONE) {
		assert(systex->GetTextureHandle() == 0);
		systex->SetHardwareState(secondary ? IHardwareTexture::HardwareState::CACHING : IHardwareTexture::HardwareState::LOADING, 0);
		
		FImageTexture* fLayerTexture = dynamic_cast<FImageTexture*>(layer->layerTexture);
		params = layer->layerTexture->GetImage()->NewLoaderParams(
			fLayerTexture ? (fLayerTexture->GetNoRemap0() ? FImageSource::noremap0 : FImageSource::normal) : FImageSource::normal,
			translation,
			remap
		);

		if (params != nullptr) {
			// Only generate SPI if it's not already there
			spi.generateSpi = makeSPI && !mat->sourcetex->HasSpritePositioning();
			spi.notrimming = mat->sourcetex->GetNoTrimming();
			spi.shouldExpand = shouldExpand;

			GlTexLoadIn in = {
				layer->layerTexture->GetImage(),
				params,
				spi,
				systex,
				mat->sourcetex,
				0,
				allowMipmaps
			};

			if (secondary) secondaryTexQueue.queue(in);
			else primaryTexQueue.queue(in);
		}
		else {
			systex->SetHardwareState(IHardwareTexture::HardwareState::READY, 0); // TODO: Set state to a special "unloadable" state
			return false;
		}
	}


	const int numLayers = mat->NumLayers();
	for (int i = 1; i < numLayers; i++)
	{
		FImageLoadParams* params = nullptr;
		auto syslayer = static_cast<FHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		lump = layer->layerTexture->GetSourceLump();
		rLump = lump >= 0 ? fileSystem.GetFileAt(lump) : nullptr;

		if (rLump && !secondary && syslayer->GetState(i) == IHardwareTexture::HardwareState::CACHING) {
			GlTexLoadIn in;
			if (secondaryTexQueue.dequeueSearch(in, syslayer,
				[](void* a, GlTexLoadIn& b)
			{ return (FHardwareTexture*)a == b.tex; })) {
				syslayer->SetHardwareState(IHardwareTexture::HardwareState::LOADING, i);
				primaryTexQueue.queue(in);
				return true;
			}
		}
		else if (rLump && syslayer->GetState(i) == IHardwareTexture::HardwareState::NONE) {
			syslayer->SetHardwareState(secondary ? IHardwareTexture::HardwareState::CACHING : IHardwareTexture::HardwareState::LOADING, i);
			
			FImageTexture* fLayerTexture = dynamic_cast<FImageTexture*>(layer->layerTexture);
			params = layer->layerTexture->GetImage()->NewLoaderParams(
				fLayerTexture ? (fLayerTexture->GetNoRemap0() ? FImageSource::noremap0 : FImageSource::normal) : FImageSource::normal,
				0, // translation
				nullptr// remap
			);

			if (params != nullptr) {
				assert(syslayer->GetTextureHandle() == 0);

				GlTexLoadIn in = {
					layer->layerTexture->GetImage(),
					params,
					{
						false, shouldExpand, true
					},
					syslayer,
					nullptr,
					i,
					allowMipmaps
				};

				if (secondary) secondaryTexQueue.queue(in);
				else primaryTexQueue.queue(in);
			}
			else {
				syslayer->SetHardwareState(IHardwareTexture::HardwareState::READY, i); // TODO: Set state to a special "unloadable" state
			}
		}
	}

	statMaxQueued = max(statMaxQueued, primaryTexQueue.size());
	statMaxQueuedSecondary = max(statMaxQueuedSecondary, secondaryTexQueue.size());

	return true;
}


void OpenGLFrameBuffer::StopBackgroundCache() {
	primaryTexQueue.clear();
	secondaryTexQueue.clear();
	patchQueue.clear();

	for (auto& tfr : bgTransferThreads) {
		tfr->stop();
	}
}


void OpenGLFrameBuffer::FlushBackground() {
	int nq = primaryTexQueue.size() + secondaryTexQueue.size();
	bool active = nq;

	if (!active)
		for (auto& tfr : bgTransferThreads) active = active || tfr->isActive();

	Printf(TEXTCOLOR_GREEN"OpenGLFrameBuffer[%s]: Flushing [%d + %d] = %d texture load ops\n", active ? "active" : "inactive", nq, patchQueue.size(), nq + patchQueue.size());

	// Make sure active is marked if we have patches waiting
	active = active || patchQueue.size() > 0;

	// Finish anything queued, and send anything that needs to be loaded from the patch queue
	UpdateBackgroundCache(true);

	// Wait for everything to load, kinda cheating here but this shouldn't be called in the game loop only at teardown
	cycle_t check = cycle_t();
	check.Clock();

	while (active) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		UpdateBackgroundCache(true);

		active = false;
		for (auto& tfr : bgTransferThreads)
			active = active || tfr->isActive();
	}

	// Finish anything that was loaded
	UpdateBackgroundCache(true);

	check.Unclock();
	Printf(TEXTCOLOR_GOLD"OpenGLFrameBuffer::FlushBackground() took %f ms\n", check.TimeMS());
}


void OpenGLFrameBuffer::UpdateBackgroundCache(bool flush) {
	// Check for completed cache items and link textures to the data
	GlTexLoadOut loaded;

	while (outputTexQueue.size() > 0) {
		if (!outputTexQueue.dequeue(loaded)) break;

		// Set the sprite positioning if we loaded it and it hasn't already been applied
		if (loaded.spi.generateSpi && loaded.gtex && !loaded.gtex->HasSpritePositioning()) {
			SpritePositioningInfo* spi = (SpritePositioningInfo*)ImageArena.Alloc(2 * sizeof(SpritePositioningInfo));
			memcpy(spi, loaded.spi.info, 2 * sizeof(SpritePositioningInfo));
			loaded.gtex->SetSpriteRect(spi);
		}
		
		if (loaded.tex->GetState(loaded.texUnit) == IHardwareTexture::HardwareState::READY || loaded.tex->GetTextureHandle() > 0) {
			// If we already have a texture in place here, destroy the loaded texture
			// This can happen because the engine has already loaded the texture on the main thread
			// Or somehow multiple requests to load this texture were made
			statCollisions++;
			loaded.tex->DestroyLoadedImage();
		}
		else {
			bool swapped = loaded.tex->SwapToLoadedImage();
			assert(swapped);

			loaded.tex->SetHardwareState(IHardwareTexture::HardwareState::READY, loaded.texUnit);
			if (loaded.gtex) loaded.gtex->SetTranslucent(loaded.isTranslucent);
		}
	}

	// Submit all of the patches that need to be loaded
	QueuedPatch qp;
	while (patchQueue.dequeue(qp)) {
		FMaterial* gltex = FMaterial::ValidateTexture(qp.tex, qp.scaleFlags, true);
		if (gltex && !gltex->IsHardwareCached(qp.translation)) {
			BackgroundCacheMaterial(gltex, qp.translation, qp.generateSPI);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

OpenGLFrameBuffer::OpenGLFrameBuffer(void *hMonitor, bool fullscreen) : 
	Super(hMonitor, fullscreen) 
{
	// SetVSync needs to be at the very top to workaround a bug in Nvidia's OpenGL driver.
	// If wglSwapIntervalEXT is called after glBindFramebuffer in a frame the setting is not changed!
	Super::SetVSync(vid_vsync);
	FHardwareTexture::InitGlobalState();

	// Make sure all global variables tracking OpenGL context state are reset..
	gl_RenderState.Reset();

	GLRenderer = nullptr;
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
	PPResource::ResetAll();

	bgTransferThreads.clear();

	if (mVertexData != nullptr) delete mVertexData;
	if (mSkyData != nullptr) delete mSkyData;
	if (mViewpoints != nullptr) delete mViewpoints;
	if (mLights != nullptr) delete mLights;
	mShadowMap.Reset();

	if (GLRenderer)
	{
		delete GLRenderer;
		GLRenderer = nullptr;
	}
}

//==========================================================================
//
// Initializes the GL renderer
//
//==========================================================================

void OpenGLFrameBuffer::InitializeState()
{
	static bool first=true;

	if (first)
	{
		if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
		{
			I_FatalError("Failed to load OpenGL functions.");
		}
	}

	gl_LoadExtensions();

	mPipelineNbr = clamp(*gl_pipeline_depth, 1, HW_MAX_PIPELINE_BUFFERS);
	mPipelineType = gl_pipeline_depth > 0;

	// Move some state to the framebuffer object for easier access.
	hwcaps = gl.flags;
	glslversion = gl.glslversion;
	uniformblockalignment = gl.uniformblockalignment;
	maxuniformblock = gl.maxuniformblock;
	vendorstring = gl.vendorstring;

	if (first)
	{
		first=false;
		gl_PrintStartupLog();
	}

	glDepthFunc(GL_LESS);

	glEnable(GL_DITHER);
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_CLAMP);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LINE_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	SetViewportRects(nullptr);

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight(), screen->mPipelineNbr);
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new HWViewpointBuffer(screen->mPipelineNbr);
	mLights = new FLightBuffer(screen->mPipelineNbr);
	GLRenderer = new FGLRenderer(this);
	GLRenderer->Initialize(GetWidth(), GetHeight());
	static_cast<GLDataBuffer*>(mLights->GetBuffer())->BindBase();

	mDebug = std::make_unique<FGLDebug>();
	mDebug->Update();

	
	int numThreads = min((int)gl_max_transfer_threads, gl_numAUXContexts());
	for (int x = 0; x < numThreads; x++) {
		std::unique_ptr<GlTexLoadThread> ptr(new GlTexLoadThread(this, x, &primaryTexQueue, &secondaryTexQueue, &outputTexQueue));
		ptr->start();
		bgTransferThreads.push_back(std::move(ptr));
	}
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

void OpenGLFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	Flush3D.Clock();
	GLRenderer->Flush();
	Flush3D.Unclock();

	Swap();
	Super::Update();
}

void OpenGLFrameBuffer::CopyScreenToBuffer(int width, int height, uint8_t* scr)
{
	IntRect bounds;
	bounds.left = 0;
	bounds.top = 0;
	bounds.width = width;
	bounds.height = height;
	GLRenderer->CopyToBackbuffer(&bounds, false);

	// strictly speaking not needed as the glReadPixels should block until the scene is rendered, but this is to safeguard against shitty drivers
	glFinish();
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, scr);
}

//===========================================================================
//
// Camera texture rendering
//
//===========================================================================

void OpenGLFrameBuffer::RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc)
{
	GLRenderer->StartOffscreen();
	GLRenderer->BindToFrameBuffer(tex);

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = FHardwareTexture::GetTexDimension(tex->GetWidth());
	bounds.height = FHardwareTexture::GetTexDimension(tex->GetHeight());

	renderFunc(bounds);
	GLRenderer->EndOffscreen();

	tex->SetUpdated(true);
	static_cast<OpenGLFrameBuffer*>(screen)->camtexcount++;
}

//===========================================================================
//
// 
//
//===========================================================================

const char* OpenGLFrameBuffer::DeviceName() const 
{
	return gl.modelstring;
}

//==========================================================================
//
// Swap the buffers
//
//==========================================================================

CVAR(Bool, gl_finishbeforeswap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

void OpenGLFrameBuffer::Swap()
{
	bool swapbefore = gl_finishbeforeswap && camtexcount == 0;
	Finish.Reset();
	Finish.Clock();
	if (gl_pipeline_depth < 1)
	{
		if (swapbefore) glFinish();
		FPSLimit();
		SwapBuffers();
		if (!swapbefore) glFinish();
	}
	else
	{
		mVertexData->DropSync();

		FPSLimit();
		SwapBuffers();

		mVertexData->NextPipelineBuffer();
		mVertexData->WaitSync();

		RenderState()->SetVertexBuffer(screen->mVertexData); // Needed for Raze because it does not reset it
	}
	Finish.Unclock();
	camtexcount = 0;
	FHardwareTexture::UnbindAll();
	gl_RenderState.ClearLastMaterial();
	mDebug->Update();
}

//==========================================================================
//
// Enable/disable vertical sync
//
//==========================================================================

void OpenGLFrameBuffer::SetVSync(bool vsync)
{
	// Switch to the default frame buffer because some drivers associate the vsync state with the bound FB object.
	GLint oldDrawFramebufferBinding = 0, oldReadFramebufferBinding = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebufferBinding);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	Super::SetVSync(vsync);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
}

//===========================================================================
//
//
//===========================================================================

void OpenGLFrameBuffer::SetTextureFilterMode()
{
	if (GLRenderer != nullptr && GLRenderer->mSamplerManager != nullptr) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

IHardwareTexture *OpenGLFrameBuffer::CreateHardwareTexture(int numchannels) 
{ 
	return new FHardwareTexture(numchannels);
}

void OpenGLFrameBuffer::PrecacheMaterial(FMaterial *mat, int translation)
{
	if (mat->Source()->GetUseType() == ETextureType::SWCanvas) return;

	int numLayers = mat->NumLayers();
	MaterialLayerInfo* layer;
	auto base = static_cast<FHardwareTexture*>(mat->GetLayer(0, translation, &layer));

	if (base->BindOrCreate(layer->layerTexture, 0, CLAMP_NONE, translation, layer->scaleFlags))
	{
		for (int i = 1; i < numLayers; i++)
		{
			auto systex = static_cast<FHardwareTexture*>(mat->GetLayer(i, 0, &layer));
			systex->BindOrCreate(layer->layerTexture, i, CLAMP_NONE, 0, layer->scaleFlags);
		}
	}
	// unbind everything. 
	FHardwareTexture::UnbindAll();
	gl_RenderState.ClearLastMaterial();
}

IVertexBuffer *OpenGLFrameBuffer::CreateVertexBuffer()
{ 
	return new GLVertexBuffer; 
}

IIndexBuffer *OpenGLFrameBuffer::CreateIndexBuffer()
{ 
	return new GLIndexBuffer; 
}

IDataBuffer *OpenGLFrameBuffer::CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize)
{
	return new GLDataBuffer(bindingpoint, ssbo);
}

void OpenGLFrameBuffer::BlurScene(float amount, bool force)
{
	GLRenderer->BlurScene(amount, force);
}

void OpenGLFrameBuffer::InitLightmap(int LMTextureSize, int LMTextureCount, TArray<uint16_t>& LMTextureData)
{
	if (LMTextureData.Size() > 0)
	{
		GLint activeTex = 0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
		glActiveTexture(GL_TEXTURE0 + 17);

		if (GLRenderer->mLightMapID == 0)
			glGenTextures(1, (GLuint*)&GLRenderer->mLightMapID);

		glBindTexture(GL_TEXTURE_2D_ARRAY, GLRenderer->mLightMapID);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB16F, LMTextureSize, LMTextureSize, LMTextureCount, 0, GL_RGB, GL_HALF_FLOAT, &LMTextureData[0]);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

		glActiveTexture(activeTex);

		LMTextureData.Reset(); // We no longer need this, release the memory
	}
}

void OpenGLFrameBuffer::SetViewportRects(IntRect *bounds)
{
	Super::SetViewportRects(bounds);
	if (!bounds)
	{
		auto vrmode = VRMode::GetVRMode(true);
		vrmode->AdjustViewport(this);
	}
}

void OpenGLFrameBuffer::UpdatePalette()
{
	if (GLRenderer)
		GLRenderer->ClearTonemapPalette();
}

FRenderState* OpenGLFrameBuffer::RenderState()
{
	return &gl_RenderState;
}

void OpenGLFrameBuffer::AmbientOccludeScene(float m5)
{
	gl_RenderState.EnableDrawBuffers(1);
	GLRenderer->AmbientOccludeScene(m5);
	glViewport(screen->mSceneViewport.left, mSceneViewport.top, mSceneViewport.width, mSceneViewport.height);
	GLRenderer->mBuffers->BindSceneFB(true);
	gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
	gl_RenderState.Apply();
}

void OpenGLFrameBuffer::FirstEye()
{
	GLRenderer->mBuffers->CurrentEye() = 0;  // always begin at zero, in case eye count changed
}

void OpenGLFrameBuffer::NextEye(int eyecount)
{
	GLRenderer->mBuffers->NextEye(eyecount);
}

void OpenGLFrameBuffer::SetSceneRenderTarget(bool useSSAO)
{
	GLRenderer->mBuffers->BindSceneFB(useSSAO);
}

void OpenGLFrameBuffer::UpdateShadowMap()
{
	if (mShadowMap.PerformUpdate())
	{
		FGLDebug::PushGroup("ShadowMap");

		FGLPostProcessState savedState;

		static_cast<GLDataBuffer*>(screen->mShadowMap.mLightList)->BindBase();
		static_cast<GLDataBuffer*>(screen->mShadowMap.mNodesBuffer)->BindBase();
		static_cast<GLDataBuffer*>(screen->mShadowMap.mLinesBuffer)->BindBase();

		GLRenderer->mBuffers->BindShadowMapFB();

		GLRenderer->mShadowMapShader->Bind();
		GLRenderer->mShadowMapShader->Uniforms->ShadowmapQuality = gl_shadowmap_quality;
		GLRenderer->mShadowMapShader->Uniforms->NodesCount = screen->mShadowMap.NodesCount();
		GLRenderer->mShadowMapShader->Uniforms.SetData();
		static_cast<GLDataBuffer*>(GLRenderer->mShadowMapShader->Uniforms.GetBuffer())->BindBase();

		glViewport(0, 0, gl_shadowmap_quality, 1024);
		GLRenderer->RenderScreenQuad();

		const auto& viewport = screen->mScreenViewport;
		glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

		GLRenderer->mBuffers->BindShadowMapTexture(16);
		FGLDebug::PopGroup();
		screen->mShadowMap.FinishUpdate();
	}
}

void OpenGLFrameBuffer::WaitForCommands(bool finish)
{
	glFinish();
}

void OpenGLFrameBuffer::SetSaveBuffers(bool yes)
{
	if (!GLRenderer) return;
	if (yes) GLRenderer->mBuffers = GLRenderer->mSaveBuffers;
	else GLRenderer->mBuffers = GLRenderer->mScreenBuffers;
}

//===========================================================================
//
// 
//
//===========================================================================

void OpenGLFrameBuffer::BeginFrame()
{
	SetViewportRects(nullptr);

	UpdateBackgroundCache();

	if (GLRenderer != nullptr)
		GLRenderer->BeginFrame();
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

TArray<uint8_t> OpenGLFrameBuffer::GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma)
{
	const auto &viewport = mOutputLetterbox;

	// Grab what is in the back buffer.
	// We cannot rely on SCREENWIDTH/HEIGHT here because the output may have been scaled.
	TArray<uint8_t> pixels;
	pixels.Resize(viewport.width * viewport.height * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(viewport.left, viewport.top, viewport.width, viewport.height, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	// Copy to screenshot buffer:
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	TArray<uint8_t> ScreenshotBuffer(w * h * 3, true);

	float rcpWidth = 1.0f / w;
	float rcpHeight = 1.0f / h;
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			float u = (x + 0.5f) * rcpWidth;
			float v = (y + 0.5f) * rcpHeight;
			int sx = u * viewport.width;
			int sy = v * viewport.height;
			int sindex = (sx + sy * viewport.width) * 3;
			int dindex = (x + (h - y - 1) * w) * 3;
			ScreenshotBuffer[dindex] = pixels[sindex];
			ScreenshotBuffer[dindex + 1] = pixels[sindex + 1];
			ScreenshotBuffer[dindex + 2] = pixels[sindex + 2];
		}
	}

	pitch = w * 3;
	color_type = SS_RGB;

	// Screenshot should not use gamma correction if it was already applied to rendered image
	gamma = 1;
	if (vid_hdr_active && vid_fullscreen)
		gamma *= 2.2f;
	return ScreenshotBuffer;
}

//===========================================================================
// 
// 2D drawing
//
//===========================================================================

void OpenGLFrameBuffer::Draw2D()
{
	if (GLRenderer != nullptr)
	{
		GLRenderer->mBuffers->BindCurrentFB();
		::Draw2D(twod, gl_RenderState);
	}
}

void OpenGLFrameBuffer::PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D)
{
	if (!swscene) GLRenderer->mBuffers->BlitSceneToTexture(); // Copy the resulting scene to the current post process texture
	GLRenderer->PostProcessScene(fixedcm, flash, afterBloomDrawEndScene2D);
}

bool OpenGLFrameBuffer::CompileNextShader()
{
	return GLRenderer->mShaderManager->CompileNextShader();
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeStartScreen
//
// Called before the current screen has started rendering. This needs to
// save what was drawn the previous frame so that it can be animated into
// what gets drawn this frame.
//
//==========================================================================

FTexture *OpenGLFrameBuffer::WipeStartScreen()
{
	const auto &viewport = screen->mScreenViewport;

	auto tex = new FWrapperTexture(viewport.width, viewport.height, 1);
	tex->GetSystemTexture()->CreateTexture(nullptr, viewport.width, viewport.height, 0, false, "WipeStartScreen");
	glFinish();
	static_cast<FHardwareTexture*>(tex->GetSystemTexture())->Bind(0, false);

	GLRenderer->mBuffers->BindCurrentFB();
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport.left, viewport.top, viewport.width, viewport.height);
	return tex;
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeEndScreen
//
// The screen we want to animate to has just been drawn.
//
//==========================================================================

FTexture *OpenGLFrameBuffer::WipeEndScreen()
{
	GLRenderer->Flush();
	const auto &viewport = screen->mScreenViewport;
	auto tex = new FWrapperTexture(viewport.width, viewport.height, 1);
	tex->GetSystemTexture()->CreateTexture(NULL, viewport.width, viewport.height, 0, false, "WipeEndScreen");
	glFinish();
	static_cast<FHardwareTexture*>(tex->GetSystemTexture())->Bind(0, false);
	GLRenderer->mBuffers->BindCurrentFB();
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport.left, viewport.top, viewport.width, viewport.height);
	return tex;
}

}
