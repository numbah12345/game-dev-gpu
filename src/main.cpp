#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <stdio.h>
#include <graphics.hpp>


//implement appstate struct
static SDL_Window* window = NULL;
static SDL_GPUDevice* gpu_device = NULL;

//graphics stuff
static SDL_GPUGraphicsPipeline* triangle_pipeline = NULL;
static SDL_GPUGraphicsPipeline* texture_pipeline = NULL;

SDL_GPUBuffer* vertex_buffer = NULL;
SDL_GPUBuffer* index_buffer = NULL;
SDL_GPUTexture* texture = NULL;
SDL_GPUSampler *samplers = NULL;
SDL_GPUCommandBuffer* upload_command_buffer = NULL;

// ImGui state
static bool show_demo_window = true;
static bool show_another_window = false;
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


// --------------
// SDL_AppInit()
// --------------
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window = SDL_CreateWindow("Gaming",
                              (int)(1280 * main_scale), (int)(720 * main_scale),
                              window_flags);
    if (!window)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
    if (!gpu_device)
    {
        printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
    {
        printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = gpu_device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);

    //first shader
    SDL_GPUShader *vertex_shader = LoadShader(gpu_device, "/assets/Shaders/RawTriangle.vert", 0,0,0,0);
    SDL_GPUShader *fragment_shader = LoadShader(gpu_device, "/assets/Shaders/SolidColor.frag", 0,0,0,0);

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL
        },
        .target_info = {
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = SDL_GetGPUSwapchainTextureFormat(gpu_device, window)
            }},
            .num_color_targets = 1
        }
    };

    
    triangle_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipelineCreateInfo);
    if (triangle_pipeline == NULL)
    {
        SDL_Log("Failed to create fill pipeline!");
        return SDL_APP_FAILURE;
    }

    //2nd shader (texture surface)
    
    
    SDL_GPUShader *vertex_shader2 = LoadShader(
        gpu_device, 
        "/assets/Shaders/TexturedQuad.vert", 
        0,0,0,0);
    SDL_GPUShader *fragment_shader2 = LoadShader(gpu_device, 
        "/assets/Shaders/TexturedQuad.frag", 
        1,0,0,0);

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo2 = {
        .vertex_shader = vertex_shader2,
        .fragment_shader = fragment_shader2,
        .vertex_input_state = (SDL_GPUVertexInputState) {
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .slot = 0,
                .pitch = sizeof(PositionTextureVertex),
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .instance_step_rate = 0
            }},
            .num_vertex_buffers = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .location = 0,
                .buffer_slot = 0,
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                .offset = 0
            }, {
                .location = 1,
                .buffer_slot = 0,
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                .offset = sizeof(float) *3
            }},
            .num_vertex_attributes = 2
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        /* .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL
        }, */
        .target_info = {
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = SDL_GetGPUSwapchainTextureFormat(gpu_device, window)
            }},
            .num_color_targets = 1
        }
    };

    
    texture_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipelineCreateInfo2);
    if (texture_pipeline == NULL)
    {
        SDL_Log("Failed to create fill pipeline!");
        return SDL_APP_FAILURE;
    }
    
    SDL_ReleaseGPUShader(gpu_device, vertex_shader);
    SDL_ReleaseGPUShader(gpu_device, fragment_shader);

    SDL_ReleaseGPUShader(gpu_device, vertex_shader2);
    SDL_ReleaseGPUShader(gpu_device, fragment_shader2);


    SDL_Surface *image_data = LoadImage("/assets/Images/lettuce.bmp", 4);
    if (image_data == NULL)
    {
        SDL_Log("Could not load image data!");
        return SDL_APP_FAILURE;
    }
    SDL_GPUSamplerCreateInfo sampler_info{
    .min_filter = SDL_GPU_FILTER_NEAREST,
    .mag_filter = SDL_GPU_FILTER_NEAREST,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    samplers = SDL_CreateGPUSampler(gpu_device, &sampler_info);

    SDL_GPUBufferCreateInfo vertex_buffer_info {
    .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
    .size = sizeof(PositionTextureVertex) * 4
    };

    vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_info);
    SDL_SetGPUBufferName(gpu_device, vertex_buffer, "Lettuce Vertex Buffer");
    

    SDL_GPUBufferCreateInfo index_buffer_info {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = sizeof(Uint16) * 6
    };

    index_buffer = SDL_CreateGPUBuffer(gpu_device, &index_buffer_info);
    
    SDL_GPUTextureCreateInfo texture_info {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = static_cast<Uint32>(image_data->w),
        .height = static_cast<Uint32>(image_data->h),
        .layer_count_or_depth = 1,
        .num_levels = 1
    };
    texture = SDL_CreateGPUTexture(gpu_device, &texture_info);
    
    SDL_SetGPUTextureName(gpu_device,texture, "Lettuce Texture");

    SDL_GPUTransferBufferCreateInfo transfer_info {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
    };

    SDL_GPUTransferBuffer* buffer_transfer_buffer = SDL_CreateGPUTransferBuffer(
        gpu_device, 
        &transfer_info
    );

    PositionTextureVertex* transfer_data = (PositionTextureVertex*)SDL_MapGPUTransferBuffer(
        gpu_device,
        buffer_transfer_buffer,
        false
    );

    transfer_data[1] = (PositionTextureVertex) {  1,  1, 0, 1, 0 };
    transfer_data[0] = (PositionTextureVertex) { -1,  1, 0, 0, 0 };
    transfer_data[2] = (PositionTextureVertex) {  1, -1, 0, 1, 1 };
    transfer_data[3] = (PositionTextureVertex) { -1, -1, 0, 0, 1 };

    Uint16* index_data = (Uint16*) &transfer_data[4];
    index_data[0] = 0;
    index_data[1] = 1;
    index_data[2] = 2;
    index_data[3] = 0;
    index_data[4] = 2;
    index_data[5] = 3;

    SDL_UnmapGPUTransferBuffer(gpu_device, buffer_transfer_buffer);


    SDL_GPUTransferBufferCreateInfo texture_buffer_info {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = static_cast<Uint32>(image_data->w * image_data->h * 4)
    };
    SDL_GPUTransferBuffer* texture_transfer_buffer = SDL_CreateGPUTransferBuffer(
        gpu_device,
        &texture_buffer_info
    );

    Uint8* p_texture_transfer = (Uint8*)SDL_MapGPUTransferBuffer(
        gpu_device,
        texture_transfer_buffer,
        false
    );
    SDL_memcpy(p_texture_transfer, image_data->pixels, image_data->w * image_data->h * 4);
    SDL_UnmapGPUTransferBuffer(gpu_device,texture_transfer_buffer);


    //UPLOAD BUFFERS TO GPU
    upload_command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);
    
    SDL_GPUTransferBufferLocation vertex_location {
        .transfer_buffer = buffer_transfer_buffer,
        .offset = 0
    };

    SDL_GPUBufferRegion vertex_region {
        .buffer = vertex_buffer,
        .offset = 0,
        .size = sizeof(PositionTextureVertex) * 4
    };

    SDL_UploadToGPUBuffer(
        copy_pass,
        &vertex_location,
        &vertex_region,
        false
    );

    SDL_GPUTransferBufferLocation index_location {
        .transfer_buffer = buffer_transfer_buffer,
        .offset = sizeof(PositionTextureVertex) * 4
    };
    SDL_GPUBufferRegion index_region {
        .buffer = index_buffer,
        .offset = 0,
        .size = sizeof(Uint16) * 6
    };
    SDL_UploadToGPUBuffer(
        copy_pass,
        &index_location,
        &index_region,
        false
    );

    SDL_GPUTextureTransferInfo texture_transfer_info {
        .transfer_buffer = texture_transfer_buffer,
        .offset = 0
    };
    SDL_GPUTextureRegion texture_region {
        .texture = texture,
        .w = static_cast<Uint32>(image_data->w),
        .h = static_cast<Uint32>(image_data->h),
        .d = 1
    };
    SDL_UploadToGPUTexture(
        copy_pass,
        &texture_transfer_info,
        &texture_region,
        false
    );

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_command_buffer);
    SDL_WaitForGPUIdle(gpu_device);
    
    SDL_DestroySurface(image_data);
    
    SDL_ReleaseGPUTransferBuffer(gpu_device, buffer_transfer_buffer);
    SDL_ReleaseGPUTransferBuffer(gpu_device, texture_transfer_buffer);

    

    return SDL_APP_CONTINUE; // success
}

