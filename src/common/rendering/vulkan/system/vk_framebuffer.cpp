/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "volk/volk.h"

#include <inttypes.h>

#include "v_video.h"
#include "m_png.h"

#include "r_videoscale.h"
#include "i_time.h"
#include "v_text.h"
#include "version.h"
#include "v_draw.h"

#include "hw_clock.h"
#include "hw_vrmodes.h"
#include "hw_cvars.h"
#include "hw_skydome.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "flatvertices.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hw_lightbuffer.h"

#include "vk_framebuffer.h"
#include "vk_hwbuffer.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/renderer/vk_descriptorset.h"
#include "vulkan/renderer/vk_streambuffer.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/renderer/vk_raytrace.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_swapchain.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/system/vk_buffer.h"
#include "engineerrors.h"
#include "c_dispatch.h"

#include "image.h"

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, cl_capfps)

CCMD(vk_memstats)
{
	if (screen->IsVulkan())
	{
		VmaStats stats = {};
		vmaCalculateStats(static_cast<VulkanFrameBuffer*>(screen)->device->allocator, &stats);
		Printf("Allocated objects: %d, used bytes: %d MB\n", (int)stats.total.allocationCount, (int)stats.total.usedBytes / (1024 * 1024));
		Printf("Unused range count: %d, unused bytes: %d MB\n", (int)stats.total.unusedRangeCount, (int)stats.total.unusedBytes / (1024 * 1024));
	}
	else
	{
		Printf("Vulkan is not the current render device\n");
	}
}

CVAR(Bool, vk_raytrace, false, 0/*CVAR_ARCHIVE | CVAR_GLOBALCONFIG*/)	// @Cockatrice - UGH



ADD_STAT(vkloader)
{
	static int maxQueue = 0, maxSecondaryQueue = 0, queue, total;
	static double minLoad = 0, maxLoad = 0, avgLoad = 0;

	auto sc = dynamic_cast<VulkanFrameBuffer *>(screen);

	if (sc) {
		sc->GetBGQueueSize(queue, maxQueue, maxSecondaryQueue, total);
		sc->GetBGStats(minLoad, maxLoad, avgLoad);

		FString out;
		out.AppendFormat(
			"Queued: %3.3d Max: %3.3d Max Sec: %3.3d Tot: %d\n"
			"Min: %.3fms\n"
			"Max: %.3fms\n"
			"Avg: %.3fms\n",
			queue, maxQueue, maxSecondaryQueue, total, minLoad, maxLoad, avgLoad
		);
		return out;
	}
	
	return "No Vulkan Device";
}

CCMD(vk_rstbgstats) {
	auto sc = dynamic_cast<VulkanFrameBuffer *>(screen);
	if (sc) sc->ResetBGStats();
}

void VulkanFrameBuffer::GetBGQueueSize(int &current, int &max, int &maxSec, int &total) {
	current = bgTransferThread->numQueued();
	max = bgTransferThread->statMaxQueued();
	maxSec = bgTransferThread->statMaxSecondaryQueued();
	total = bgTransferThread->statTotalLoaded();
}

void VulkanFrameBuffer::GetBGStats(double &min, double &max, double &avg) {
	min = bgTransferThread->statMinLoadTime();
	max = bgTransferThread->statMaxLoadTime();
	avg = bgTransferThread->statAvgLoadTime();
}

void VulkanFrameBuffer::ResetBGStats() {
	bgTransferThread->resetStats();
}


// @Cockatrice - Background Loader Stuff ===========================================
// =================================================================================
VkTexLoadThread::~VkTexLoadThread() {
	/*VulkanDevice* device = cmd->GetFrameBuffer()->device;

	// Finish up anything that was running so we can destroy the resources
	if(submits > 0) {
		vkWaitForFences(device->device, submits, submitWaitFences, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(device->device, submits, submitWaitFences);
		deleteList.clear();
	}*/
}

bool VkTexLoadThread::moveToMainQueue(VkHardwareTexture *tex) {
	// Find and requeue
	VkTexLoadIn in;
	if(mInputSecondaryQ.dequeueSearch(in, tex,
		[](void *a, VkTexLoadIn &b)
		{ return (VkHardwareTexture*)a == b.tex; })) {
		tex->SetHardwareState(IHardwareTexture::HardwareState::LOADING);
		queue(in);
		return true;
	}
	return false;
}

