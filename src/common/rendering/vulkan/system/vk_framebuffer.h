#pragma once

#include "gl_sysfb.h"
#include "vk_device.h"
#include "vk_objects.h"
#include "TSQueue.h"
#include "bitmap.h"
#include "printf.h"
#include "image.h"

struct FRenderViewpoint;
class VkSamplerManager;
class VkBufferManager;
class VkTextureManager;
class VkShaderManager;
class VkCommandBufferManager;
class VkDescriptorSetManager;
class VkRenderPassManager;
class VkRaytrace;
class VkRenderState;
class VkStreamBuffer;
class VkHardwareDataBuffer;
class VkHardwareTexture;
class VkRenderBuffers;
class VkPostprocess;
class SWSceneDrawer;

struct VkTexLoadSpi {
	bool generateSpi, shouldExpand, notrimming;
	SpritePositioningInfo *info;
};

struct VkTexLoadIn {
	FImageSource *imgSource;
	FImageLoadParams *params;
	VkTexLoadSpi spi;
	VkHardwareTexture *tex;		// We can create the texture on the main thread
	FGameTexture *gtex;
};

struct VkTexLoadOut {
	VkHardwareTexture *tex;
	FGameTexture *gtex;
	VkTexLoadSpi spi;
	int conversion, translation;
	bool isTranslucent, createMipmaps;
	FImageSource *imgSource;
	VulkanSemaphore *releaseSemaphore;
};

// @Cockatrice - Background loader thread to handle transfer of texture data
class VkTexLoadThread : public ResourceLoader<VkTexLoadIn, VkTexLoadOut> {
public:
	VkTexLoadThread(VkCommandBufferManager *bgCmd) { cmd = bgCmd; }
	int getCurrentImageID() { return currentImageID.load(); }

protected:
	VkCommandBufferManager *cmd;
	std::atomic<int> currentImageID;
	std::atomic<int> maxQueue;

	bool loadResource(VkTexLoadIn &input, VkTexLoadOut &output) override;
	void cancelLoad() override;
	void completeLoad() override;
};


class VulkanFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;


public:
	VulkanDevice *device;

	VkCommandBufferManager* GetCommands() { return mCommands.get(); }
	VkCommandBufferManager* GetBGCommands() { return mBGTransferCommands.get(); }
	VkShaderManager *GetShaderManager() { return mShaderManager.get(); }
	VkSamplerManager *GetSamplerManager() { return mSamplerManager.get(); }
	VkBufferManager* GetBufferManager() { return mBufferManager.get(); }
	VkTextureManager* GetTextureManager() { return mTextureManager.get(); }
	VkDescriptorSetManager* GetDescriptorSetManager() { return mDescriptorSetManager.get(); }
	VkRenderPassManager *GetRenderPassManager() { return mRenderPassManager.get(); }
	VkRaytrace* GetRaytrace() { return mRaytrace.get(); }
	VkRenderState *GetRenderState() { return mRenderState.get(); }
	VkPostprocess *GetPostprocess() { return mPostprocess.get(); }
	VkRenderBuffers *GetBuffers() { return mActiveRenderBuffers; }
	FRenderState* RenderState() override;

	unsigned int GetLightBufferBlockSize() const;

	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();
	bool IsVulkan() override { return true; }

	void Update() override;

	void InitializeState() override;
	bool CompileNextShader() override;
	void PrecacheMaterial(FMaterial *mat, int translation) override;
	bool BackgroundCacheMaterial(FMaterial *mat, int translation, bool makeSPI = false) override;
	bool BackgroundCacheTextureMaterial(FGameTexture *tex, int translation, int scaleFlags, bool makeSPI = false) override;
	bool CachingActive() override { return bgTransferThread->isActive(); }
	bool SupportsBackgroundCache() override { return true; }
	void FlushBackground() override;
	float CacheProgress() override { return float(bgTransferThread->numQueued()); }	// TODO: Change this to report the actual progress once we have a way to mark the total number of objects to load
	void UpdateBackgroundCache() override;
	void UpdatePalette() override;
	const char* DeviceName() const override;
	int Backend() override { return 1; }
	void SetTextureFilterMode() override;
	void StartPrecaching() override;
	void BeginFrame() override;
	void InitLightmap(int LMTextureSize, int LMTextureCount, TArray<uint16_t>& LMTextureData) override;
	//void BlurScene(float amount) override;
	void BlurScene(float amount, bool force = false) override;
	void PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D) override;
	void AmbientOccludeScene(float m5) override;
	void SetSceneRenderTarget(bool useSSAO) override;
	void SetLevelMesh(hwrenderer::LevelMesh* mesh) override;
	void UpdateShadowMap() override;
	void SetSaveBuffers(bool yes) override;
	void ImageTransitionScene(bool unknown) override;
	void SetActiveRenderTarget() override;

	IHardwareTexture *CreateHardwareTexture(int numchannels) override;
	FMaterial* CreateMaterial(FGameTexture* tex, int scaleflags) override;
	IVertexBuffer *CreateVertexBuffer() override;
	IIndexBuffer *CreateIndexBuffer() override;
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) override;

	FTexture *WipeStartScreen() override;
	FTexture *WipeEndScreen() override;

	TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) override;

	bool GetVSync() { return mVSync; }
	void SetVSync(bool vsync) override;

	void Draw2D() override;

	void WaitForCommands(bool finish) override;

	bool RaytracingEnabled();

	// Cache stats helpers
	void GetBGQueueSize(int &current, int &max, int &total);
	void GetBGStats(double &min, double &max, double &avg);
	void ResetBGStats();

private:
	void RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc) override;
	void PrintStartupLog();
	void CopyScreenToBuffer(int w, int h, uint8_t *data) override;

	struct QueuedPatch {
		FGameTexture *tex;
		int translation, scaleFlags;
		bool generateSPI;
	};

	TSQueue<QueuedPatch> patchQueue;									// @Cockatrice - Thread safe queue of textures to create materials for and submit to the bg thread
	std::unique_ptr<VkTexLoadThread> bgTransferThread;					// @Cockatrice - Thread that handles the background transfers

	std::unique_ptr<VkCommandBufferManager> mCommands;
	std::unique_ptr<VkCommandBufferManager> mBGTransferCommands;		// @Cockatrice - Command pool for submitting background transfers
	std::unique_ptr<VkBufferManager> mBufferManager;
	std::unique_ptr<VkSamplerManager> mSamplerManager;
	std::unique_ptr<VkTextureManager> mTextureManager;
	std::unique_ptr<VkShaderManager> mShaderManager;
	std::unique_ptr<VkRenderBuffers> mScreenBuffers;
	std::unique_ptr<VkRenderBuffers> mSaveBuffers;
	std::unique_ptr<VkPostprocess> mPostprocess;
	std::unique_ptr<VkDescriptorSetManager> mDescriptorSetManager;
	std::unique_ptr<VkRenderPassManager> mRenderPassManager;
	std::unique_ptr<VkRaytrace> mRaytrace;
	std::unique_ptr<VkRenderState> mRenderState;

	VkRenderBuffers *mActiveRenderBuffers = nullptr;

	bool mVSync = false;
};
