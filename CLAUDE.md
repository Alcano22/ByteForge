# ByteForge

C++23 game engine and editor (CMake 4.1+, vcpkg, Vulkan 1.3). The primary path is 2D (EnTT scenes, Renderer2D, Box2D).
A 3D path (MeshRenderer, PerspectiveCamera) exists but is currently only used by Sandbox.
Scripting: native C++, C# (.NET 10) and a visual node graph.

## Targets

- `Engine`: shared library (`Engine/`)
- `Editor`: ImGui docking editor (`Editor/`)
- `Sandbox`: small 3D demo app (`Sandbox/`)
- `ScriptCore`: C# assembly, built by CMake through `dotnet build`

## Build and run

Configure and build through the presets in `CMakePresets.json`.

```bash
cmake --preset default                  # Ninja, build dir `build/`, Debug, vcpkg toolchain
cmake --build build                     # there is no build preset, build the directory directly
cmake --build build --target Editor     # or a single target (Engine, Editor, Sandbox, ScriptCore)
```

- The `VCPKG_ROOT` environment variable must be set (the preset uses `$env{VCPKG_ROOT}`).
- Binaries land in `build/<Target>/`, for example `build/Editor/Editor`.
- Needs the .NET 10 SDK (`dotnet`) at build time and at editor runtime.
- Dependencies live in `vcpkg.json`. A new dependency needs: `vcpkg.json` + `find_package` + `target_link_libraries`.
- The Editor must be started from a project directory that contains `assets/`. It also writes `GameScripts.csproj`
  and `.byteforge/` into the working directory.
- There is no automated test suite yet. Verification means: builds with zero warnings, then run Editor or Sandbox.
  If you cannot run the app (GPU/window needed), say so explicitly instead of claiming it works.
- Strict check: configure with `-DBYTEFORGE_WARNINGS_AS_ERRORS=ON`. Warnings are `-Wall -Wextra -Wpedantic -Wshadow
  -Wconversion`, so use explicit `static_cast` for narrowing conversions.

## Layout

- `Engine/include/Engine/<Module>/`: public, backend-agnostic headers (`#include "Engine/..."`)
- `Engine/src/<Module>/`: private implementation; internal headers are included as `"Platform/..."`, `"Renderer/..."`,
  `"Scripting/..."`
- `Engine/src/Platform/Vulkan/`: the only place with Vulkan types. `Linux/`, `Windows/`, `MacOS/` hold OS code.
- `Engine/resources/shaders/*.hlsl`: HLSL, compiled to SPIR-V at runtime via DXC
- `Editor/src/Editor/`: editor code (`#include "Editor/..."`, engine via `<Engine/...>`)
- `Editor/vendor/`: third-party, do not edit
- `ScriptCore/Source/`: C# scripting API and interop
- `context.txt` and `collect_context.sh`: generated repo snapshot, never edit, never treat as source of truth

## Architecture rules

- Public headers must not include Vulkan. Add a new GPU resource in this order: public interface with a static
  `Create` in `Engine/include/Engine/Renderer/`, a virtual in `Renderer/RenderBackend.h`, an override in
  `Platform/Vulkan/VulkanBackend.cpp`, and the implementation in `Platform/Vulkan/`.
- Create GPU objects with `MakeVulkanObject<>`. Deletion is deferred through `VulkanDeletionQueue`, never `delete`
  a GPU object directly. Frames in flight: 2.
- Vulkan 1.3 only: dynamic rendering and synchronization2, no `VkRenderPass`/framebuffers. Use `CmdImageBarrier`.
- `Engine` is built with hidden visibility. Everything the Editor/Sandbox uses that is not header-inline needs
  `BYTEFORGE_API`.
- New `.cpp` files must be added to the explicit source lists in `Engine/CMakeLists.txt` / `Editor/CMakeLists.txt`.
- Asset registry and asset manager are main-thread only. Heavy work goes through `JobSystem`, results are polled.
- Shaders: entry points `VSMain`/`PSMain`. Set 0 = scene uniforms, set 1 = material (at most one UBO, textures,
  samplers named `<Texture>Sampler`), one push-constant block. The vertex layout must match the shader inputs
  (checked by reflection). Shader errors appear at runtime, not at build time.
- Colors passed to renderers are sRGB; the renderers convert for sRGB targets.

## C++ conventions (match the surrounding code)

