# Project Overview: Custom Forward+ Engine
You are assisting in the development of a custom 3D rendering engine built from scratch. The core rendering philosophy is "Mixed Lighting": Real-time Direct PBR lighting combined with pre-baked 3rd-order Spherical Harmonics (SH) for indirect global illumination. 

## Tips
- Build and test the project using the following commands:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -B build -S . && cmake --build build --parallel 12 && ./build/sh_renderer_core_test && ./build/sh_renderer_test
```
- **Coding Style:** google c++ style guide. Prefer free functions over classes unless the states are complex.
- **Language:** C++20

## Technology Stack
- **Build System** CMake
- **Testing Framework:** googletest (find_package)
- **Logging:** the in-tree shim `src/log.h` (glog-style `LOG`, `LOG_EVERY_N`, `CHECK*`, `DCHECK` over a sink the host installs; the default sink writes to stderr and aborts on fatal). glog is not used.
- **Command Line Argument Parsing:** gflags (find_package), viewer only.
- **Graphics API:** OpenGL 4.6 Core Profile through the bundled glad loader. glfw3 (find_package) is the viewer's and the glTF tests' window library only; the core never calls GLFW.
- **Shading Language:** GLSL 460 core.
- **Math Library:** Eigen3 (find_package)
- **glTF Loader:** tinygltf (find_path & add_library), glTF target only.
- **JSON Library:** nlohmann/json (find_path & add_library), glTF target only.
- **Image Library:** stb_image, tinyexr (find_path & add_library), glTF target only.


## Strict Architectural Directives

### 1. Modern OpenGL (4.6) Enforcement
- **ZERO Legacy OpenGL:** Do not output any deprecated OpenGL functions. 
- **Direct State Access (DSA):** You MUST use OpenGL 4.5+ Direct State Access for all object creation and manipulation (e.g., `glCreateTextures` instead of `glGenTextures`, `glTextureParameteri` instead of `glTexParameteri`). Never bind an object just to modify its state.
- **Buffer Management:** Use Shader Storage Buffer Objects (SSBOs) via `glNamedBufferStorage` for scene data, light lists, and tile data. Avoid UBOs for variable-length or large data structures.

### 2. Rendering Pipeline: Forward+ (Tile-Based)
The engine uses a Forward+ architecture. Do not suggest or write code for Deferred rendering (no G-Buffers). 
- **Depth Pre-pass:** The pipeline must start with a strict depth pre-pass to populate a depth texture.
- **Compute Light Culling:** Use a Compute Shader (`.comp`) to divide the screen into 16x16 pixel tiles, read the depth buffer to calculate min/max depth per tile, build a frustum, and intersect it with the scene's light bounding volumes.
- **Light Index List:** The compute shader must output a list of visible light indices per tile into an SSBO for the final forward pass to consume.
- **Transparency Pass:** After the opaque forward pass, render transparent geometry sorted back-to-front. Transparent objects must **NOT** write to the depth buffer (`glDepthMask(GL_FALSE)`) but should still read from the same compute shader light list SSBOs to evaluate direct lighting.

### 3. Lighting & Shading Model
- **Real-Time Direct Lighting:** Evaluate direct light using a standard glTF 2.0 PBR BRDF in the forward fragment shader.
- **Baked Indirect Lighting (SH):** The engine reads 9 coefficients (3rd-order Spherical Harmonics) per vertex or texel. Apply Ramamoorthi/Hanrahan cosine convolution factors for the diffuse indirect. Evaluate the SH along the reflection vector to compute the specular indirect irradiance.
- **Local Light Shadowing (Atlas):** - Support a dynamic shadow map atlas for a maximum of the 16 most "important" dynamic local point/spot lights.
  - Allocate atlas resolutions into 3 tiers (1024x1024, 512x512, 256x256) based on a heuristic of `flux/(distance_to_camera^2)`.
  - Use PCF (Percentage-Closer Filtering) for shadow sampling.
- **Directional Light Shadowing (CSM):**
  - The Sun (Directional Light) must use a separate Cascaded Shadow Maps (CSM) implementation.
  - Divide the camera frustum into 3 or 4 distance-based splits, rendering a tightly fit orthographic shadow map for each.
- **Shadow Masking:** Characters and environments must share the same direct light and shadow maps to prevent double-shadowing artifacts.

### 4. AI Interaction Rules
- **Code Generation:** When generating GLSL, provide the complete shader unless explicitly asked for a snippet. 
- **Performance First:** Prioritize data-oriented design. Minimize state changes. Group draw calls.
- **Debugging:** If an OpenGL error is suspected, suggest using `glDebugMessageCallback` rather than legacy `glGetError` loops.
- **No Hallucinations on Scope:** Stay within the bounds of a Forward+ architecture. If a feature request conflicts with Forward+ or the SH baked lighting model, warn the user before implementing.


### Phase 0: Build System

* **Step 0: CMake Project Setup**
* **Goal:** Initialize a CMake project with a basic build configuration. Source files go to the `src` directory.
* **Verification:** A successful build with no errors.
* **Targets (current layout):**
  * `sh_renderer_core` (STATIC, `POSITION_INDEPENDENT_CODE ON`, so it can link into a shared module such as ioquake3's `renderer_sh`): camera, cascade, culling, the draw passes and light culling, glad, render targets, scene, shader loader, SSBOs, the logging shim, the file reader, the frame API (`frame.{h,cpp}`) and the engine facade (`engine_api.{h,cpp}`). It links only OpenGL and Eigen. It must never link or include glog, gflags, GLFW, tinygltf, tinyexr, Embree or stb_image's implementation.
  * `sh_renderer_gltf` (STATIC): the glTF loader, `gltf_scene_io` (EXR lightmaps, `ComputeLightRadius`), `implementations.cpp` (tinygltf, stb_image and tinyexr implementations), window, input and interaction. It links the core plus tinygltf, tinyexr, stb_image, nlohmann_json, GLFW, gflags, ZLIB and Threads.
  * `sh_renderer_main` (the viewer) links `sh_renderer_gltf`.
  * Tests: `sh_renderer_core_test` (camera, cascade, culling, scene, shader, log, file_reader; no GL context) links the core; `sh_renderer_test` (loader, loader_layers, window, input, interaction, shader_gl, frame: everything needing GLFW or a context) links the glTF target.
* **Options:**
  * `SH_RENDERER_BUILD_GLTF` (ON): builds the glTF target, the viewer and `sh_renderer_test`, and is the only place gflags, glfw3, tinygltf, tinyexr, stb_image and nlohmann/json are looked for.
  * `SH_RENDERER_BUILD_TESTS` (ON): looks for GoogleTest and builds the test binaries. A host embedding the core (`add_subdirectory`) turns both off and gets only `sh_renderer_core`.
  * `SH_RENDERER_EMBED_GLSL` (OFF): builds `glsl/` into the core (`MakeEmbeddedReader`).
  * `SH_RENDERER_Q3_COORDINATE` (OFF): defines `Q3_COORDINATE` (Z-up Quake 3 map units) as a PUBLIC compile definition of the core, so the core's translation units and every target that links it (tests, the engine glue) see the same macro. The viewer and the tests build with it off.
* Release builds use `-O3 -march=native` for every target.

* **Step 0.5: Main Entry**
* **Goal:** Add a `src/main.cpp`. The program accept an argument `--input` that specifies the path to a glTF scene file. Parses flags with gflags; logging goes through `src/log.h`'s default stderr sink. Set namespace to `sh_renderer`.
* **Verification:** A successful build with no errors.


### Phase 1: Engine Foundation & DSA

* **Step 1: Window & OpenGL 4.6 Context**
* **Goal:** Initialize your windowing library (GLFW), implement camera interactions, load the OpenGL 4.6 Core profile, and establish the main render loop.
* **Verification:** A clear screen with a specific clear color. No legacy OpenGL errors are thrown.
* **Frame API (current):** the render loop is inverted so a host (the viewer, or ioquake3's `renderer_sh` module) owns the window, the GL context, input and the swap, and drives the renderer one frame at a time (`src/frame.h`):
  * `CreateRenderer(&error)` builds every program through the default file reader and the size-independent resources, then enables `GL_FRAMEBUFFER_SRGB`. On failure it returns null with one line per failed program (name, files, and the missing file or the driver's info log) and leaves the GL state alone.
  * `BeginFrame(r, width, height)` makes or resizes the size-dependent targets and sets the viewport; it returns false for a 0x0 drawable (a minimised window), and the host skips the frame.
  * `RenderScene(r, scene, camera, time_seconds)` clears the HDR target to `clear_color` and, with a scene, runs the shadow, depth, SSAO, light culling, radiance and sky passes. The scene may be null.
  * `EndFrame(r)` tonemaps into framebuffer 0 without swapping, then unbinds the program, vertex array and texture unit 0 and disables depth testing; other state is as the last pass left it (documented in `frame.h`).
  * `DestroyRenderer(std::move(r))` deletes the GL objects with the context current.
  * **Display gamma:** the tonemap raises the sRGB-encoded colour e to e^(1/g). `SetDisplayGamma` clamps g to [0.5, 3.0] like stock `r_gamma`, and a non-finite or non-positive g becomes 1 with one warning. At 1 the output is the plain tonemap.
* **Engine facade (`src/engine_api.h`):** the only header an engine translation unit includes. It uses standard types and an opaque `ShRenderer*`, so engine code sees no Eigen, glad or core compiler flags: `ShCreate(ShHostServices)` (GL loader, version check for 4.6, log/fatal sink and file reader; `ShLastError()` on failure), `ShBeginFrame`, `ShEndFrame`, `ShSetClearColor`, `ShSetDisplayGamma`, `ShReadPixels` and `ShDestroy`. `ShEmbeddedReader()` and `ShHasEmbeddedGlsl()` give the host the built-in GLSL reader (always missing without `SH_RENDERER_EMBED_GLSL`), so it can install or wrap it without including core headers.


* **Step 2: DSA Abstractions**
* **Goal:** Create modern C++ wrappers for Textures, Buffers (SSBOs), and Framebuffers using strict Direct State Access (e.g., `glCreateTextures`, `glNamedBufferStorage`).
* **Verification:** Successfully compile and bind an empty SSBO and Texture without ever using legacy `glBind*` state machine functions for initialization.



### Phase 2: The Forward+ Pre-requisites

* **Step 3: Unlit Geometry Pass**
* **Goal:** Load basic test geometry (like the Sponza model) and render it with a basic flat/unlit shader to ensure vertex transformations are correct.
* **Verification:** The camera moves correctly, and geometry is visible on screen.


* **Step 4: The Depth Pre-pass**
* **Goal:** Create a Framebuffer Object (FBO) with a depth attachment. Render the scene's opaque geometry into it using a depth-only shader. This is mandatory for Forward+ culling.
* **Verification:** Render the resulting depth buffer to a fullscreen quad. Linearize the depth values in the debug shader so closer objects appear darker and distant objects appear lighter, confirming the buffer is populated correctly.
* Note: During the subsequent Forward passes, ensure glDepthFunc(GL_LEQUAL) is set.


### Phase 3: Forward Pipeline with Shadowed Sun


* **Step 5: Direct PBR Shading (Forward)**
* **Goal:** Write the final Forward fragment shader. For each pixel, evaluate the glTF PBR BRDF against the sun light.
* **Verification:** The scene is rendered with correct PBR lighting.


* **Step 6: Shadowed Sun (CSM)**
* **Goal:** Implement Cascaded Shadow Maps for the sun light.
* **Verification:** The scene is rendered with correct shadows.


* **Step 7: Integrating Baked Indirect SH**
* **Goal:** Pass your baked 3rd-order SH texture maps into the shader. Evaluate the SH irradiance (use the cosine factors from Ramamoorthi and Hanrahan) for the indirect diffuse term. Evaluate the SH radiance at the direction of reflection vector for the indirect specular term. Add the indirect contribution to the final PBR lighting equation.
* **Verification:** Soft, multi-directional indirect light blends seamlessly with the sharp, real-time direct specular highlights. Visually no artifacts are introduced.


### Phase 4: Compute-Based Light Culling

* **Step 8: Scene Light Management (SSBO)**
* **Goal:** Define a standard `Light` struct (Position, Radius, Color, Intensity). Push an array of hundreds of test point lights to the GPU via an SSBO.
* **Verification:** Read back the SSBO data on the CPU to ensure data alignment (std430) is correct, or use a simple debug shader to draw tiny unlit spheres at the light positions.


* **Step 9: Compute Shader - Min/Max Depth & Frustum**
* **Goal:** Split the screen into 16x16 pixel tiles. Write a Compute Shader where each thread group (representing a tile) calculates the minimum and maximum depth from the depth pre-pass using atomic operations. Then, calculate the tile's view-space frustum planes.
* **Verification:** Output the min/max depth values to a debug texture and visualize it on-screen to ensure the depth bounds map correctly to the geometry.


* **Step 10: Compute Shader - Light Intersection**
* **Goal:** In the same compute shader, intersect the tile's frustum with all lights in the scene. Store the active light indices for that tile into a global light index SSBO.
* **Verification:** Output a "heatmap" texture where tile brightness correlates to the number of intersecting lights (e.g., black = 0 lights, white = max lights). You should clearly see the tiles lighting up around your test light spheres.



### Phase 5: Forward+ Shading

* **Step 11: Direct PBR Shading (Forward+)**
* **Goal:** Write the final Forward fragment shader. For each pixel, determine its screen-space tile, read the list of active light indices from the SSBO, and loop *only* over those lights to compute the glTF direct lighting.
* **Verification:** Hundreds of dynamic lights rendering at high framerates with correct specular highlights, without performance tanking.



### Phase 6: Dynamic Shadows

* **Step 12: Light Importance Ranking**
* **Goal:**  Frustum cull lights based on their bounding box. Rank lights by form factor (`flux/distance^2`).
* **Verification:**  Log the ranking to console.


* **Step 13: The Shadow Atlas**
* **Goal:** Create a 2048x2048 depth texture atlas for important dynamic lights. Top 2 lights consume two 1024x1024 tiles each. The following 4 lights consume 512x512 tiles each. The remaining 16 lights consume 256x256 tiles each. Point lights don't cast shadow. Render their shadow maps in a pass before the depth pre-pass.
* **Verification:** Visualize the shadow atlas texture array on-screen to ensure the 22 shadow maps are updating dynamically.


* **Step 14: Final Shadow Application**
* **Goal:** In the Step 11 Forward+ shader, sample the shadow atlas with Percentage-Closer Filtering (PCF) during the direct light evaluation.
* **Verification:** Smooth, dynamic shadows that correctly occlude both characters and the environment without double-shadowing artifacts.
