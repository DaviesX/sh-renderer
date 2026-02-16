# Project Overview: Custom Forward+ Engine
You are assisting in the development of a custom 3D rendering engine built from scratch. The core rendering philosophy is "Mixed Lighting": Real-time Direct PBR lighting combined with pre-baked 3rd-order Spherical Harmonics (SH) for indirect global illumination. 

## Tips
- Build and test the project using the following commands:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -B build -S . && cmake --build build --parallel 12 && ./build/sh_baker_test
```
- **Coding Style:** google c++ style guide. Prefer free functions over classes unless the states are complex.
- **Language:** C++20

## Technology Stack
- **Build System** CMake
- **Testing Framework:** googletest (find_package)
- **Logging Library:** glog (find_package)
- **Command Line Argument Parsing:** gflags (find_package)
- **Graphics API:** OpenGL 4.6 Core Profile, glfw3 (find_package).
- **Shading Language:** GLSL 460 core.
- **Math Library:** Eigen3 (find_package)
- **glTF Loader:** tinygltf (find_path & add_library)
- **JSON Library:** nlohmann/json (find_path & add_library)
- **Image Library:** stb_image, tinyexr (find_path & add_library)


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

* **Step 0.5: Main Entry**
* **Goal:** Add a `src/main.cpp`. The program accept an argument `--input` that specifies the path to a glTF scene file. Initializes GLOG and GFLAGS properly. Set namespace to `sh_renderer`.
* **Verification:** A successful build with no errors.


### Phase 1: Engine Foundation & DSA

* **Step 1: Window & OpenGL 4.6 Context**
* **Goal:** Initialize your windowing library (GLFW/SDL), load the OpenGL 4.6 Core profile, and establish the main render loop.
* **Verification:** A clear screen with a specific clear color. No legacy OpenGL errors are thrown.


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