bool VkTexLoadThread::loadResource(VkTexLoadIn &input, VkTexLoadOut &output) {
	currentImageID.store(input.imgSource->GetId());

	FImageLoadParams *params = input.params;

	output.conversion = params->conversion;
	output.imgSource = input.imgSource;
	output.translation = params->translation;
	output.tex = input.tex;
	output.spi.generateSpi = input.spi.generateSpi;
	output.spi.notrimming = input.spi.notrimming;
	output.spi.shouldExpand = input.spi.shouldExpand;
	output.gtex = input.gtex;
	output.releaseSemaphore = nullptr;

	// Load pixels directly with the reader we copied on the main thread
	auto *src = input.imgSource;
	FBitmap pixels;
	pixels.Create(src->GetWidth(), src->GetHeight());	// TODO: Error checking

	output.isTranslucent = src->ReadPixels(params, &pixels);
	
	/*if (input.translationRemap) {

	} else if (IsLuminosityTranslation(input.translation)) {
		V_ApplyLuminosityTranslation(input.translation, pixels.GetPixels(), src->GetWidth() * src->GetHeight());
	}*/

	delete input.params;

	// We have the image now, let's upload it through the channel created exclusively for background ops
	// If we really wanted to be efficient, we would do the disk loading in one thread and the texture upload in another
	// But for now this approach should yield reasonable results
	VulkanDevice* device = cmd->GetFrameBuffer()->device;
	bool indexed = false;	// TODO: Determine this properly
	bool mipmap = !indexed;
	VkFormat fmt = indexed ? VK_FORMAT_R8_UNORM : VK_FORMAT_B8G8R8A8_UNORM;
	output.tex->BackgroundCreateTexture(pixels.GetWidth(), pixels.GetHeight(), indexed ? 1 : 4, indexed ? VK_FORMAT_R8_UNORM : VK_FORMAT_B8G8R8A8_UNORM, pixels.GetPixels(), !indexed);
	output.createMipmaps = mipmap && !device->uploadFamilySupportsGraphics;
	
	// If we need sprite positioning info, generate it here and assign it in the main thread later
	if (input.spi.generateSpi) {
		FGameTexture::GenerateInitialSpriteData(output.spi.info, &pixels, input.spi.shouldExpand, input.spi.notrimming);
	}

	// If we created the texture on a different family than the graphics family, we need to release access 
	// to the image on this queue
	if(device->graphicsFamily != device->uploadFamily) {
		auto cmds = cmd->CreateUnmanagedCommands();
		cmds->SetDebugName("BGThread::QueueMoveCMDS");
		output.releaseSemaphore = new VulkanSemaphore(device);
		output.tex->ReleaseLoadedFromQueue(cmds.get(), device->uploadFamily, device->graphicsFamily);
		cmds->end();

		QueueSubmit submit;
		submit.AddCommandBuffer(cmds.get());
		submit.AddSignal(output.releaseSemaphore);
		
		deleteList.push_back(std::move(cmds));

		//if(++submits == 8) {
			// TODO: We have to wait for each submit right now, because for some reason we can't rely on sempaphores
			// being used by the time we get back to the main thread and move resources to the main graphics queue.
			// I believe this is incorrect, we should be able to move on here without having to wait.
			submits = 1;
			submit.Execute(device, device->uploadQueue, submitFences[submits - 1].get());
			vkWaitForFences(device->device, submits, submitWaitFences, VK_TRUE, std::numeric_limits<uint64_t>::max());
			vkResetFences(device->device, submits, submitWaitFences);
			deleteList.clear();
			submits = 0;
		//} else {
		//	submit.Execute(device, device->uploadQueue, submitFences[submits - 1].get());
		//}
	}



	// Always return true, because failed images need to be marked as unloadable
	// TODO: Mark failed images as unloadable so they don't keep coming back to the queue
	// TODO: Load indexed images properly
	return true;
}

void VkTexLoadThread::cancelLoad() { currentImageID.store(0); }
void VkTexLoadThread::completeLoad() { currentImageID.store(0); }

// END Background Loader Stuff =====================================================

