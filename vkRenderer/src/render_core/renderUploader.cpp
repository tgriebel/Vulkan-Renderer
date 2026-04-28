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

	geometry.vb.Create(
		"VB",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		MaxVertices,
		sizeof( vsInput_t ),
		bufferType_t::VERTEX,
		renderContext->localMemory
	);

	geometry.ib.Create(
		"IB",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		MaxIndices,
		sizeof( uint32_t ),
		bufferType_t::INDEX,
		renderContext->localMemory
	);

	geometry.stagingBuffer.Create(
		"Geo Staging",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::REBOOT,
		1,
		MaxGeometryUploadMemory,
		bufferType_t::STAGING,
		renderContext->sharedMemory
	);
}


void RenderUploader::Shutdown()
{
	imageFreeSlot = 0;
	geometry.vbBufElements = 0;
	geometry.ibBufElements = 0;
}


void RenderUploader::QueueModelUpload( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();
	if( model.uploadId != -1 ) {
		return;
	}
	model.uploadId = geometry.surfUploads.Count();
	geometry.surfUploads.Grow( model.surfCount );
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