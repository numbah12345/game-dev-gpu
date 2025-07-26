#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <stdio.h>
#include <graphics.hpp>
#include <glm/glm.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // for DirectX-like clip space (0 to 1)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // <-- This gives you ortho, perspective, etc.
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#include <SDL3_shadercross/SDL_shadercross.h> 

#include <entt/entt.hpp>

struct Camera {
    glm::vec3 eye = {0.0f, 0.0f, 2.0f};
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};

    glm::mat4 model = glm::mat4(1.0f); // Optional; you might want this per-object later

    float fov = glm::radians(60.0f);

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(eye, target, up);
    }

    glm::mat4 getProjectionMatrix(float aspect_ratio) const {
        return glm::perspective(fov, aspect_ratio, 0.1f, 100.0f);
    }

    glm::mat4 getMVP(float aspect_ratio) const {
        return getProjectionMatrix(aspect_ratio) * getViewMatrix() * model;
    }
};

struct AppState {
    SDL_Window* window = nullptr;
    SDL_GPUDevice* gpu_device = nullptr;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    
    int window_width = 1280;
    int window_height = 720;

    float aspect_ratio = (float)window_width / (float)window_height;

    Camera camera;

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = {0.45f, 0.55f, 0.60f, 1.0f};


};



// --------------
// SDL_AppInit()
// --------------
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState* state = new AppState();   // Allocate your app state
    *appstate = static_cast<void*>(state); // Store it in the void** provided


    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    state->window = SDL_CreateWindow("Gaming",
                              (int)(1280 * main_scale), (int)(720 * main_scale),
                              window_flags);
                              
    if (!state->window)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetWindowPosition(state->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(state->window);

    state->gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
    if (!state->gpu_device)
    {
        printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(state->gpu_device, state->window))
    {
        printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetGPUSwapchainParameters(state->gpu_device, state->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);

    
    // ImGui setup
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        ImGui_ImplSDL3_InitForSDLGPU(state->window);
        ImGui_ImplSDLGPU3_InitInfo init_info = {};
        init_info.Device = state->gpu_device;
        init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window);
        init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        ImGui_ImplSDLGPU3_Init(&init_info);
    }
    
    //GPU setup
    //Loading Shaders
    SDL_GPUShader* vertex_shader = ShaderCrossLoadShader(state->gpu_device, "assets/Shaders/vertex_shader.vert");
    if (vertex_shader == NULL) {
        SDL_Log("Shader failed to load. %s", SDL_GetError());
    }

    SDL_GPUShader* fragment_shader = ShaderCrossLoadShader(state->gpu_device, "assets/Shaders/SolidColor.frag");
    if (fragment_shader == NULL) {
        SDL_Log("Shader failed to load. %s", SDL_GetError());
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
        .target_info = {
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window)
            }},
            .num_color_targets = 1,
        }
    };
    
    state->pipeline = SDL_CreateGPUGraphicsPipeline(state->gpu_device, &pipeline_create_info);
    if (state->pipeline == NULL)
    {
        SDL_Log("Failed to create fill pipeline! %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    SDL_ReleaseGPUShader(state->gpu_device, vertex_shader);
    SDL_ReleaseGPUShader(state->gpu_device, fragment_shader);
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
    AppState* state = static_cast<AppState*>(appstate); // Recover your state

    if (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    SDL_GetWindowSize(state->window, &state->window_width, &state->window_height);



    // ImGui windows
    if (state->show_demo_window)
        ImGui::ShowDemoWindow(&state->show_demo_window);
    // Imgui stuff
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Demo Window", &state->show_demo_window);
        ImGui::Checkbox("Another Window", &state->show_another_window);
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit4("clear color", (float*)&state->clear_color);
        if (ImGui::Button("Button")) counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    if (state->show_another_window)
    {
        ImGui::Begin("Another Window", &state->show_another_window);
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            state->show_another_window = false;
        ImGui::End();
    }

    // Render
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    //Create commandbuffer and swapchain texture
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);
    if (command_buffer == NULL) {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture* swapchain_texture;
    if (!SDL_AcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, NULL, NULL)) {
        SDL_Log("AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    glm::mat4 mvp_transposed = glm::transpose(state->camera.getMVP(state->aspect_ratio));  // Required for HLSL (row-major)

    SDL_PushGPUVertexUniformData(command_buffer, 0, &mvp_transposed, sizeof(mvp_transposed));

    //my renderpass
    if (swapchain_texture != NULL) {
        SDL_GPUColorTargetInfo color_target_info = {0};
        color_target_info.texture = swapchain_texture;
        color_target_info.clear_color = {state->clear_color.x, state->clear_color.y, state->clear_color.z, state->clear_color.w};
        color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target_info.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, NULL);


        SDL_BindGPUGraphicsPipeline(render_pass, state->pipeline);

        
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(render_pass);

    }

    //imgui gpu render pass
    if (swapchain_texture && !is_minimized)
    {
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor{ state->clear_color.x, state->clear_color.y, state->clear_color.z, state->clear_color.w };
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
    AppState* state = static_cast<AppState*>(appstate);

    SDL_WaitForGPUIdle(state->gpu_device);

    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    
    SDL_ReleaseGPUGraphicsPipeline(state->gpu_device, state->pipeline);

    
    SDL_ReleaseWindowFromGPUDevice(state->gpu_device, state->window);
    SDL_DestroyGPUDevice(state->gpu_device);
    SDL_DestroyWindow(state->window);
    SDL_Quit();
}