void VulkanFrameBuffer::FlushBackground() {
	int nq = bgTransferThread->numQueued();

	Printf(TEXTCOLOR_GREEN"VulkanFrameBuffer[%s]: Flushing [%d + %d] = %d texture load ops\n", bgTransferThread->isActive() ? "active" : "inactive", nq, patchQueue.size(), nq + patchQueue.size());

	// Finish anything queued, and send anything that needs to be loaded from the patch queue
	UpdateBackgroundCache(true);
		
	// Wait for everything to load, kinda cheating here but this shouldn't be called in the game loop only at teardown
	cycle_t check = cycle_t();
	check.Clock();
	while (bgTransferThread->isActive()) {
		std::this_thread::sleep_for(std::chrono::nanoseconds(50000));

		check.Unclock();
		if (check.TimeMS() > 100) {
			check.Reset();
			Printf(TEXTCOLOR_GOLD"VulkanFrameBuffer::FlushBackground() Is taking a while to finish load ops (100ms+)...\n");
			check.Clock();
		}
	}

	// Finish anything that was loaded
	UpdateBackgroundCache(true);
}

void VulkanFrameBuffer::UpdateBackgroundCache(bool flush) {
	// Check for completed cache items and link textures to the data
	VkTexLoadOut loaded;
	
	// If we have previously made a submit, make sure it has finished and release resources
	if(bgtFence.get() && bgtHasFence) {
		bgtHasFence = false;
		vkWaitForFences(device->device, 1, &bgtFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(device->device, 1, &bgtFence->fence);
		bgtCmds.release();
		bgtSm4List.clear();
	}

	// Start processing finished resource loads
	if (bgTransferThread->numFinished() > 0) {
		std::unique_ptr<VulkanCommandBuffer> cmds = device->uploadFamily != device->graphicsFamily ? mCommands->CreateUnmanagedCommands() : nullptr;
		if(cmds.get()) cmds->SetDebugName("MainThread::QueueMoveCMDS");

		unsigned int sm4StartIndex = (unsigned int)bgtSm4List.size() - 1;

		while (bgTransferThread->popFinished(loaded)) {
			if(loaded.tex->hwState == IHardwareTexture::HardwareState::READY) {
				//Printf("Background proc Loaded an already-loaded image: %s What a farting mistake!\n", loaded.gtex ? loaded.gtex->GetName().GetChars() : "UNKNOWN");
				
				// Set the sprite positioning if we loaded it and it hasn't already been applied
				if(loaded.spi.generateSpi && loaded.gtex && !loaded.gtex->HasSpritePositioning()) {
					//Printf("Setting SPI even though we rejected the texture.\n");
					SpritePositioningInfo *spi = (SpritePositioningInfo*)ImageArena.Alloc(2 * sizeof(SpritePositioningInfo));
					memcpy(spi, loaded.spi.info, 2 * sizeof(SpritePositioningInfo));
					loaded.gtex->SetSpriteRect(spi);
				}

				// TODO: Release resources now, instead of automatically doing it later
				continue;
			}

			// If this image was created in a different queue family, it now needs to be moved over to
			// the graphics queue faimly
			if (device->uploadFamily != device->graphicsFamily && loaded.tex->mLoadedImage) {
				loaded.tex->AcquireLoadedFromQueue(cmds.get(), device->uploadFamily, device->graphicsFamily);
				
				// If we cannot create mipmaps in the background, tell the GPU to create them now
				if (loaded.createMipmaps) {
					loaded.tex->mLoadedImage.get()->GenerateMipmaps(cmds.get());
				}

				if (loaded.releaseSemaphore) {
					loaded.releaseSemaphore->SetDebugName("BGT::RlsA");
					bgtSm4List.push_back(std::unique_ptr<VulkanSemaphore>(loaded.releaseSemaphore));
				}

				//Printf(" moving: %d\n", loaded.imgSource->GetId());
			}

			loaded.tex->SwapToLoadedImage();
			loaded.tex->SetHardwareState(IHardwareTexture::HardwareState::READY);
			if (loaded.gtex) loaded.gtex->SetTranslucent(loaded.isTranslucent);

			// Set the sprite positioning info if generated
			if (loaded.spi.generateSpi && loaded.gtex) {
				if(!loaded.gtex->HasSpritePositioning()) {
					SpritePositioningInfo *spi = (SpritePositioningInfo*)ImageArena.Alloc(2 * sizeof(SpritePositioningInfo));
					memcpy(spi, loaded.spi.info, 2 * sizeof(SpritePositioningInfo));
					loaded.gtex->SetSpriteRect(spi);
				}
			}
		}

		// We only need to run commands on the GPU if transfer/mipmaps were needed
		if (cmds != nullptr) {
			QueueSubmit submit;
			// Just in case no mipmaps were created, but we transferred ownership
			// Make sure fragment shaders don't process before our transfers are done
			PipelineBarrier().AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT).Execute(
				cmds.get(),
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			);

			cmds->end();

			submit.AddCommandBuffer(cmds.get());

			for(unsigned int x = sm4StartIndex; x < bgtSm4List.size(); x++) {
				submit.AddWait(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, bgtSm4List[x].get());
			}
			
			if(!bgtFence.get()) bgtFence.reset(new VulkanFence(device));
			
			submit.Execute(device, device->graphicsQueue, bgtFence.get());

			if(flush) {
				// Wait if flushing
				bgtHasFence = false;
				vkWaitForFences(device->device, 1, &bgtFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
				vkResetFences(device->device, 1, &bgtFence->fence);
				bgtSm4List.clear();
			} else {
				bgtHasFence = true;
				bgtCmds = std::move(cmds);
			}
		}
	}

	// Submit all of the patches that need to be loaded
	QueuedPatch qp;
	while (patchQueue.dequeue(qp)) {
		FMaterial * gltex = FMaterial::ValidateTexture(qp.tex, qp.scaleFlags, true);
		if (gltex && !gltex->IsHardwareCached(qp.translation)) {
			BackgroundCacheMaterial(gltex, qp.translation, qp.generateSPI);
		}
	}
}



VulkanFrameBuffer::VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev) : 
	Super(hMonitor, fullscreen) 
{
	device = dev;
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
	vkDeviceWaitIdle(device->device); // make sure the GPU is no longer using any objects before RAII tears them down

	delete mVertexData;
	delete mSkyData;
	delete mViewpoints;
	delete mLights;
	mShadowMap.Reset();

	if (mDescriptorSetManager)
		mDescriptorSetManager->Deinit();
	if (mTextureManager)
		mTextureManager->Deinit();
	if (mBufferManager)
		mBufferManager->Deinit();
	if (mShaderManager)
		mShaderManager->Deinit();

	mCommands->DeleteFrameObjects();
	mBGTransferCommands->DeleteFrameObjects();
}

