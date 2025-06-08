# Particles

Just some GPU accelerated particles to test out [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross)

### Building

#### Windows

Install the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) for glslc

```bash
git clone https://github.com/jsoulier/particles --recurse-submodules
cd particles
mkdir build
cd build
cmake ..
cmake --build . --parallel 8 --config Release
cd bin
./particles.exe
```

#### Linux

```bash
sudo apt install glslc
```

```bash
git clone https://github.com/jsoulier/particles --recurse-submodules
cd particles
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 8
cd bin
./particles
```

#### MacOS

I have no idea. But it should work!