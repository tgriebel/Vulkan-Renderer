#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include "../scene/entity.h"

void Renderer::BeginUploadCommands( UploadContext& uploadContext )
{
	vkResetCommandBuffer( uploadContext.CommandBuffer(), 0 );

	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer( uploadContext.CommandBuffer(), &beginInfo );
}


void Renderer::EndUploadCommands( UploadContext& uploadContext )
{
	vkEndCommandBuffer( uploadContext.CommandBuffer() );
	uploadContext.Submit();
	vkQueueWaitIdle( context.gfxContext );
}