void VulkanFrameBuffer::StopBackgroundCache() {
	if (bgTransferThread.get()) {
		bgTransferThread->clearInputQueue();
		bgTransferThread->clearSecondaryInputQueue();
		bgTransferThread->stop();
	}
}

void VulkanFrameBuffer::InitializeState()
{
	static bool first = true;
	if (first)
	{
		PrintStartupLog();
		first = false;
	}

	// Use the same names here as OpenGL returns.
	switch (device->PhysicalDevice.Properties.vendorID)
	{
	case 0x1002: vendorstring = "ATI Technologies Inc.";     break;
	case 0x10DE: vendorstring = "NVIDIA Corporation";  break;
	case 0x8086: vendorstring = "Intel";   break;
	default:     vendorstring = "Unknown"; break;
	}

	hwcaps = RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
	glslversion = 4.50f;
	uniformblockalignment = (unsigned int)device->PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment;
	maxuniformblock = device->PhysicalDevice.Properties.limits.maxUniformBufferRange;

	mCommands.reset(new VkCommandBufferManager(this));
	mBGTransferCommands.reset(new VkCommandBufferManager(this, true));

	mSamplerManager.reset(new VkSamplerManager(this));
	mTextureManager.reset(new VkTextureManager(this));
	mBufferManager.reset(new VkBufferManager(this));
	mBufferManager->Init();

	mScreenBuffers.reset(new VkRenderBuffers(this));
	mSaveBuffers.reset(new VkRenderBuffers(this));
	mActiveRenderBuffers = mScreenBuffers.get();

	mPostprocess.reset(new VkPostprocess(this));
	mDescriptorSetManager.reset(new VkDescriptorSetManager(this));
	mRenderPassManager.reset(new VkRenderPassManager(this));
	mRaytrace.reset(new VkRaytrace(this));

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new HWViewpointBuffer;
	mLights = new FLightBuffer();

	mShaderManager.reset(new VkShaderManager(this));
	mDescriptorSetManager->Init();
#ifdef __APPLE__
	mRenderState.reset(new VkRenderStateMolten(this));
#else
	mRenderState.reset(new VkRenderState(this));
#endif

	// @Cockatrice - Init the background loader
	bgTransferThread.reset(new VkTexLoadThread(mBGTransferCommands.get(), device));
	bgTransferThread->start();
}

void VulkanFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	Flush3D.Clock();

	GetPostprocess()->SetActiveRenderTarget();

	Draw2D();
	twod->Clear();

	mRenderState->EndRenderPass();
	mRenderState->EndFrame();

	Flush3D.Unclock();

	mCommands->WaitForCommands(true);
	mCommands->UpdateGpuStats();

	Super::Update();
}

bool VulkanFrameBuffer::CompileNextShader()
{
	return mShaderManager->CompileNextShader();
}

void VulkanFrameBuffer::RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc)
{
	auto BaseLayer = static_cast<VkHardwareTexture*>(tex->GetHardwareTexture(0, 0));

	VkTextureImage *image = BaseLayer->GetImage(tex, 0, 0);
	VkTextureImage *depthStencil = BaseLayer->GetDepthStencil(tex);

	mRenderState->EndRenderPass();

	VkImageTransition()
		.AddImage(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true)
		.Execute(mCommands->GetDrawCommands());

	mRenderState->SetRenderTarget(image, depthStencil->View.get(), image->Image->width, image->Image->height, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT);

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = min(tex->GetWidth(), image->Image->width);
	bounds.height = min(tex->GetHeight(), image->Image->height);

	renderFunc(bounds);

	mRenderState->EndRenderPass();

	VkImageTransition()
		.AddImage(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false)
		.Execute(mCommands->GetDrawCommands());

	mRenderState->SetRenderTarget(&GetBuffers()->SceneColor, GetBuffers()->SceneDepthStencil.View.get(), GetBuffers()->GetWidth(), GetBuffers()->GetHeight(), VK_FORMAT_R16G16B16A16_SFLOAT, GetBuffers()->GetSceneSamples());

	tex->SetUpdated(true);
}

void VulkanFrameBuffer::PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D)
{
	if (!swscene) mPostprocess->BlitSceneToPostprocess(); // Copy the resulting scene to the current post process texture
	mPostprocess->PostProcessScene(fixedcm, flash, afterBloomDrawEndScene2D);
}

const char* VulkanFrameBuffer::DeviceName() const
{
	const auto &props = device->PhysicalDevice.Properties;
	return props.deviceName;
}

void VulkanFrameBuffer::SetVSync(bool vsync)
{
	Printf("Vsync changed to: %d", vsync);
	mVSync = vsync;
}

void VulkanFrameBuffer::PrecacheMaterial(FMaterial *mat, int translation)
{
	if (mat->Source()->GetUseType() == ETextureType::SWCanvas) return;

	MaterialLayerInfo* layer;

	auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(0, translation, &layer));
	systex->GetImage(layer->layerTexture, translation, layer->scaleFlags);

	int numLayers = mat->NumLayers();
	for (int i = 1; i < numLayers; i++)
	{
		auto syslayer = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		syslayer->GetImage(layer->layerTexture, 0, layer->scaleFlags);
	}
}

void VulkanFrameBuffer::PrequeueMaterial(FMaterial *mat, int translation) 
{
	BackgroundCacheMaterial(mat, translation, true, true);	
}


// @Cockatrice - Cache a texture material, intended for use outside of the main thread
bool VulkanFrameBuffer::BackgroundCacheTextureMaterial(FGameTexture *tex, int translation, int scaleFlags, bool makeSPI) {
	if (!tex || !tex->isValid() || tex->GetID().GetIndex() == 0) return false;

	QueuedPatch qp = {
		tex, translation, scaleFlags, makeSPI
	};

	patchQueue.queue(qp);

	return true;
}