- 4 spaces, Allman braces, `} else` / `} catch` on the closing-brace line. `#pragma once`. `namespace ByteForge`.
- PascalCase for types, functions, constants, enumerators and struct fields. camelCase for locals/parameters.
  Prefixes: `m_` members, `s_` statics, `t_` thread_local.
- `enum class` only. `const` on by-value parameters and locals. `[[nodiscard]]` on getters/factories.
  `explicit` single-argument constructors, `override`, `final` on leaf classes.
- Ownership: `Scope<T>` (unique_ptr), `Ref<T>` (shared_ptr), `MakeScope`, `MakeRef`. No raw owning pointers.
  RAII everywhere, `NonCopyable` base for resource owners.
- Class layout: `public:`, then `private:` methods, then a second `private:` for data members.
- Prefer `std::format`, `std::ranges`, designated initializers for spec structs, `static_cast` over C casts.
- Errors: exceptions for programmer errors/invariants (`std::runtime_error`, message format `"Class::Method: reason"`),
  `std::expected<T, std::string>` for recoverable failures (I/O, parsing, validation). Vulkan calls use `VK_CHECK`.
- Logging: `CORE_*` macros inside Engine, `APP_*` in Editor and game code. Format-string style (spdlog).
- Comments are rare. Only explain non-obvious *why*, never *what*.

## Checklists

Adding a component:
1. Define it in `Engine/include/Engine/Scene/Components.h`.
2. `SceneSerializer.cpp`: `ReadComponents`, `SerializeEntity`, `RemoveSerializableComponents`.
3. `Scene.cpp`: add it to `DuplicableComponents`.
4. `Editor/Inspectors/ComponentInspectors.cpp`: draw function and an `Inspect<>` entry. A new icon needs
   `EditorIcon`, the `IconFiles` array (same order) and an SVG in `Editor/resources/icons/`.

Adding a script API function:
1. `Engine/src/Scripting/API/CoreAPI.cpp`: write `Ret Fn(const ScriptCallContext&, ...)` and
   `api.Register<&Fn>("Category.Name", "Display Name", Pure|Impure, { "Param", ... })`. It automatically becomes a
   visual-graph node.
2. For C#: `ScriptCore/Source/Interop/ScriptAPI.cs` needs the matching `Resolve(...)` with the exact signature
   string, for example `(Entity,Vector3)->Void` or `(Asset:AudioClip)->Void`, plus a wrapper in `Source/Scene/`.
   A signature mismatch makes `Bind()` fail and disables the C# backend.
3. The 16-byte value slot layout in `ManagedValue.cpp` and `APIValue.cs` must stay in sync.

Adding an asset type:
1. `AssetType` enum in `AssetType.h` **and** `ScriptCore/Source/Assets/Asset.cs` (same order).
2. `AssetTypeFromExtension`, `DefaultAssetSettings`, `DeserializeAssetSettings`, `EditorIcons::ForFile`,
   `DrawAssetInspector`.

Adding an editor panel:
1. Derive from `EditorPanel`, constructor takes `EditorContext&`, add the `.cpp` to `Editor/CMakeLists.txt`,
   open it with `context.Open<T>()` in `EditorLayer::OnAttach`.
2. Undoable changes go through `EditorCommand` and `CommandHistory`. Entity create/rename/delete goes through
   `EditorContext` methods (they defer and record commands).
3. New shortcut actions go into `EditorActions`. Update the explicit size of the `std::array` in `All()`.

## Do not hand-edit

- `*.meta` sidecar files (generated by `AssetRegistry`)
- `GameScripts.csproj` (generated by the editor on startup)
- `context.txt`, anything under `Editor/vendor/`

## Working agreement

- Read the surrounding code first and mirror existing patterns. Keep diffs small and focused.
- No shortcuts or hacks. If a clean solution needs a refactor, propose it before doing it.
- Ask before: adding dependencies, changing public API, changing file formats (scene `.bfscene`, graph `.bfgraph`).
  Format changes must keep old files loadable.
- Build after every change, fix all warnings, and report what you actually ran.
- Everything written into the repo is in English: code, comments, logs, error messages, commit messages.
  The conversation itself may be in German.

## Git

- Only commit when asked.
- Commit message: a single short English one-liner in imperative mood, no body, no trailers
  (no `Co-Authored-By`). Example: `Add physics material picker to collider inspector`
