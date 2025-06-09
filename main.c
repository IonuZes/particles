#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static SDL_GPUBuffer* buffer;

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

    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        SDL_GPUSwapchainComposition composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        SDL_SetGPUSwapchainParameters(device, window, composition, SDL_GPU_PRESENTMODE_MAILBOX);
    }

    SDL_GPUShader* render_frag_shader = load_shader(device, "render.frag");
    SDL_GPUShader* render_vert_shader = load_shader(device, "render.vert");
    update_pipeline = load_compute_pipeline(device, "update.comp");
    if (!render_frag_shader || !render_vert_shader || !update_pipeline)
    {
        SDL_Log("Failed to load shader(s)");
        return;
    }

    {
        SDL_GPUGraphicsPipelineCreateInfo info =
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
        };
        render_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }

    if (!render_pipeline)
    {
        SDL_Log("Failed to create render pipeline: %s", SDL_GetError());
        return;
    }

    SDL_ReleaseGPUShader(device, render_frag_shader);
    SDL_ReleaseGPUShader(device, render_vert_shader);

    SDL_GPUTransferBuffer* transfer_buffer;
    
    {
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = PARTICLES * sizeof(particle_t);
        transfer_buffer = SDL_CreateGPUTransferBuffer(device, &info);
    }
    {
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_VERTEX;
        info.size = PARTICLES * sizeof(particle_t);
        buffer = SDL_CreateGPUBuffer(device, &info);
    }

    if (!buffer || !transfer_buffer)
    {
        SDL_Log("Failed to create buffer(s): %s", SDL_GetError());
        return;
    }

    particle_t* particles = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (!particles)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }

    srand(time(NULL));

    int w;
    int h;
    SDL_GetWindowSize(window, &w, &h);

    float radius = SDL_sqrtf(w * w + h * h) / 4.0f;

    for (uint32_t i = 0; i < PARTICLES; i++)
    {
        float a = (float) rand() / RAND_MAX * 2.0f * SDL_PI_F;
        float r = (float) rand() / RAND_MAX * radius;

        particles[i].position.x = w / 2 + r * SDL_cosf(a);
        particles[i].position.y = h / 2 + r * SDL_sinf(a);

        particles[i].velocity.x = 0.0f;
        particles[i].velocity.y = 0.0f;
    }

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to acquire copy pass: %s", SDL_GetError());
        return;
    }

    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = transfer_buffer;

    SDL_GPUBufferRegion region = {0};
    region.buffer = buffer;
    region.size = PARTICLES * sizeof(particle_t);

    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, false);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    snprintf(title, sizeof(title), "particles [%s] [%s]", format->name, SDL_GetGPUDeviceDriver(device));
    SDL_SetWindowTitle(window, title);
}

static void quit()
{
    SDL_ReleaseGPUBuffer(device, buffer);

    buffer = NULL;

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
    SDL_WaitForGPUSwapchain(device, window);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }

    SDL_GPUTexture* swapchain_texture;
    if (!SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, NULL, NULL))
    {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return;
    }

    SDL_GPUColorTargetInfo color_info = {0};
    color_info.texture = swapchain_texture;
    color_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_info.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUStorageBufferReadWriteBinding rw_buffer_binding = {0};
    rw_buffer_binding.buffer = buffer;

    SDL_GPUBufferBinding buffer_binding = {0};
    buffer_binding.buffer = buffer;

    float x;
    float y;
    SDL_MouseButtonFlags flags = SDL_GetMouseState(&x, &y);

    float force = 0.0f;
    if (flags & SDL_BUTTON_RMASK)
    {
        force = -FORCE;
    }
    else if (flags & SDL_BUTTON_LMASK)
    {
        force = FORCE;
    }

    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);

    struct
    {
        struct
        {
            float width;
            float height;
        }
        viewport;
        struct
        {
            float x;
            float y;
        }
        cursor;
        uint32_t particles;
        float delta_time;
        float force;
        float resistance;
    }
    params =
    {
        .viewport =
        {
            .width = WIDTH,
            .height = HEIGHT,
        },
        .cursor =
        {
            .x = x / width * WIDTH,
            .y = HEIGHT - y / height * HEIGHT,
        },
        .particles = PARTICLES,
        .delta_time = dt,
        .force = force,
        .resistance = RESISTANCE,
    };

    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, NULL, 0, &rw_buffer_binding, 1);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }

    SDL_BindGPUComputePipeline(compute_pass, update_pipeline);
    SDL_PushGPUComputeUniformData(command_buffer, 0, &params, sizeof(params));
    SDL_DispatchGPUCompute(compute_pass, (PARTICLES + 64 - 1) / 64, 1, 1);
    SDL_EndGPUComputePass(compute_pass);

    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_info, 1, NULL);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }

    SDL_BindGPUGraphicsPipeline(render_pass, render_pipeline);
    SDL_PushGPUVertexUniformData(command_buffer, 0, &params.viewport, sizeof(params.viewport));
    SDL_BindGPUVertexBuffers(render_pass, 0, &buffer_binding, 1);
    SDL_DrawGPUPrimitives(render_pass, PARTICLES, 1, 0, 0);
    SDL_EndGPURenderPass(render_pass);

    SDL_SubmitGPUCommandBuffer(command_buffer);
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

    window = SDL_CreateWindow("", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
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
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_SPACE)
                {
                    quit();
                    shader_format = (shader_format + 1) % SDL_arraysize(shader_formats);
                    init();
                }
                break;
            case SDL_EVENT_QUIT:
                running = false;
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