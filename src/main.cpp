#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

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

   SDL_GPUShader *vertexShader{LoadShader(Device, "RawTriangle.vert", 0, 0, 0, 0)};
   if(!vertexShader){
      throw SDL_Exception("Failed to load vertex shader!");
   }

   SDL_GPUShader *fragmentShader{LoadShader(Device, "SolidColor.frag", 0, 0, 0, 0)};
   if(!fragmentShader){
      throw SDL_Exception("Failed to load fragment shader!");
   }

   SDL_GPUColorTargetDescription colorTargetDescription{};
   colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(Device, Window.m_window);
   std::vector colorTargetsDescriptions{colorTargetDescription};

   SDL_GPUGraphicsPipelineTargetInfo pipelineTargetInfo{};
   pipelineTargetInfo.color_target_descriptions = colorTargetsDescriptions.data();
   pipelineTargetInfo.num_color_targets = colorTargetsDescriptions.size();

   SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{};
   pipelineCreateInfo.vertex_shader = vertexShader;
   pipelineCreateInfo.fragment_shader = fragmentShader;
   pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
   pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
   pipelineCreateInfo.target_info = pipelineTargetInfo;

   SDL_GPUGraphicsPipeline *graphicsPipeline{SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo)};
   if(!graphicsPipeline){
      throw SDL_Exception("Couldn't create GPU Graphics Pipeline");
   }

   SDL_ReleaseGPUShader(Device, vertexShader);
   SDL_ReleaseGPUShader(Device, fragmentShader);

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
         colorTarget.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
         std::vector colorTargets{colorTarget};
         SDL_GPURenderPass *renderPass{SDL_BeginGPURenderPass(commandBuffer, colorTargets.data(), colorTargets.size(), nullptr)};

         SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);
         SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

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