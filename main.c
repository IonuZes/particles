#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "shader.h"

typedef struct
{
    SDL_GPUShaderFormat format;
    const char* name;
}
shader_format_t;

typedef struct
{
    struct
    {
        float x;
        float y;
    }
    position, velocity;
}
particle_t;

static SDL_Window* window;
static SDL_GPUDevice* device;

static SDL_GPUGraphicsPipeline* render_pipeline;
static SDL_GPUComputePipeline* update_pipeline;

static const shader_format_t shader_formats[3] =
{{
    .format = SDL_GPU_SHADERFORMAT_SPIRV,
    .name = "spirv",
},
{
    .format = SDL_GPU_SHADERFORMAT_DXIL,
    .name = "dxil",
},
{
    .format = SDL_GPU_SHADERFORMAT_MSL,
    .name = "msl",
}};

static int shader_format;

static void init()
{
    const shader_format_t* format = &shader_formats[shader_format];

    char title[256] = {0};

    snprintf(title, sizeof(title), "particles [%s] [unsupported]", format->name);
    SDL_SetWindowTitle(window, title);

    if (!SDL_GPUSupportsShaderFormats(format->format, NULL))
    {
        SDL_Log("Shader format is unsupported: %s", format->name);
        return;
    }

    device = SDL_CreateGPUDevice(format->format, true, NULL);
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to create swapchain: %s", SDL_GetError());
        return;
    }

    SDL_GPUShader* render_frag_shader = load_shader(device, "render.frag");
    SDL_GPUShader* render_vert_shader = load_shader(device, "render.vert");
    update_pipeline = load_compute_pipeline(device, "update.comp");
    if (!render_frag_shader || !render_vert_shader || !update_pipeline)
    {
        SDL_Log("Failed to load shader(s)");
        return;
    }

    render_pipeline = SDL_CreateGPUGraphicsPipeline(device, &(SDL_GPUGraphicsPipelineCreateInfo)
    {
        .fragment_shader = render_frag_shader,
        .vertex_shader = render_vert_shader,
        .target_info = 
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .instance_step_rate = 0,
                .pitch = sizeof(particle_t),
                .slot = 0,
            }},
            .num_vertex_attributes = 2,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .buffer_slot = 0,
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                .location = 0,
                .offset = offsetof(particle_t, position),
            },
            {
                .buffer_slot = 0,
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                .location = 1,
                .offset = offsetof(particle_t, velocity),
            }},
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST,
    });
    if (!render_pipeline)
    {
        SDL_Log("Failed to create render pipeline: %s", SDL_GetError());
        return;
    }

    SDL_ReleaseGPUShader(device, render_frag_shader);
    SDL_ReleaseGPUShader(device, render_vert_shader);

    snprintf(title, sizeof(title), "particles [%s] [%s]", format->name, SDL_GetGPUDeviceDriver(device));
    SDL_SetWindowTitle(window, title);
}

static void quit()
{
    SDL_ReleaseGPUGraphicsPipeline(device, render_pipeline);
    SDL_ReleaseGPUComputePipeline(device, update_pipeline);

    render_pipeline = NULL;
    update_pipeline = NULL;

    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);

    device = NULL;
}

static void tick(float dt)
{

}

int main(int argc, char** argv)
{
    SDL_SetAppMetadata("particles", NULL, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    init();

    uint64_t t1 = SDL_GetPerformanceCounter();
    uint64_t t2 = 0;

    bool running = true;
    while (running)
    {
        t2 = t1;
        t1 = SDL_GetPerformanceCounter();

        const float frequency = SDL_GetPerformanceFrequency();
        const float dt = (t1 - t2) / frequency;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                quit();
                shader_format = (shader_format + 1) % SDL_arraysize(shader_formats);
                init();
                break;
            }
        }

        if (!running)
        {
            break;
        }

        if (device)
        {
            tick(dt);
        }
    }

    quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}