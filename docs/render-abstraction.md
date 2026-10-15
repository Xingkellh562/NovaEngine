# 渲染模块抽象方案

> 目标：把图形 API（gl 对象与 gl 函数）从高层系统中彻底抽离，同时为未来的 UI、复杂可动模型、骨骼动画、粒子等留出升级入口。
>
> 范围：**只抽象现有功能**，不提前实现任何未来特性。
>
> **配套文档**：[渲染抽象：完整实现框架](./render-abstraction-implementation.md) —— 每个新文件的完整代码框架 + 每个设计决策的原理说明。
>
> ⚠️ 落地前请先读实现框架文档的 **第 0.3 节**，其中列出了本文档的四处修正。

---

## 目录

- [1. 现状诊断](#1-现状诊断)
- [2. 这套结构为什么挡死了未来方向](#2-这套结构为什么挡死了未来方向)
- [3. 本次范围：明确不做的事](#3-本次范围明确不做的事)
- [4. 目标分层](#4-目标分层)
- [5. 最小改动面清单](#5-最小改动面清单)
- [6. 关键设计](#6-关键设计)
- [7. 依赖方向修正](#7-依赖方向修正)
- [8. CMake 收口](#8-cmake-收口)
- [9. 迁移步骤与验证不变式](#9-迁移步骤与验证不变式)
- [10. 「入口」到底留在哪](#10-入口到底留在哪)
- [11. 顺手修掉的既有缺陷](#11-顺手修掉的既有缺陷)
- [12. 附录：改造前后文件树对照](#12-附录改造前后文件树对照)

---

## 1. 现状诊断

NovaEngine 现在的分层是**「目录分层」而不是「依赖分层」**。真正的问题不是「gl 调用散落各处」，而是**三个概念被塞进了同一个命名空间和同一个对象**：

1. **GPU 资源**（VAO/VBO/EBO、纹理、FBO、Shader Program）
2. **场景/绘制数据**（位置、朝向、网格数据、可见性）
3. **渲染流程编排**（第几遍、读写哪个 RT、设什么状态）

其中 `render/opengl/context/render_context.h` 里的 `RenderContext` 就是这个糊在一起的物理载体。

### 1.1 gl 泄漏点清单

| # | 位置 | 泄漏内容 | 严重度 |
|---|---|---|---|
| 1 | `Engine/core/input/input_system.h:5` | `core/` 直接 `#include <render/opengl/context/render_context.h>`；`input_system.cpp:63-95` 操作 `context.chunkMesh.GetLayer(Default)->RequireMeshUpdate()` | **致命**（依赖方向反了：游戏逻辑依赖图形 API） |
| 2 | `Engine/render/system/render_system.cpp:24-146` | 近 30 处裸 `gl*`：`glBlendFunc`/`glClear`/`glEnable`/`glDepthFunc`/`glBindFramebuffer`/`glClearBufferfv`/`glBindTexture`/`glTexParameteri`。整个 Pass 顺序 + 状态机全在这里手写 | **致命** |
| 3 | `Engine/render/opengl/context/render_context.h` | 一个 struct 同时持有 GPU 对象（`ChunkMeshCache` 持 `vector<GLMesh>`、`LodMeshCache` 持八叉树）、GPU 渲染器（5 个 `IRenderer`）、策略开关（`useShadow` 等） | **致命** |
| 4 | `Engine/render/opengl/glmesh.h:42-44` | `GLMesh` = 3 个 VAO + 5 个 VBO + EBO，顶点布局硬编码为 `Main/Shadow/Light` 三种。加一种顶点格式必须改这个类 | **严重**（扩展性） |
| 5 | `Engine/render/opengl/mesh_data.h` | 固定 4 个属性（vertices/uvws/aos/normals）+ `vector<int>` 索引，无 AABB、无布局标识 | **严重**（扩展性） |
| 6 | `Engine/render/opengl/renderer/renderer.h:9` | `IRenderer::Render(const GLMesh&)` —— **抽象层次错了**：它是「单次 draw call 的包装」，不是渲染抽象。没有 Pass / DrawList 的概念 | **严重** |
| 7 | `Engine/render/system/render_system.cpp:98-103, 137-140` | 直接改 `TextureArrayLibrary` 拥有的纹理的 `GL_TEXTURE_MIN_FILTER`（采样状态被当成全局变量用，且改完不恢复） | **严重**（实际 bug） |
| 8 | `Engine/window/window.h:2-4` / `window.cpp:30,259-263,281-283,364-376` | 窗口类内含 glad + GLFW，直接 `glViewport`/`glClearColor`/`glClear`/`glfwSwapBuffers`；resize 回调手工 fan-out 给 5 个 renderer + `FrameBufferLibrary` | **严重** |
| 9 | `Engine/render/opengl/library/` 全部 | 字符串键单例（`"MainFrame"`/`"BlockTextures"`/`"BaseShader"`），**构造函数里就在编译 shader / 建纹理**（要求 GL 上下文已就绪），静态初始化顺序脆弱 | 中 |
| 10 | `Engine/utils/loaders/model_loader.h:8` | 资源加载器依赖 `GLTextureArray*` —— 以后加模型加载会被 GL 绑住 | 中 |
| 11 | `Engine/ui/debug_ui/debug_ui_manager.h:4-5` | ImGui 后端硬编码 `imgui_impl_opengl3`，UI 层无法脱离 GL | 中 |
| 12 | `Engine/CMakeLists.txt:20-22` + `CMake/Dependencies.cmake:23-25` | `glm` 这个 INTERFACE target 把整个 `Linking/include` 暴露出去（里面有 glad/GLFW/imgui），且 `NovaEngineCore` PUBLIC 链接 `glfw glad glm imgui`。**所以任何人想 include glad 都能 include**，边界无法强制 | 中（根因之一） |

另有一个同源症状：`render_system.cpp:55, 113, 147` 里 `blockRenderer.Render(emptyMesh)` 传空网格，只是为了「顺便把 shader 和状态设置一次」。这是**接口缺了「Pass 开始」概念**的症状。

---

## 2. 这套结构为什么挡死了未来方向

| 未来需求 | 现在会被什么卡住 |
|---|---|
| **复杂可动模型加载** | ① `MeshData` 单索引列表，无子网格划分 → 多材质模型无处安放；② 顶点属性固定 4 个 → 切线/顶点色/UV1 加不进去；③ 材质靠 `ShaderLibrary` 里硬编码的 `AddShader("XxxShader", ...)` → 运行时按模型生成管线做不到；④ `ModelLoader` 依赖 `GLTextureArray` → 加载器无法离线程/离 GL |
| **骨骼动画** | ① 无 Buffer/UBO 抽象 → 骨骼矩阵没地方传；② 顶点格式写死在 `GLMesh` 的 `Main/Shadow/Light` 里 → jointIndex/jointWeight 加不进去；③ 每帧要更新的是实例级/骨骼级数据，现在只有 `GLMesh::position/rotation`，且 `BlockRenderer` 每 mesh 一次 `SetMat4("model", ...)` → 没有实例化路径 |
| **粒子** | ① 无实例化；② 无动态 Buffer（只有 `GL_STATIC_DRAW` 的 mesh 上传）；③ 无 Pass/排序策略扩展点，只能再往 `RenderSystem::Render` 里塞分支；④ 无材质抽象 → 每个粒子材质 = 又一个 `IRenderer` 子类 + `ShaderLibrary` 里再加一行 + `RenderContext` 里再加一个成员 |
| **UI** | ① `CommandList` 没有 `SetScissor` → 滚动列表/裁剪区做不了；② 没有正交/屏幕空间 Pass 的挂载点；③ `DebugUIManager` 把 `imgui_impl_opengl3` 焊死在 `ui/` 层，游戏 UI 想复用同一套提交路径做不到；④ 没有 `layerMask`/排序键 → UI 永远在最上层只能靠「最后调用」保证 |

一句话概括**当前模式的规模问题**：

> 现在「一个渲染特性 = 一个 `IRenderer` 子类 + 一个 `ShaderLibrary` 条目 + 一个 `RenderContext` 成员 + 一段 `RenderSystem::Render` 里的 gl 代码」。加第四个特性时 `RenderSystem::Render` 会变成 500 行状态机。

---

## 3. 本次范围：明确不做的事

| 暂不做 | 理由 |
|---|---|
| 帧图 / RenderGraph + 瞬态资源池 | 现在是固定 5 个 Pass、固定顺序、固定资源。手写一个有序列表 80 行就够，帧图是 400 行买 0 收益 |
| Material / ParameterBlock / shader 反射 | uniform 照旧按名字设，只是入口从 `shader_->SetVec3()` 变成 `cmd.SetShaderParam()` |
| `MeshData` 加 SubMesh / `std::byte` 交错块 | 保持现有 SoA 形状，只挪位置 + 改索引类型 |
| 实例化 / `BufferHandle` / `SetScissor` | 现在没有粒子、没有 UI 裁剪需求 |
| glTF / 骨骼 / 粒子 / 游戏 UI | 不在本次范围 |
| 多后端（D3D11/Vulkan）实现 | 只建立接口与目录约束，不写第二个实现 |

**净效果**：不需要写任何「为未来准备的空接口」。真正要做的是**搬迁 + 换类型 + 拆 target**。

---

## 4. 目标分层

核心原则：**按「依赖方向」切层，而不是按「功能」切目录。箭头只能向下。**

```
┌─────────────────────────────────────────────────────────────┐
│ core/  voxel/  utils/  ui/                                  │  ← 游戏逻辑：看不见图形 API
├─────────────────────────────────────────────────────────────┤
│ render/scene/    ChunkMeshRegistry / LodMeshRegistry        │  ← 纯数据 + 句柄
│                  RenderView                                  │
├─────────────────────────────────────────────────────────────┤
│ render/framework/  5 个 Pass + FrameRenderer + 资源句柄表     │  ← 没有任何 gl*
├─────────────────────────────────────────────────────────────┤
│ render/device/   IRenderDevice / ICommandList（纯虚接口）     │  ← 只有引擎自己的枚举
├─────────────────────────────────────────────────────────────┤
│ render/gl/       GLDevice / GLCommandList / GLMeshBuffers    │  ← ★ 全项目唯一能碰 gl 的地方
│                  GLResourceCache / GLContext / ImguiGLBackend│
├─────────────────────────────────────────────────────────────┤
│ render/core/     Camera / MeshData / Handle / 枚举            │  ← 只依赖 glm，被所有人依赖
└─────────────────────────────────────────────────────────────┘
```

注意 `render/core` 在**最底下**：它被所有人依赖，所以它绝不能反向依赖任何东西。这也是为什么 `Camera` 必须从 `render/opengl/` 挪出来 —— 它现在在 `opengl` 命名空间下，但 `camera.h` 只有 `glm/vec3.hpp` 和 `glm/mat4x4.hpp`，**一行 GL 都没有**，纯粹是放错柜子了。

### 最终不变式

> `Engine/core`、`Engine/voxel`、`Engine/utils`、`Engine/ui`、`Engine/window`、`Engine/render/system`、`Engine/render/framework` 这七个目录里，**一个 `gl*` 调用都没有、一行 `#include <glad/glad.h>` 都没有**。

---

## 5. 最小改动面清单

### 5.1 必须新增

| 文件 | 内容 | 估计行数 |
|---|---|---|
| `render/core/handles.h` | `MeshHandle`/`TextureHandle`/`ShaderHandle`/`RenderTargetHandle`（4 个就够） | ~40 |
| `render/core/enums.h` | `BlendMode`/`DepthTest`/`CullMode`/`LoadOp`/`Topology`，替换 `GL_ONE`/`GL_LESS`/`GL_BACK` | ~30 |
| `render/core/pipeline_state.h` | 见 [6.3](#63-pipelinestate-取代-irenderer-子类--这是减代码) | ~30 |
| `render/core/render_settings.h` | 从 `RenderContext` 剥出的策略开关 | ~25 |
| `render/scene/mesh_instance.h` | `{ MeshHandle mesh; glm::vec3 position, rotation; bool awake; }` | ~15 |
| `render/scene/chunk_mesh_registry.{h,cpp}` | 现在的 `ChunkMeshCache`，`GLMesh` → `MeshHandle` | ~150（搬迁） |
| `render/scene/lod_mesh_registry.{h,cpp}` | 现在的 `LodMeshCache`，同上 | ~80（搬迁） |
| `render/scene/render_view.h` | 见 [6.2](#62-rendercontext-一拆为三这是整个改造的支点) | ~30 |
| `render/device/render_device.h` | `IRenderDevice`，~10 个纯虚函数 | ~70 |
| `render/device/command_list.h` | `ICommandList`，~9 个纯虚函数 | ~60 |
| `render/framework/frame_renderer.{h,cpp}` | Pass 编排 | ~80 |
| `render/framework/passes/*.{h,cpp}` | 5 个 Pass | ~350 |
| `render/framework/render_resources.h` | 后端资源句柄表 | ~40 |
| `render/gl/gl_context.{h,cpp}` | `makeContextCurrent` + `gladLoadGLLoader` | ~40 |
| `render/gl/gl_device.{h,cpp}` | `IRenderDevice` 实现 + VAO 缓存 | ~250 |
| `render/gl/gl_command_list.{h,cpp}` | `ICommandList` 实现 | ~200 |
| `render/gl/gl_mesh_buffers.{h,cpp}` | 只持 VBO/EBO，不持 VAO | ~120 |
| `render/gl/gl_resource_cache.{h,cpp}` | 4 个 `*Library` 单例合并 | ~150 |
| `render/gl/gl_render_target.{h,cpp}` | 现在的 `FrameBuffer` | ~150（搬迁） |
| `render/gl/gl_shader.{h,cpp}` | 现在的 `Shader` + uniform 位置缓存 | ~250（搬迁） |
| `render/gl/gl_texture.{h,cpp}` | 现在的 `GLTexture`/`GLTextureArray` | ~150（搬迁） |
| `render/gl/imgui_gl_backend.{h,cpp}` | `ImGui_ImplOpenGL3_*` + `ImGui_ImplGlfw_*` | ~60 |
| `ui/ui_backend.h` | `IUiBackend` 纯虚接口 | ~20 |

合计约 **+1400 行**。

### 5.2 必须删除

| 目标 | 行数 |
|---|---|
| `render/opengl/renderer/` 5 个 `IRenderer` 子类 + 基类 | ~200 |
| `render/opengl/library/frame_buffer_library.{h,cpp}` | ~75 |
| `render/system/render_system.cpp` 里的 gl 演出（变成 Pass） | ~120 |

**净变化 ≈ +1000 行，但分布从「散落 6 个目录」收敛到「render/gl 一个目录」。**

### 5.3 必须修改

| 文件 | 改动 |
|---|---|
| `render/opengl/camera.{h,cpp}` | 移到 `render/core/`，命名空间 `render::opengl` → `render` |
| `render/opengl/mesh_data.h` | 移到 `render/core/`；`vector<int> indices` → `vector<uint32_t>` |
| `render/system/render_system.{h,cpp}` | 参数 `RenderContext&` → 产出 `RenderView`；`Render()` 拆到 Pass |
| `core/world/world.h` | 加 `DirtyRegionQueue dirtyRegions;` |
| `core/input/input_system.{h,cpp}` | 去掉 `RenderContext&`，改 `World&`（内部走 dirty 队列） |
| `utils/loaders/model_loader.h` | `const GLTextureArray*` → `int textureBaseIndex` |
| `window/window.{h,cpp}` | 去 glad；持 `unique_ptr<IRenderDevice>` |
| `ui/debug_ui/debug_ui_manager.{h,cpp}` | 依赖 `IUiBackend&`，只 include `imgui.h` |
| `CMakeLists.txt` / `CMake/Dependencies.cmake` | 见 [第 8 节](#8-cmake-收口) |

### 5.4 完全不动

- `render/utils/mesh_builder.{h,cpp}`、`render/utils/smooth_mesher.{h,cpp}`（已经是干净的 CPU 网格构建）
- `render/texture.{h,cpp}`（`TextureData` + `LoadTexture`）
- `collection/`、`voxel/`、`utils/`、`log/`、`core/concurrency/`
- 所有 shader glsl 文件
- `Tests/`（可新增测试，不改现有）

---

## 6. 关键设计

### 6.1 句柄类型

```cpp
// render/core/handles.h
template <class Tag> struct Handle {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
    bool operator==(const Handle&) const = default;
};

using MeshHandle         = Handle<struct MeshTag>;
using TextureHandle      = Handle<struct TextureTag>;
using ShaderHandle       = Handle<struct ShaderTag>;
using RenderTargetHandle = Handle<struct RenderTargetTag>;
```

收益：`MeshInstance`、`ChunkMeshRegistry`、`LodMeshRegistry` 都可以持句柄，**不需要认识 `GLuint`，也不需要 include glad**。字符串键（`"MainFrame"`、`"BlockTextures"`）退化成上层资产层的名字 → 句柄映射，不再散落在渲染代码里。

> 只保留 4 个句柄。`PipelineHandle`/`MaterialHandle`/`BufferHandle` 现在都不需要，等真的要数据化管线时再加。

### 6.2 `RenderContext` 一拆为三（整个改造的支点）

现在 `render_context.h` 一个 struct 干了三件事，拆开就解决大半问题：

```cpp
// ① render/core/render_settings.h —— 策略开关，纯 POD，谁都能改
struct RenderSettings {
    bool  useShadow    = true;
    bool  useLucency   = true;
    bool  useLod       = true;
    bool  debugUI      = true;

    float shadowOrthoSize   = 64.0f;
    int   shadowResolution  = 2048;
    float shadowDistance    = 128.0f;
    glm::vec3 lightDirection{0.3f, 0.8f, 0.5f};
};
```

```cpp
// ② render/scene/chunk_mesh_registry.h —— 现在的 ChunkMeshCache
//    只把 GLMesh 换成 MeshHandle（内部不再有 vao_/vbo_）
class ChunkMeshRegistry {
public:
    bool Exist(int x, int y, int z) const;
    const MeshInstance* FindMesh(int x, int y, int z) const;
    void FreeMesh(int x, int y, int z);
    void Clear();
    void SubmitMesh(int x, int y, int z, const MeshData& data);

    void RequireMeshUpdate(int x, int y, int z);      // 逻辑原样保留
    bool GetUpdateRequirement(glm::ivec3& out);

    const std::vector<MeshInstance>& GetAllMesh() const { return meshs_; }
    // ...
private:
    std::vector<MeshInstance> meshs_;                 // MeshHandle + position + awake
    std::vector<glm::ivec3>   needUpdate_;
    std::unordered_set<glm::ivec3> buildSet_;
    std::vector<size_t>       pool_;
    std::unordered_map<glm::vec3, size_t> indexMap_;
};
```

`LodMeshRegistry` 同理：`AdaptiveLodOctree<GLMesh, ...>` → `AdaptiveLodOctree<MeshInstance, ...>`（模板改动量为 0）。

```cpp
// ③ render/framework/render_resources.h —— 后端资源句柄表，只有 framework/gl 看得见
struct RenderResources {
    RenderTargetHandle main, light, shadow, oit;
    ShaderHandle baseShader, lightShader, shadowShader, postShader,
                 depthPrepassShader, accumPassShader;
    TextureHandle blockTextures;
    // ...
};
```

```cpp
// ④ render/scene/render_view.h —— 渲染层唯一的输入
struct RenderView {
    const Camera*            camera        = nullptr;
    const Camera*            shadowCamera  = nullptr;
    const RenderSettings*    settings      = nullptr;
    const ChunkMeshRegistry* chunkMesh     = nullptr;   // Default / Lucency 两层
    const LodMeshRegistry*   lodMesh       = nullptr;
    glm::mat4                shadowMatrix{1.0f};
};
```

`RenderView` 只持指针，**不做每帧拷贝** —— 现在 `ChunkMeshCache` 里可能有上千个 mesh，逐帧复制成 `RenderScene` 是不必要的开销。

### 6.3 `PipelineState` 取代 `IRenderer` 子类 —— 这是**减代码**

现在 5 个 `IRenderer` 子类，每个都做同样三件事：选 shader、设状态、绑资源、draw。而「设状态」那部分其实全在 `render_system.cpp:24-146` 里用裸 `gl*` 手写。合起来就是：

```cpp
// render/core/pipeline_state.h
struct PipelineState {
    ShaderHandle shader;
    BlendMode    blend        = BlendMode::Opaque;
    DepthTest    depthTest    = DepthTest::Less;
    bool         depthWrite   = true;
    CullMode     cull         = CullMode::Back;
    uint32_t     colorTargets = 1;    // WBOIT 需要 2，这是现有需求不是预留
    uint64_t     vertexLayout = 0;    // 见 6.4
};
```

各 pass 的取值（对应现有行为）：

| Pass | blend | depthTest | depthWrite | cull | colorTargets |
|---|---|---|---|---|---|
| Shadow | Opaque | Less | true | Back | 1 |
| Opaque | Opaque | Less | true | Back | 1 |
| Light | Opaque | Less | true | Back | 1 |
| Transparent(WBOIT) | Opaque（累加在 shader 里做） | LessEqual | true→false | Back | **2** |
| PostProcess | 关闭 | 无 | — | 无 | 1 |

然后：

```cpp
// render/framework/passes/shadow_pass.cpp —— 取代 render_system.cpp:32-46
void ShadowPass::Record(const RenderView& v, ICommandList& cmd) {
    if (!v.settings->useShadow) return;

    cmd.BeginRenderTarget(res_.shadow, LoadOp::Clear, {1.0f, 1.0f, 1.0f, 1.0f});
    cmd.SetViewport({0, 0, res_.shadowSize, res_.shadowSize});
    cmd.SetPipelineState(pipeline_);

    for (const auto& m : *v.chunkMesh->Layer(MeshLayer::Default)) {
        if (!m.awake) continue;
        cmd.DrawMesh(m.mesh, v.shadowCamera->ViewProj(m));
    }

    cmd.EndRenderTarget();
}
```

`IRenderer` 那 5 个类连同基类一起删掉。**新代码比旧代码少**，同时 `lightDir`、`shadowOrthoSize` 这些从硬编码变成 `RenderSettings` 的字段。

> 注意：`PipelineState` 是**现在就需要**的结构，不是预留。因为 blend/depth/cull 这几个状态现在就在 pass 之间变来变去（见 `render_system.cpp:24-26, 36-38, 52-54, 77-79, 144-146`），把它们打包成一个值比散成 8 次 `glEnable`/`glDisable` 调用**更简单**，而不是更复杂。

### 6.4 顶点布局：唯一需要「预留」的地方（用最便宜的版本）

现在 `glmesh.h:9-11, 42-44` 里，`GLMesh` **预先创建 3 个 VAO**，对应 `VaoType::{Main, Shadow, Light}` 三种写死的顶点布局。这意味着：

- 网格对象必须知道**未来会有哪几个 shader**；
- 加第 4 个 shader 用不同属性 → 必须改 `GLMesh`；
- 这个信息本来就属于 **Pass/Pipeline**，不属于 **Mesh**。

**最便宜的修法（约 40 行）**：`GLMesh` 只持有 VBO/EBO，不持有 VAO；VAO 由后端缓存，键是 `(meshId, pipeline.vertexLayout)`：

```cpp
// render/gl/gl_device.cpp
GLuint GLDevice::GetOrCreateVao(MeshHandle mesh, uint64_t layoutKey) {
    const uint64_t key = (uint64_t(mesh.id) << 16) | layoutKey;
    if (auto it = vaoCache_.find(key); it != vaoCache_.end())
        return it->second;

    SetUpVao(key, mesh, layoutKey);   // 今天 layoutKey 恒为 0，只有一种属性排布
    return vaoCache_[key];
}
```

今天 `vertexLayout` 始终是 `0`：

```cpp
constexpr uint64_t kVoxelLayout = 0;
```

**未来**要加骨骼/粒子格式时，`layoutKey` 自然长成一个真正的描述符结构，而**此时一行都不用改**，只是 `kVoxelLayout` 旁边多了 `kSkinnedLayout`。

这就是「入口」的正确形态：**不是空接口，是一个已经正确归位的信息。**

> 如果连这 40 行都想省，可以推迟到 S5 再做 —— 但 `GLMesh` 里那 3 个 VAO 是现有代码的**实际缺陷**（不是预留），修它本身就有价值。
>
> 备选更省的做法：`GLMesh` 保留 VAO，但只保留**一个**，由 `RenderResources` 在初始化时告诉它「你需要为这几个 pipeline 建 VAO」。这比现状好，但不如后端缓存干净。

### 6.5 设备接口最小集

```cpp
// render/device/render_device.h
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    // 资源生命周期
    virtual MeshHandle         CreateMesh(const MeshData&) = 0;
    virtual void               UpdateMesh(MeshHandle, const MeshData&) = 0;
    virtual void               DestroyMesh(MeshHandle) = 0;

    virtual TextureHandle      CreateTexture(const TextureDesc&, const TextureData&) = 0;
    virtual TextureHandle      CreateTextureArray(const TextureArrayDesc&) = 0;
    virtual ShaderHandle       CreateShader(const ShaderDesc&) = 0;   // stages + defines
    virtual RenderTargetHandle CreateRenderTarget(const RenderTargetDesc&) = 0;

    // 帧
    virtual void          BeginFrame(const FrameInfo&) = 0;
    virtual ICommandList& Commands() = 0;
    virtual void          Present() = 0;
    virtual void          OnResize(uint32_t w, uint32_t h) = 0;   // ★ 取代 5 路手工 fan-out
    virtual RenderTargetHandle BackBuffer() const = 0;

    virtual Rect BackBufferSize() const = 0;
};
```

```cpp
// render/device/command_list.h
class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void BeginRenderTarget(RenderTargetHandle, LoadOp color, LoadOp depth,
                                   const ClearValue&) = 0;
    virtual void EndRenderTarget() = 0;

    virtual void SetViewport(const Rect&) = 0;
    virtual void SetPipelineState(const PipelineState&) = 0;
    virtual void SetShaderParam(std::string_view name, const ShaderParam&) = 0;
    virtual void BindTexture(std::string_view slot, TextureHandle,
                             uint32_t arrayIndex = 0) = 0;

    virtual void DrawMesh(MeshHandle, const glm::mat4& model) = 0;
};
```

**设计取舍说明：**

- 一共 **9 个虚函数**。`DrawMesh` 而不是通用的 `DrawIndexed(indexCount, firstIndex, instanceCount)`，因为现在没有多段索引、没有实例化。
- `SetShaderParam` 用 `std::variant<float, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4, int>` + 名字 —— 和现在的 `Shader::SetVec3("lightDir", ...)` **完全同构**，只是不再强制调用方 include glad。
- 这是「立即模式形状」的接口，**不是 Vulkan 形状**。对单人项目这是正确取舍：迁移成本接近零、没有额外间接层、将来上 D3D11 后端可平移。真正需要的是「调用里没有 `GLenum`、没有全局状态假设」，而不是「多线程录制命令缓冲」。

### 6.6 `ShaderDesc` / `TextureDesc` / `RenderTargetDesc`

```cpp
// render/core/shader_desc.h
enum class ShaderStage { Vertex, Fragment, Geometry, Compute };

struct ShaderStageDesc { ShaderStage stage; std::string path; };
struct ShaderDesc {
    std::vector<ShaderStageDesc> stages;
    std::vector<std::string>     defines;
};
```

```cpp
// render/core/texture_desc.h
enum class TextureFormat : uint8_t {
    RGBA8, RGB16F, RGBA16F, R16F, Depth32F, Depth24Stencil8
};
enum class TextureFilter : uint8_t { Nearest, Linear, NearestMipmapLinear, LinearMipmapLinear };
enum class TextureWrap   : uint8_t { Repeat, ClampToEdge, ClampToBorder };

struct TextureDesc {
    uint32_t width = 0, height = 0, layers = 1;
    TextureFormat format = TextureFormat::RGBA8;
    TextureFilter minFilter = TextureFilter::Linear;
    TextureFilter magFilter = TextureFilter::Linear;
    TextureWrap   wrap = TextureWrap::ClampToEdge;
    bool generateMipmaps = false;
    std::array<float, 4> borderColor{1, 1, 1, 1};
    bool depthCompareMode = false;         // ShadowMap 需要
};

struct TextureArrayDesc : TextureDesc {
    struct LayerSource { std::string name; std::string path; int index = 0; };
    std::vector<LayerSource> layerSources;
};
```

```cpp
// render/core/render_target_desc.h
struct RenderTargetAttachment {
    std::string     name;
    TextureFormat   format;
    bool            isDepth;
    uint32_t        index;        // color attachment index
    uint32_t        fixedWidth = 0, fixedHeight = 0;   // 0 = 跟随窗口
};

struct RenderTargetDesc {
    std::vector<RenderTargetAttachment> attachments;
};
```

> **注意**：`minFilter`/`magFilter` 进 `TextureDesc` 正是为了修掉第 11 节的 bug 1。

---

## 7. 依赖方向修正

### 7.1 `InputSystem`（致命项 #1）

现在 `input_system.cpp:63-95` 是「玩家改了体素 → 直接调用渲染层的 `RequireMeshUpdate`」。按 README 里「数据与逻辑分离」的既有理念，正确做法是**写数据、由渲染侧来取**：

```cpp
// core/world/world.h —— 加一个纯数据队列
struct World {
    // ... 现有字段
    voxel::chunk::DirtyRegionQueue dirtyRegions;
};
```

```cpp
// core/input/input_system.cpp —— 只写数据，不认识渲染
void InputSystem::Update(double time, world::World& world) {
    // ... 移动、射线检测、放置/破坏方块（不变）

    if (state_.destroyBlock) {
        chunk->SetVoxel(...);
        world.dirtyRegions.MarkDirty(pos, /*radiusChunks=*/1);
        state_.destroyBlock = false;
    }
    if (state_.buildBlock) {
        chunkBuild->SetVoxel(...);
        world.dirtyRegions.MarkDirty(pos, /*radiusChunks=*/1);
        state_.buildBlock = false;
    }
}
```

```cpp
// render/scene/chunk_mesh_registry.cpp —— 渲染侧自己来取
void ChunkMeshRegistry::ConsumeDirty(const core::world::World& world) {
    for (const glm::ivec3& p : world.dirtyRegions.Drain()) {
        for (int nx = -1; nx <= 1; ++nx)
            for (int ny = -1; ny <= 1; ++ny)
                for (int nz = -1; nz <= 1; ++nz)
                    RequireMeshUpdate(p.x + nx * 2, p.y + ny * 2, p.z + nz * 2);
    }
}
```

`input_system.h` 的 `#include <render/opengl/context/render_context.h>` 换成 `#include <core/world/world.h>`（本来就有）。**这是本次改造里唯一一处真正的「架构方向修正」**，其余都是搬迁。

### 7.2 `ModelLoader`

```cpp
// 现在
static BlockMesh LoadBlockModelFromJsonPath(const char* path,
                                            const render::opengl::GLTextureArray* tex);

// 改成
static BlockMesh LoadBlockModelFromJsonPath(const char* path);
// QuadFace::uvs 里已经是 [0,1] 归一化坐标；
// 纹理索引由上层用 BlockLibrary / TextureArrayLibrary 的名字映射完成
```

### 7.3 `LodSystem`

```cpp
// 现在
void Update(opengl::context::RenderContext&, glm::vec3 playerPos);
void Subdivide(opengl::context::LodMeshCache&, const TerrainGenerator&, int, int, int, int, bool);

// 改成
void Update(render::scene::ChunkMeshRegistry&, render::scene::LodMeshRegistry&, glm::vec3 playerPos);
void Subdivide(render::scene::LodMeshRegistry&, const TerrainGenerator&, int, int, int, int, bool);
```

`lod_system.h` 的 `#include <render/opengl/context/render_context.h>` 删掉，换 `render/scene/*.h`。

### 7.4 `Window`

```cpp
// window/window.h —— 去 glad，甚至不暴露 GLFW 类型
#pragma once
#include <render/device/render_device.h>
#include <memory>

struct GLFWwindow;                     // 前置声明即可

namespace nova_engine::window {
    class Window {
    public:
        static Window& GetInstance() { static Window instance; return instance; }
        void Run();
    private:
        GLFWwindow* window_ = nullptr;
        std::unique_ptr<render::device::IRenderDevice> device_;
        // ...
    };
}
```

```cpp
// window/window.cpp
void Window::Init() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    // ...
    window_ = glfwCreateWindow(800, 600, "NovaEngine", nullptr, nullptr);

    device_ = render::gl::CreateDevice(window_);   // ★ 唯一碰 GL 的一行
    if (!device_) { /* 报错 */ }

    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*, int w, int h) {
        if (w == 0 || h == 0) return;
        Window::GetInstance().device_->OnResize(w, h);   // ★ 一行取代 7 行 fan-out
    });
    // 其余回调不变（纯 GLFW）
}

void Window::Render(double alpha) {
    auto& cmd = device_->Commands();

    render::scene::RenderView view = render::system::RenderSystem::GetInstance().BuildView(world);
    view.settings = &settings_;

    frameRenderer_.Execute(view, *device_, cmd);

    if (settings_.debugUI) {
        ui::debug_ui::DebugUIManager::GetInstance().BeginFrame();
        ui::debug_ui::DebugUIManager::GetInstance().EndFrame();   // 内部走 IUiBackend
    }

    device_->Present();
}
```

改造后 `window.cpp` **只剩 GLFW 调用，一个 `gl*` 都没有**。

---

## 8. CMake 收口

不做这步，「彻底」就守不住。

### 8.1 先修根因

`CMake/Dependencies.cmake:23-25` 里 `glm` 这个 INTERFACE target 把整个 `Linking/include` 暴露出去，里面有 glad/GLFW —— **所以现在任何文件都能随手 include `<glad/glad.h>`**。必须收窄：

```cmake
# CMake/Dependencies.cmake
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${NE_THIRD_PARTY_INCLUDE_DIR}/glm)   # ← 只给 glm

add_library(ne_gl_headers INTERFACE)                                          # ← 只有后端能链
target_include_directories(ne_gl_headers INTERFACE
    ${NE_THIRD_PARTY_INCLUDE_DIR}/glad
    ${NE_THIRD_PARTY_INCLUDE_DIR}/GLFW
    ${NE_THIRD_PARTY_INCLUDE_DIR}/khrplatform)

# ImGui 拆两半：核心 + 后端
file(GLOB IMGUI_CORE_SOURCES ${NE_THIRD_PARTY_SRC_DIR}/imgui/*.cpp)
add_library(imgui_core STATIC ${IMGUI_CORE_SOURCES})
target_include_directories(imgui_core PUBLIC ${NE_THIRD_PARTY_INCLUDE_DIR}/imgui)

add_library(imgui_gl STATIC
    ${NE_THIRD_PARTY_SRC_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${NE_THIRD_PARTY_SRC_DIR}/imgui/backends/imgui_impl_glfw.cpp)
target_include_directories(imgui_gl PUBLIC ${NE_THIRD_PARTY_INCLUDE_DIR}/imgui/backends)
target_link_libraries(imgui_gl PUBLIC imgui_core glfw ne_gl_headers glad)
```

### 8.2 再拆 target

`Engine/CMakeLists.txt` 现在是 `GLOB_RECURSE` 全收 + PUBLIC 链 `glfw glad glm imgui`，必须拆：

| target | 目录 | 链接 |
|---|---|---|
| `NovaRenderCore` | `render/core/` `render/scene/` | `glm` |
| `NovaRenderBackend` | `render/device/`（纯头） | `NovaRenderCore` |
| `NovaRenderGL` | `render/gl/` | `NovaRenderBackend` `ne_gl_headers` `glad` `imgui_gl` |
| `NovaRenderFramework` | `render/framework/` | `NovaRenderCore` `NovaRenderBackend` |
| `NovaPlatform` | `window/` | `NovaRenderBackend` `glfw` |
| `NovaEngineCore` | `core/` `voxel/` `utils/` `log/` `ui/` | `NovaRenderCore` `glm` `spdlog` `json` `imgui_core` — **无 glad / glfw** |
| `NovaEngine` | `main.cpp` | 全部 |

```cmake
# Engine/CMakeLists.txt 骨架
add_library(NovaRenderCore STATIC ${RENDER_CORE_SOURCES})
target_link_libraries(NovaRenderCore PUBLIC glm)

add_library(NovaRenderBackend INTERFACE)
target_link_libraries(NovaRenderBackend INTERFACE NovaRenderCore)

add_library(NovaRenderGL STATIC ${RENDER_GL_SOURCES})
target_link_libraries(NovaRenderGL PUBLIC
    NovaRenderBackend ne_gl_headers glad imgui_gl)

add_library(NovaRenderFramework STATIC ${RENDER_FRAMEWORK_SOURCES})
target_link_libraries(NovaRenderFramework PUBLIC NovaRenderCore NovaRenderBackend)

add_library(NovaPlatform STATIC ${PLATFORM_SOURCES})
target_link_libraries(NovaPlatform PUBLIC NovaRenderBackend glfw)

add_library(NovaEngineCore STATIC ${ENGINE_CORE_SOURCES})
target_link_libraries(NovaEngineCore PUBLIC
    NovaRenderCore glm spdlog nlohmann imgui_core)   # ★ 无 glad / glfw
```

### 8.3 加分层守卫测试

这是让「彻底」可持续的关键 —— 半年后你（或任何人）想在 `render_system.cpp` 里再写一句 `glBindFramebuffer`，会**直接构建失败**。

```cmake
# cmake/CheckLayering.cmake
# 用法: cmake -DNE_ROOT=... -P cmake/CheckLayering.cmake

set(FORBIDDEN_DIRS
    "core" "voxel" "utils" "ui" "window"
    "render/system" "render/scene" "render/core" "render/framework"
)

set(VIOLATIONS "")

foreach(dir ${FORBIDDEN_DIRS})
    file(GLOB_RECURSE SOURCES
        "${NE_ROOT}/Engine/${dir}/*.cpp"
        "${NE_ROOT}/Engine/${dir}/*.h"
        "${NE_ROOT}/Engine/${dir}/*.tpp")

    foreach(src ${SOURCES})
        file(READ "${src}" CONTENT)

        # 1) 禁止包含 glad / GLFW
        if(CONTENT MATCHES "#[ \t]*include[ \t]*[<\"](glad|GLFW|khrplatform)/")
            list(APPEND VIOLATIONS "${src}: forbidden GL header include")
        endif()

        # 2) 禁止调用 gl 函数
        if(CONTENT MATCHES "(^|[^A-Za-z0-9_])gl[A-Z][A-Za-z0-9_]*[ \t]*\\(")
            list(APPEND VIOLATIONS "${src}: forbidden gl* call")
        endif()

        # 3) 禁止引用 GL_ 常量
        if(CONTENT MATCHES "(^|[^A-Za-z0-9_])GL_[A-Z0-9_]+")
            list(APPEND VIOLATIONS "${src}: forbidden GL_* constant")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    foreach(v ${VIOLATIONS})
        message(SEND_ERROR "${v}")
    endforeach()
    message(FATAL_ERROR "Layering check failed.")
else()
    message(STATUS "Layering check passed.")
endif()
```

```cmake
# 根 CMakeLists.txt 或 Tests/CMakeLists.txt
add_test(NAME layering_guard
         COMMAND ${CMAKE_COMMAND} -DNE_ROOT=${CMAKE_SOURCE_DIR}
                 -P ${CMAKE_SOURCE_DIR}/cmake/CheckLayering.cmake)
```

> 注意：守卫检查里的正则 `gl[A-Z]` 不会误伤 `glm::` 和 `glm/...`（因为 `m` 是小写），也不会误伤 `Google`（因为 `gl` 后必须跟大写字母 + `(`）。

---

## 9. 迁移步骤与验证不变式

每一步都**编译通过**，且都有**可自动验证的不变式**。

### S1 — 建 target + 挪文件（零行为变化）

**内容**
1. 建 `render/core/` 目录
2. `render/opengl/camera.{h,cpp}` → `render/core/camera.{h,cpp}`，命名空间 `render::opengl` → `render`
3. `render/opengl/mesh_data.h` → `render/core/mesh_data.h`，`vector<int> indices` → `vector<uint32_t>`
4. 新增 `handles.h` / `enums.h` / `pipeline_state.h` / `render_settings.h` / `shader_desc.h` / `texture_desc.h` / `render_target_desc.h`
5. `MeshLayer` / `LodDepth` 枚举从 `static_mesh.h` 全局命名空间收进 `render/core`，改 `enum class`
6. 拆 CMake target、拆 imgui target、收窄 `glm` 的 include
7. 加 `cmake/CheckLayering.cmake` + `add_test`（此时**允许多处失败**，可先只作为脚本手动跑）

**验证不变式**
- 画面与改造前**逐像素一致**
- `NovaRenderCore` 只链 `glm`
- `Tests/EngineCoreTests` 全部通过

**风险**：极低（纯搬迁）。

---

### S2 — 依赖方向掰正

**内容**
1. `core/world/world.h` 加 `DirtyRegionQueue dirtyRegions;`
2. 新增 `voxel/chunk/dirty_region_queue.{h,cpp}`（`MarkDirty(pos, radius)` / `Drain()`）
3. `InputSystem::Update(double, World&)` —— 去掉 `RenderContext&` 参数，改走 dirty 队列
4. `ChunkMeshRegistry::ConsumeDirty(const World&)` 消费队列
5. `ModelLoader::LoadBlockModelFromJsonPath(const char*)` —— 去掉 `GLTextureArray*`

**验证不变式**
```bash
grep -rE "glad|GLFW|gl[A-Z]" Engine/core Engine/voxel Engine/utils   # → 0 命中
```
- 放置/破坏方块后，区块网格仍能正确更新

**风险**：低。唯一要注意的是 `MarkDirty` 的 3×3×3 邻域语义要和现在 `input_system.cpp:66-70` 的 `pos + n*2` 循环一致。

---

### S3 — `RenderContext` 三分

**内容**
1. 新增 `render/scene/mesh_instance.h` / `chunk_mesh_registry.{h,cpp}` / `lod_mesh_registry.{h,cpp}` / `render_view.h`
2. `ChunkMeshCache` → `ChunkMeshRegistry`：`GLMesh` → `MeshHandle` + `MeshInstance`
3. `LodMeshCache` → `LodMeshRegistry`：`AdaptiveLodOctree<GLMesh,...>` → `AdaptiveLodOctree<MeshInstance,...>`
4. `StaticMesh` 并入 `ChunkMeshRegistry` 的 layer 数组
5. `RenderSystem::Render` 暂时保留但改成从 `RenderView` 读；`RenderSystem::Update` 改为 `BuildView(World&) → RenderView`
6. 补一个临时适配层：`GLMesh` 由 `MeshHandle` 索引的 `std::vector<GLMesh>` 持有，直到 S4

**验证不变式**
```bash
grep -E "gl[A-Z]|glad" Engine/render/system/render_system.cpp   # → 0 命中
```
- `Engine/render/system/` 可在一个不链 glad 的 target 里编译
- 画面一致

**风险**：中。这是第一次真正改数据结构，但改动是机械的（类型替换）。

---

### S4 — Device 接口 + GL 实现

**内容**
1. `render/device/render_device.h` / `command_list.h`（纯虚）
2. `render/gl/gl_context.{h,cpp}` —— `makeContextCurrent` + `gladLoadGLLoader`
3. `render/gl/gl_device.{h,cpp}` / `gl_command_list.{h,cpp}` / `gl_mesh_buffers.{h,cpp}` / `gl_render_target.{h,cpp}` / `gl_shader.{h,cpp}` / `gl_texture.{h,cpp}`
4. `GLMesh` → `GLMeshBuffers`：只持 VBO/EBO；VAO 进 `GLDevice::vaoCache_`，键 `(meshId, vertexLayout)`
5. 4 个 `*Library` 单例合并成 `GLResourceCache`：**构造期不建 GL 对象**，改显式 `Init(IRenderDevice&)`
6. `FrameBufferLibrary` 的硬编码 FBO 定义移到 `render/framework/render_resources.cpp`，用 `RenderTargetDesc` 声明
7. `GLShader` 在 link 后用 `glGetActiveUniform` 建 `name → GLint` 表

**验证不变式**
- 资源创建全部走 `IRenderDevice`
- `grep -E "gl[A-Z]|glad|GL_" Engine/render/framework` → 0 命中
- 无 GL 上下文时 `NovaRenderFramework` 可编译

**风险**：中高。这是本次改造里最重的一步，建议单独一个提交。

---

### S5 — Pass 拆分

**内容**
1. `render/framework/passes/`：`ShadowPass` / `OpaquePass` / `LightPass` / `TransparentPass` / `PostProcessPass`
2. `render/framework/frame_renderer.{h,cpp}`：固定顺序调用 5 个 pass
3. 删 `IRenderer` 体系（5 个子类 + 基类 + `renderer.h`）
4. 删 `FrameBufferLibrary`
5. `Window` 去 GL：`window.h` 不再 include glad；持 `unique_ptr<IRenderDevice>`
6. `glViewport`/`glClearColor`/`glClear`/`glfwSwapBuffers` → `device_->OnResize()` / `cmd.BeginRenderTarget(BackBuffer, Clear, ...)` / `device_->Present()`
7. `resize` 回调只调 `device_->OnResize(w, h)`

**验证不变式**
```bash
grep -rE "gl[A-Z]|glad|GLFW_" Engine/window Engine/render/system Engine/render/framework
# → 0 命中
```
```bash
grep -rlE "gl[A-Z]|glad" Engine --include=*.cpp --include=*.h
# → 只应出现 Engine/render/gl/** 和 Engine/render/gl/imgui_gl_backend.*
```
- 窗口 resize、F1~F4 开关、阴影开关、半透明开关全部正常
- 画面一致

**风险**：中。Pass 顺序必须与现在 `render_system.cpp:30-147` 严格一致。

---

### S6 — UI 后端注入

**内容**
1. `ui/ui_backend.h`：`IUiBackend { Init(void* window, const char* glslVersion); Shutdown(); NewFrame(); RenderDrawData(); }`
2. `render/gl/imgui_gl_backend.{h,cpp}`：实现 `IUiBackend`，内含 `ImGui_ImplOpenGL3_*` + `ImGui_ImplGlfw_*`
3. `DebugUIManager::Init(IUiBackend&)` —— `ui/debug_ui/` 只 include `imgui.h`，不 include 任何 `imgui_impl_*`
4. `Window::Init` 里构造 `ImguiGLBackend` 并注入

**验证不变式**
```bash
grep -r "imgui_impl" Engine/ui   # → 0 命中
```
- ImGui 调试面板正常显示
- 顺手修掉 [第 11 节](#11-顺手修掉的既有缺陷) 的 bug 1

**风险**：低。

---

### 建议节奏

- **S1–S3 收益最大、风险最低**（纯搬迁 + 类型替换，不改任何渲染结果），可以先只做这三步，此时 `core`/`voxel`/`utils` 已经完全看不见图形 API。
- **S4–S5 是真正的管线改写**，建议各自独立提交，做完 S5 才算「彻底」。
- S6 独立，随时可做。

---

## 10. 「入口」到底留在哪

**入口 ≠ 空接口 ≠ TODO 注释。** 真正的入口是三样东西：

1. **依赖方向正确** —— 未来的实现类只需要新增一个目录，不需要改动任何调用方
2. **数据结构可扩展** —— 未来加字段不破坏现有代码
3. **提交路径唯一收口** —— 未来加能力只需要扩展一个接口

| 未来升级 | 入口（改造后自然存在的东西） | 现在额外付出 |
|---|---|---|
| 换 D3D11 / Vulkan 后端 | `render/device/` 是纯虚接口，`render/gl/` 是唯一实现 | **0**（只是目录约束） |
| 复杂模型 / 骨骼 / 粒子 的顶点格式 | `PipelineState::vertexLayout` 已是值，后端按 `(mesh, layout)` 缓存 VAO | ~40 行（且同时修掉现有缺陷） |
| 材质 / 管线数据化 | `PipelineState` 已是结构体，只是构造点在 Pass 里；把它挪到数据里即可 | **0** |
| 动态数据（粒子、骨骼矩阵） | `SetShaderParam` 是 uniform 的唯一入口，将来在它旁边加 `SetUniformBuffer` 即可 | **0** |
| 多材质模型 | `MeshData` 是可扩展结构体，将来加 `std::vector<MeshRange>` 不破坏现有代码（`indices` 已改 `uint32_t`，偏移语义就位） | **0** |
| 帧图 / 自动 RT 池化 | Pass 已不拥有自己的 render target，只声明「我要写哪个 handle」 | **0** |
| 游戏 UI 复用同一提交路径 | `ICommandList` 是唯一提交路径；`IUiBackend` 已把 ImGui 核心与 GL 后端分开 | ~60 行 |
| 实例化（粒子） | `DrawMesh` 增加 `instanceCount` 参数 + `PipelineState` 增加 per-instance layout | **0**（签名扩展，调用方不变） |
| UI 裁剪 | `ICommandList` 增加 `SetScissor` | **0** |

---

## 11. 顺手修掉的既有缺陷

改造过程中这三个是「白捡」的，不额外花时间：

### bug 1：共享纹理的采样状态被当成全局变量

`render_system.cpp:98-103` 和 `:137-140`：

```cpp
// WBOIT pass 前
glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// ...
// post process pass 前
glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
```

`TextureArrayLibrary` 拥有的纹理被渲染流程直接改滤波状态，改完不恢复。这是把采样状态当全局变量用，会产生难查的画面闪烁。

**修法**：滤波状态进 `TextureDesc`（创建时设定），或进 `PipelineState` 的 sampler 描述。改造后问题自然消失。

### bug 2：uniform 位置每次设置都查询

`shader.cpp:13-17`：

```cpp
GLint Shader::GetVarLocation(const char* name) const {
    GLint loc = glGetUniformLocation(id_, name);
    assert(loc != -1 && "Could not find uniform variable in shader");
    return loc;
}
```

每帧每个 mesh 都要 `SetMat4("view", ...)` + `SetMat4("model", ...)`，每次都走一次字符串查找 + `glGetUniformLocation`。

**修法**：`GLShader` link 后用 `glGetActiveUniform` 遍历一次，建 `unordered_map<string, GLint>`（或 `string_view` + 哈希）。

### bug 3：索引类型语义不一致

`mesh_data.h:10` 是 `std::vector<int> indices`，但 `glmesh.cpp:148` 按 `GL_UNSIGNED_INT` 上传：

```cpp
glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(int), NULL, GL_STATIC_DRAW);
// ...
glDrawElements(GL_TRIANGLES, mesh.GetElementLength(), GL_UNSIGNED_INT, 0);
```

虽然 `sizeof(int) == sizeof(GLuint) == 4`，在目标平台上恰好等宽，但语义是错的。

**修法**：改 `std::vector<uint32_t>`。

### 附带清理

`render/opengl/context/static_mesh.h:7-13` 的 `MeshLayer` / `LodDepth` 两个枚举**定义在全局命名空间**（连 `nova_engine` 都没有）。顺手收进 `nova_engine::render` 并改 `enum class`。

---

## 12. 附录：改造前后文件树对照

### 改造前

```
Engine/
├── core/
│   ├── input/input_system.{h,cpp}          ← ❌ include render/opengl/context/render_context.h
│   └── world/world.h
├── window/window.{h,cpp}                   ← ❌ include glad + GLFW，直接调 gl*
├── ui/debug_ui/debug_ui_manager.{h,cpp}    ← ❌ include imgui_impl_opengl3.h
├── utils/loaders/model_loader.h            ← ❌ 依赖 GLTextureArray*
├── render/
│   ├── mesh_data.h                         ← 位置不对（在 opengl 下）
│   ├── texture.{h,cpp}                     ← ✅ 干净
│   ├── utils/mesh_builder.{h,cpp}          ← ✅ 干净
│   ├── utils/smooth_mesher.{h,cpp}         ← ✅ 干净
│   ├── system/
│   │   ├── render_system.{h,cpp}           ← ❌ ~30 处 gl*
│   │   └── lod_system.{h,cpp}              ← ❌ 依赖 opengl::context::LodMeshCache
│   └── opengl/                             ← ❌ 什么都往里塞
│       ├── glmesh.{h,cpp}                  ← GPU + 场景数据混合，3 个硬编码 VAO
│       ├── shader.{h,cpp}
│       ├── frame_buffer.{h,cpp}
│       ├── camera.{h,cpp}                  ← ✅ 其实一行 GL 都没有
│       ├── mesh_data.h
│       ├── texture/gltexture*.{h,cpp}
│       ├── library/*Library.{h,cpp}        ← 字符串键单例，构造期建 GL 对象
│       ├── renderer/IRenderer + 5 个子类   ← 抽象层次错误
│       ├── context/render_context.h        ← "上帝对象"
│       ├── context/{static,chunk_,lod_}mesh*
│       └── shaders/*.glsl
```

### 改造后

```
Engine/
├── core/                                   ← ✅ 零 GL
│   ├── input/input_system.{h,cpp}             只写 world.dirtyRegions
│   └── world/world.h                          + DirtyRegionQueue
├── window/window.{h,cpp}                   ← ✅ 只剩 GLFW
├── ui/                                     ← ✅ 只剩 ImGui 核心
│   ├── ui_backend.h                           IUiBackend 纯虚
│   └── debug_ui/debug_ui_manager.{h,cpp}
├── utils/loaders/model_loader.{h,cpp}      ← ✅ 无 GL 依赖
├── render/
│   ├── texture.{h,cpp}                     ← 不动
│   ├── utils/mesh_builder.{h,cpp}          ← 不动
│   ├── utils/smooth_mesher.{h,cpp}         ← 不动
│   ├── core/                               ← 🆕 只依赖 glm，被所有人依赖
│   │   ├── camera.{h,cpp}                     （从 opengl/ 迁入）
│   │   ├── mesh_data.h                        （从 opengl/ 迁入，uint32_t 索引）
│   │   ├── handles.h
│   │   ├── enums.h
│   │   ├── pipeline_state.h
│   │   ├── render_settings.h
│   │   ├── shader_desc.h
│   │   ├── texture_desc.h
│   │   └── render_target_desc.h
│   ├── scene/                              ← 🆕 纯数据 + 句柄
│   │   ├── mesh_instance.h
│   │   ├── chunk_mesh_registry.{h,cpp}        （原 ChunkMeshCache）
│   │   ├── lod_mesh_registry.{h,cpp}          （原 LodMeshCache）
│   │   └── render_view.h
│   ├── device/                             ← 🆕 纯虚接口
│   │   ├── render_device.h
│   │   └── command_list.h
│   ├── framework/                          ← 🆕 无 GL
│   │   ├── frame_renderer.{h,cpp}
│   │   ├── render_resources.h
│   │   └── passes/
│   │       ├── shadow_pass.{h,cpp}
│   │       ├── opaque_pass.{h,cpp}
│   │       ├── light_pass.{h,cpp}
│   │       ├── transparent_pass.{h,cpp}
│   │       └── post_process_pass.{h,cpp}
│   ├── system/                             ← ✅ 只剩逻辑
│   │   ├── render_system.{h,cpp}              产出 RenderView
│   │   └── lod_system.{h,cpp}
│   └── gl/                                 ← ★ 全项目唯一允许 gl* 的地方
│       ├── gl_context.{h,cpp}
│       ├── gl_device.{h,cpp}                  + VAO 缓存
│       ├── gl_command_list.{h,cpp}
│       ├── gl_mesh_buffers.{h,cpp}
│       ├── gl_render_target.{h,cpp}
│       ├── gl_shader.{h,cpp}                  + uniform 位置缓存
│       ├── gl_texture.{h,cpp}
│       ├── gl_resource_cache.{h,cpp}
│       ├── imgui_gl_backend.{h,cpp}
│       └── shaders/*.glsl
└── cmake/CheckLayering.cmake               ← 🆕 分层守卫（CTest）
```

---

## 结语

这次收窄后的方案不新增任何为未来准备的抽象，只做三件事：

1. **把 `render/core`（数据）和 `render/gl`（实现）拆到依赖链的两端**
2. **用 `MeshHandle` + `PipelineState` 替换掉高层代码里的 `GLMesh` + `GL_*` 常量**
3. **用拆 CMake target + 守卫测试把边界变成构建期约束**

预期 S1–S3 完成后，`core`/`voxel`/`utils` 就完全看不见图形 API 了，而画面不会有任何变化。
