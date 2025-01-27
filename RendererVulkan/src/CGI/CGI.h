// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_VULKAN_DESTROY(vk, fn) {if (VK_NULL_HANDLE != vk) {fn; vk = VK_NULL_HANDLE;}}

#include "Native.h"
#include "GeometryVulkan.h"
#include "TextureVulkan.h"
#include "ShaderVulkan.h"
#include "PipelineVulkan.h"
#include "CommandBufferVulkan.h"
#include "ExtRealityVulkan.h"
#include "RendererVulkan.h"
