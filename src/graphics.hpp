#pragma once

#include <SDL3/SDL.h>

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;

SDL_GPUShader* LoadShader(
    SDL_GPUDevice* device,
    const char* shaderFilename,
    Uint32 samplerCount,
    Uint32 uniformBufferCount,
    Uint32 storageBufferCount,
    Uint32 storageTextureCount
);

SDL_GPUShader* ShaderCrossLoadShader(SDL_GPUDevice* gpu_device, const char* shader_filename);

void InitializeAssetLoader();

SDL_Surface *LoadImage(const char *image_file_name, int desired_channels);
