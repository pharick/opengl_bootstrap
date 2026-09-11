# opengl_bootstrap

A C++23 OpenGL 3.3 scaffold for working through
[_Learning Modern 3D Graphics Programming_](https://paroj.github.io/gltut/) (gltut), built with
CMake presets, clang-tidy-as-errors and sanitizer builds, targeting macOS.

Each chapter is its own small executable; everything reusable — window, GL object wrappers, shaders,
camera, meshes, textures — lives in a shared `glcore` static library (~4,600 lines). Every GL
resource is RAII-owned, no raw `new`/`delete`, and shader edits show up live without restarting.
The parts of `glcore` that need no GL context are covered by Catch2 tests.

## Highlights

- **One RAII owner for every GL object type** (`handle.hpp`). `GlObject<Traits>` is a move-only
  template parameterised on a `gen`/`del` pair; `Buffer`, `VertexArray`, `Texture`, `Sampler`,
  `Framebuffer`, `Renderbuffer`, `ProgramHandle` and `ShaderHandle` are aliases of it. The
  conversion to `bool` is `explicit`, so an owning handle cannot silently decay into a raw `GLuint`.
- **GLSL `#include` resolved in a hand-written preprocessor** (`glsl_source.cpp`), since core GLSL
  and macOS drivers have none. It hoists `#version`, emits `#line <n> <source-index>` so driver
  errors map back to the right file and line, detects include cycles and reports them as a chain,
  and returns `std::expected` rather than throwing so the hot-reloader can keep running.
- **Shader hot-reload that watches the full include closure** (`shader_watcher.cpp`). Timestamps are
  polled at most every 0.25 s; a failed rebuild is logged, the last working program keeps
  rendering, and the timestamps are re-snapshotted so a broken file is not retried every frame.
- **`Program` makes invalid programs unrepresentable**: no default constructor, static factories
  only, `tryFromFiles` returns `std::expected<Program, std::string>`. Uniform locations are cached
  in a map with a transparent `string_view` hash; `set(name, v)` throws on a typo and debug builds
  verify the program is actually bound before a uniform is written (GL 3.3 has no
  `glProgramUniform`).
- **Error checking designed around a macOS constraint**: `glDebugMessageCallback` is GL 4.3 and
  macOS stops at 4.1, so `GLC_CHECK(expr)` wraps a call and throws with the expression, file and
  line in Debug builds while compiling to the bare call in Release; `FrameErrorScope` drains the
  error queue once per frame in every build.
- **Initialisation order encoded as named types**, not comments. `App` declares
  `GlfwLibrary → Window → GlLoader → optional<ImGuiLayer> → ShaderWatcher` so construction and
  destruction order follow from member order, and `onInit()` is deliberately not called from the
  constructor. `GLC_MAX_FRAMES=45` turns any tutorial into a headless-ish smoke test under
  ASan/UBSan.
- **Build hygiene**: three CMake presets (`dev`, `asan`, `release`); clang-tidy runs *during*
  compilation with `--warnings-as-errors=*` in `dev`; a `-Wall -Wextra -Wconversion -Wshadow
  -Wold-style-cast …` policy is an INTERFACE target linked into everything except `third_party`,
  whose headers are `SYSTEM`; gli's vendored glm is kept off the include path so exactly one glm
  exists in the build (ODR).

## How it works

```
tutorials/tutNN_*/main.cpp        one class deriving glc::App per gltut chapter
        │  onInit / onUpdate / onRender / onGui / onKey / onResize
        ▼
glcore::App  ── run loop ──►  Input.beginFrame → glfwPollEvents → ShaderWatcher.poll
                              → onUpdate(dt) → ImGui frame + onGui → [FrameErrorScope]
                              clear + onRender → ImGui render → swapBuffers → frame cap
```

| Module (`glcore/`)                       | Responsibility                                                                 |
| ---------------------------------------- | ------------------------------------------------------------------------------ |
| `handle.hpp`                             | `GlObject<Traits>`; all GL object aliases                                      |
| `window.hpp` / `glfw.hpp` / `gl.hpp`     | `GlfwLibrary`, `Window`, `GlLoader` (GLEW init); header include order          |
| `app.hpp`                                | `AppConfig`, `App` base class, frame loop, `runApp<T>()` exception boundary   |
| `program.hpp`                            | Shader compile/link, uniform cache, uniform-block binding                      |
| `glsl_source.hpp`                        | `#include` expansion, `#line` bookkeeping, source-index legend                 |
| `shader_watcher.hpp`                     | `ReloadableProgram`, `ShaderWatcher`                                           |
| `gl_check.hpp`                           | `GLC_CHECK`, `FrameErrorScope`, `drainGlErrors`                                |
| `scoped_bind.hpp`                        | Scope-tied bind/unbind for VAO, buffer, texture (no DSA in GL 3.3)             |
| `buffer.hpp` / `vertex_array.hpp`        | `makeBuffer`, `AttributeDesc`, `makeVertexArray`                               |
| `uniform_buffer.hpp`                     | UBO creation, `bindToPoint`, `bindRange`, std140 notes                         |
| `mesh.hpp`                               | gltut XML mesh loader (tinyxml2): attributes, named VAOs, render commands      |
| `texture.hpp`                            | KTX/DDS loading via gli, `makeTexture2D`, sampler objects, anisotropy          |
| `camera.hpp`                             | `Camera` (view/projection cache), `OrbitController`, `FlyController`           |
| `matrix_stack.hpp`                       | gltut-compatible `MatrixStack` with a movable RAII `Frame`                     |
| `input.hpp`                              | Polled keyboard/mouse with per-frame edge detection; ImGui capture             |
| `imgui_layer.hpp` / `timer.hpp` / `log.hpp` / `paths.hpp` | ImGui backend, frame timer, `std::format` logging, path resolution |

## Prerequisites

```sh
brew install cmake ninja glfw glew glm tinyxml2 catch2
```

`tinyxml2` is only needed for gltut's XML meshes (Tutorial 7 onward). Without it the project still
configures and builds; the mesh loader and the chapters that load XML meshes are simply skipped.
`catch2` is only needed for the tests — pass `-DGLCORE_BUILD_TESTS=OFF` to skip them.

Dear ImGui and [gli](https://github.com/g-truc/gli) are pinned git submodules — neither has a
formula worth using. Everything else comes from Homebrew. CMake 3.28+ and a C++23 compiler
(`<expected>`, `<format>`) are required; GLFW 3.4 is the minimum `find_package` accepts.

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

Cache options behind the presets: `GLCORE_WERROR`, `GLCORE_TIDY`, `GLCORE_BUILD_TESTS`,
`GLCORE_SANITIZE` (e.g. `address,undefined`, applied to `third_party` too so ASan interposes) and
`GLCORE_GL_VERSION` (`3.3` or `4.1`).

Configuring also symlinks `compile_commands.json` into the project root, so clangd works with no
extra setup. It repoints at whichever preset you configured last — run `cmake --preset dev` if your
editor starts showing release flags. The symlink is gitignored.

## Testing

Tests (the parts that need no GL context: `#include` expansion, path resolution and the matrix
stack) run with `ctest --preset dev` or `ctest --preset asan`. Catch2's `catch_discover_tests`
registers each `TEST_CASE` as its own CTest entry, so `ctest --preset dev -R matrix` runs just the
matrix-stack cases and a failure names the case rather than the binary. The test target gets the
same inline clang-tidy as the library.

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
never formatted or linted. The check set is `bugprone-*`, `clang-analyzer-*`,
`cppcoreguidelines-*`, `misc-*`, `modernize-*`, `performance-*` and `readability-*`; every disabled
check has a one-line justification in `.clang-tidy`.

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
LLVM keg too, so `brew install llvm` is all that is needed. Homebrew's clang-tidy is also handed the
SDK sysroot from `xcrun --show-sdk-path`, without which it cannot find libc++'s `<format>` and
`<expected>` and reports garbage from a half-parsed AST.

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

That is the whole process. `add_tutorial()` globs the directory, links `glcore` and the warning
policy, and injects `GLC_TUTORIAL_DIR` so the binary finds its shaders from any working directory
and the hot-reloader watches the source files rather than a copy in `build/`. Shaders live next to
the tutorial that uses them, which is what keeps chapters independent.

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
`AppConfig` covers depth test, face culling and winding, ImGui and the stats overlay, auto-clear
and clear colour, the maximum frame delta, Escape-to-quit and the frame cap.

## Included examples

| Tutorial                | Shows                                                                      |
| ----------------------- | -------------------------------------------------------------------------- |
| `tut01_hello_triangle`  | Buffer, VAO, program, hot-reload                                           |
| `tut02_cube_and_camera` | XML mesh, orbit camera, matrix stack, ImGui panel                          |
| `tut03_textured_quad`   | KTX loading, samplers, mipmaps, anisotropy, linear vs sRGB                 |
| `tut09_basic_lighting`  | Diffuse lighting, generated normals, the normal matrix, a `Projection` UBO |
| `tut10_point_lights`    | Point light, per-vertex vs per-fragment shading, attenuation, shared GLSL  |
| `tut11_specular_lights` | Specular highlights: Phong, Blinn-Phong and Gaussian, isolated per term    |

Shared GLSL lives in `assets/shaders/common/` (`lighting.glsl`, `specular.glsl`, `gamma.glsl`) and
is pulled in with `#include`.

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
source `[2]`, and the legend is printed with the error. Quoted includes resolve next to the
including file first, then under `assets/shaders`; angled includes only the latter.

**Error checking** (`gl_check.hpp`) — macOS caps OpenGL at 4.1, so `glDebugMessageCallback` (4.3)
does not exist. `GLC_CHECK(expr)` runs the call and throws with the expression, file and line if the
error queue is dirty; it compiles to the bare call in release. `FrameErrorScope` drains once per
frame in every build.

**Path resolution** (`paths.hpp`) — the project root is injected at compile time, so binaries run
from any working directory. `paths::asset("meshes/UnitCube.xml")`,
`paths::tutorialShader("my.vert")`. If the injected root no longer exists (a relocated build) it
falls back to a marker search upward from the executable.

**`Program`** (`program.hpp`) — static factories only, so an invalid program is unrepresentable.
Uniform locations are cached; `set(name, value)` throws on a typo, `setIfPresent` does not. Debug
builds verify the program is actually bound before a uniform is set.

**`MatrixStack`** (`matrix_stack.hpp`) — deliberately API-compatible with the book's
`glutil::MatrixStack`, PascalCase and degrees included, so listings transfer unchanged. `push()`
returns a movable RAII frame; the nesting invariant is checked in every build, and popping the base
matrix is refused rather than corrupting the stack.

Also: `Camera` with `OrbitController`/`FlyController` (the book's ViewPole/ObjectPole role),
`Input` with edge detection that goes quiet while ImGui has focus, `Mesh` for gltut's XML format
(main VAO plus the file's named attribute subsets, indexed and array draw commands),
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
in `ScopedBind` so the pairs cannot drift apart. Unbinding restores 0 rather than the previous
binding, which avoids a `glGet` round-trip and matches the book's sample code.

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

## Project layout

```
CMakeLists.txt, CMakePresets.json   options, dependency lookup, sanitizers, presets
cmake/                              CompilerWarnings, AddTutorial, Lint (format/tidy targets)
glcore/include/glcore/, glcore/src/ the library (22 headers, 19 sources)
tutorials/tutNN_*/                  one executable per chapter, shaders alongside
assets/meshes|shaders|textures/     gltut XML meshes, shared GLSL, KTX + PNG masters
tests/                              Catch2 tests for glsl_source, matrix_stack, paths
third_party/                        imgui and gli submodules, wrapped as SYSTEM targets
tools/png_to_ktx.py                 offline texture conversion
```

About 6,000 lines of C++ across 50 files plus ~420 lines of GLSL across 17 shader files.

## Limitations

- The build is written for macOS with Homebrew: `brew --prefix` is added to `CMAKE_PREFIX_PATH`,
  clang-tidy is located in the LLVM keg and given the Xcode SDK sysroot, and the GL ceiling of 4.1
  drives several design choices. MSVC warning flags exist in `CompilerWarnings.cmake`, but Linux
  and Windows have not been the target.
- Only six of the book's chapters are present so far; the later texturing and framebuffer chapters
  have library support (`texture.hpp`, `Framebuffer`/`Renderbuffer` handles) but no tutorial yet.
- Unit tests cover only the context-free parts of `glcore`; GL-dependent code is exercised through
  the `GLC_MAX_FRAMES` sanitizer smoke runs rather than automated assertions.
- Hot-reload polls `last_write_time` rather than using a filesystem watcher; the 0.25 s interval is
  configurable via `ShaderWatcher::setInterval`.
- There is no CI configuration in the repository.

## Context

A personal scaffold for following gltut in modern C++ rather than the book's own GLUT-era
framework. Several comments in `glcore` refer to `humangl`, an earlier project whose implicit
handle conversions, non-movable matrix-stack frames and comment-encoded initialisation order this
codebase deliberately fixes.
