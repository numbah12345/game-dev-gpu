#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <SDL3_shadercross/SDL_shadercross.h> 

using namespace::glm;

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;


static const char* base_path = NULL;
void InitializeAssetLoader()
{
    base_path = SDL_GetBasePath();
}

SDL_GPUShader* LoadShader(
    SDL_GPUDevice* device,
    const char* shader_filename,
    Uint32 sampler_count,
    Uint32 uniform_buffer_count,
    Uint32 storage_buffer_count,
    Uint32 storage_texture_count
) {

    InitializeAssetLoader();

    SDL_GPUShaderStage stage;
    if (SDL_strstr(shader_filename, ".vert"))
    {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else if (SDL_strstr(shader_filename, ".frag"))
    {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    else
    {
        SDL_Log("Invalid shader stage!");
        return NULL;
    }

    char fullPath[256];
    SDL_GPUShaderFormat backend_formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char *entry_point;
    
    if (backend_formats & SDL_GPU_SHADERFORMAT_DXIL) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%s../%s.dxil", base_path, shader_filename);
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entry_point = "main";
    } 
     else if (backend_formats & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%s../%s.msl", base_path, shader_filename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entry_point = "main0";
    } else if (backend_formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%s../%s.spv", base_path, shader_filename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entry_point = "main";
    } else {
        SDL_Log("%s", "Unrecognized backend shader format!");
        return NULL;
    }

    size_t code_size;
    void* code = SDL_LoadFile(fullPath, &code_size);
    if (code == NULL)
    {
        SDL_Log("Failed to load shader from disk! %s", fullPath);
        return NULL;
    }


    SDL_GPUShaderCreateInfo shaderInfo = {
        .code_size = code_size,
        .code = (const Uint8 *)code,
        .entrypoint = entry_point,
        .format = format,
        .stage = stage,
        .num_samplers = sampler_count,
        .num_storage_textures = storage_texture_count,
        .num_storage_buffers = storage_buffer_count,
        .num_uniform_buffers = uniform_buffer_count
    };

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == NULL)
    {
        SDL_Log("Failed to create shader!");
        SDL_free(code);
        return NULL;
    }

    SDL_free(code);
    return shader;
}

SDL_GPUShader* ShaderCrossLoadShader(SDL_GPUDevice* gpu_device, const char* shader_filename) {
    // Initialize
    InitializeAssetLoader();
    if (!SDL_ShaderCross_Init()) {
        SDL_Log("ShaderCross failed to initialize!");
        return NULL;
    }
    
    // Determine shader stage
    SDL_ShaderCross_ShaderStage stage;
    bool is_compute = false;
    if (SDL_strstr(shader_filename, ".vert")) {
        stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    }
    else if (SDL_strstr(shader_filename, ".frag")) {
        stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    }
    else if (SDL_strstr(shader_filename, ".comp")) {
        stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;
        is_compute = true;
    }
    else {
        SDL_Log("Invalid shader stage!");
        SDL_ShaderCross_Quit();
        return NULL;
    }
    
    char full_path[256];
    size_t code_size;
    SDL_GPUShader* shader = NULL;
    SDL_snprintf(full_path, sizeof(full_path), "%s../%s.hlsl", base_path, shader_filename);
    char* code = static_cast<char*>(SDL_LoadFile(full_path, &code_size));
    if (code == NULL) {
        SDL_Log("Failed to load shader from disk! %s", full_path);
        SDL_ShaderCross_Quit();
        return NULL;
    }
    
    const char *entry_point = "main";
    SDL_ShaderCross_HLSL_Info hlsl_info = {0};
    hlsl_info.source = static_cast<const char*>(code);
    hlsl_info.entrypoint = entry_point;
    hlsl_info.include_dir = NULL;
    hlsl_info.defines = NULL;
    hlsl_info.shader_stage = stage;
    hlsl_info.enable_debug = false;
    hlsl_info.name = full_path;
    hlsl_info.props = 0;
    
    size_t bytecode_size;
    Uint8* spirv_buffer = static_cast<Uint8*>(SDL_ShaderCross_CompileSPIRVFromHLSL(
        &hlsl_info,
        &bytecode_size));
    
    if (spirv_buffer == NULL) {
        SDL_Log("Shader Compilation failed. %s", SDL_GetError());
        SDL_free(code);
        SDL_ShaderCross_Quit();
        return NULL;
    }
    
    // Properly initialize SPIRV info
    SDL_ShaderCross_SPIRV_Info spirv_info = {0};
    spirv_info.bytecode = spirv_buffer;
    spirv_info.bytecode_size = bytecode_size;
    spirv_info.entrypoint = entry_point;
    spirv_info.shader_stage = stage;
    spirv_info.enable_debug = false;
    spirv_info.name = full_path;
    spirv_info.props = 0;
    
    if (is_compute) {
        // Handle compute shaders - note this returns a compute pipeline, not a shader
        SDL_Log("Compute shaders require different handling - this function only supports graphics shaders");
        SDL_free(code);
        SDL_free(spirv_buffer);
        SDL_ShaderCross_Quit();
        return NULL;
    } else {
        // Get metadata through reflection
        SDL_ShaderCross_GraphicsShaderMetadata* metadata = 
            SDL_ShaderCross_ReflectGraphicsSPIRV(spirv_buffer, bytecode_size, 0);
        
        if (metadata == NULL) {
            SDL_Log("Failed to reflect shader metadata. %s", SDL_GetError());
            SDL_free(code);
            SDL_free(spirv_buffer);
            SDL_ShaderCross_Quit();
            return NULL;
        }
        
        // Compile the graphics shader
        shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
            gpu_device, 
            &spirv_info, 
            metadata, 
            0);  // Use 0 instead of SDL_PropertiesID {0}
        
        // Clean up metadata
        SDL_free(metadata);
    }
    
    // Clean up temporary resources
    SDL_free(code);
    SDL_free(spirv_buffer);
    
    if (shader == NULL) {
        SDL_Log("Failed to compile shader from SPIRV. %s", SDL_GetError());
    }
    
    // De-initialize Shadercross
    SDL_ShaderCross_Quit();
    return shader;
}

SDL_Surface *LoadImage(const char *image_file_name, int desired_channels)
{
    char full_path[256];
    SDL_Surface *result;
    SDL_PixelFormat format;

    SDL_snprintf(full_path, sizeof(full_path), "%s../%s", base_path, image_file_name);

    result = SDL_LoadBMP(full_path);
    if (result == NULL)
    {
        SDL_Log("Failed to load BMP: %s", SDL_GetError());
        return NULL;
    }

    if (desired_channels == 4)
    {
        format = SDL_PIXELFORMAT_ABGR8888;
    }
    else
    {
        SDL_assert(!"Unexpected desired channels");
        SDL_DestroySurface(result);
        return NULL;
    }
    if (result->format != format)
    {
        SDL_Surface *next = SDL_ConvertSurface(result, format);
        SDL_DestroySurface(result);
        result = next;
    }
    
    return result;
}


