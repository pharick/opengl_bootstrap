# opengl_bootstrap

A modern C++23 scaffold for working through
[_Learning Modern 3D Graphics Programming_](https://paroj.github.io/gltut/) (gltut).

Each chapter is its own small executable; everything reusable — window, GL object wrappers, shaders,
camera, meshes, textures — lives in a shared `glcore` library. Every GL resource is RAII-owned, no
raw `new`/`delete`, and shader edits show up live without restarting.

## Prerequisites

```sh
brew install cmake ninja glfw glew glm tinyxml2 catch2
```

`tinyxml2` is only needed for gltut's XML meshes (Tutorial 7 onward). Without it the project still
configures and builds; the mesh loader and the chapters that load XML meshes are simply skipped.
`catch2` is only needed for the tests — pass `-DGLCORE_BUILD_TESTS=OFF` to skip them.

Dear ImGui and [gli](https://github.com/g-truc/gli) are pinned git submodules — neither has a
formula worth using. Everything else comes from Homebrew.

## Build and run

```sh
git submodule update --init --recursive
cmake --preset dev
cmake --build --preset dev
./build/dev/bin/tut01_hello_triangle
```

| Preset    | What it gives you                                                                      |
| --------- | -------------------------------------------------------------------------------------- |
| `dev`     | Debug, `-Werror`, clang-tidy as errors, `GLC_CHECK` active, uniform-binding assertions |
| `asan`    | `dev` plus AddressSanitizer and UndefinedBehaviorSanitizer                             |
| `release` | RelWithDebInfo, no clang-tidy, error checks compiled out                               |

Configuring also symlinks `compile_commands.json` into the project root, so clangd works with no
extra setup. It repoints at whichever preset you configured last — run `cmake --preset dev` if your
editor starts showing release flags. The symlink is gitignored.

Tests (the parts that need no GL context) run with `ctest --preset dev`. Catch2's
`catch_discover_tests` registers each `TEST_CASE` as its own CTest entry, so `ctest --preset dev -R
matrix` runs just the matrix-stack cases and a failure names the case rather than the binary.

Any tutorial can be run as a headless-ish smoke test by capping its frame count, which is how the
GL resource lifetimes get exercised end to end under a sanitizer:

```sh
GLC_MAX_FRAMES=45 ./build/asan/bin/tut01_hello_triangle
```

## Code style

Enforced by `.clang-format` and `.clang-tidy`; both are checked in and both run from CMake:

```sh
cmake --build --preset dev --target format         # rewrite sources in place
cmake --build --preset dev --target format-check   # fail if anything is unformatted
cmake --build --preset dev --target tidy           # clang-tidy over everything at once
```

The `dev` preset also runs clang-tidy _during_ compilation with `--warnings-as-errors=*`, so a new
finding fails the build. That costs roughly 2x wall-clock on a clean build (11.4s vs 6.0s here);
turn it off with `-DGLCORE_TIDY=OFF` if it gets in the way while experimenting. `third_party` is
never formatted or linted.

The conventions, in brief:

- **Tabs indent, spaces align.** `UseTab: ForIndentation`, width 4, 100 columns. Wrapped arguments
  and trailing comments therefore line up at any tab width.
- **`PascalCase`** types, **`camelCase`** functions and variables, **`kCamelCase`** constants,
  **`trailing_`** underscore on private members, **`snake_case.hpp`** filenames, everything in
  namespace `glc`.
- **American spelling** (`color`, not `colour`) — it matches `glClearColor`, `GL_COLOR_BUFFER_BIT`,
  glm and ImGui, which you are reading alongside your own code all day.
  - The one deliberate exception is `MatrixStack`, whose `PascalCase` methods and degree-valued
    angles mirror gltut's `glutil::MatrixStack` so book listings compile unchanged.
- **Short function bodies are expanded**, except empty ones — `virtual void onInit() {}` stays on
  one line.

Homebrew keeps `clang-format`/`clang-tidy` out of the default `PATH`; the CMake targets look in the
LLVM keg too, so `brew install llvm` is all that is needed.

Everything that is not C++ is covered too:

| Files                                     | Handled by                                                              |
| ----------------------------------------- | ----------------------------------------------------------------------- |
| `.glsl` / `.vert` / `.frag`               | clang-format, via the same `format` target — it treats them as C++      |
| `.md`, `.json`                            | Prettier, pinned to 100 columns by `.prettierrc` to match the C++ limit |
| `CMakeLists.txt`, `.cmake`                | 4-space, declared in `.editorconfig` (no formatter — kept by hand)      |
| `tools/*.py`                              | PEP 8 at 100 columns, declared in `.editorconfig` (kept by hand)        |
| `.clang-format`, `.clang-tidy`, `.clangd` | deliberately in `.prettierignore`                                       |
| everything                                | `.editorconfig`: UTF-8, LF, final newline, no trailing whitespace       |

`.editorconfig` matters most for the C++ itself: `.clang-format` indents with tabs, and an editor
inserting spaces on new lines would fight it on every keystroke.

The clang tooling configs are YAML but are excluded from Prettier on purpose — `.clang-tidy`'s
`Checks: >` block scalar is load-bearing, and Prettier does rewrite these files (it turns
`WarningsAsErrors: ''` into `""`).

## Adding a chapter

1. `mkdir -p tutorials/tut04_whatever/shaders`
2. Write `main.cpp` and your `.vert` / `.frag`
3. Add `add_tutorial(tut04_whatever)` to `tutorials/CMakeLists.txt`

That is the whole process. Shaders live next to the tutorial that uses them, which is what keeps
chapters independent and hot-reload pointed at the file you are actually editing.

A minimal tutorial:

```cpp
#include <glcore/app.hpp>
#include <glcore/buffer.hpp>
#include <glcore/paths.hpp>
#include <glcore/scoped_bind.hpp>
#include <glcore/vertex_array.hpp>

class MyTutorial final : public glc::App {
public:
    MyTutorial() : glc::App({.window = {.title = "gltut 04"}}) {}

protected:
    void onInit() override {
        program_ = &shaders().add(glc::paths::tutorialShader("my.vert"),
                                  glc::paths::tutorialShader("my.frag"));
        vbo_ = glc::makeBuffer(GL_ARRAY_BUFFER, kVertices);
        vao_ = glc::makeVertexArray(vbo_, std::array{glc::AttributeDesc{.location = 0}});
    }

    void onRender() override {
        program_->get().use();
        const glc::ScopedBind bind{vao_};
        GLC_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
    }

private:
    glc::ReloadableProgram* program_ = nullptr;
    glc::Buffer vbo_;
    glc::VertexArray vao_;
};

int main() { return glc::runApp<MyTutorial>(); }
```

`App` hooks: `onInit`, `onUpdate(dt)`, `onRender`, `onResize`, `onKey`, `onGui`.
`App` accessors: `window()`, `input()`, `camera()`, `matrices()`, `shaders()`, `timer()`, `aspect()`.

## Included examples

| Tutorial                | Shows                                                                      |
| ----------------------- | -------------------------------------------------------------------------- |
| `tut01_hello_triangle`  | Buffer, VAO, program, hot-reload                                           |
| `tut02_cube_and_camera` | XML mesh, orbit camera, matrix stack, ImGui panel                          |
| `tut03_textured_quad`   | KTX loading, samplers, mipmaps, anisotropy, linear vs sRGB                 |
| `tut09_basic_lighting`  | Diffuse lighting, generated normals, the normal matrix, a `Projection` UBO |
| `tut10_point_lights`    | Point light, per-vertex vs per-fragment shading, attenuation, shared GLSL  |
| `tut11_specular_lights` | Specular highlights: Phong, Blinn-Phong and Gaussian, isolated per term    |

## What glcore gives you

**`GlObject<Traits>`** (`handle.hpp`) — one move-only owner for every GL object type.
`Buffer`, `VertexArray`, `Texture`, `Sampler`, `Framebuffer`, `Renderbuffer`, `ProgramHandle`,
`ShaderHandle` are all aliases of it.

**Shader hot-reload** (`shader_watcher.hpp`) — register a program with `shaders().add(...)` and
edits rebuild it on the next frame. A compile error is logged and the previously working program
keeps rendering. The watch list is the full `#include` closure, so editing a shared header reloads
everything that uses it.

**GLSL `#include`** (`glsl_source.hpp`) — resolved before the driver sees the source, with cycle
detection. Line numbers survive via `#line`; a driver error reported as `2(15)` means line 15 of
source `[2]`, and the legend is printed with the error.

**Error checking** (`gl_check.hpp`) — macOS caps OpenGL at 4.1, so `glDebugMessageCallback` (4.3)
does not exist. `GLC_CHECK(expr)` runs the call and throws with the expression, file and line if the
error queue is dirty; it compiles to the bare call in release. `FrameErrorScope` drains once per
frame in every build.

**Path resolution** (`paths.hpp`) — the project root is injected at compile time, so binaries run
from any working directory. `paths::asset("meshes/UnitCube.xml")`,
`paths::tutorialShader("my.vert")`.

**`Program`** (`program.hpp`) — static factories only, so an invalid program is unrepresentable.
Uniform locations are cached; `set(name, value)` throws on a typo, `setIfPresent` does not. Debug
builds verify the program is actually bound before a uniform is set.

**`MatrixStack`** (`matrix_stack.hpp`) — deliberately API-compatible with the book's
`glutil::MatrixStack`, PascalCase and degrees included, so listings transfer unchanged. `push()`
returns a movable RAII frame; the nesting invariant is checked in every build.

Also: `Camera` with `OrbitController`/`FlyController` (the book's ViewPole/ObjectPole role),
`Input` with edge detection that goes quiet while ImGui has focus, `Mesh` for gltut's XML format,
`UniformBuffer` for Tutorial 9+, `loadTexture2D`/`makeTexture2D`/`makeSampler` for Tutorial 14+.

## Notes

**macOS reports a 4.1 context even though the project requests 3.3.** That is expected — macOS only
ships 3.2 and 4.1 core profiles, so a 3.3 request is satisfied by 4.1. GLSL `#version 330` shaders
compile in it unchanged. Switch targets with `-DGLCORE_GL_VERSION=4.1` if a chapter needs it.

**gltut's XML meshes are wound clockwise.** `AppConfig::frontFace` defaults to `GL_CCW`, so a
chapter that loads them with `cullFace = true` needs `.frontFace = GL_CW`. Get it wrong and nothing
errors — culling just keeps the wrong half of every double-sided surface. `UnitPlane.xml` carries a
`+Y` copy and a `−Y` copy of the same four corners, so you end up looking at the underside, lit by
normals pointing at the floor, and the plane renders black.

**No DSA.** `glCreateBuffers` and friends are GL 4.5. Everything here is bind-then-modify, wrapped
in `ScopedBind` so the pairs cannot drift apart.

**Textures are KTX/DDS only — PNG and JPEG are not loaded at runtime.** gli reads GPU-ready
containers, not compressed image formats, so source images are converted offline:

```sh
python3 tools/png_to_ktx.py assets/textures/checker.png                  # -> checker.ktx, GL_RGB8
python3 tools/png_to_ktx.py assets/textures/checker.png --srgb -o x.ktx  # -> GL_SRGB8
```

The payoff is that the internal format lives in the asset: sRGB-vs-linear, block compression and any
mip chain are authored once rather than decided at every upload. That is why `tut03` loads two
_files_ to compare `GL_RGB8` against `GL_SRGB8`. The cost is that every new texture needs a
conversion step, and `.png` sources are kept in the repo purely as the editable master.

Textures generated procedurally in code — which several of the texturing chapters do — skip all of
this and go through `glc::makeTexture2D(w, h, internalFormat, format, type, pixels)`.

**GLEW quirks are handled once**, in `GlLoader`: `glewExperimental` is set for the core profile, and
the spurious `GL_INVALID_ENUM` that `glewInit` leaves behind is discarded so it cannot be blamed on
your first draw call.