// --------------
// SDL_AppEvent()
// --------------
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

    

    return SDL_APP_CONTINUE; // keep going
}

// --------------
// SDL_AppIterate()
// --------------
SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    // ImGui windows
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Checkbox("Another Window", &show_another_window);
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit4("clear color", (float*)&clear_color);
        if (ImGui::Button("Button")) counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }

    // Render
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUTexture* swapchain_texture;
    SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);

    
    //my renderpass
    if (swapchain_texture != NULL) {
        SDL_GPUColorTargetInfo color_target_info = {0};
        color_target_info.texture = swapchain_texture;
        color_target_info.clear_color = SDL_FColor{ clear_color.x, clear_color.y, clear_color.z, clear_color.w };
        color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target_info.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, NULL);
        
        
        //new code
        SDL_GPUBufferBinding vertex_binding {
            .buffer = vertex_buffer,
            .offset = 0
        };
        SDL_GPUBufferBinding index_binding {
            .buffer = index_buffer,
            .offset = 0
        };
        SDL_GPUTextureSamplerBinding texture_binding = {
            .texture = texture,
            .sampler = samplers
        };

        SDL_BindGPUGraphicsPipeline(render_pass, texture_pipeline);
        SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
        SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_binding, 1);
        SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);

        SDL_BindGPUGraphicsPipeline(render_pass, triangle_pipeline);
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
        
        SDL_EndGPURenderPass(render_pass);
    }

    //imgui gpu render pass
    if (swapchain_texture && !is_minimized)
    {
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor{ clear_color.x, clear_color.y, clear_color.z, clear_color.w };
        target_info.load_op = SDL_GPU_LOADOP_LOAD;
        target_info.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }


    SDL_SubmitGPUCommandBuffer(command_buffer);
    

    return SDL_APP_CONTINUE;
}

// --------------
// SDL_AppQuit()
// --------------
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_WaitForGPUIdle(gpu_device);

    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    SDL_free(vertex_buffer);
    SDL_free(index_buffer);
    SDL_free(texture);
    SDL_free(upload_command_buffer);
    SDL_free(window);

    SDL_ReleaseGPUGraphicsPipeline(gpu_device, triangle_pipeline); //fix?
    SDL_ReleaseGPUGraphicsPipeline(gpu_device, texture_pipeline);
    SDL_ReleaseGPUSampler(gpu_device,samplers);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
