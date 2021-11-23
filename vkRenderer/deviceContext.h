#pragma once
#include "common.h"

VkImageView CreateImageView( VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels );
VkShaderModule CreateShaderModule( const std::vector<char>& code );