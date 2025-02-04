#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <span>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <SDL3_image/SDL_image.h>

#include "kernel.cuh"
#include "SDL_Exception.hpp"



static std::string BasePath{};
static void InitializeAssetLoader(){
   BasePath = std::string{SDL_GetBasePath()};
}

class Window{
public:
   SDL_Window *m_window;

   Window(const std::string title, int width = 800, int height = 600, SDL_WindowFlags Flags = (SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE)){
      m_window = SDL_CreateWindow(title.c_str(), width, height, Flags);
      if(!m_window){
         throw SDL_Exception("Failed to create window");
      }
   }
   ~Window(){
      SDL_DestroyWindow(m_window);
   }

   void show(){
      SDL_ShowWindow(m_window);
   }
};

// Handle Input
void handleInput(bool &isRunning) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
         switch (event.type)
         {
            case SDL_EVENT_QUIT:
               isRunning = false;
               break;
            case SDL_EVENT_KEY_DOWN:
               if(event.key.key == SDLK_ESCAPE){
                  isRunning = false;
                  break;
               }
            default:
               break;
         }
      }
}

SDL_Surface* LoadImage(const char* imageFilename, int desiredChannels)
{
	const std::string fullPath = std::format("{}/Content/Images/{}", BasePath, imageFilename);
	SDL_PixelFormat format;
	SDL_Surface *result = IMG_Load(fullPath.c_str());
   
   if(!result){
      throw SDL_Exception("Failed to load image!");
   }      

	if (desiredChannels == 4)
	{
		format = SDL_PIXELFORMAT_ABGR8888;
	}
	else
	{
		SDL_DestroySurface(result);
		throw std::runtime_error("Unsupported number of channels!");
	}
	if (result->format != format)
	{
		SDL_Surface *next = SDL_ConvertSurface(result, format);
		SDL_DestroySurface(result);
		result = next;
	}

	return result;
}