// @Cockatrice - Submit each texture in the material to the background loader
// Call from main thread only
bool VulkanFrameBuffer::BackgroundCacheMaterial(FMaterial *mat, int translation, bool makeSPI, bool secondary) {
	if (mat->Source()->GetUseType() == ETextureType::SWCanvas) return false;

	MaterialLayerInfo* layer;

	auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(0, translation, &layer));
	auto remap = translation <= 0 || IsLuminosityTranslation(translation) ? nullptr : GPalette.TranslationToTable(translation);
	if (remap && remap->Inactive) remap = nullptr;

	// Submit each layer to the background loader
	int lump = layer->layerTexture->GetSourceLump();
	FResourceLump *rLump = lump >= 0 ? fileSystem.GetFileAt(lump) : nullptr;
	FImageLoadParams *params;
	VkTexLoadSpi spi;

	// If the texture is already submitted to the cache, find it and move it to the normal queue to reprioritize it
	if (rLump && systex->GetState() == IHardwareTexture::HardwareState::CACHING) {
		bgTransferThread->moveToMainQueue(systex);
	} else if (rLump && systex->GetState() == IHardwareTexture::HardwareState::NONE) {
		systex->SetHardwareState(secondary ? IHardwareTexture::HardwareState::CACHING : IHardwareTexture::HardwareState::LOADING);

		FImageTexture *fLayerTexture = dynamic_cast<FImageTexture*>(layer->layerTexture);
		params = layer->layerTexture->GetImage()->NewLoaderParams(
			fLayerTexture ? (fLayerTexture->GetNoRemap0() ? FImageSource::noremap0 : FImageSource::normal) : FImageSource::normal,
			translation,
			remap
		);

		if(params) {
			if (makeSPI) {
				// Only generate SPI if it's not already there
				if (!mat->sourcetex->HasSpritePositioning()) {
					spi.generateSpi = true;
					spi.notrimming = mat->sourcetex->GetNoTrimming();
					spi.shouldExpand = mat->sourcetex->ShouldExpandSprite();
					// TODO: Be careful with this, if a texture load fails and returns to the queue over and over, it will eat up all available memory in the arena
					//spi.info = (SpritePositioningInfo*)ImageArena.Alloc(2 * sizeof(SpritePositioningInfo));
				}
				else {
					//Printf("%s already has SPI so skipping it!\n", mat->sourcetex->GetName(), mat->Source());
					spi.generateSpi = false;
					//spi.info = nullptr;
				}
			}
			else {
				spi.generateSpi = false;
				//spi.info = nullptr;
			}

			VkTexLoadIn in = {
				layer->layerTexture->GetImage(),
				params,
				spi,
				systex,
				mat->sourcetex
			};

			if(secondary) bgTransferThread->queueSecondary(in);
			else bgTransferThread->queue(in);
		}
		else {
			systex->SetHardwareState(IHardwareTexture::HardwareState::READY); // TODO: Set state to a special "unloadable" state
			return false;
		}
	}
	

	const int numLayers = mat->NumLayers();
	for (int i = 1; i < numLayers; i++)
	{
		auto syslayer = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		lump = layer->layerTexture->GetSourceLump();
		rLump = lump >= 0 ? fileSystem.GetFileAt(lump) : nullptr;

		if (rLump && syslayer->GetState() == IHardwareTexture::HardwareState::CACHING) {
			bgTransferThread->moveToMainQueue(syslayer);
		} else if (rLump && syslayer->GetState() == IHardwareTexture::HardwareState::NONE) {
			syslayer->SetHardwareState(secondary ? IHardwareTexture::HardwareState::CACHING : IHardwareTexture::HardwareState::LOADING);
			
			FImageTexture *fLayerTexture = dynamic_cast<FImageTexture*>(layer->layerTexture);
			params = layer->layerTexture->GetImage()->NewLoaderParams(
				fLayerTexture ? (fLayerTexture->GetNoRemap0() ? FImageSource::noremap0 : FImageSource::normal) : FImageSource::normal,
				0, // translation
				nullptr// remap
			);
			
			if (params) {
				VkTexLoadIn in = {
					layer->layerTexture->GetImage(),
					params,
					{
						false, false, false//, nullptr
					},
					syslayer,
					nullptr
				};

				if(secondary) bgTransferThread->queueSecondary(in);
				else bgTransferThread->queue(in);
			}
			else {
				syslayer->SetHardwareState(IHardwareTexture::HardwareState::READY); // TODO: Set state to a special "unloadable" state
			}
		}
	}

	return true;
}

IHardwareTexture *VulkanFrameBuffer::CreateHardwareTexture(int numchannels)
{
	return new VkHardwareTexture(this, numchannels);
}

