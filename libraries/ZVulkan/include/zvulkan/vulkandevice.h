#pragma once

#include "vulkaninstance.h"

#include <functional>
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>

class VulkanSwapChain;
class VulkanSemaphore;
class VulkanFence;
class VulkanPhysicalDevice;
class VulkanSurface;
class VulkanCompatibleDevice;

struct VulkanUploadSlot {
	VkQueue queue;
	int queueFamily, queueIndex;
	bool familySupportsGraphics;
};

class VulkanDevice
{
public:
	VulkanDevice(std::shared_ptr<VulkanInstance> instance, std::shared_ptr<VulkanSurface> surface, const VulkanCompatibleDevice& selectedDevice);
	~VulkanDevice();

	std::set<std::string> EnabledDeviceExtensions;
	VulkanDeviceFeatures EnabledFeatures;

	VulkanPhysicalDevice PhysicalDevice;

	std::shared_ptr<VulkanInstance> Instance;
	std::shared_ptr<VulkanSurface> Surface;

	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;

	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	VkQueue PresentQueue = VK_NULL_HANDLE;
	//VkQueue uploadQueue = VK_NULL_HANDLE;
	std::vector<VulkanUploadSlot> uploadQueues;

	int GraphicsFamily = -1;
	int PresentFamily = -1;
	int uploadFamily = -1;
	int uploadQueuesSupported = 1;

	bool GraphicsTimeQueries = false;
	bool uploadFamilySupportsGraphics = false;

	bool SupportsExtension(const char* ext) const;

	void SetObjectName(const char* name, uint64_t handle, VkObjectType type);

private:
	bool DebugLayerActive = false;

	void CreateDevice();
	void CreateAllocator();
	void ReleaseResources();
};