SDL_GPUShader* LoadShader(
	SDL_GPUDevice* device,
	const std::string &shaderFilename,
	const Uint32 samplerCount,
	const Uint32 uniformBufferCount,
	const Uint32 storageBufferCount,
	const Uint32 storageTextureCount
) {
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;
	if (shaderFilename.contains(".vert")){
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if (shaderFilename.contains(".frag")){
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else{
      throw std::runtime_error{"Unrecognized shader stage!"};
   }

	std::string fullPath;
	const SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char *entrypoint;

	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
      fullPath = BasePath + "Content/Shaders/Compiled/SPIRV/" + shaderFilename + ".spv";
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
		fullPath = BasePath + "Content/Shaders/Compiled/MSL/" + shaderFilename + ".msl";
      format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
		fullPath = BasePath + "Content/Shaders/Compiled/DXIL/" + shaderFilename + ".dxil";
      format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	} else {
		throw std::runtime_error{"No supported shader formats found!"};
	}

   std::ifstream file{fullPath, std::ios::binary};
   if(!file){
      throw std::runtime_error{"Failed to open shader!"};
   }
   std::vector<Uint8> code{std::istreambuf_iterator(file), {}};

   SDL_GPUShaderCreateInfo shaderInfo;
   shaderInfo.code = code.data();
   shaderInfo.code_size = code.size();
   shaderInfo.entrypoint = entrypoint;
   shaderInfo.format = format;
   shaderInfo.stage = stage;
   shaderInfo.num_samplers = samplerCount;
   shaderInfo.num_uniform_buffers = uniformBufferCount;
   shaderInfo.num_storage_buffers = storageBufferCount;
   shaderInfo.num_storage_textures = storageTextureCount;

	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
	if (!shader)
	{
		throw SDL_Exception("Failed to create shader!");
		return nullptr;
	}
	return shader;
}

struct Vertex{
   glm::vec3 position;
   glm::vec2 uv;
};

int main() {

   // Init SDL Video
   if(!SDL_InitSubSystem(SDL_INIT_VIDEO)){
      throw SDL_Exception("Failed to initialize SDL Video Subsystem!");
   }

   InitializeAssetLoader();

   // Create Window
   Window Window("CUDA");

   // Create GPU Device
   SDL_GPUDevice *Device{SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, nullptr)};
   if(!Device){
      throw SDL_Exception("Failed to create GPU Device!");
   }

   // Claim Window for GPU Device
   if(!SDL_ClaimWindowForGPUDevice(Device, Window.m_window)){
      throw SDL_Exception("Failed to claim window for GPU Device!");
   }

   SDL_GPUShader *vertexShader{LoadShader(Device, "TexturedQuad.vert", 0, 0, 0, 0)};
   if(!vertexShader){
      throw SDL_Exception("Failed to load vertex shader!");
   }

   SDL_GPUShader *fragmentShader{LoadShader(Device, "TexturedQuad.frag", 1, 0, 0, 0)};
   if(!fragmentShader){
      throw SDL_Exception("Failed to load fragment shader!");
   }

   SDL_Surface *image{LoadImage("a_star.png", 4)};



   SDL_GPUColorTargetDescription colorTargetDescription{};
   colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(Device, Window.m_window);
   std::vector colorTargetsDescriptions{colorTargetDescription};

   SDL_GPUGraphicsPipelineTargetInfo pipelineTargetInfo{};
   pipelineTargetInfo.color_target_descriptions = colorTargetsDescriptions.data();
   pipelineTargetInfo.num_color_targets = colorTargetsDescriptions.size();

   std::vector<SDL_GPUVertexAttribute> vertexAttributes{};
   std::vector<SDL_GPUVertexBufferDescription> vertexBufferDescriptions{};

   vertexAttributes.emplace_back(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, position));
   vertexAttributes.emplace_back(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv));

   vertexBufferDescriptions.emplace_back(0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0);

   SDL_GPUVertexInputState vertexInputState{};
   vertexInputState.vertex_attributes = vertexAttributes.data();
   vertexInputState.num_vertex_attributes = vertexAttributes.size();
   vertexInputState.vertex_buffer_descriptions = vertexBufferDescriptions.data();
   vertexInputState.num_vertex_buffers = vertexBufferDescriptions.size();



   SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{};
   pipelineCreateInfo.vertex_shader = vertexShader;
   pipelineCreateInfo.fragment_shader = fragmentShader;
   pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
   pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
   pipelineCreateInfo.target_info = pipelineTargetInfo;
   pipelineCreateInfo.vertex_input_state = vertexInputState;

   SDL_GPUGraphicsPipeline *graphicsPipeline{SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo)};
   if(!graphicsPipeline){
      throw SDL_Exception("Couldn't create GPU Graphics Pipeline");
   }

   SDL_ReleaseGPUShader(Device, vertexShader);
   SDL_ReleaseGPUShader(Device, fragmentShader);
   
   SDL_GPUSamplerCreateInfo samplerCreateInfo{};
   samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
   samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
   samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
   samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
   samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
   samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

	auto sampler = SDL_CreateGPUSampler(Device, &samplerCreateInfo);

   SDL_GPUTextureCreateInfo textureCreateInfo{};
   textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
   textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
   textureCreateInfo.width = image->w;
   textureCreateInfo.height = image->h;
   textureCreateInfo.layer_count_or_depth = 1;
   textureCreateInfo.num_levels = 1;
   textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

   auto *texture{SDL_CreateGPUTexture(Device, &textureCreateInfo)};
   if(!texture){
      throw SDL_Exception("Failed to create texture!");
   }
   SDL_SetGPUTextureName(Device, texture, "a_star.png");

   // rectangle
   std::vector<Vertex> vertices{
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
      {{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}}
   };

   std::vector<Uint32> indices{
      0, 1, 2,
      0, 2, 3
   };

   SDL_GPUBufferCreateInfo vertexBufferCreateInfo{};
   vertexBufferCreateInfo.size = vertices.size() * sizeof(Vertex);
   vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
   auto *vertexBuffer{SDL_CreateGPUBuffer(Device, &vertexBufferCreateInfo)};
   if(!vertexBuffer){
      throw SDL_Exception("Failed to create vertex buffer!");
   }
   SDL_SetGPUBufferName(Device, vertexBuffer, "Vertex Buffer");

   SDL_GPUBufferCreateInfo indexBufferCreateInfo{};
   indexBufferCreateInfo.size = indices.size() * sizeof(Uint32);
   indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
   auto *indexBuffer{SDL_CreateGPUBuffer(Device, &indexBufferCreateInfo)};
   if(!indexBuffer){
      throw SDL_Exception("Failed to create index buffer!");
   }
   SDL_SetGPUBufferName(Device, indexBuffer, "Index Buffer");

   SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{};
   transferBufferCreateInfo.size = vertexBufferCreateInfo.size + indexBufferCreateInfo.size;
   transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
   auto *transferBuffer{SDL_CreateGPUTransferBuffer(Device, &transferBufferCreateInfo)};
   if(!transferBuffer){
      throw SDL_Exception("Failed to create transfer buffer!");
   }

   auto transferBufferDataPtr{static_cast<Uint8*>(SDL_MapGPUTransferBuffer(Device, transferBuffer, false))};
   if(!transferBufferDataPtr){
      throw SDL_Exception("Failed to map transfer buffer!");
   }

   // vertex buffer data span
   std::span vertexBufferData{reinterpret_cast<Vertex *>(transferBufferDataPtr), vertices.size()};
   std::ranges::copy(vertices, vertexBufferData.begin());

   // index buffer data span
   std::span indexBufferData{reinterpret_cast<Uint32 *>(transferBufferDataPtr + vertexBufferCreateInfo.size), indices.size()};
   std::ranges::copy(indices, indexBufferData.begin());

   SDL_UnmapGPUTransferBuffer(Device, transferBuffer);

   SDL_GPUTransferBufferCreateInfo textureTransferBufferCreateInfo{};
   textureTransferBufferCreateInfo.size = image->pitch * image->h;
   textureTransferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
   auto *textureTransferBuffer{SDL_CreateGPUTransferBuffer(Device, &textureTransferBufferCreateInfo)};
   if(!textureTransferBuffer){
      throw SDL_Exception("Failed to create texture transfer buffer!");
   }

   auto textureTransferBufferDataPtr{static_cast<Uint8*>(SDL_MapGPUTransferBuffer(Device, textureTransferBuffer, false))};
   if(!textureTransferBufferDataPtr){
      throw SDL_Exception("Failed to map texture transfer buffer!");
   }

   std::memcpy(textureTransferBufferDataPtr, image->pixels, textureTransferBufferCreateInfo.size);
   
   SDL_UnmapGPUTransferBuffer(Device, textureTransferBuffer);

   SDL_GPUCommandBuffer *transferCommandBuffer{SDL_AcquireGPUCommandBuffer(Device)};
   if(!transferCommandBuffer){
      throw SDL_Exception("Failed to acquire GPU Command Buffer!");
   }

   SDL_GPUCopyPass *copyPass{SDL_BeginGPUCopyPass(transferCommandBuffer)};
   
   SDL_GPUTransferBufferLocation source{};
   source.transfer_buffer = transferBuffer;
   source.offset = 0;
   SDL_GPUBufferRegion destination{};
   destination.buffer = vertexBuffer;
   destination.offset = 0;
   destination.size = vertexBufferCreateInfo.size;
   SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

   source.offset = vertexBufferCreateInfo.size;
   destination.buffer = indexBuffer;
   destination.offset = 0;
   destination.size = indexBufferCreateInfo.size;
   SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

   SDL_GPUTextureTransferInfo textureTransferInfo{};
   textureTransferInfo.transfer_buffer = textureTransferBuffer;
   textureTransferInfo.offset = 0;
   
   SDL_GPUTextureRegion textureRegion{};
   textureRegion.texture = texture;
   textureRegion.w = image->w;
   textureRegion.h = image->h;
   textureRegion.d = 1;
   
   SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);

   SDL_EndGPUCopyPass(copyPass);
   if(!SDL_SubmitGPUCommandBuffer(transferCommandBuffer)){
      throw SDL_Exception("Failed to submit GPU Command Buffer!");
   }
   SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);

   // Show Window
   Window.show();

   // Main Loop
   bool isRunning = true;
   while (isRunning) {
      handleInput(isRunning);

      SDL_GPUCommandBuffer *commandBuffer{SDL_AcquireGPUCommandBuffer(Device)};
      if(!commandBuffer){
         throw SDL_Exception("Failed to acquire GPU Command Buffer!");
      }

      SDL_GPUTexture *swapchainTexture;
      SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, Window.m_window, &swapchainTexture, nullptr, nullptr);
      if(swapchainTexture){
         SDL_GPUColorTargetInfo colorTarget{};
         colorTarget.texture = swapchainTexture;
         colorTarget.store_op = SDL_GPU_STOREOP_STORE;
         colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
         colorTarget.clear_color = SDL_FColor{0.1f, 0.1f, 0.1f, 1.0f};
         std::vector colorTargets{colorTarget};
         SDL_GPURenderPass *renderPass{SDL_BeginGPURenderPass(commandBuffer, colorTargets.data(), colorTargets.size(), nullptr)};

         SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);
         std::vector<SDL_GPUBufferBinding> bindings{{vertexBuffer, 0}};
         SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), bindings.size());
         SDL_GPUBufferBinding indexBufferBinding{indexBuffer, 0};
         SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
         SDL_GPUTextureSamplerBinding textureSamplerBinding{};
         textureSamplerBinding.texture = texture;
         textureSamplerBinding.sampler = sampler;
         std::vector textureSamplerBindings{textureSamplerBinding};
         SDL_BindGPUFragmentSamplers(renderPass, 0, textureSamplerBindings.data(), textureSamplerBindings.size());
         SDL_DrawGPUIndexedPrimitives(renderPass, indices.size(), 1, 0, 0, 0);
         SDL_EndGPURenderPass(renderPass);
      }

      if(!SDL_SubmitGPUCommandBuffer(commandBuffer)){
         throw SDL_Exception("Failed to submit GPU Command Buffer!");
      }
   }


   // Quit SDL
   SDL_Quit();

   return EXIT_SUCCESS;
}