FMaterial* VulkanFrameBuffer::CreateMaterial(FGameTexture* tex, int scaleflags)
{
	return new VkMaterial(this, tex, scaleflags);
}

IVertexBuffer *VulkanFrameBuffer::CreateVertexBuffer()
{
	return GetBufferManager()->CreateVertexBuffer();
}

IIndexBuffer *VulkanFrameBuffer::CreateIndexBuffer()
{
	return GetBufferManager()->CreateIndexBuffer();
}

IDataBuffer *VulkanFrameBuffer::CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize)
{
	return GetBufferManager()->CreateDataBuffer(bindingpoint, ssbo, needsresize);
}

void VulkanFrameBuffer::SetTextureFilterMode()
{
	if (mSamplerManager)
	{
		mDescriptorSetManager->ResetHWTextureSets();
		mSamplerManager->ResetHWSamplers();
	}
}

void VulkanFrameBuffer::StartPrecaching()
{
	// Destroy the texture descriptors to avoid problems with potentially stale textures.
	mDescriptorSetManager->ResetHWTextureSets();
}

void VulkanFrameBuffer::BlurScene(float amount, bool force)
{
	if (mPostprocess)
		mPostprocess->BlurScene(amount, force);
}

void VulkanFrameBuffer::UpdatePalette()
{
	if (mPostprocess)
		mPostprocess->ClearTonemapPalette();
}

FTexture *VulkanFrameBuffer::WipeStartScreen()
{
	SetViewportRects(nullptr);

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<VkHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeStartScreen");

	return tex;
}

FTexture *VulkanFrameBuffer::WipeEndScreen()
{
	GetPostprocess()->SetActiveRenderTarget();
	Draw2D();
	twod->Clear();

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<VkHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeEndScreen");

	return tex;
}

void VulkanFrameBuffer::CopyScreenToBuffer(int w, int h, uint8_t *data)
{
	VkTextureImage image;

	// Convert from rgba16f to rgba8 using the GPU:
	image.Image = ImageBuilder()
		.Format(VK_FORMAT_R8G8B8A8_UNORM)
		.Usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.Size(w, h)
		.DebugName("CopyScreenToBuffer")
		.Create(device);

	GetPostprocess()->BlitCurrentToImage(&image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// Staging buffer for download
	auto staging = BufferBuilder()
		.Size(w * h * 4)
		.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU)
		.DebugName("CopyScreenToBuffer")
		.Create(device);

	// Copy from image to buffer
	VkBufferImageCopy region = {};
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	mCommands->GetDrawCommands()->copyImageToBuffer(image.Image->image, image.Layout, staging->buffer, 1, &region);

	// Submit command buffers and wait for device to finish the work
	mCommands->WaitForCommands(false);

	// Map and convert from rgba8 to rgb8
	uint8_t *dest = (uint8_t*)data;
	uint8_t *pixels = (uint8_t*)staging->Map(0, w * h * 4);
	int dindex = 0;
	for (int y = 0; y < h; y++)
	{
		int sindex = (h - y - 1) * w * 4;
		for (int x = 0; x < w; x++)
		{
			dest[dindex] = pixels[sindex];
			dest[dindex + 1] = pixels[sindex + 1];
			dest[dindex + 2] = pixels[sindex + 2];
			dindex += 3;
			sindex += 4;
		}
	}
	staging->Unmap();
}

void VulkanFrameBuffer::SetActiveRenderTarget()
{
	mPostprocess->SetActiveRenderTarget();
}

TArray<uint8_t> VulkanFrameBuffer::GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma)
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	IntRect box;
	box.left = 0;
	box.top = 0;
	box.width = w;
	box.height = h;
	mPostprocess->DrawPresentTexture(box, true, true);

	TArray<uint8_t> ScreenshotBuffer(w * h * 3, true);
	CopyScreenToBuffer(w, h, ScreenshotBuffer.Data());

	pitch = w * 3;
	color_type = SS_RGB;
	gamma = 1.0f;
	return ScreenshotBuffer;
}

