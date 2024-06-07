#pragma once

#include "vulkaninstance.h"

class VulkanSurface;

class VulkanCompatibleDevice
{
public:
	VulkanPhysicalDevice* Device = nullptr;

	int GraphicsFamily = -1;
	int PresentFamily = -1;
	int uploadFamily = -1;

	bool GraphicsTimeQueries = false;
	bool uploadFamilySupportsGraphics = false;

	std::set<std::string> EnabledDeviceExtensions;
	VulkanDeviceFeatures EnabledFeatures;
};