#include "renderUploader.h"
#include "renderer.h"
#include "resourceContext.h"

void RenderUploader::Boot( RenderContext* context, ResourceContext* resources )
{
	imageFreeSlot = 0;

	renderContext = context;
	resourceContext = resources;

	textureStagingBuffer.Create(
		"Texture Staging",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		1,
		MaxTexturingUploadMemory,
		bufferType_t::STAGING,
		renderContext->sharedMemory
	);
}


void RenderUploader::Shutdown()
{
	imageFreeSlot = 0;
}


void RenderUploader::UploadImage( Asset<Image>* imageAsset )
{
	if( imageAsset->IsLoaded() == false )
	{
		return;
	}
	if( imageAsset->IsUploaded() )
	{
		return;
	}

	Image* image = &imageAsset->Get();

	if( image == nullptr )
	{
		return;
	}

	if( ( image->gpuImage == nullptr ) || ( image->gpuImage->GetId() < 0 ) )
	{
		uploadTextures.insert( imageAsset->Handle() );
	}
	if( imageAsset->IsUploaded() )
	{
		updateTextures.insert( imageAsset->Handle() );
	}
}