void VulkanFrameBuffer::BeginFrame()
{
	SetViewportRects(nullptr);
	
	UpdateBackgroundCache();

	mCommands->BeginFrame();
	mTextureManager->BeginFrame();
	mScreenBuffers->BeginFrame(screen->mScreenViewport.width, screen->mScreenViewport.height, screen->mSceneViewport.width, screen->mSceneViewport.height);
	mSaveBuffers->BeginFrame(SAVEPICWIDTH, SAVEPICHEIGHT, SAVEPICWIDTH, SAVEPICHEIGHT);
	mRenderState->BeginFrame();
	mDescriptorSetManager->BeginFrame();
}

void VulkanFrameBuffer::InitLightmap(int LMTextureSize, int LMTextureCount, TArray<uint16_t>& LMTextureData)
{
	if (LMTextureData.Size() > 0)
	{
		GetTextureManager()->SetLightmap(LMTextureSize, LMTextureCount, LMTextureData);
		LMTextureData.Reset(); // We no longer need this, release the memory
	}
}

void VulkanFrameBuffer::Draw2D()
{
	::Draw2D(twod, *mRenderState);
}

void VulkanFrameBuffer::WaitForCommands(bool finish)
{
	mCommands->WaitForCommands(finish);
}

unsigned int VulkanFrameBuffer::GetLightBufferBlockSize() const
{
	return mLights->GetBlockSize();
}

void VulkanFrameBuffer::PrintStartupLog()
{
	const auto &props = device->PhysicalDevice.Properties;

	FString deviceType;
	switch (props.deviceType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = "other"; break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "integrated gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "discrete gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "virtual gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "cpu"; break;
	default: deviceType.Format("%d", (int)props.deviceType); break;
	}

	FString apiVersion, driverVersion;
	apiVersion.Format("%d.%d.%d", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
	driverVersion.Format("%d.%d.%d", VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));

	Printf("Vulkan device: " TEXTCOLOR_ORANGE "%s\n", props.deviceName);
	Printf("Vulkan device type: %s\n", deviceType.GetChars());
	Printf("Vulkan version: %s (api) %s (driver)\n", apiVersion.GetChars(), driverVersion.GetChars());

	Printf(PRINT_LOG, "Vulkan extensions:");
	for (const VkExtensionProperties &p : device->PhysicalDevice.Extensions)
	{
		Printf(PRINT_LOG, " %s", p.extensionName);
	}
	Printf(PRINT_LOG, "\n");

	const auto &limits = props.limits;
	Printf("Max. texture size: %d\n", limits.maxImageDimension2D);
	Printf("Max. uniform buffer range: %d\n", limits.maxUniformBufferRange);
	Printf("Min. uniform buffer offset alignment: %" PRIu64 "\n", limits.minUniformBufferOffsetAlignment);
	Printf("Graphics Queue Family: #%d\nPresent Queue Family:  #%d\nUpload Queue Family:   #%d\nUpload Queue Supports Graphics: %s\n", device->graphicsFamily, device->presentFamily, device->uploadFamily, device->uploadFamilySupportsGraphics ? "Yes" : "No");
}

void VulkanFrameBuffer::SetLevelMesh(hwrenderer::LevelMesh* mesh)
{
	mRaytrace->SetLevelMesh(mesh);
}

void VulkanFrameBuffer::UpdateShadowMap()
{
	mPostprocess->UpdateShadowMap();
}

void VulkanFrameBuffer::SetSaveBuffers(bool yes)
{
	if (yes) mActiveRenderBuffers = mSaveBuffers.get();
	else mActiveRenderBuffers = mScreenBuffers.get();
}

void VulkanFrameBuffer::ImageTransitionScene(bool unknown)
{
	mPostprocess->ImageTransitionScene(unknown);
}

FRenderState* VulkanFrameBuffer::RenderState()
{
	return mRenderState.get();
}

void VulkanFrameBuffer::AmbientOccludeScene(float m5)
{
	mPostprocess->AmbientOccludeScene(m5);
}

void VulkanFrameBuffer::SetSceneRenderTarget(bool useSSAO)
{
	mRenderState->SetRenderTarget(&GetBuffers()->SceneColor, GetBuffers()->SceneDepthStencil.View.get(), GetBuffers()->GetWidth(), GetBuffers()->GetHeight(), VK_FORMAT_R16G16B16A16_SFLOAT, GetBuffers()->GetSceneSamples());
}

bool VulkanFrameBuffer::RaytracingEnabled()
{
	return vk_raytrace && device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
}
