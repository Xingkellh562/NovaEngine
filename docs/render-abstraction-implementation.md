# 渲染抽象：完整实现框架

> 配套文档：[渲染模块抽象方案](./render-abstraction.md)（诊断 + 分层 + 迁移步骤）
>
> 本文档给出**每个新文件的完整代码框架**与**每个设计决策的原理说明**。
>
> **本文档不修改任何现有代码。** 所有代码都是新增文件；需要改动的既有文件在第 6 节以「参考实现」形式给出，供你自行落刀。

---

## 目录

**第一部分：原理**

- [0. 分层原理与依赖规则](#0-分层原理与依赖规则)
  - [0.1 三条切分依据](#01-三条切分依据)
  - [0.2 依赖规则表](#02-依赖规则表)
  - [0.3 与初版方案的四处修正](#03-与初版方案的四处修正)

**第二部分：完整实现框架**

- [1. `render/core` —— 零依赖基础层](#1-rendercore--零依赖基础层)
- [2. `render/device` —— 纯虚接口层](#2-renderdevice--纯虚接口层)
- [3. `render/scene` —— 场景账本层](#3-renderscene--场景账本层)
- [4. `render/framework` —— 流程编排层](#4-renderframework--流程编排层)
- [5. `render/gl` —— 唯一后端](#5-rendergl--唯一后端)

**第三部分：接线与落地**

- [6. 需要修改的既有文件（参考实现）](#6-需要修改的既有文件参考实现)
- [7. 构建系统](#7-构建系统)
- [8. 设计决策的原理说明](#8-设计决策的原理说明)
- [9. 落地检查清单](#9-落地检查清单)

---

# 0. 分层原理与依赖规则

## 0.1 三条切分依据

现有代码把三样东西糊在了一起，抽象的第一步是**承认它们是三样东西**：

| 概念 | 回答的问题 | 变化频率 | 归属层 |
|---|---|---|---|
| **资源描述** | 「一个网格/纹理/管线长什么样」 | 低（加新特性时变） | `render/core` |
| **场景账本** | 「现在世界里有哪些东西要画、在哪」 | 每帧 | `render/scene` |
| **流程编排** | 「第几遍画、读写哪块显存、设什么状态」 | 低（加新 Pass 时变） | `render/framework` |
| **API 调用** | 「怎么把这个意图翻译成 GL 调用」 | 只在换后端时变 | `render/gl` |

**关键判据：如果一段代码在「换一个图形 API」时需要重写，它就必须在 `render/gl`；如果它只是「描述数据」，它就必须在 `render/core`。**

用这条判据检查现有代码：

- `GLMesh::GetModelMatrix()` 里 `glm::lookAt` + `glm::inverse` —— 换 API 不用重写 → 应该出 `render/gl`（现在没有，它和 `vao_` 挤在一个类里）
- `Camera` —— 换 API 不用重写 → 应该出 `render/opengl`（现在只是放错了命名空间）
- `MeshData` —— 换 API 不用重写 → 应该出 `render/core`（现在在 `render/opengl/` 下）
- `ChunkMeshCache` —— 它同时是「坐标→网格的账本」（不用重写）和「GLMesh 的容器」（要重写）→ **必须拆**

## 0.2 依赖规则表

```
┌────────────────────────────────────────────────────────────────────────┐
│  core/  voxel/  utils/  ui/  window/  render/system                     │  游戏逻辑
│  禁止：gl* 调用、glad/GLFW 头文件、GL_* 常量、GLuint 类型                 │
└───────────────────────────┬────────────────────────────────────────────┘
                            │ 只依赖 render/core、render/scene
┌───────────────────────────▼────────────────────────────────────────────┐
│  render/framework         Pass 编排 + 资源装配                          │
│  禁止：gl*、glad                                                     │
└───────┬───────────────────────────────────┬────────────────────────────┘
        │                                   │
┌───────▼──────────────────┐   ┌────────────▼───────────────────────────┐
│  render/scene            │   │  render/device   （纯虚接口）           │
│  账本 + 变换 + RenderView │──▶│  禁止：gl*、任何 GL 类型                │
└───────┬──────────────────┘   └────────────┬───────────────────────────┘
        │                                   │
┌───────▼───────────────────────────────────▼────────────────────────────┐
│  render/gl          全项目唯一允许出现 gl* 的地方                        │
│  实现 IRenderDevice / ICommandList，持有所有 GLuint                      │
└───────────────────────────┬────────────────────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────────────────────┐
│  render/core        Handle / 枚举 / MeshData / Camera / 各种 Desc        │
│  只依赖 glm。被所有人依赖，所以不能反向依赖任何东西                        │
└────────────────────────────────────────────────────────────────────────┘
```

**规则**：

1. `render/core` 只 include `glm` / 标准库。**它被所有人依赖，所以它是依赖图的叶子。**
2. `render/device` 只 include `render/core`。它是纯虚接口，没有实现。
3. `render/scene` include `render/core` + `render/device`（只用接口）。**它不 include `render/framework`，也不 include `render/gl`。**
4. `render/framework` include `core` + `device` + `scene`。
5. `render/gl` include `core` + `device`，**它不 include `scene` 和 `framework`** —— 后端不认识场景概念，只认识句柄。
6. `render/system` include `core` + `scene`。它产出 `RenderView`，不知道 `device` 的存在。
7. 游戏逻辑层 include `core` + `scene`，**永远不 include `device`**。

第 5 条和第 7 条是这个设计的两个硬约束：
- **后端不认识场景** → 换后端只重写 `render/gl`
- **游戏逻辑不认识设备** → 加渲染特性不改游戏逻辑

## 0.3 与初版方案的四处修正

写实现框架时暴露了四个初版方案的疏漏，这里修正（[render-abstraction.md](./render-abstraction.md) 的对应章节也应同步）：

| # | 初版方案 | 修正 | 原因 |
|---|---|---|---|
| 1 | `render/scene` 是「纯数据，不依赖 device」 | **`render/scene` 依赖 `render/device` 接口** | 账本要调 `device.UpdateMesh()` 才能上传网格。不注入 device 就得引入第二个「上传器接口」，反而更绕。依赖纯虚接口没有坏处 |
| 2 | `render/framework` 只有 Pass | **`render/framework` 还负责资源装配**（`CreateRenderResources`） | 「创建哪些着色器 / 渲染目标、各自什么格式」是**资产声明**，不是后端实现。放在 `framework` 才能让 `render/gl` 不认识具体资产 |
| 3 | `PipelineState` 有 7 个字段 | **加 `colorWrite`，`BlendMode` 加 `WboitAccum`** | WBOIT 深度预通道要 `glColorMask(FALSE)`；WBOIT 累加要按附件分别 `glBlendFunci`。这两个是**现有需求**，不是预留 |
| 4 | 纹理滤波状态「进 `TextureDesc`」 | **改为 `PipelineState::sampler`** | 现有代码里同一个方块纹理数组在透明通道要 `GL_LINEAR`、在后处理通道要 `GL_NEAREST_MIPMAP_LINEAR`。设进 `TextureDesc` 解决不了冲突，必须用采样器对象 |

这四处修正都不增加代码量，只是把信息放对了位置。

---

# 1. `render/core` —— 零依赖基础层

## 1.1 原理

这一层是**依赖图的叶子**。它被上面所有层 include，所以它的任何 include 都会传染。因此：

- **只 include `glm` 和标准库**
- **不出现任何 `GLuint` / `GLenum` / `GL_*`**
- **不承受任何平台知识**（窗口、交换链都不在这里）

它也不该承载逻辑。`Camera` 是个例外，因为它的成员函数全是纯数学（`glm::perspective` / `glm::lookAt`），没有状态、没有分支，放在这层最合适。

### 1.1.1 命名空间：为什么 `core` 层是扁平的

这是一个**必须提前定死**的决策 —— 写错会导致全项目编译错误，而且症状会出现在离原因很远的地方。

本方案的命名空间划分：

| 目录 | 命名空间 |
|---|---|
| `render/core/` | **`nova_engine::render`**（扁平） |
| `render/device/` | `nova_engine::render::device` |
| `render/scene/` | `nova_engine::render::scene` |
| `render/framework/` | `nova_engine::render::framework` |
| `render/gl/` | `nova_engine::render::gl` |

`core` 层刻意是扁平的 —— **不能**叫 `nova_engine::render::core`。原因是与现有模块的真实名字冲突：

`Engine/core/` 的命名空间是 `nova_engine::core`（`nova_engine::core::world::World`、`nova_engine::core::config::CHUNK_BIT1`、`nova_engine::core::concurrency::Worker`）。而 `ChunkMeshRegistry` 的实现里要同时用到两边：

```cpp
// Engine/render/scene/chunk_mesh_registry.cpp
namespace nova_engine::render::scene {

	void ChunkMeshRegistry::RequireMeshUpdate(int x, int y, int z) {
		const int bit = core::config::CHUNK_BIT1;   // ← 想要的是 nova_engine::core::config
		...
	}
}
```

C++ 的非限定名查找从最内层作用域往外走。一旦存在 `nova_engine::render::core`，在 `nova_engine::render::scene` 里写 `core::config` 会**先命中 `nova_engine::render::core`** —— 它里面没有 `config`，直接编译失败。更糟的情况是两边恰好有同名类型（比如都叫 `Handle`），那就会**静默解析到错误的类型**。

三个选项：

| 写法 | 结论 |
|---|---|
| `nova_engine::render::core::TextureDesc` | ❌ 与 `nova_engine::core` 冲突 |
| `nova_engine::render::TextureDesc`（**采用**） | ✅ 扁平，无冲突 |
| `nova_engine::render::base::TextureDesc` | ⚠️ 也可行（`base` 不与现有命名空间冲突），若更偏好「子命名空间统一」可选 |

选扁平的理由：`Handle`、`TextureDesc`、`PipelineState`、`MeshData`、`Camera` 这些类型几乎出现在每个函数签名里，`core::` 前缀**不携带信息**（include 路径 `<render/core/...>` 已经说明了层级），只增加噪声。README 中「按 `nova_engine::<模块>::<子模块>` 层级组织」的约定，对 `core` 这一层做一次**明确记录的例外**。

> **落地时的机械操作**：模块内部不要把 `nova_engine::render` 下的类型写成 `core::TextureDesc` / `core::MeshHandle` —— 直接写 `TextureDesc` / `MeshHandle`（在 `nova_engine::render` 及其子命名空间内），或从模块外引用时写 `render::TextureDesc`。
>
> 注意区分：`nova_engine::core::*`（`Engine/core/` 目录的模块）**保留** `core::` 前缀，它是另一个命名空间。

## 1.2 `Engine/render/core/handles.h`

**原理**：句柄是「高层系统与图形资源之间的唯一连接点」。它必须满足三个条件：可拷贝、可比较、**不泄露底层类型**。用 `uint32_t` + 标签类型而不是裸 `uint32_t`，是为了让编译器拦住「把 `TextureHandle` 传进 `DrawMesh`」这类错误 —— 零运行时开销的类型安全。

```cpp
#pragma once

#include <cstdint>

namespace nova_engine::render {

	// 通用句柄模板。
	// Tag 只用于类型区分，不参与运行时表示 —— sizeof(Handle<T>) == sizeof(uint32_t)。
	template <typename Tag>
	struct Handle {
		uint32_t id = 0;

		constexpr bool valid() const noexcept { return id != 0; }
		constexpr explicit operator bool() const noexcept { return valid(); }

		friend constexpr bool operator==(Handle, Handle) noexcept = default;
	};

	// 四种资源句柄。刻意不提供 PipelineHandle / MaterialHandle / BufferHandle ——
	// 现在没有对应需求，等真的需要时再加，避免出现永远为空的接口。
	using MeshHandle         = Handle<struct MeshTag>;
	using TextureHandle      = Handle<struct TextureTag>;
	using ShaderHandle       = Handle<struct ShaderTag>;
	using RenderTargetHandle = Handle<struct RenderTargetTag>;

} // namespace nova_engine::render
```

## 1.3 `Engine/render/core/enums.h`

**原理**：这些枚举的唯一职责是**把 `GL_*` 常量挡在高层之外**。它们的取值刻意与 GL 不一一对应（例如 `BlendMode::WboitAccum` 对应两次 `glBlendFunci` 调用），因为报出「我想要加法混合」这个**意图**比报出「我要 `GL_ONE, GL_ONE`」更稳定 —— 后者换到 D3D 就要重写调用点。

```cpp
#pragma once

#include <cstdint>

namespace nova_engine::render {

	// 混合模式。报「意图」而不是报「混合因子」：
	// 具体映射到几组 (srcFactor, dstFactor) 由后端决定。
	enum class BlendMode : uint8_t {
		Opaque,       // 关闭混合
		AlphaBlend,   // 常规 alpha：src = SRC_ALPHA, dst = ONE_MINUS_SRC_ALPHA
		Additive,     // src = ONE, dst = ONE
		WboitAccum,   // WBOIT 累加通道，按颜色附件分别设置：
		              //   attachment 0 (accum) : ONE, ONE
		              //   attachment 1 (reveal): ZERO, ONE_MINUS_SRC_COLOR
	};

	enum class DepthTest : uint8_t {
		None,         // 关闭深度测试
		Less,
		LessEqual,
		Greater,
		Always,
	};

	enum class CullMode : uint8_t {
		None,
		Front,
		Back,
	};

	// 渲染目标的加载行为
	enum class LoadOp : uint8_t {
		Load,         // 保留上一帧内容
		Clear,
		DontCare,     // 内容未定义（后端可自由优化，tile GPU 上能省一次带宽）
	};

	enum class Topology : uint8_t {
		TriangleList,
		LineList,
		PointList,
	};

} // namespace nova_engine::render
```

## 1.4 `Engine/render/core/sampler_state.h`

**原理**：修掉初版方案的第 4 个疏漏。

现有代码 `render_system.cpp:98-103` 和 `:137-140` 对**同一个**方块纹理数组设了两种不同的 `GL_TEXTURE_MIN_FILTER`：

- 透明通道要求 `GL_LINEAR`（`glm::length2` 距离排序后的 WBOIT 累加）
- 后处理通道要求 `GL_NEAREST_MIPMAP_LINEAR`

这是「同一个纹理在不同 Pass 需要不同采样状态」的**真实冲突**，把滤波设进 `TextureDesc` 解决不了。正确做法是采样器对象（GL 的 `glBindSampler` / D3D 的 `ID3D11SamplerState`）—— 一个 `SamplerState` 值就是一个采样器对象的 key。

```cpp
#pragma once

#include <cstdint>

namespace nova_engine::render {

	enum class TextureFilter : uint8_t {
		Nearest,
		Linear,
		NearestMipmapLinear,
		LinearMipmapLinear,
	};

	enum class TextureWrap : uint8_t {
		Repeat,
		ClampToEdge,
		ClampToBorder,
	};

	// 采样状态值。后端按这个值缓存采样器对象。
	// 它属于「管线状态」而不是「纹理属性」，因为同一个纹理在不同 Pass 下需要不同采样。
	struct SamplerState {
		TextureFilter minFilter = TextureFilter::Linear;
		TextureFilter magFilter = TextureFilter::Linear;
		TextureWrap   wrap      = TextureWrap::ClampToEdge;

		// ★ 深度比较模式必须在这里，不能只设在纹理上。
		//
		// GL 规范（3.3+）把 GL_TEXTURE_COMPARE_MODE / GL_TEXTURE_COMPARE_FUNC
		// 归为**采样器对象状态**，采样器绑定到纹理单元时会覆盖纹理自身的设置。
		//
		// 踩过的坑：ApplyPipelineState 对 unit 0~7 统一绑一个默认采样器，
		// 其 COMPARE_MODE == GL_NONE，把 ShadowMap 纹理的
		// GL_COMPARE_REF_TO_TEXTURE 覆盖掉了 —— 而 post_process_frag.glsl 里
		// 声明的是 uniform sampler2DShadow，compare mode 为 NONE 时
		// texture() 的返回值是**未定义的**。阴影静默失效，不报任何错。
		bool depthCompareMode = false;

		friend bool operator==(const SamplerState&, const SamplerState&) = default;
	};

} // namespace nova_engine::render
```

## 1.5 `Engine/render/core/pipeline_state.h`

**原理**：这是**取代 `IRenderer` 体系**的核心类型。

现有代码里「渲染状态」是散在 `render_system.cpp` 的 8 组 `glEnable`/`glDisable`/`glDepthFunc`/`glCullFace`/`glColorMask` 里的。把它们打包成一个值有三个好处：

1. Pass 里只剩一句 `cmd.SetPipelineState(...)`，可读性提升
2. 后端可以按这个值缓存状态对象（避免每帧几十次冗余状态切换）
3. **未来数据化管线时，这个结构体直接变成 `Material` 的成员，不需要改任何调用点**

`VertexLayoutId` 是唯一的「预留」，但如 [0.3](#03-与初版方案的四处修正) 所述，它在**今天就已经需要** —— 后处理的全屏四边形和体素网格的顶点属性排布不同。

```cpp
#pragma once

#include "enums.h"
#include "handles.h"
#include "sampler_state.h"

#include <cstdint>

namespace nova_engine::render {

	// 顶点属性排布的唯一标识。
	//
	// 现有代码把这个信息写死在 GLMesh 里（VaoType::Main / Shadow / Light 三个 VAO），
	// 导致「网格对象必须知道未来会有哪几个 shader」。正确归属是管线状态，不是网格。
	//
	// 这里的四个取值不是预留，每一个都对应一个现有的 glsl：
	//
	//   VoxelMain   -> vert.glsl
	//                  0=aPos(vec3) 1=aColor(vec3)=uvw 2=aAo(float) 3=aNormal(vec3)
	//   VoxelShadow -> para_vert.glsl
	//                  0=aPos(vec3)
	//   VoxelLight  -> light_vert.glsl
	//                  0=aPos(vec3) 1=aNormal(vec3) 2=aUv(vec3)
	//   ScreenQuad  -> post_process_vert.glsl
	//                  0=aPos(vec2) 1=aUV(vec2)
	//
	// 注意 VoxelLight 的 location 1 是 normal，而 VoxelMain 的 location 1 是 uvw ——
	// 两者的属性语义不同，所以**无法合并成一个布局**。这正是改动前 GLMesh 必须
	// 预建三个 VAO 的根本原因，也是这个枚举必须存在（而不是"未来才需要"）的证据。
	//
	// 未来加骨骼 / 粒子格式时，这里新增取值即可；
	// 后端按 (mesh, layout) 缓存 VAO 的逻辑与调用点都不需要改动。
	enum class VertexLayoutId : uint64_t {
		VoxelMain   = 0,
		VoxelShadow = 1,
		VoxelLight  = 2,
		ScreenQuad  = 3,
	};

	struct PipelineState {
		ShaderHandle   shader{};

		BlendMode      blend        = BlendMode::Opaque;
		SamplerState   sampler{};                    // 应用到本管线绑定的所有纹理单元

		DepthTest      depthTest    = DepthTest::Less;
		bool           depthWrite   = true;
		bool           colorWrite   = true;          // WBOIT 深度预通道需要 false

		CullMode       cull         = CullMode::Back;
		Topology       topology     = Topology::TriangleList;

		uint32_t       colorTargets = 1;             // WBOIT 需要 2
		VertexLayoutId vertexLayout = VertexLayoutId::VoxelMain;
	};

} // namespace nova_engine::render
```

## 1.6 `Engine/render/core/render_settings.h`

**原理**：从 `RenderContext` 里剥出的「策略开关」。

现有代码的 F1~F4 按键直接改 `RenderContext` 的成员（`window.cpp:74-97`），这是游戏逻辑伸手进渲染上下文的典型症状。把开关变成独立的 POD 之后，输入系统改的是设置值，渲染系统读的是同一份值，双方都不需要知道对方。

`clearColor` 的加入是必要的：现有代码用 `glClearColor(0.4, 0.5, 0.9, 1.0)` 在 `Window::Load()` 里设天空色，然后 `LightPass` 前保存/恢复（`render_system.cpp:73-92`）。改成「清屏色是场景设置的一部分」之后，那个保存/恢复 hack 自然消失。

```cpp
#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>

namespace nova_engine::render {

	// 渲染策略开关 —— 纯 POD，输入系统 / UI / 配置文件都能直接改。
	// 刻意不从 RenderContext 继承任何东西：它就是一个设置集合。
	struct RenderSettings {
		// 开关（对应现有 F1~F4）
		bool useShadow  = true;
		bool useLucency = true;
		bool useLod     = true;
		bool debugUI    = true;

		// 平行光阴影（现有硬编码在 render_system.cpp:254-258 的参数）
		float     shadowOrthoSize  = 64.0f;
		uint32_t  shadowResolution = 2048;
		float     shadowDistance   = 128.0f;
		glm::vec3 lightDirection{0.3f, 0.8f, 0.5f};

		// 天空色（现有硬编码在 window.cpp:282 的 glClearColor）
		glm::vec4 clearColor{0.4f, 0.5f, 0.9f, 1.0f};
	};

} // namespace nova_engine::render
```

## 1.7 `Engine/render/core/mesh_data.h`

**原理**：**保持现有 SoA 形状不变**，只做两件事：移到 `core/`、修正索引类型。

刻意不加 `SubMesh` / `VertexLayout` / AABB —— 那是「多材质模型」阶段的事。现在加进去只会让 mesher 被迫填一堆用不上的字段。

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace nova_engine::render {

	// 体素网格的 CPU 侧数据，SoA 排布。
	//
	// 与改动前的唯一区别：indices 从 std::vector<int> 改为 std::vector<uint32_t>。
	// 原因：glmesh.cpp:148 按 GL_UNSIGNED_INT 上传索引，用 int 声明虽然恰好等宽，
	// 但语义是错的。改成 uint32_t 后，将来加 SubMesh 时 offset 语义也就位了。
	struct MeshData {
		// ★ 这份数据是按哪个顶点布局排的。
		//
		// vertices / uvws / aos / normals 都是 std::vector<float>，
		// **每个顶点占几个 float 完全由消费它的 VertexLayoutId 决定** ——
		// 数据自己不携带这个信息。于是「数据元素大小」和「布局步长」
		// 是两个独立的事实，编译器无法校验它们一致。
		//
		// 不一致的症状极难查：改数据画面会变，但形状对不上，
		// 因为数据确实在被读，只是步长错了。
		// 带上这个字段后，CreateMesh / DrawMesh 至少能做一次检查。
		VertexLayoutId layout = VertexLayoutId::VoxelMain;

		std::vector<float>    vertices;   // xyz
		std::vector<float>    uvws;       // uvw（w 用于纹理数组层索引）
		std::vector<float>    aos;        // 环境光遮蔽
		std::vector<float>    normals;    // xyz
		std::vector<uint32_t> indices;

		bool isValid() const {
			return !vertices.empty() && !indices.empty();
		}
	};

} // namespace nova_engine::render
```

## 1.8 `Engine/render/core/transform.h`

**原理**：把 `GLMesh::GetModelMatrix(cameraPos)` 的数学**原样提取**出来。

这是一个典型的「因为住在 `GLMesh` 里所以被当成 GL 代码」的例子 —— 它其实只有 `glm::lookAt` 和 `glm::inverse`。提取之后，Pass 可以在完全不碰网格对象的情况下算出模型矩阵，这才让 `DrawMesh(handle, model)` 这个签名成立。

```cpp
#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace nova_engine::render {

	// 与改动前 GLMesh::GetModelMatrix(cameraPos) 逐字符等价的实现。
	//
	// position : 网格在世界空间的位置
	// rotation : 网格的朝向（现有代码用 lookAt 的目标点表达朝向）
	// origin   : 渲染相对原点，通常是主相机位置
	inline glm::mat4 MakeModelMatrix(const glm::vec3& position,
	                                 const glm::vec3& rotation,
	                                 const glm::vec3& origin) {
		const glm::vec3 fixedPos = position - origin;
		const glm::mat4 view = glm::lookAt(
			fixedPos, fixedPos + rotation, glm::vec3(0.0f, 1.0f, 0.0f));
		return glm::inverse(view);
	}

} // namespace nova_engine::render
```

## 1.9 `Engine/render/core/camera.h` / `camera.cpp`

**原理**：**从 `render/opengl/` 搬到 `render/core/`，函数体一字不改**，只改命名空间。

改前它的 include 只有 `glm/vec3.hpp` 和 `glm/mat4x4.hpp`，一行 GL 都没有。它被放在 `opengl` 命名空间下纯属历史原因，而这个位置让 `window` / `input` / `lod_system` 都不得不间接依赖 GL 头文件。

```cpp
#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace nova_engine::render {

	class Camera {
	public:
		Camera(float nearPlane = 0.1f,
		       float farPlane  = 5000.0f,
		       float fieldOfView = 3.14159265358979323846f / 3.0f)
			: nearPlane_(nearPlane), farPlane_(farPlane), fieldOfView_(fieldOfView) {}

		~Camera() = default;

		const glm::mat4 GetTransformMatrix(int width, int height) const {
			return glm::perspective(fieldOfView_, static_cast<float>(width) / height,
			                        nearPlane_, farPlane_);
		}
		const glm::mat4 GetViewMatrix() const {
			return glm::lookAt(position, position + rotation, GetWorldUp());
		}
		const glm::mat4 GetRelativeViewMatrix() const {
			return glm::lookAt(glm::vec3(0.0f), rotation, GetWorldUp());
		}
		const glm::mat4 GetRelativeViewMatrix(const glm::vec3& pos) const {
			return glm::lookAt(position - pos, position - pos + rotation, GetWorldUp());
		}
		const glm::mat4 GetOrthoMatrix(float halfSize) const {
			return glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 0.1f, 300.0f);
		}

		glm::vec3 GetWorldUp() const { return glm::vec3(0.0f, 1.0f, 0.0f); }

		glm::vec3 position{0.0f};
		glm::vec3 rotation{0.0f, 0.0f, 1.0f};

	private:
		float nearPlane_;
		float farPlane_;
		float fieldOfView_;
	};

} // namespace nova_engine::render
```

> `camera.cpp` 可以删除：全部函数已在头文件内联。函数体与改动前 `camera.cpp:5-19` 完全一致，只是改成 `const` + 命名空间变更。

## 1.10 `Engine/render/core/shader_desc.h`

**原理**：把「着色器」从 `Shader` 这个 **GL 对象**降级为一份**描述**。

现有 `ShaderLibrary` 的构造函数里直接 `AddShader("BaseShader", {{"shaders/vert.glsl", GL_VERTEX_SHADER}, ...})` —— 它同时做了三件事：声明资产、翻译成 GLenum、调用 GL 编译。拆开后「声明」留在 `ShaderDesc`（`core`），「翻译 + 编译」在 `render/gl`。

`defines` 字段现在没用到，但它不是预留 —— 它是 `#define` 列表的数据表示，任何时候你要给同一个 glsl 文件做两个变体（比如带/不带阴影），加的就是一个字符串。

```cpp
#pragma once

#include <string>
#include <vector>

namespace nova_engine::render {

	enum class ShaderStage {
		Vertex,
		Fragment,
		Geometry,
		Compute,
	};

	struct ShaderStageDesc {
		ShaderStage stage = ShaderStage::Vertex;
		std::string path;                 // 相对可执行文件的 glsl 路径
	};

	struct ShaderDesc {
		std::vector<ShaderStageDesc> stages;
		std::vector<std::string>     defines;   // 形如 "USE_SHADOW_MAP=1"
	};

} // namespace nova_engine::render
```

## 1.11 `Engine/render/core/texture_desc.h`

**原理**：把 `FrameBufferTexArgs`（`frame_buffer.h:9-16`，全是 `GLenum`）翻译成引擎枚举。

现有 `GLTextureArray(const char* path)` 从目录里读 PNG 并自己决定索引；`TextureArrayDesc::sources` 把「哪个文件对应哪一层」变成显式数据，这样 `BlockLibrary` 的方块名 → 纹理层索引映射才有依据。

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nova_engine::render {

	enum class TextureFormat : uint8_t {
		RGBA8,
		RGB16F,
		RGBA16F,
		R16F,
		Depth32F,
	};

	struct TextureDesc {
		uint32_t width  = 0;
		uint32_t height = 0;
		uint32_t layers = 1;                 // > 1 表示 2D 数组纹理

		TextureFormat format = TextureFormat::RGBA8;

		// 纹理自身的默认采样状态。
		// 注意：想按 Pass 改采样状态请用 PipelineState::sampler（采样器对象），
		// 不要回写到纹理上 —— 改动前 render_system.cpp:98-103 就是这么引入 bug 的。
		SamplerState defaultSampler{};

		bool generateMipmaps = false;

		// 深度比较模式（阴影贴图需要）
		bool  depthCompareMode = false;
		float borderColor[4]   = {1.0f, 1.0f, 1.0f, 1.0f};
	};

	// 2D 数组纹理的一层
	struct TextureArrayLayer {
		std::string name;                    // 方块名，用于建索引
		std::string path;                    // png 路径
		int         index = 0;               // 在数组中的层号
	};

	struct TextureArrayDesc : TextureDesc {
		// 单层边长。
		//
		// ⚠️ 0 表示「由第一张图决定」—— 但这个推导**必须由设备层实现**
		//    （见 gl_device.cpp 的 CreateTextureArray）。调用方不加载图片，
		//    不可能知道边长。
		//
		//    漏掉推导的后果极其隐蔽：
		//      glTexImage3D(..., 0, 0, layers, ...)  合法，不报错
		//      glTexSubImage3D(..., 0, 0, ...)       合法，不传输像素
		//    纹理数组一个像素都没有，所有采样变成未定义行为。
		uint32_t tileSize = 0;

		// ⚠️ layers 必须显式设置（TextureDesc 里默认是 1）。
		//    只填 sources 忘了填 layers 的话，glTexImage3D 只分配 1 层，
		//    且 CreateArray 的上传循环 i < layers 只处理第 0 层。
		//    设备层应当校验 layers == sources.size() 并在不符时告警。
		std::vector<TextureArrayLayer> sources;
	};

} // namespace nova_engine::render
```

## 1.12 `Engine/render/core/render_target_desc.h`

**原理**：取代 `FrameBufferLibrary` 构造函数里那段硬编码（`frame_buffer_library.cpp:5-42`）。

三个关键设计：

1. **`fixedWidth`/`fixedHeight` 为 0 表示跟随窗口** —— 这就是现有代码里 `lightFrame->AddTexture(..., 0, 0)` 注释说的「大小0,0表示跟随屏幕窗口大小」。改动前由 `Window::framebuffer_size_callback` 手工 fan-out 给 5 个 renderer + `FrameBufferLibrary`；现在由 `device->OnResize()` 一次处理。
2. **`imported` 支持复用已有纹理** —— WBOIT 的 `OITFrame` 需要把 `MainFrame` 的深度附件挂到自己身上（`semi_transparent_renderer.cpp:25-29` 的 `glFramebufferTexture2D`）。
3. **`name` 是查询键** —— 取代 `FindTexture("ColorTex")` 这类字符串查找，但只在初始化期用一次，句柄缓存进 `RenderResources`。

```cpp
#pragma once

#include "handles.h"
#include "texture_desc.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nova_engine::render {

	struct RenderTargetAttachment {
		std::string   name;

		TextureFormat format = TextureFormat::RGBA8;
		bool          isDepth = false;

		// 颜色附件序号（0,1,...）；深度附件忽略此字段
		uint32_t      index = 0;

		// 固定尺寸；两个都为 0 表示跟随窗口尺寸
		uint32_t      fixedWidth  = 0;
		uint32_t      fixedHeight = 0;

		// 非空表示复用已有纹理，不新建。
		// WBOIT 的 OITFrame 就是这样复用 MainFrame 的深度附件。
		TextureHandle imported{};
	};

	struct RenderTargetDesc {
		std::vector<RenderTargetAttachment> attachments;
	};

} // namespace nova_engine::render
```

## 1.13 `Engine/render/core/shader_param.h`

**原理**：让 `SetShaderParam` 的签名不依赖 GL。

现有 `Shader::SetVec3("lightDir", value)` 的调用点其实已经和 GL 无关了 —— 唯一的问题是 `Shader` 类本身 include 了 glad。用一个 `std::variant` 承载所有参数类型，调用点保持「名字 + 值」的形状，一行都不用改。

```cpp
#pragma once

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <variant>

namespace nova_engine::render {

	// uniform 参数的取值集合。
	// 与改动前 Shader::SetXxx 系列方法一一对应，只是不再需要调用方 include glad。
	using ShaderParam = std::variant<
		int,
		float,
		glm::vec2,
		glm::vec3,
		glm::vec4,
		glm::mat3,
		glm::mat4
	>;

} // namespace nova_engine::render
```

---

# 2. `render/device` —— 纯虚接口层

## 2.1 原理

这一层只有两个类，但它承担了整个改造的**承重职责**：它是「高层意图」和「底层 API」之间唯一的膜。

### 为什么是「立即模式」而不是「命令缓冲」

一个常见的误解是「抽象图形 API 就要做成 Vulkan 那样：录制命令缓冲、显式 barrier、描述符集」。这是**照抄了某个后端的形状，而不是抽出所有后端的共性**。

实际的取舍：

| 方案 | 迁移现有代码的成本 | 未来换 D3D11 | 未来换 Vulkan | 额外间接层 |
|---|---|---|---|---|
| 立即模式接口（本方案） | 极低 | 可平移 | 需要重做 | 0 |
| 命令缓冲 + 帧图 | 高 | 可平移 | 可平移 | 1 层 |

对单人项目，**用 20% 的成本拿到 80% 的收益**是正确取舍。真正需要被抽象掉的是三样东西：

1. **调用里不出现 `GLenum` / `GLuint`** —— 高层不知道「纹理单元」是 GL 的概念
2. **不依赖全局状态机** —— `BeginRenderTarget` / `EndRenderTarget` 成对出现，而不是散落的 `glBindFramebuffer`
3. **资源生命周期显式** —— `CreateXxx` / `DestroyXxx`，而不是靠析构函数里偷偷调 GL

这三条都做到了，就已经「彻底抽离」了。命令缓冲和多线程录制是**性能优化**，不是抽象 —— 现在没有这个性能需求。

### 为什么用 `DrawMesh(handle, model)` 而不是 `DrawIndexed(...)`

`DrawIndexed(indexCount, firstIndex, instanceCount)` 是**后端的形状**。`DrawMesh(MeshHandle, model)` 是**使用者的意图**：「把那个网格画在这个变换上」。

现有代码里每个 Pass 都要写三行才能画一个网格（算 model、绑 VAO、glDrawElements），还要自己传 `GetElementLength()`。改成 `DrawMesh` 之后，索引数量、VAO 选择、索引缓冲绑定全部由后端处理，Pass 从 3 行变成 1 行。

未来加实例化时，`DrawMesh` 增加一个可选参数即可，**所有现有调用点不用改**。

## 2.2 `Engine/render/device/render_device.h`

**原理**：

- **`OnResize` 是接口的一部分**，而不是上层的手工 fan-out。改动前 `Window::framebuffer_size_callback` 要通知 5 个 renderer + `FrameBufferLibrary`（`window.cpp:32-41`），任何新增的渲染目标都会漏掉。放进设备后，「谁需要重建」由后端自己知道。
- **句柄在 `OnResize` 后保持有效**。这是一条硬契约：`RenderResources` 只在初始化时查询一次附件纹理句柄并缓存，resize 时不能失效。后端只需重新分配存储。
- **`FullscreenQuad()` 由后端内建**。后处理的全屏四边形顶点布局（`VertexLayoutId::ScreenQuad`）和体素网格不同，如果让上层构造它，就得把 `MeshData` 改造成能表达两种布局 —— 为一个 6 顶点的四边形不值得。由后端持有一个内建网格，语义清晰、成本为零。
- **`GetAttachment` 存在的唯一理由是 WBOIT**：`OITFrame` 要把 `MainFrame` 的深度附件挂到自己身上（`semi_transparent_renderer.cpp:25-29`），后处理通道也要把这些附件当纹理读。它在初始化期用一次，结果缓存进 `RenderResources`。

```cpp
#pragma once

#include <render/core/handles.h>
#include <render/core/mesh_data.h>
#include <render/core/render_target_desc.h>
#include <render/core/shader_desc.h>
#include <render/core/texture_desc.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace nova_engine::render::device {

	class ICommandList;

	struct FrameInfo {
		uint32_t backBufferWidth  = 0;
		uint32_t backBufferHeight = 0;
		double   deltaSeconds     = 0.0;
	};

	// 渲染设备：全项目唯一的图形 API 收口。
	//
	// 契约：
	//  1. 所有 Create 返回的句柄在 Destroy 之前一直有效；
	//  2. OnResize 不改变任何句柄，只重新分配背后的存储；
	//  3. 所有方法只能在渲染线程调用。
	class IRenderDevice {
	public:
		virtual ~IRenderDevice() = default;

		// ---------------- 网格 ----------------
		virtual MeshHandle CreateMesh(const MeshData&) = 0;
		// 用新数据覆盖已有网格；传空 MeshData 表示释放其显存但保留句柄
		virtual void       UpdateMesh(MeshHandle, const MeshData&) = 0;
		virtual void       DestroyMesh(MeshHandle) = 0;

		// ---------------- 纹理 ----------------
		// pixels 的排布必须与 desc.format 一致；pixels 为空表示只分配不初始化
		virtual TextureHandle CreateTexture(const TextureDesc&,
		                                    std::span<const std::byte> pixels) = 0;
		// 按 desc.sources 里的路径逐个加载并上传
		virtual TextureHandle CreateTextureArray(const TextureArrayDesc&) = 0;
		virtual void          DestroyTexture(TextureHandle) = 0;

		// ---------------- 着色器 ----------------
		virtual ShaderHandle CreateShader(const ShaderDesc&) = 0;
		virtual void         DestroyShader(ShaderHandle) = 0;

		// ---------------- 渲染目标 ----------------
		virtual RenderTargetHandle CreateRenderTarget(const RenderTargetDesc&) = 0;
		virtual void               DestroyRenderTarget(RenderTargetHandle) = 0;

		// 查询渲染目标的某个附件纹理句柄。
		// 用途：① WBOIT 复用主帧深度；② 后处理通道把附件当纹理读。
		virtual TextureHandle GetAttachment(RenderTargetHandle,
		                                    std::string_view attachmentName) const = 0;

		// ---------------- 内建资源 ----------------
		// 后端自有的全屏四边形，顶点布局为 VertexLayoutId::ScreenQuad。
		virtual MeshHandle FullscreenQuad() const = 0;

		// ---------------- 帧 ----------------
		virtual void          BeginFrame(const FrameInfo&) = 0;
		virtual ICommandList& Commands() = 0;
		virtual void          Present() = 0;

		virtual void               OnResize(uint32_t width, uint32_t height) = 0;

		// ★ 必须返回一个**有效**句柄（id != 0）。
		//
		// 不能返回空句柄来代表"默认帧缓冲" —— 那样「默认帧缓冲」和
		// 「资源创建失败」就是同一个值，调用方的校验循环无法区分，
		// 会对 backBuffer 报出假的 creation FAILED。
		//
		// 正确做法：设备在初始化时给默认帧缓冲注册一个真实句柄，
		// 其内部 FBO 为 0（见 GLDevice 构造函数）。
		virtual RenderTargetHandle BackBuffer() const = 0;

		virtual const char* Name() const = 0;   // 例如 "OpenGL 4.5"
	};

} // namespace nova_engine::render::device
```

## 2.3 `Engine/render/device/command_list.h`

**原理**：

- **`BeginRenderTarget` / `EndRenderTarget` 成对出现**。改动前「当前绑定哪个 FBO」是隐式全局状态，`semi_transparent_renderer.cpp:47-48` 在渲染中途重新绑定 `MainFrame`，而 `render_system.cpp:111` 又绑回默认帧缓冲 —— 这类代码的阅读成本极高。成对 API 强制把「这一段画到哪」写清楚。
- **`ClearValue` 支持多颜色附件**。WBOIT 需要对 attachment 0 清 `(0,0,0,0)`、attachment 1 清 `(1,0,0,0)`（`render_system.cpp:106-110` 的 `glClearBufferfv`）。用一个固定大小数组表达，不引入动态分配。
- **`BindTexture(unit, sampler, handle)`**。`unit` 是设备无关的「纹理槽位号」（GL 的 texture unit、D3D 的 SRV slot 都是这个概念）；`sampler` 是 shader 里的 uniform 名字。这一句同时完成三件事：激活槽位、绑定纹理、设置 sampler uniform 指向该槽位 —— 正是改动前 `glActiveTexture` + `glBindTexture` + `SetTextureUnit` 三行代码做的事。
- **采样状态随管线走**（`PipelineState::sampler`），不随纹理走。原因见 [1.4](#14-engineerendercore_sampler_stateh)。

```cpp
#pragma once

#include <render/core/enums.h>
#include <render/core/handles.h>
#include <render/core/pipeline_state.h>
#include <render/core/shader_param.h>

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <string_view>

namespace nova_engine::render::device {

	struct Rect {
		int x = 0, y = 0;
		int width = 0, height = 0;
	};

	struct ClearValue {
		static constexpr uint32_t kMaxColorAttachments = 4;

		float color[kMaxColorAttachments][4] = {
			{0.0f, 0.0f, 0.0f, 1.0f},
			{1.0f, 0.0f, 0.0f, 0.0f},   // WBOIT reveal 的初值
			{0.0f, 0.0f, 0.0f, 1.0f},
			{0.0f, 0.0f, 0.0f, 1.0f},
		};
		float depth   = 1.0f;
		int   stencil = 0;
	};

	// 绘制命令列表。
	//
	// 这是全项目唯一的「提交路径」：任何东西要上屏，都必须经过这里。
	// 因此未来加 UI / 粒子 / 模型，只需要扩展这个接口，不需要新增旁路。
	class ICommandList {
	public:
		virtual ~ICommandList() = default;

		// ---------------- 渲染目标 ----------------
		virtual void BeginRenderTarget(RenderTargetHandle,
		                               LoadOp colorLoad, LoadOp depthLoad,
		                               const ClearValue&) = 0;
		virtual void EndRenderTarget() = 0;

		virtual void SetViewport(const Rect&) = 0;

		// ---------------- 管线与参数 ----------------
		// 设置完整管线状态（着色器 + 混合 + 深度 + 剔除 + 采样器 + 顶点布局）。
		virtual void SetPipelineState(const PipelineState&) = 0;

		// 设置 uniform。name 是 glsl 里的变量名。
		virtual void SetShaderParam(std::string_view name, const ShaderParam&) = 0;

		// 把纹理绑到槽位 unit，并让 sampler 这个 uniform 指向该槽位。
		//
		// samplerState 是**按单元**的采样状态覆盖点：
		//   nullptr（默认）→ 沿用 PipelineState::sampler
		//   非空           → 该单元用这个采样状态
		//
		// 为什么必须能按单元覆盖：一个 Pass 会同时绑定普通纹理和阴影贴图，
		// 阴影贴图所在单元必须开 depthCompareMode，其余单元必须关。
		// 采样器对象是 per-unit 的，PipelineState::sampler 一个值表达不了两种需求。
		virtual void BindTexture(uint32_t unit, std::string_view sampler,
		                         TextureHandle,
		                         const SamplerState* samplerState = nullptr) = 0;

		// ---------------- 绘制 ----------------
		// 用 model 变换绘制整个网格。当前管线状态的 vertexLayout 决定如何解释顶点。
		virtual void DrawMesh(MeshHandle, const glm::mat4& model) = 0;
	};

} // namespace nova_engine::render::device
```

---

# 3. `render/scene` —— 场景账本层

## 3.1 原理

这一层是**「世界里的东西」到「要画的东西」的翻译**。

改动前的结构问题是：`ChunkMeshCache` 同时是「坐标 → 网格」的账本（这部分与图形 API 无关，是纯逻辑）和「`GLMesh` 的容器」（这部分要重写）。拆开之后：

- **账本逻辑（分配槽位、空闲池、脏标记、邻域更新）原样保留** —— 它们本来就是对的
- **GPU 资源换成句柄** —— 账本不再认识 `GLuint`

### 为什么这一层依赖 `render/device` 接口

这是 [0.3](#03-与初版方案的四处修正) 的第 1 处修正。初版方案说「`render/scene` 是纯数据，不依赖 device」，但写实现时发现行不通：

`ChunkMeshRegistry::SubmitMesh(x, y, z, data)` 拿到 CPU 侧顶点数据后**必须**上传到 GPU（改动前是 `GLMesh::GetOrUpdateData` 里直接 `glBufferData`）。如果不给它 device，就必须再引入一个只含三个方法的「网格上传器」接口，然后让 `IRenderDevice` 去适配它 —— 多一个类型、多一层间接，换来的是**名义上的纯度**。

依赖一个纯虚接口没有任何坏处：`render/scene` 依然不知道 OpenGL 的存在，测试时也可以塞一个假设备进去。

### `MeshInstance` 为什么持有 device 反向指针

改动前 `GLMesh` 的析构函数会 `glDeleteVertexArrays` / `glDeleteBuffers`（`glmesh.cpp:20-22`），这是 RAII 的资源归还。`AdaptiveLodOctree` 内部用 `std::vector<std::unique_ptr<T>>` 存数据，LOD 细分/合并时会大量替换和销毁这些对象（`lod_mesh_cache.cpp:25-32, 66-77, 92-101`）——如果 `MeshInstance` 不负责归还，LOD 每帧的细分合并会持续泄漏 GPU 网格。

三个可选做法：

| 方案 | 代价 |
|---|---|
| 给 `AdaptiveLodOctree` 模板化 deleter | 需要改 `collection/data_structure/adaptive_lod_octree.h`，且模板签名变复杂 |
| 注册表登记所有句柄，析构时批量销毁 | **运行期泄漏无界**（每次细分合并都产生新网格句柄） |
| `MeshInstance` 持非拥有设备指针，析构时归还 | 结构体多一个指针字段 |

选第三个。它是唯一能保持 RAII 语义又不改动既有数据结构的做法。

## 3.2 `Engine/render/scene/mesh_instance.h`

```cpp
#pragma once

#include <render/core/handles.h>

#include <glm/vec3.hpp>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::scene {

	// 场景里的一个网格实例。
	//
	// 它同时承担两个角色：
	//   1. 「在哪里、朝向如何、是否可见」—— 场景数据（取代改动前 GLMesh 的 position/rotation/awake_）
	//   2. 「GPU 网格的所有者」—— RAII（取代改动前 GLMesh 的析构归还）
	//
	// owner 是非拥有指针，只在析构时用来归还 GPU 网格。
	struct MeshInstance {
		device::IRenderDevice* owner = nullptr;
		MeshHandle             mesh{};

		glm::vec3 position{0.0f};
		glm::vec3 rotation{0.0f, 0.0f, 1.0f};
		bool      awake = true;

		MeshInstance() = default;
		explicit MeshInstance(device::IRenderDevice& dev) : owner(&dev) {}

		~MeshInstance() {
			if (owner && mesh.valid()) {
				owner->DestroyMesh(mesh);
				mesh = {};
			}
		}

		MeshInstance(const MeshInstance&) = delete;
		MeshInstance& operator=(const MeshInstance&) = delete;

		MeshInstance(MeshInstance&& other) noexcept
			: owner(other.owner), mesh(other.mesh),
			  position(other.position), rotation(other.rotation), awake(other.awake) {
			other.owner = nullptr;
			other.mesh  = {};
		}
		MeshInstance& operator=(MeshInstance&& other) noexcept {
			if (this != &other) {
				if (owner && mesh.valid()) owner->DestroyMesh(mesh);
				owner     = other.owner;
				mesh      = other.mesh;
				position  = other.position;
				rotation  = other.rotation;
				awake     = other.awake;
				other.owner = nullptr;
				other.mesh  = {};
			}
			return *this;
		}

		void Sleep() noexcept { awake = false; }
		void Wake()  noexcept { awake = true; }
	};

} // namespace nova_engine::render::scene
```

## 3.3 `Engine/render/scene/chunk_mesh_registry.h` / `.cpp`

**原理**：这是 `ChunkMeshCache` 的**忠实移植**。逐条对照改动前的行为：

| 改动前 | 改动后 |
|---|---|
| `indexMap_: unordered_map<glm::vec3, size_t>` | 不变 |
| `meshs_: vector<GLMesh>` | `meshs_: vector<MeshInstance>` |
| `pool_` 空闲槽位池 | 不变 |
| `AcquireSlotInternal()` 空池则 `emplace_back`，否则复用 | 不变 |
| `GLMesh::GetOrUpdateData(data)` | `device_.CreateMesh` / `UpdateMesh` |
| `GLMesh::Sleep()` / `Wake()` | `MeshInstance::Sleep()` / `Wake()` |
| `FreeMesh` 只 `Sleep` + 入池，**不释放显存** | 不变（保持现有显存行为） |
| 空 `MeshData` → 释放缓冲 + 不 Wake（`glmesh.cpp:98-115`） | 不变（`UpdateMesh(handle, {})`） |

`MeshLayer` / `LodDepth` 两个枚举从 `static_mesh.h` 的**全局命名空间**收进 `nova_engine::render::scene` 并改为 `enum class`。

```cpp
#pragma once

#include <render/core/handles.h>
#include <render/core/mesh_data.h>

#include "mesh_instance.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::scene {

	// 网格分层。取代改动前 static_mesh.h 里定义在全局命名空间的 MeshLayer。
	enum class MeshLayer : uint32_t {
		Default = 0,
		Lucency = 1,
		Liquid  = 2,
		Lod     = 3,
		Count   = 4,
	};

	// 区块网格账本。
	//
	// 职责：维护「区块坐标 -> 网格实例」的映射、槽位池、脏区块队列。
	// 它不认识任何图形 API 类型，所有 GPU 操作都通过 IRenderDevice 接口完成。
	class ChunkMeshRegistry {
	public:
		explicit ChunkMeshRegistry(device::IRenderDevice& device)
			: device_(device) {}

		~ChunkMeshRegistry();

		ChunkMeshRegistry(const ChunkMeshRegistry&) = delete;
		ChunkMeshRegistry& operator=(const ChunkMeshRegistry&) = delete;

		// ---- 查询 ----
		bool                Exist(int x, int y, int z) const;
		const MeshInstance* FindMesh(int x, int y, int z) const;

		// ---- 生命周期 ----
		// 失活对应网格并把槽位入池。与改动前一致：不释放显存。
		void FreeMesh(int x, int y, int z);
		void Clear();

		// ---- 数据提交 ----
		// 内部自行判断是新建还是更新，并完成 GPU 上传。
		void SubmitMesh(int x, int y, int z, const MeshData& data);

		// ---- 脏区块队列 ----
		void RequireMeshUpdate(int x, int y, int z);
		bool GetUpdateRequirement(glm::ivec3& out);

		// ---- 渲染遍历 ----
		const std::vector<MeshInstance>& GetAllMesh() const { return meshs_; }

		int PoolLength() const { return static_cast<int>(pool_.size()); }
		int MeshCount()  const { return static_cast<int>(meshs_.size()); }

	private:
		device::IRenderDevice& device_;

		std::unordered_map<glm::vec3, std::size_t> indexMap_;
		std::vector<MeshInstance>                  meshs_;
		std::vector<glm::ivec3>                    needUpdate_;
		std::unordered_set<glm::ivec3>             buildSet_;
		std::vector<std::size_t>                   pool_;

		std::size_t AcquireSlotInternal();
	};

	// 分层容器。取代改动前 render/opengl/context/static_mesh.h 的 StaticMesh。
	class ChunkMeshSet {
	public:
		explicit ChunkMeshSet(device::IRenderDevice& device,
		                      uint32_t layerCount = static_cast<uint32_t>(MeshLayer::Count));

		ChunkMeshRegistry*       Layer(MeshLayer layer);
		const ChunkMeshRegistry* Layer(MeshLayer layer) const;

	private:
		std::vector<std::unique_ptr<ChunkMeshRegistry>> layers_;
	};

} // namespace nova_engine::render::scene
```

```cpp
// chunk_mesh_registry.cpp
#include "chunk_mesh_registry.h"

#include <render/device/render_device.h>

#include <core/config/config.h>
#include <log/logger.h>

namespace nova_engine::render::scene {

	namespace {
		// 区块坐标对齐：与改动前 chunk_mesh_cache.cpp:7-8, 25-27 的位移完全一致
		inline glm::vec3 MakeKey(int x, int y, int z) {
			const int bit = core::config::CHUNK_BIT1;
			return { static_cast<float>(x >> bit),
			         static_cast<float>(y >> bit),
			         static_cast<float>(z >> bit) };
		}
	} // namespace

	ChunkMeshRegistry::~ChunkMeshRegistry() {
		// MeshInstance 的析构会归还各自持有的 GPU 网格，这里无需额外处理。
		meshs_.clear();
	}

	bool ChunkMeshRegistry::Exist(int x, int y, int z) const {
		return indexMap_.find(MakeKey(x, y, z)) != indexMap_.end();
	}

	const MeshInstance* ChunkMeshRegistry::FindMesh(int x, int y, int z) const {
		auto it = indexMap_.find(MakeKey(x, y, z));
		return it == indexMap_.end() ? nullptr : &meshs_[it->second];
	}

	std::size_t ChunkMeshRegistry::AcquireSlotInternal() {
		if (pool_.empty()) {
			meshs_.emplace_back(device_);
			return meshs_.size() - 1;
		}
		const std::size_t idx = pool_.back();
		pool_.pop_back();
		meshs_[idx].Sleep();
		return idx;
	}

	void ChunkMeshRegistry::SubmitMesh(int x, int y, int z, const MeshData& data) {
		const int chunkBit  = core::config::CHUNK_BIT1;
		const int chunkMask = core::config::CHUNK_SIZE - 1;

		const glm::vec3 key = MakeKey(x, y, z);

		std::size_t idx = 0;
		if (auto it = indexMap_.find(key); it == indexMap_.end()) {
			idx = AcquireSlotInternal();
			indexMap_.emplace(key, idx);
		} else {
			idx = it->second;
		}

		if (idx >= meshs_.size()) {
			// 与改动前 chunk_mesh_cache.cpp:44-48 相同的边界检查
			NE_CORE_ERROR("ChunkMeshRegistry",
				"SubmitMesh: index {} out of bounds (size: {})", idx, meshs_.size());
			return;
		}

		MeshInstance& inst = meshs_[idx];

		// 与改动前 chunk_mesh_cache.cpp:43, 49-55 一致：位置按区块对齐，朝向固定
		inst.position = glm::vec3(static_cast<float>(x & ~chunkMask),
		                          static_cast<float>(y & ~chunkMask),
		                          static_cast<float>(z & ~chunkMask));
		inst.rotation = glm::vec3(0.0f, 0.0f, -1.0f);

		// 与改动前 GLMesh::GetOrUpdateData 的空网格分支一致（glmesh.cpp:98-115）：
		// 释放显存、置零索引数、保持休眠
		if (!data.isValid()) {
			if (inst.mesh.valid()) device_.UpdateMesh(inst.mesh, MeshData{});
			inst.Sleep();
			return;
		}

		if (!inst.mesh.valid()) {
			inst.mesh = device_.CreateMesh(data);
		} else {
			device_.UpdateMesh(inst.mesh, data);
		}
		inst.Wake();   // 对应改动前 GLMesh::GetOrUpdateData 结尾的 Wake()
	}

	void ChunkMeshRegistry::FreeMesh(int x, int y, int z) {
		const glm::vec3 key = MakeKey(x, y, z);

		auto it = indexMap_.find(key);
		if (it == indexMap_.end()) return;

		const std::size_t idx = it->second;
		if (idx >= meshs_.size()) return;

		meshs_[idx].Sleep();          // 失活，渲染遍历会跳过
		pool_.push_back(idx);
		indexMap_.erase(it);
	}

	void ChunkMeshRegistry::Clear() {
		indexMap_.clear();
		pool_.clear();
		for (std::size_t i = 0; i < meshs_.size(); ++i) {
			pool_.push_back(i);
			meshs_[i].Sleep();
		}
	}

	void ChunkMeshRegistry::RequireMeshUpdate(int x, int y, int z) {
		const int bit = core::config::CHUNK_BIT1;
		const glm::ivec3 pos = { (x >> bit) << bit, (y >> bit) << bit, (z >> bit) << bit };

		if (buildSet_.insert(pos).second) {
			needUpdate_.push_back(pos);
		}
	}

	bool ChunkMeshRegistry::GetUpdateRequirement(glm::ivec3& out) {
		if (needUpdate_.empty()) return false;
		out = needUpdate_.back();
		needUpdate_.pop_back();
		buildSet_.erase(out);
		return true;
	}

	// ---------------------------------------------------------------------------

	ChunkMeshSet::ChunkMeshSet(device::IRenderDevice& device, uint32_t layerCount) {
		for (uint32_t i = 0; i < layerCount; ++i) {
			layers_.push_back(std::make_unique<ChunkMeshRegistry>(device));
		}
	}

	ChunkMeshRegistry* ChunkMeshSet::Layer(MeshLayer layer) {
		const auto i = static_cast<std::size_t>(layer);
		return i < layers_.size() ? layers_[i].get() : nullptr;
	}

	const ChunkMeshRegistry* ChunkMeshSet::Layer(MeshLayer layer) const {
		const auto i = static_cast<std::size_t>(layer);
		return i < layers_.size() ? layers_[i].get() : nullptr;
	}

} // namespace nova_engine::render::scene
```

> **行为说明**：`RequireMeshUpdate` 的语义与改动前 `chunk_mesh_cache.h:34-48` **完全一致**（按 CHUNK_BIT1 对齐、`buildSet_` 去重、LIFO 弹出）。改动前 `FreeMesh` 里 `pool_.push_back(idx)` 之后没有去重，同一个槽位可能被重复入池，这里保持原样（属于既有行为，不在本次改造范围）。

## 3.4 `Engine/render/scene/lod_mesh_registry.h` / `.cpp`

**原理**：`LodMeshCache` 的忠实移植。核心改动只有一处：`AdaptiveLodOctree<GLMesh, ...>` → `AdaptiveLodOctree<MeshInstance, ...>`。

由于 `MeshInstance` 自带 RAII 归还，八叉树内部替换/销毁节点数据时（`adaptive_lod_octree.h:130, 137, 179, 186, 196`）会自动归还 GPU 网格 —— 与改动前 `unique_ptr<GLMesh>` 的行为一致。

需要注意 `MakeInstance` 辅助：每次创建 `MeshInstance` 都要 `make_unique` + `CreateMesh` + 设置位置朝向。改动前这段代码在 `TryEmplace` / `TrySubdivide` / `TryCombine` 里重复了三遍（`lod_mesh_cache.cpp:25-27, 66-73, 93-100`），这里提取成私有辅助函数。

```cpp
#pragma once

#include <render/core/handles.h>
#include <render/core/mesh_data.h>

#include "mesh_instance.h"

#include <collection/data_structure/adaptive_lod_octree.h>
#include <core/config/config.h>

#include <array>
#include <memory>
#include <ranges>
#include <unordered_map>

#include <glm/vec3.hpp>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::scene {

	// LOD 网格账本：以八叉树组织，支持细分（Subdivide）与合并（Combine）。
	//
	// 与改动前 LodMeshCache 的唯一区别：节点数据从 GLMesh 变为 MeshInstance。
	// 八叉树本身是模板，改动量为 0。
	class LodMeshRegistry {
	public:
		explicit LodMeshRegistry(device::IRenderDevice& device) : device_(device) {}
		~LodMeshRegistry() = default;

		LodMeshRegistry(const LodMeshRegistry&) = delete;
		LodMeshRegistry& operator=(const LodMeshRegistry&) = delete;

		MeshInstance* Find(int x, int y, int z, int* depth = nullptr) const;

		bool TryEmplace(int x, int y, int z, const MeshData& data);
		bool TrySubdivide(int x, int y, int z, int depth, std::array<MeshData*, 8> datas);
		bool TryCombine(int x, int y, int z, int depth, const MeshData* data);
		bool TryDeleteOctree(int x, int y, int z);

		// 扁平遍历视图：给渲染 Pass 用。与改动前 LodMeshCache::AllData() 等价。
		auto AllData() const {
			return datas_
				| std::views::values
				| std::views::transform(
					[](const Octree& octree) -> const auto& { return octree.Data(); })
				| std::views::join
				| std::views::filter([](const std::unique_ptr<MeshInstance>& p) {
					return p != nullptr; })
				| std::views::transform([](const std::unique_ptr<MeshInstance>& p)
					-> const MeshInstance& { return *p; });
		}

		const auto& AllOctree() const { return datas_; }

	private:
		using Octree = collection::data_structure::AdaptiveLodOctree<
			MeshInstance, core::config::OCTREE_SIZE, core::config::CHUNK_SIZE>;

		device::IRenderDevice& device_;
		std::unordered_map<glm::ivec3, Octree> datas_;

		glm::ivec3 GetKeyWithCoo(int x, int y, int z) const {
			const int bit = core::config::OCTREE_BIT1;
			return { x >> bit, y >> bit, z >> bit };
		}

		// 统一的实例构造：分配 MeshInstance + 上传 GPU 网格 + 设置位置朝向。
		// 改动前这段逻辑在三处重复（lod_mesh_cache.cpp:25-27, 66-73, 93-100）。
		std::unique_ptr<MeshInstance> MakeInstance(const MeshData& data,
		                                           const glm::vec3& position);
	};

} // namespace nova_engine::render::scene
```

```cpp
// lod_mesh_registry.cpp
#include "lod_mesh_registry.h"

#include <render/device/render_device.h>

namespace nova_engine::render::scene {

	std::unique_ptr<MeshInstance> LodMeshRegistry::MakeInstance(const MeshData& data,
	                                                            const glm::vec3& position) {
		auto inst = std::make_unique<MeshInstance>(device_);
		inst->mesh     = device_.CreateMesh(data);
		inst->position = position;
		inst->rotation = glm::vec3(0.0f, 0.0f, -1.0f);
		return inst;
	}

	MeshInstance* LodMeshRegistry::Find(int x, int y, int z, int* depth) const {
		const int mask = core::config::OCTREE_SIZE - 1;
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		auto it = datas_.find(key);
		if (it == datas_.end()) {
			if (depth) *depth = 0;
			return nullptr;
		}
		return it->second.Find(x & mask, y & mask, z & mask, depth);
	}

	bool LodMeshRegistry::TryEmplace(int x, int y, int z, const MeshData& data) {
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		auto it = datas_.find(key);
		if (it == datas_.end()) return false;

		const int mask = core::config::OCTREE_SIZE - 1;
		const glm::vec3 pos = glm::vec3(x & ~mask, y & ~mask, z & ~mask);

		if (data.vertices.empty()) {
			return it->second.TryEmplace(x & mask, y & mask, z & mask, nullptr);
		}
		return it->second.TryEmplace(x & mask, y & mask, z & mask,
		                             MakeInstance(data, pos));
	}

	bool LodMeshRegistry::TrySubdivide(int x, int y, int z, int depth,
	                                   std::array<MeshData*, 8> datas) {
		static const std::array<glm::ivec3, 8> kChildOffset = { {
			{0,0,0},{1,0,0},{0,1,0},{1,1,0},
			{0,0,1},{1,0,1},{0,1,1},{1,1,1}
		} };

		const int size = core::config::OCTREE_SIZE >> depth;
		const int mask = core::config::OCTREE_SIZE - 1;
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		const int px = x & mask, py = y & mask, pz = z & mask;
		const int ox = x & ~mask, oy = y & ~mask, oz = z & ~mask;

		std::array<std::unique_ptr<MeshInstance>, 8> instances = {};
		for (int i = 0; i < 8; ++i) {
			if (!datas[i]) continue;
			// 与改动前 lod_mesh_cache.cpp:69-73 的位置计算完全一致（注意运算符优先级）
			const glm::vec3 pos = glm::vec3(
				(px + kChildOffset[i].x * size + ox) & ~(size - 1),
				(py + kChildOffset[i].y * size + oy) & ~(size - 1),
				(pz + kChildOffset[i].z * size + oz) & ~(size - 1));
			instances[i] = MakeInstance(*datas[i], pos);
		}

		auto [it, inserted] = datas_.try_emplace(key);
		return it->second.TrySubdivide(px, py, pz, std::move(instances));
	}

	bool LodMeshRegistry::TryCombine(int x, int y, int z, int depth, const MeshData* data) {
		const int mask = core::config::OCTREE_SIZE - 1;
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);

		std::unique_ptr<MeshInstance> instance;
		if (data && !data->vertices.empty()) {
			instance = MakeInstance(*data, glm::vec3(x, y, z));
		}

		auto [it, inserted] = datas_.try_emplace(key);
		return it->second.TryCombine(x & mask, y & mask, z & mask, std::move(instance));
	}

	bool LodMeshRegistry::TryDeleteOctree(int x, int y, int z) {
		const glm::ivec3 key = GetKeyWithCoo(x, y, z);
		auto it = datas_.find(key);
		if (it != datas_.end()) return false;
		datas_.erase(key);
		return true;
	}

} // namespace nova_engine::render::scene
```

> **原样保留的两处既有怪异行为**（不在本次改造范围，但移植时必须一致，否则画面会变）：
> 1. `TryDeleteOctree` 的逻辑是反的（`it != end()` 时返回 false）—— `lod_system.cpp:116-130` 依赖了这个行为，改了会出问题。
> 2. `TrySubdivide` 里 `px + off.x * size + ox & ~(size - 1)` 依赖 C++ 运算符优先级（`&` 优先级低于 `+`），等价于 `(px + off.x*size + ox) & ~(size-1)`。上面显式加了括号，语义不变。

## 3.5 `Engine/render/scene/render_view.h`

**原理**：这是**高层与渲染层之间的唯一接口**。

设计要点：

- **只持指针，不做每帧拷贝**。`ChunkMeshSet` 里可能有上千个网格实例，逐帧复制成 `std::vector<MeshInstance>` 是纯浪费。
- **`relativeOrigin` 显式化**。现有代码用「相机位置」作为渲染相对原点（`GLMesh::GetModelMatrix(cameraPos)`），这个约定隐藏在每处调用里。显式放进 `RenderView` 之后，Pass 不需要再问相机要位置。
- **`shadowViewProj` 由 `RenderSystem` 算好**。改动前这个矩阵的计算散在 `render_system.cpp:252-275`（相机对齐到纹素）和 `parallel_light_renderer.cpp:38`（乘投影）两处。合并成一个矩阵后，阴影 Pass 只需一句 `SetShaderParam`。

```cpp
#pragma once

#include <render/core/camera.h>
#include <render/core/render_settings.h>

#include "chunk_mesh_registry.h"
#include "lod_mesh_registry.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

namespace nova_engine::render::scene {

	// 一帧渲染所需的全部输入。由 RenderSystem::BuildView 产出。
	//
	// 这是高层与渲染层之间唯一的桥梁：
	//   - 游戏逻辑 / RenderSystem 只负责填这个结构
	//   - Pass 只负责读这个结构
	// 双方都不需要知道对方的存在。
	struct RenderView {
		const Camera*        camera       = nullptr;
		const Camera*        shadowCamera = nullptr;
		const RenderSettings* settings    = nullptr;

		const ChunkMeshSet*   chunkMesh = nullptr;
		const LodMeshRegistry* lodMesh  = nullptr;

		// 渲染相对原点。
		//
		// 需要两个是**继承自现有行为**：改动前 BlockRenderer / LightRenderer /
		// SemiTransparentRenderer 用 MainCamera->position 作相对原点
		// （block_renderer.cpp:40、light_renderer.cpp:9、semi_transparent_renderer.cpp:63），
		// 而 ParallelLightRenderer 用的是 ShadowCamera->position
		// （parallel_light_renderer.cpp:39）。这是既有行为，平移时必须保持一致。
		glm::vec3 mainOrigin{0.0f};      // 主相机位置，用于 opaque / light / transparent
		glm::vec3 shadowOrigin{0.0f};    // 阴影相机位置，用于 shadow

		// 视口尺寸（原 MainFrame 的分辨率）
		uint32_t viewportWidth  = 0;
		uint32_t viewportHeight = 0;

		// 平行光阴影的 view-projection 矩阵。
		//
		// 需要两个，同样是**继承自现有行为**：改动前阴影 Pass 用
		//   GetOrthoMatrix(32) * shadowCamera->GetRelativeViewMatrix()
		// （parallel_light_renderer.cpp:19, 36-38），
		// 而后处理通道用
		//   GetOrthoMatrix(32) * shadowCamera->GetRelativeViewMatrix(mainCamera->position)
		// （post_process_renderer.cpp:58-60）。
		// 两者是不同的矩阵。移植时必须分别保留，否则阴影会错位。
		glm::mat4 shadowViewProj{1.0f};      // 阴影 Pass 使用
		glm::mat4 shadowSampleMatrix{1.0f};  // 后处理通道采样阴影使用

		// 是否已被 BuildView 填充。
		//
		// 存在的理由：默认构造的 RenderView 里 camera / settings / chunkMesh 全是 nullptr，
		// 任何 Pass 解引用都会读到 0 号地址附近（例如 settings->useLucency 在偏移 1，
		// 崩溃信息会是「读取位置 0x0000000000000001」）。
		// 首帧时序很容易让「未填充的 RenderView」流到 Render()，见 6.6。
		bool IsValid() const {
			return camera != nullptr
			    && shadowCamera != nullptr
			    && settings != nullptr
			    && chunkMesh != nullptr
			    && viewportWidth > 0
			    && viewportHeight > 0;
		}
	};

} // namespace nova_engine::render::scene
```

---

# 4. `render/framework` —— 流程编排层

## 4.1 原理

这一层回答两个问题：

1. **资源怎么装配**（`CreateRenderResources`）—— 「需要哪几个渲染目标、什么格式、哪些着色器」是**资产声明**，不是后端实现。放在这里，`render/gl` 才能做到「不认识具体资产」。
2. **一帧按什么顺序画**（`FrameRenderer` + 5 个 Pass）—— 这就是改动前 `render_system.cpp:30-147` 那 120 行 GL 演出的归属地，只是换成了不含 GL 的形式。

### 为什么不做帧图

初版方案里提过 `RenderGraph`，这里明确**不做**。理由是量化过的：

| | 手写 Pass 列表 | 帧图 |
|---|---|---|
| 代码量 | ~80 行 | ~400 行 |
| 自动 RT 池化 | ❌（但只有 4 个 RT，手工管理成本极低） | ✅ |
| 自动依赖排序 | ❌（5 个 Pass 顺序固定，不需要排序） | ✅ |
| 自动 resize | ✅（`device->OnResize` 已解决） | ✅ |
| 未来上 Vulkan 时自动插 barrier | ❌ | ✅ |

现在没有任何一条需求指向帧图。**但入口是留好了的**：Pass 不拥有自己的渲染目标，只在 `Record` 里使用 `RenderResources` 里的句柄 —— 将来要换成帧图的瞬态资源，只需把 `RenderResources` 的字段换成帧图的 `RGResourceHandle`，**Pass 的函数体一行都不用改**。

### 为什么 `RenderPass` 只有 `Record` 没有 `Setup`

帧图才需要 `Setup`/`Compile` 阶段来构建依赖图。没有帧图的情况下，多一个虚函数只会让每个 Pass 多写一个空实现。`Record(view, resources, cmd)` 三个参数已经完整描述了 Pass 的输入。

## 4.2 `Engine/render/framework/render_pass.h`

```cpp
#pragma once

#include <string_view>

namespace nova_engine::render::device { class ICommandList; }
namespace nova_engine::render::scene  { struct RenderView; }

namespace nova_engine::render::framework {

	struct RenderResources;

	// 一个渲染阶段。
	//
	// 契约：
	//   - Record 必须是纯「提交」逻辑：不做剔除、不做排序、不做 LOD 决策，
	//     那些属于 RenderSystem::BuildView（产出 RenderView 的那一步）。
	//   - Record 不拥有渲染目标，只使用 RenderResources 里的句柄。
	//     这条契约是将来替换为帧图瞬态资源的接口保证。
	class RenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual std::string_view Name() const = 0;

		// 返回 false 表示这一帧整个跳过（不绑定渲染目标、不清屏）。
		// 注意：与「执行清屏但跳过绘制」语义不同，具体语义由各 Pass 自行决定。
		virtual bool Enabled(const scene::RenderView&) const { return true; }

		virtual void Record(const scene::RenderView& view,
		                    const RenderResources& resources,
		                    device::ICommandList& cmd) = 0;

	protected:
		RenderPass() = default;
	};

} // namespace nova_engine::render::framework
```

## 4.3 `Engine/render/framework/render_resources.h`

**原理**：这是「句柄表」。

改动前的等价物是 `FrameBufferLibrary` + `ShaderLibrary` + `TextureArrayLibrary` + `CameraLibrary` 四个**字符串键的单例**，而且它们的构造函数里就直接调 GL。改成：

- **一个纯数据结构**，没有方法、没有单例、没有构造副作用
- 由 `CreateRenderResources(device, ...)` 一次性填好
- **所有句柄在 `device->OnResize()` 后依然有效**（接口契约，见 [2.2](#22-engine-renderdevicerender_deviceh)），所以 resize 时不需要重建这个结构

```cpp
#pragma once

#include <render/core/handles.h>
#include <render/core/pipeline_state.h>

#include <cstdint>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::framework {

	// 一帧渲染所需的全部后端资源句柄。
	//
	// 只有 framework / gl 两层看得见它。游戏逻辑和 scene 层都不应该知道它的存在。
	// 它是纯数据：没有构造副作用，没有单例，句柄在 OnResize 后保持有效。
	struct RenderResources {
		// ---- 渲染目标 ----
		RenderTargetHandle mainFrame{};
		RenderTargetHandle lightFrame{};
		RenderTargetHandle shadowFrame{};
		RenderTargetHandle oitFrame{};
		RenderTargetHandle backBuffer{};

		// ---- 附件纹理（供后处理通道采样）----
		TextureHandle mainColor{};
		TextureHandle mainDepth{};
		TextureHandle lightTex{};
		TextureHandle shadowMap{};
		TextureHandle accumTex{};
		TextureHandle revealTex{};

		// ---- 纹理数组 ----
		TextureHandle blockTextures{};

		// ---- 着色器 ----
		ShaderHandle baseShader{};
		ShaderHandle shadowShader{};
		ShaderHandle lightShader{};
		ShaderHandle depthPrepassShader{};
		ShaderHandle accumPassShader{};
		ShaderHandle postProcessShader{};

		// ---- 管线状态 ----
		PipelineState shadowPipeline{};
		PipelineState opaquePipeline{};
		PipelineState lightPipeline{};
		PipelineState wboitDepthPipeline{};
		PipelineState wboitAccumPipeline{};
		PipelineState postProcessPipeline{};

		// ---- 内建网格 ----
		MeshHandle fullscreenQuad{};

		// ---- 采样状态 ----
		//
		// 采样状态按用途在这里声明一次，由 Pass 在 BindTexture 时按单元传入。
		// 不要只写进 PipelineState::sampler —— 一个 Pass 会同时绑普通纹理和
		// 阴影贴图，两者需要相反的 depthCompareMode，一个值表达不了。
		SamplerState blockSampler{};    // 方块纹理数组：NearestMipmapLinear / Nearest / ClampToEdge
		SamplerState linearSampler{};   // 渲染目标纹理：Linear / Linear / ClampToEdge
		SamplerState shadowSampler{};   // 阴影贴图：Linear / Linear / ClampToEdge + depthCompareMode

		// ---- 尺寸 ----
		uint32_t shadowResolution = 2048;
	};

	// 一次性创建全部资源。在 GL 上下文就绪后调用。
	RenderResources CreateRenderResources(device::IRenderDevice& device,
	                                       uint32_t width, uint32_t height);

	// 释放全部资源。顺序与创建顺序相反。
	void DestroyRenderResources(device::IRenderDevice& device, RenderResources& res);

} // namespace nova_engine::render::framework
```

## 4.4 `Engine/render/framework/render_resources.cpp`

**原理**：这里逐条复刻 `frame_buffer_library.cpp:5-42` 的 FBO 定义和 `shader_library.cpp:5-29` 的着色器定义，只是换成了引擎枚举。

每个管线状态的取值都直接对应改动前的 GL 状态设置，注释里标出行号，便于逐条核对。

```cpp
#include "render_resources.h"

#include <render/device/render_device.h>

#include <log/logger.h>

namespace nova_engine::render::framework {

	using namespace nova_engine::render;

	namespace {

		// 呼应 gltexture_array.cpp:42-43 的纹理数组默认采样状态
		//   GL_TEXTURE_MIN_FILTER = GL_NEAREST_MIPMAP_LINEAR
		//   GL_TEXTURE_MAG_FILTER = GL_NEAREST
		SamplerState BlockArraySampler() {
			return SamplerState{ TextureFilter::NearestMipmapLinear,
			                     TextureFilter::Nearest,
			                     TextureWrap::ClampToEdge };
		}

		// 呼应 render_system.cpp:102 在 WBOIT 通道前改成的 GL_LINEAR
		SamplerState LinearSampler() {
			return SamplerState{ TextureFilter::Linear,
			                     TextureFilter::Linear,
			                     TextureWrap::ClampToEdge };
		}

		ShaderHandle MakeShader(device::IRenderDevice& device,
		                        std::initializer_list<ShaderStageDesc> stages) {
			ShaderDesc desc;
			desc.stages.assign(stages.begin(), stages.end());
			return device.CreateShader(desc);
		}

	} // namespace

	RenderResources CreateRenderResources(device::IRenderDevice& device,
	                                       uint32_t width, uint32_t height) {
		RenderResources res;

		// ================= 着色器 =================
		// 对应改动前 shader_library.cpp:6-28
		res.baseShader = MakeShader(device, {
			{ ShaderStage::Vertex,   "shaders/vert.glsl" },
			{ ShaderStage::Fragment, "shaders/frag.glsl" },
		});
		res.shadowShader = MakeShader(device, {
			{ ShaderStage::Vertex,   "shaders/para_vert.glsl" },
			{ ShaderStage::Fragment, "shaders/para_frag.glsl" },
		});
		res.postProcessShader = MakeShader(device, {
			{ ShaderStage::Vertex,   "shaders/post_process_vert.glsl" },
			{ ShaderStage::Fragment, "shaders/post_process_frag.glsl" },
		});
		res.depthPrepassShader = MakeShader(device, {
			{ ShaderStage::Vertex,   "shaders/depth_prepass_vert.glsl" },
			{ ShaderStage::Fragment, "shaders/depth_prepass_frag.glsl" },
		});
		res.accumPassShader = MakeShader(device, {
			{ ShaderStage::Vertex,   "shaders/accum_pass_vert.glsl" },
			{ ShaderStage::Fragment, "shaders/accum_pass_frag.glsl" },
		});
		res.lightShader = MakeShader(device, {
			{ ShaderStage::Vertex,   "shaders/light_vert.glsl" },
			{ ShaderStage::Fragment, "shaders/light_frag.glsl" },
		});

		// ================= 渲染目标 =================
		// 对应改动前 frame_buffer_library.cpp:6-41

		// MainFrame: ColorTex(GL_RGBA8) + DepthTex(GL_DEPTH_COMPONENT32F)，跟随窗口
		{
			RenderTargetDesc desc;
			desc.attachments.push_back({ "ColorTex", TextureFormat::RGBA8,  false, 0, 0, 0 });
			desc.attachments.push_back({ "DepthTex", TextureFormat::Depth32F, true, 0, 0, 0 });
			res.mainFrame = device.CreateRenderTarget(desc);
		}

		// LightFrame: LightTex(GL_RGB16F) + DepthTex(GL_DEPTH_COMPONENT32F)，跟随窗口
		{
			RenderTargetDesc desc;
			desc.attachments.push_back({ "LightTex", TextureFormat::RGB16F,  false, 0, 0, 0 });
			desc.attachments.push_back({ "DepthTex", TextureFormat::Depth32F, true, 0, 0, 0 });
			res.lightFrame = device.CreateRenderTarget(desc);
		}

		// ShadowFrame: ShadowMap(GL_DEPTH_COMPONENT32F)，固定 2048x2048
		// 深度比较模式 + 边框色 1.0，对应 frame_buffer_library.cpp:24-33
		{
			RenderTargetDesc desc;
			RenderTargetAttachment sm;
			sm.name = "ShadowMap";
			sm.format = TextureFormat::Depth32F;
			sm.isDepth = true;
			sm.fixedWidth = 2048;
			sm.fixedHeight = 2048;
			desc.attachments.push_back(sm);
			res.shadowFrame = device.CreateRenderTarget(desc);
			res.shadowResolution = 2048;
		}

		// OITFrame: AccumTex(GL_RGBA16F) + RevealTex(GL_R16F)，深度复用 MainFrame
		// 对应 semi_transparent_renderer.cpp:25-29 的 glFramebufferTexture2D
		{
			res.mainDepth = device.GetAttachment(res.mainFrame, "DepthTex");

			RenderTargetDesc desc;
			desc.attachments.push_back({ "AccumTex",  TextureFormat::RGBA16F, false, 0, 0, 0 });
			desc.attachments.push_back({ "RevealTex", TextureFormat::R16F,    false, 1, 0, 0 });

			RenderTargetAttachment depthAlias;
			depthAlias.name     = "DepthTex";
			depthAlias.isDepth  = true;
			depthAlias.imported = res.mainDepth;   // 复用主帧深度
			desc.attachments.push_back(depthAlias);

			res.oitFrame = device.CreateRenderTarget(desc);
		}

		// ================= 附件句柄 =================
		res.backBuffer = device.BackBuffer();
		res.mainColor  = device.GetAttachment(res.mainFrame, "ColorTex");
		res.lightTex   = device.GetAttachment(res.lightFrame, "LightTex");
		res.shadowMap  = device.GetAttachment(res.shadowFrame, "ShadowMap");
		res.accumTex   = device.GetAttachment(res.oitFrame, "AccumTex");
		res.revealTex  = device.GetAttachment(res.oitFrame, "RevealTex");

		// ================= 方块纹理数组 =================
		res.blockTextures = device.CreateTextureArray(MakeBlockTextureArrayDesc());

		// ================= 内建网格 =================
		res.fullscreenQuad = device.FullscreenQuad();

		// ================= 管线状态 =================
		// 每条取值都对应改动前 render_system.cpp 里的一组 GL 状态设置

		// 阴影（render_system.cpp:36-38）
		res.shadowPipeline.shader       = res.shadowShader;
		res.shadowPipeline.blend        = BlendMode::Opaque;
		res.shadowPipeline.depthTest    = DepthTest::Less;
		res.shadowPipeline.depthWrite   = true;
		res.shadowPipeline.colorWrite   = true;
		res.shadowPipeline.cull         = CullMode::Back;
		res.shadowPipeline.colorTargets = 1;
		res.shadowPipeline.vertexLayout = VertexLayoutId::VoxelShadow;

		// 不透明（render_system.cpp:52-54 + block_renderer.cpp:18-20）
		res.opaquePipeline.shader       = res.baseShader;
		res.opaquePipeline.blend        = BlendMode::Opaque;
		res.opaquePipeline.depthTest    = DepthTest::Less;
		res.opaquePipeline.depthWrite   = true;
		res.opaquePipeline.cull         = CullMode::Back;
		res.opaquePipeline.colorTargets = 1;
		res.opaquePipeline.vertexLayout = VertexLayoutId::VoxelMain;
		res.opaquePipeline.sampler      = BlockArraySampler();

		// 冯氏光照（render_system.cpp:77-79 + light_renderer.cpp:19）
		res.lightPipeline               = res.opaquePipeline;
		res.lightPipeline.shader        = res.lightShader;
		res.lightPipeline.vertexLayout  = VertexLayoutId::VoxelLight;

		// WBOIT 深度预通道（semi_transparent_renderer.cpp:50-57）
		// 关键：colorWrite = false 对应 glColorMask(FALSE,FALSE,FALSE,FALSE)
		res.wboitDepthPipeline                = res.opaquePipeline;
		res.wboitDepthPipeline.shader         = res.depthPrepassShader;
		res.wboitDepthPipeline.blend          = BlendMode::Opaque;
		res.wboitDepthPipeline.depthTest      = DepthTest::LessEqual;
		res.wboitDepthPipeline.depthWrite     = true;
		res.wboitDepthPipeline.colorWrite     = false;
		res.wboitDepthPipeline.cull           = CullMode::Back;
		res.wboitDepthPipeline.sampler        = LinearSampler();

		// WBOIT 累加（semi_transparent_renderer.cpp:84-100）
		// 关键：colorTargets = 2 且 blend = WboitAccum（按附件分别设置混合函数）
		res.wboitAccumPipeline                = res.wboitDepthPipeline;
		res.wboitAccumPipeline.shader         = res.accumPassShader;
		res.wboitAccumPipeline.blend          = BlendMode::WboitAccum;
		res.wboitAccumPipeline.depthWrite     = false;
		res.wboitAccumPipeline.colorWrite     = true;
		res.wboitAccumPipeline.colorTargets   = 2;

		// 后处理（render_system.cpp:144-146）
		res.postProcessPipeline.shader       = res.postProcessShader;
		res.postProcessPipeline.blend        = BlendMode::Opaque;
		res.postProcessPipeline.depthTest    = DepthTest::None;
		res.postProcessPipeline.depthWrite   = false;
		res.postProcessPipeline.cull         = CullMode::None;
		res.postProcessPipeline.colorTargets = 1;
		res.postProcessPipeline.vertexLayout = VertexLayoutId::ScreenQuad;
		res.postProcessPipeline.sampler      = LinearSampler();

		// ================= 采样状态 =================
		res.blockSampler  = BlockArraySampler();
		res.linearSampler = LinearSampler();

		// 阴影贴图采样器：必须开 depthCompareMode，
		// 且必须通过 BindTexture 的按单元覆盖传给 unit 3 —— 见 SamplerState 注释
		res.shadowSampler = LinearSampler();
		res.shadowSampler.depthCompareMode = true;

		// ================= 校验（不要省）=================
		//
		// CreateRenderTarget / CreateTexture 失败时返回空句柄（id == 0）。
		// 空句柄不会抛异常，也不会报错 —— 它会让 FramebufferOf() 返回 0，
		// 于是所有渲染**静默地画到默认帧缓冲**上，画面全错但日志干净。
		// 这是最容易浪费半天的一类故障，所以在这里一次性拦住。
		const std::array<std::pair<const char*, RenderTargetHandle>, 4> targets = { {
			{ "MainFrame",   res.mainFrame   },
			{ "LightFrame",  res.lightFrame  },
			{ "ShadowFrame", res.shadowFrame },
			{ "OITFrame",    res.oitFrame    },
		} };
		const std::array<std::pair<const char*, TextureHandle>, 7> textures = { {
			{ "mainColor",     res.mainColor     },
			{ "mainDepth",     res.mainDepth     },
			{ "lightTex",      res.lightTex      },
			{ "shadowMap",     res.shadowMap     },
			{ "accumTex",      res.accumTex      },
			{ "revealTex",     res.revealTex     },
			{ "blockTextures", res.blockTextures },
		} };
		const std::array<std::pair<const char*, ShaderHandle>, 6> shaders = { {
			{ "baseShader",         res.baseShader         },
			{ "shadowShader",       res.shadowShader       },
			{ "lightShader",        res.lightShader        },
			{ "depthPrepassShader", res.depthPrepassShader },
			{ "accumPassShader",    res.accumPassShader    },
			{ "postProcessShader",  res.postProcessShader  },
		} };

		for (const auto& [name, h] : targets) {
			if (!h.valid()) NE_CORE_ERROR("RenderFramework", "{} creation FAILED", name);
		}
		for (const auto& [name, h] : textures) {
			if (!h.valid()) NE_CORE_ERROR("RenderFramework", "{} creation FAILED", name);
		}
		for (const auto& [name, h] : shaders) {
			if (!h.valid()) NE_CORE_ERROR("RenderFramework", "{} creation FAILED", name);
		}

		// 附件句柄必须互不相同 —— 这是 GetAttachment 曾经返回错附件的症状
		if (res.mainColor.id == res.mainDepth.id) {
			NE_CORE_ERROR("RenderFramework", "mainColor == mainDepth (GetAttachment 查错了)");
		}
		if (res.accumTex.id == res.revealTex.id) {
			NE_CORE_ERROR("RenderFramework", "accumTex == revealTex (GetAttachment 查错了)");
		}

		NE_CORE_INFO("RenderFramework",
			"Resources: main={} light={} shadow={} oit={} | color={} depth={} "
			"lightTex={} shadowMap={} accum={} reveal={} blocks={}",
			res.mainFrame.id, res.lightFrame.id, res.shadowFrame.id, res.oitFrame.id,
			res.mainColor.id, res.mainDepth.id, res.lightTex.id, res.shadowMap.id,
			res.accumTex.id, res.revealTex.id, res.blockTextures.id);

		NE_CORE_INFO("RenderFramework", "Render resources created on {}", device.Name());
		return res;
	}

	void DestroyRenderResources(device::IRenderDevice& device, RenderResources& res) {
		device.DestroyRenderTarget(res.oitFrame);
		device.DestroyRenderTarget(res.shadowFrame);
		device.DestroyRenderTarget(res.lightFrame);
		device.DestroyRenderTarget(res.mainFrame);

		device.DestroyTexture(res.blockTextures);

		device.DestroyShader(res.lightShader);
		device.DestroyShader(res.accumPassShader);
		device.DestroyShader(res.depthPrepassShader);
		device.DestroyShader(res.postProcessShader);
		device.DestroyShader(res.shadowShader);
		device.DestroyShader(res.baseShader);

		res = {};
	}

} // namespace nova_engine::render::framework
```

> **过渡期说明**：方块纹理数组的层清单目前硬编码在改动前的 `gltexture_array.cpp` 里（读目录）。移植时把那份清单挪到 `MakeBlockTextureArrayDesc()`（本文件的私有辅助，或 `BlockLibrary` 里的一项查询）。这一步不影响分层。

## 4.5 `Engine/render/framework/passes/shadow_pass.h` / `.cpp`

**原理**：复刻 `render_system.cpp:32-46` + `parallel_light_renderer.cpp:27-50`。

**保留的既有行为**（容易在移植时改错）：

1. `useShadow == false` 时**仍然执行清屏**，只是跳过绘制。改动前 `render_system.cpp:32-35` 先绑定 + 清屏，`:40` 才判断 `useShadow`。如果这里改成「整个 Pass 跳过」，阴影贴图会残留上一帧内容，后处理采样到旧阴影 → **画面会变**。
2. 模型矩阵的相对原点是 **ShadowCamera 的位置**，不是主相机（`parallel_light_renderer.cpp:39`）。
3. viewport 是 2048×2048，与渲染目标尺寸一致。改动前用 `glGetIntegerv` 保存/恢复，现在由下一个 Pass 自己设置 viewport 即可，不需要恢复。

```cpp
#pragma once

#include "render_pass.h"

namespace nova_engine::render::framework {

	class ShadowPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "ShadowPass"; }
		// 始终执行：清屏是必须的，见原理第 1 条
		void Record(const scene::RenderView&, const RenderResources&,
		            device::ICommandList&) override;
	};

} // namespace nova_engine::render::framework
```

```cpp
// shadow_pass.cpp
#include "shadow_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

namespace nova_engine::render::framework {

	void ShadowPass::Record(const scene::RenderView& view,
	                        const RenderResources& res,
	                        device::ICommandList& cmd) {
		device::ClearValue clear{};
		clear.depth = 1.0f;

		cmd.BeginRenderTarget(res.shadowFrame,
		                      device::LoadOp::Clear, device::LoadOp::Clear, clear);
		cmd.SetViewport({ 0, 0,
		                  static_cast<int>(res.shadowResolution),
		                  static_cast<int>(res.shadowResolution) });

		// 对应 render_system.cpp:40 的 if (renderContext.useShadow)
		if (view.settings->useShadow && view.chunkMesh != nullptr) {
			cmd.SetPipelineState(res.shadowPipeline);
			cmd.SetShaderParam("lightSpaceMatrix", view.shadowViewProj);

			// 对应 render_system.cpp:41-45：只画 Default 层
			for (const scene::MeshInstance& m :
			     *view.chunkMesh->Layer(scene::MeshLayer::Default)) {
				if (!m.awake) continue;
				cmd.DrawMesh(m.mesh,
					MakeModelMatrix(m.position, m.rotation, view.shadowOrigin));
			}
		}

		cmd.EndRenderTarget();
	}

} // namespace nova_engine::render::framework
```

## 4.6 `Engine/render/framework/passes/opaque_pass.h` / `.cpp`

**原理**：复刻 `render_system.cpp:49-68` + `block_renderer.cpp:27-47`。

**改掉的两处**：

1. **删掉 `Render(emptyMesh)` 这个 hack**。改动前 `render_system.cpp:55` 传空网格调用 `BlockRenderer::Render`，只是为了借它的代码路径设置一次 `view`/`projection`/纹理绑定。改成按 Pass 设置一次 uniform 之后，这个调用没有存在意义 —— 而且顺带省掉了每帧一次无用的 `glDrawElements(count=0)`。
2. **每帧显式设置 viewport**。改动前 `block_renderer.cpp:28-32` 只在 `needResizeFBO_` 时设 viewport，这是个容易漏的状态；改成每帧设置，行为一致且更稳。

```cpp
#pragma once
#include "render_pass.h"

namespace nova_engine::render::framework {

	class OpaquePass final : public RenderPass {
	public:
		std::string_view Name() const override { return "OpaquePass"; }
		void Record(const scene::RenderView&, const RenderResources&,
		            device::ICommandList&) override;
	};

} // namespace nova_engine::render::framework
```

```cpp
// opaque_pass.cpp
#include "opaque_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

namespace nova_engine::render::framework {

	void OpaquePass::Record(const scene::RenderView& view,
	                        const RenderResources& res,
	                        device::ICommandList& cmd) {
		device::ClearValue clear{};
		clear.color[0][0] = view.settings->clearColor.r;   // 对应 window.cpp:282 的天空色
		clear.color[0][1] = view.settings->clearColor.g;
		clear.color[0][2] = view.settings->clearColor.b;
		clear.color[0][3] = view.settings->clearColor.a;
		clear.depth = 1.0f;

		// 对应 render_system.cpp:49-51（绑定 MainFrame + 清屏）
		cmd.BeginRenderTarget(res.mainFrame,
		                      device::LoadOp::Clear, device::LoadOp::Clear, clear);
		cmd.SetViewport({ 0, 0,
		                  static_cast<int>(view.viewportWidth),
		                  static_cast<int>(view.viewportHeight) });

		cmd.SetPipelineState(res.opaquePipeline);

		// 对应 block_renderer.cpp:37-43 的每帧 uniform
		cmd.SetShaderParam("view",       view.camera->GetRelativeViewMatrix());
		cmd.SetShaderParam("projection", view.camera->GetTransformMatrix(
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight)));

		// 对应 block_renderer.cpp:18-20 的纹理绑定
		cmd.BindTexture(0, "uTextures", res.blockTextures);

		// 对应 render_system.cpp:56-60
		for (const scene::MeshInstance& m :
		     *view.chunkMesh->Layer(scene::MeshLayer::Default)) {
			if (!m.awake) continue;
			cmd.DrawMesh(m.mesh, MakeModelMatrix(m.position, m.rotation, view.mainOrigin));
		}

		// 对应 render_system.cpp:62-68
		if (view.settings->useLod && view.lodMesh != nullptr) {
			for (const scene::MeshInstance& m : view.lodMesh->AllData()) {
				if (!m.awake) continue;
				cmd.DrawMesh(m.mesh, MakeModelMatrix(m.position, m.rotation, view.mainOrigin));
			}
		}

		cmd.EndRenderTarget();
	}

} // namespace nova_engine::render::framework
```

## 4.7 `Engine/render/framework/passes/light_pass.h` / `.cpp`

**原理**：复刻 `render_system.cpp:71-92` + `light_renderer.cpp:4-23`。

**改掉的一处 + 修掉的一个隐患**：

1. 删掉 `glClearColor` 的保存/恢复（`render_system.cpp:73-75, 92`）。清屏色现在是 `BeginRenderTarget` 的参数，本来就是每次进场设一次，全局状态问题自然消失。
2. **显式绑定方块纹理数组**。改动前 `light_renderer.cpp:19` 只设了 `SetTextureUnit("uTextureArray", 0)`，但**从未 `glBindTexture`** —— 它依赖 `BlockRenderer` 构造函数（`block_renderer.cpp:18-19`）留下的纹理单元 0 绑定。这是一条隐式的、跨 renderer 的顺序依赖，任何一次调用顺序调整都会让它静默失效。这里显式绑定。

```cpp
#pragma once
#include "render_pass.h"

namespace nova_engine::render::framework {

	class LightPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "LightPass"; }
		void Record(const scene::RenderView&, const RenderResources&,
		            device::ICommandList&) override;
	};

} // namespace nova_engine::render::framework
```

```cpp
// light_pass.cpp
#include "light_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>

namespace nova_engine::render::framework {

	void LightPass::Record(const scene::RenderView& view,
	                       const RenderResources& res,
	                       device::ICommandList& cmd) {
		device::ClearValue clear{};
		clear.color[0][0] = 0.0f;   // 对应 render_system.cpp:75 的 glClearColor(0,0,0,1)
		clear.color[0][1] = 0.0f;
		clear.color[0][2] = 0.0f;
		clear.color[0][3] = 1.0f;
		clear.depth = 1.0f;

		cmd.BeginRenderTarget(res.lightFrame,
		                      device::LoadOp::Clear, device::LoadOp::Clear, clear);
		cmd.SetViewport({ 0, 0,
		                  static_cast<int>(view.viewportWidth),
		                  static_cast<int>(view.viewportHeight) });

		cmd.SetPipelineState(res.lightPipeline);

		const glm::mat4 viewMatrix = view.camera->GetRelativeViewMatrix();
		const glm::mat4 projection = view.camera->GetTransformMatrix(
			static_cast<int>(view.viewportWidth),
			static_cast<int>(view.viewportHeight));

		cmd.SetShaderParam("view",       viewMatrix);
		cmd.SetShaderParam("projection", projection);
		cmd.SetShaderParam("lightDir",   view.settings->lightDirection);

		// 对应 light_renderer.cpp:19-20；改动前缺了 BindTexture，
		// 靠 BlockRenderer 构造函数留下的绑定侥幸工作，这里补上。
		cmd.BindTexture(0, "uTextureArray", res.blockTextures);

		// 对应 light_renderer.cpp:10 的每网格法线矩阵
		auto drawOne = [&](const scene::MeshInstance& m) {
			const glm::mat4 model = MakeModelMatrix(m.position, m.rotation, view.mainOrigin);
			cmd.SetShaderParam("model", model);
			cmd.SetShaderParam("normalMatrix",
				glm::transpose(glm::inverse(glm::mat3(model))));
			cmd.DrawMesh(m.mesh, model);
		};

		// 对应 render_system.cpp:80-84
		for (const scene::MeshInstance& m :
		     *view.chunkMesh->Layer(scene::MeshLayer::Default)) {
			if (m.awake) drawOne(m);
		}

		// 对应 render_system.cpp:85-91
		if (view.settings->useLod && view.lodMesh != nullptr) {
			for (const scene::MeshInstance& m : view.lodMesh->AllData()) {
				if (m.awake) drawOne(m);
			}
		}

		cmd.EndRenderTarget();
	}

} // namespace nova_engine::render::framework
```

> **注意**：`DrawMesh(handle, model)` 已经接收了 `model`，这里又调了一次 `SetShaderParam("model", model)`。这是必要的 —— `DrawMesh` 的 `model` 参数是给**后端内部**用的（将来做实例化时 backend 会把多个 model 打包进实例缓冲），而 `SetShaderParam("model", ...)` 是**当前后端实现**（每个 draw 一次 uniform 上传）所必需的。等 `IRenderDevice` 增加实例化能力后，这一行可以删掉。这是本方案中唯一一处「参数重复」，属于接口演进的过渡成本。

## 4.8 `Engine/render/framework/passes/transparent_pass.h` / `.cpp`

**原理**：复刻 `render_system.cpp:94-140` + `semi_transparent_renderer.cpp`。

这是五个 Pass 里最复杂的一个，因为 WBOIT 本身就是两个子阶段：

| 阶段 | 渲染目标 | 状态要点 | 对应改动前 |
|---|---|---|---|
| 深度预通道 | MainFrame（只写深度） | `depthTest = LessEqual`、`depthWrite = true`、**`colorWrite = false`** | `semi_transparent_renderer.cpp:47-79` |
| 累加 | OITFrame（2 个颜色附件） | `depthTest = LessEqual`、`depthWrite = false`、`blend = WboitAccum`、`colorTargets = 2` | `semi_transparent_renderer.cpp:88-111` |

### ⚠️ 两处需要你决策的行为变化

移植时发现原实现有两个问题，改与不改都会影响画面。这里给出**修正版**并明确标注：

**变化 1（建议改）：深度预通道与累加阶段的顺序**

改动前 `render_system.cpp:113-135` 对每个网格调用一次 `semiTranRenderer.Render(*mesh)`，而每次 `Render` **内部同时完成预通道和累加**。也就是实际执行顺序是：

```
mesh0 预通道 → mesh0 累加 → mesh1 预通道 → mesh1 累加 → ...
```

正确顺序应该是：

```
mesh0 预通道 → mesh1 预通道 → ... → mesh0 累加 → mesh1 累加 → ...
```

差别在于：改动前 `mesh0` 的累加只能看到 `mesh0` 自己的深度，看不到 `mesh1` 的遮挡；修正后每次累加都能看到全部不透明片元的深度。**修正后的结果才是 WBOIT 想要的**。如果改完发现半透明效果和以前不同，这是原因，不是 bug。

**变化 2（建议改）：排序比较函数的未定义行为**

改动前 `render_system.cpp:125-129`：

```cpp
std::sort(temMesh.begin(), temMesh.end(),
    [](const opengl::GLMesh* a, const opengl::GLMesh* b) {
        if (a && b)
            return glm::length2(a->position) > glm::length2(b->position);
    });   // ← 当 a 或 b 为空时没有 return 语句，是 UB
```

虽然 `temMesh` 只装非空指针所以实际不触发，但比较函数缺失返回值在标准上是未定义行为（某些 STL 实现会断言失败）。下面写成无条件返回。

另外排序键用的是 `glm::length2(position)` —— **世界坐标到原点的平方距离**，不是到相机的距离。因为 WBOIT 本身是顺序无关的，这个排序只是为了减少混合误差，保留原样。

```cpp
#pragma once
#include "render_pass.h"

namespace nova_engine::render::framework {

	class TransparentPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "TransparentPass"; }
		bool Enabled(const scene::RenderView&) const override;
		void Record(const scene::RenderView&, const RenderResources&,
		            device::ICommandList&) override;

	private:
		// 每帧重建的排序结果。复用同一个 vector 避免逐帧分配。
		mutable std::vector<const scene::MeshInstance*> sorted_;
	};

} // namespace nova_engine::render::framework
```

```cpp
// transparent_pass.cpp
#include "transparent_pass.h"

#include "../render_resources.h"

#include <render/core/transform.h>
#include <render/device/command_list.h>
#include <render/scene/render_view.h>

#include <glm/gtx/norm.hpp>

#include <algorithm>

namespace nova_engine::render::framework {

	bool TransparentPass::Enabled(const scene::RenderView& view) const {
		// 对应 render_system.cpp:114 的 if (renderContext.lucencyMesh)
		return view.settings->useLucency && view.chunkMesh != nullptr;
	}

	void TransparentPass::Record(const scene::RenderView& view,
	                             const RenderResources& res,
	                             device::ICommandList& cmd) {
		const scene::ChunkMeshRegistry* lucency =
			view.chunkMesh->Layer(scene::MeshLayer::Lucency);
		if (lucency == nullptr) return;

		// ---------- 收集 + 排序（对应 render_system.cpp:117-129）----------
		sorted_.clear();
		for (const scene::MeshInstance& m : lucency->GetAllMesh()) {
			if (m.awake) sorted_.push_back(&m);
		}
		std::sort(sorted_.begin(), sorted_.end(),
			[](const scene::MeshInstance* a, const scene::MeshInstance* b) {
				return glm::length2(a->position) > glm::length2(b->position);
			});

		if (sorted_.empty()) return;

		const int vw = static_cast<int>(view.viewportWidth);
		const int vh = static_cast<int>(view.viewportHeight);
		const glm::mat4 viewMatrix = view.camera->GetRelativeViewMatrix();
		const glm::mat4 projection  = view.camera->GetTransformMatrix(vw, vh);

		// ================= 阶段 A：深度预通道 =================
		// 对应 semi_transparent_renderer.cpp:47-79
		{
			device::ClearValue clear{};
			cmd.BeginRenderTarget(res.mainFrame,
			                      device::LoadOp::Load, device::LoadOp::Load, clear);
			cmd.SetViewport({ 0, 0, vw, vh });
			cmd.SetPipelineState(res.wboitDepthPipeline);

			cmd.SetShaderParam("uProj",  projection);
			cmd.SetShaderParam("uView",  viewMatrix);
			cmd.BindTexture(0, "uTextureArray", res.blockTextures);

			for (const scene::MeshInstance* m : sorted_) {
				cmd.DrawMesh(m->mesh,
					MakeModelMatrix(m->position, m->rotation, view.mainOrigin));
			}
			cmd.EndRenderTarget();
		}

		// ================= 阶段 B：累加 =================
		// 对应 semi_transparent_renderer.cpp:84-111
		{
			device::ClearValue clear{};
			clear.color[0][0] = 0.0f; clear.color[0][1] = 0.0f;   // accum 清 0
			clear.color[0][2] = 0.0f; clear.color[0][3] = 0.0f;
			clear.color[1][0] = 1.0f; clear.color[1][1] = 0.0f;   // reveal 清 1
			clear.color[1][2] = 0.0f; clear.color[1][3] = 0.0f;

			// 深度来自阶段 A 写入的 MainFrame 深度附件（OITFrame 复用了它），
			// 所以 depthLoad = Load
			cmd.BeginRenderTarget(res.oitFrame,
			                      device::LoadOp::Clear, device::LoadOp::Load, clear);
			cmd.SetViewport({ 0, 0, vw, vh });
			cmd.SetPipelineState(res.wboitAccumPipeline);

			cmd.SetShaderParam("uProj", projection);
			cmd.SetShaderParam("uView", viewMatrix);
			cmd.BindTexture(0, "uTextureArray", res.blockTextures);

			for (const scene::MeshInstance* m : sorted_) {
				cmd.DrawMesh(m->mesh,
					MakeModelMatrix(m->position, m->rotation, view.mainOrigin));
			}
			cmd.EndRenderTarget();
		}
	}

} // namespace nova_engine::render::framework
```

## 4.9 `Engine/render/framework/passes/post_process_pass.h` / `.cpp`

**原理**：复刻 `render_system.cpp:142-147` + `post_process_renderer.cpp:47-122`。

**改掉的三处**：

1. **全屏四边形改由设备提供**（`res.fullscreenQuad`）。改动前 `PostProcessRenderer` 的构造函数里建 VAO/VBO、析构函数里删（`post_process_renderer.cpp:18-45`）—— 这是 Pass 持有 GPU 资源，违反分层。现在它只是一个 `MeshHandle`。
2. **纹理槽位号显式化**。改动前用 `GL_TEXTURE1`~`GL_TEXTURE6` 六个魔法数字；现在写成 1~6 的普通整数，语义不变但不依赖 GL 头文件。
3. **删掉 `glBindFramebuffer(GL_FRAMEBUFFER, 0)` 之类的显式解绑**。`EndRenderTarget` 已经表达了这件事。

```cpp
#pragma once
#include "render_pass.h"

namespace nova_engine::render::framework {

	class PostProcessPass final : public RenderPass {
	public:
		std::string_view Name() const override { return "PostProcessPass"; }
		void Record(const scene::RenderView&, const RenderResources&,
		            device::ICommandList&) override;
	};

} // namespace nova_engine::render::framework
```

```cpp
// post_process_pass.cpp
#include "post_process_pass.h"

#include "../render_resources.h"

#include <render/device/command_list.h>
#include <render/scene/render_view.h>

#include <glm/gtc/matrix_inverse.hpp>

namespace nova_engine::render::framework {

	void PostProcessPass::Record(const scene::RenderView& view,
	                             const RenderResources& res,
	                             device::ICommandList& cmd) {
		const int vw = static_cast<int>(view.viewportWidth);
		const int vh = static_cast<int>(view.viewportHeight);

		device::ClearValue clear{};
		cmd.BeginRenderTarget(res.backBuffer,
		                      device::LoadOp::Clear, device::LoadOp::DontCare, clear);
		cmd.SetViewport({ 0, 0, vw, vh });
		cmd.SetPipelineState(res.postProcessPipeline);

		// 对应 post_process_renderer.cpp:88-110 的六个纹理绑定
		//
		// ★ unit 3（shadowMap）必须传 shadowSampler：
		//   post_process_frag.glsl 里是 uniform sampler2DShadow；
		//   若沿用管线的默认采样器（COMPARE_MODE = NONE），
		//   texture() 返回值未定义 —— 阴影静默失效。详见 SamplerState 的注释。
		cmd.BindTexture(1, "colorTex",  res.mainColor,  &res.linearSampler);
		cmd.BindTexture(2, "depthTex",  res.mainDepth,  &res.linearSampler);
		cmd.BindTexture(3, "shadowMap", res.shadowMap,  &res.shadowSampler);
		cmd.BindTexture(4, "accumTex",  res.accumTex,   &res.linearSampler);
		cmd.BindTexture(5, "revealTex", res.revealTex,  &res.linearSampler);
		cmd.BindTexture(6, "lightMap",  res.lightTex,   &res.linearSampler);

		// 对应 post_process_renderer.cpp:54-60, 112-113
		const glm::mat4 viewMatrix = view.camera->GetRelativeViewMatrix();
		const glm::mat4 projection  = view.camera->GetTransformMatrix(vw, vh);

		cmd.SetShaderParam("invViewProj", glm::inverse(projection * viewMatrix));
		cmd.SetShaderParam("lightSpaceMatrix", view.shadowSampleMatrix);

		// 对应 post_process_renderer.cpp:116-117 的 glDrawArrays(GL_TRIANGLES, 0, 6)
		cmd.DrawMesh(res.fullscreenQuad, glm::mat4(1.0f));

		cmd.EndRenderTarget();
	}

} // namespace nova_engine::render::framework
```

## 4.10 `Engine/render/framework/frame_renderer.h` / `.cpp`

**原理**：取代 `RenderSystem::Render` 里的**固定顺序调用**。

改动前 `render_system.cpp:30-147` 的 Pass 顺序（含被跳过的分支）在这里显式化：

```
ShadowPass      → 始终执行（内部按 useShadow 决定是否绘制）
OpaquePass      → 始终执行
LightPass       → 始终执行
TransparentPass → useLucency 为假时整段跳过
PostProcessPass → 始终执行
```

注意 `ShadowPass` 和 `TransparentPass` 的 `Enabled` 语义不同，这不是疏漏 —— `ShadowPass` 必须清屏，`TransparentPass` 整段可以不做（它不清任何东西，MainFrame/OITFrame 的内容由 OpaquePass 和它自己负责）。

```cpp
#pragma once

#include "passes/light_pass.h"
#include "passes/opaque_pass.h"
#include "passes/post_process_pass.h"
#include "passes/shadow_pass.h"
#include "passes/transparent_pass.h"

namespace nova_engine::render::device { class ICommandList; }
namespace nova_engine::render::scene  { struct RenderView; }

namespace nova_engine::render::framework {

	struct RenderResources;

	// 一帧的 Pass 编排。
	//
	// 刻意做成一个简单的有序列表而不是帧图：5 个 Pass、顺序固定、资源固定。
	// 若将来需要自动依赖排序 / 瞬态资源复用，把 Execute 换成帧图遍历即可，
	// 各 Pass 的 Record 实现不需要改动（它们不拥有渲染目标）。
	class FrameRenderer {
	public:
		void Execute(const scene::RenderView& view,
		             const RenderResources& resources,
		             device::ICommandList& cmd);

	private:
		ShadowPass      shadowPass_;
		OpaquePass      opaquePass_;
		LightPass       lightPass_;
		TransparentPass transparentPass_;
		PostProcessPass postProcessPass_;
	};

} // namespace nova_engine::render::framework
```

```cpp
// frame_renderer.cpp
#include "frame_renderer.h"

#include <render/scene/render_view.h>

namespace nova_engine::render::framework {

	void FrameRenderer::Execute(const scene::RenderView& view,
	                            const RenderResources& resources,
	                            device::ICommandList& cmd) {
		// ★ 防御性前置检查，不要省。
		//
		// 默认构造的 RenderView（camera / settings / chunkMesh 全为 nullptr）
		// 会在第一次解引用时崩在 0x0 ~ 0x30 这个地址区间 —— 典型表现是
		// 「读取位置 0x0000000000000001 时发生访问冲突」，因为
		// RenderSettings::useLucency 正好在偏移 1。
		//
		// 首帧最危险：Window::Run 的固定步长循环允许 Update() 不执行就直接 Render()。
		if (!view.IsValid()) {
			NE_CORE_WARN("FrameRenderer",
				"RenderView is not populated (settings={}, camera={}, chunkMesh={}); "
				"skipping frame. Check Window::Run ordering and RenderSystem::Init.",
				static_cast<const void*>(view.settings),
				static_cast<const void*>(view.camera),
				static_cast<const void*>(view.chunkMesh));
			return;
		}

		shadowPass_.Record(view, resources, cmd);
		opaquePass_.Record(view, resources, cmd);
		lightPass_.Record(view, resources, cmd);

		if (transparentPass_.Enabled(view)) {
			transparentPass_.Record(view, resources, cmd);
		}

		postProcessPass_.Record(view, resources, cmd);
	}

} // namespace nova_engine::render::framework
```

---

# 5. `render/gl` —— 唯一后端

## 5.1 原理

这一层是全项目**唯一**允许出现 `gl*`、`GL_*`、`GLuint`、`#include <glad/glad.h>` 的地方。

### 后端不认识场景

设计约束（[0.2](#02-依赖规则表) 第 5 条）：**`render/gl` 不 include `render/scene` 和 `render/framework`**。它只认识 `render/core` 的类型和 `render/device` 的接口。

这条约束的价值：换后端时只需重写这一个目录，其他六个目录一行不改。如果为了省事让 `GLDevice` 认识 `MeshInstance`，那这个目录就永远删不掉了。

### 句柄表：用槽位索引代替指针

改动前的资源管理方式是「对象即资源」——`GLMesh` 里直接放 `GLuint vao_`，`FrameBuffer` 里直接放 `GLuint fbo_`。问题是这些对象必须**跟着场景数据流动**，于是场景数据被迫携带 GPU 句柄。

改成**槽位表**（`SlotTable`）：

- `MeshHandle{ id }` → `GLDevice::meshes_[id - 1]`
- 句柄是 1-based，`id == 0` 表示无效（正好对上 `Handle::valid()`）
- 删除时槽位清空并入空闲表，**id 可以被复用**
- 上层只看到 `uint32_t`

### VAO 缓存：`(mesh, layout)` 而不是预建三个

改动前 `GLMesh` 在构造时就建好 3 个 VAO（`glmesh.cpp:10-12`），因为它的作者知道「将来会有 Main/Shadow/Light 三种顶点布局」。这个知识本该属于管线。

改成：**`GLMeshBuffers` 只持有 VBO/EBO，VAO 由 `GLCommandList` 按需创建并缓存**，键是 `(meshId, vertexLayoutId)`。

收益：
- 网格对象不再需要预知未来的 shader 数量
- 网格上传一次，VAO 可以按需产生任意多种
- 加新顶点布局时，只改 VAO 构建的 `switch`，加一个 case

### 状态去重

立即模式接口的一个风险是「每个 draw 都重设一遍状态」。这里在 `GLCommandList` 里缓存上一次的 `PipelineState`：

```cpp
if (currentPipeline_ == pipeline) return;   // 同一 Pass 内 N 次 DrawMesh 只设一次状态
```

改动前每画一个网格都要走一遍 `glEnable`/`glDisable`/`glDepthFunc`/`glCullFace`（`render_system.cpp` 里每个 Pass 前重复了一次），这里顺带变成一个廉价优化 —— 不是目的，但白拿。

## 5.2 `Engine/render/gl/slot_table.h`

```cpp
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace nova_engine::render::gl {

	// 句柄 -> 资源的槽位表。
	//
	// 句柄是 1-based（0 表示无效，对应 render::Handle::valid()）。
	// Remove 只清空槽位并入空闲表，不缩容 —— 避免句柄失效。
	template <typename T>
	class SlotTable {
	public:
		uint32_t Add(T&& value) {
			if (free_.empty()) {
				slots_.push_back(std::move(value));
				return static_cast<uint32_t>(slots_.size());
			}
			const uint32_t id = free_.back();
			free_.pop_back();
			slots_[id - 1] = std::move(value);
			return id;
		}

		T* Get(uint32_t id) {
			if (id == 0 || id > slots_.size()) return nullptr;
			return &slots_[id - 1];
		}
		const T* Get(uint32_t id) const {
			if (id == 0 || id > slots_.size()) return nullptr;
			return &slots_[id - 1];
		}

		void Remove(uint32_t id) {
			if (id == 0 || id > slots_.size()) return;
			slots_[id - 1] = T{};
			free_.push_back(id);
		}

		void Clear() {
			slots_.clear();
			free_.clear();
		}

		std::size_t Capacity() const { return slots_.size(); }

	private:
		std::vector<T>     slots_;
		std::vector<uint32_t> free_;
	};

} // namespace nova_engine::render::gl
```

## 5.3 `Engine/render/gl/gl_enums.h`

**原理**：所有 `引擎枚举 → GLenum` 的映射集中在这一个头文件。

集中而不是分散的原因：**这是唯一需要随图形 API 变化而重写的地方**。换 D3D 时，删掉这个文件重写一份即可，其他文件的 `switch` 语句全都不会出现。

```cpp
#pragma once

#include <render/core/enums.h>
#include <render/core/sampler_state.h>
#include <render/core/texture_desc.h>

#include <glad/glad.h>

namespace nova_engine::render::gl {

	inline GLenum ToGLDepthFunc(DepthTest v) {
		switch (v) {
		case DepthTest::Less:      return GL_LESS;
		case DepthTest::LessEqual: return GL_LEQUAL;
		case DepthTest::Greater:   return GL_GREATER;
		case DepthTest::Always:
		case DepthTest::None:
		default:                   return GL_ALWAYS;
		}
	}

	inline GLenum ToGLCullFace(CullMode v) {
		switch (v) {
		case CullMode::Front: return GL_FRONT;
		case CullMode::Back:
		default:              return GL_BACK;
		}
	}

	inline GLenum ToGLTopology(Topology v) {
		switch (v) {
		case Topology::LineList:  return GL_LINES;
		case Topology::PointList: return GL_POINTS;
		case Topology::TriangleList:
		default:                  return GL_TRIANGLES;
		}
	}

	inline GLint ToGLMinFilter(TextureFilter v) {
		switch (v) {
		case TextureFilter::Nearest:            return GL_NEAREST;
		case TextureFilter::NearestMipmapLinear:return GL_NEAREST_MIPMAP_LINEAR;
		case TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
		case TextureFilter::Linear:
		default:                                return GL_LINEAR;
		}
	}

	inline GLint ToGLMagFilter(TextureFilter v) {
		switch (v) {
		case TextureFilter::Nearest:
		case TextureFilter::NearestMipmapLinear:
			return GL_NEAREST;
		case TextureFilter::Linear:
		case TextureFilter::LinearMipmapLinear:
		default:
			return GL_LINEAR;
		}
	}

	inline GLint ToGLWrap(TextureWrap v) {
		switch (v) {
		case TextureWrap::Repeat:        return GL_REPEAT;
		case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
		case TextureWrap::ClampToEdge:
		default:                         return GL_CLAMP_TO_EDGE;
		}
	}

	// (internalFormat, format, type) 三元组。
	// 取值逐条对应改动前 frame_buffer_library.cpp:9-40 里硬编码的参数。
	struct GLTexFormat { GLenum internalFormat; GLenum format; GLenum type; };

	inline GLTexFormat ToGLTexFormat(TextureFormat v) {
		switch (v) {
		case TextureFormat::RGB16F:   return { GL_RGB16F,             GL_RGB,             GL_FLOAT };
		case TextureFormat::RGBA16F:  return { GL_RGBA16F,            GL_RGBA,            GL_FLOAT };
		case TextureFormat::R16F:     return { GL_R16F,               GL_RED,             GL_FLOAT };
		case TextureFormat::Depth32F: return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
		case TextureFormat::RGBA8:
		default:                      return { GL_RGBA,               GL_RGBA,            GL_UNSIGNED_BYTE };
		}
	}

	inline GLenum ToGLShaderStage(ShaderStage v) {
		switch (v) {
		case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
		case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
		case ShaderStage::Compute:  return GL_COMPUTE_SHADER;
		case ShaderStage::Vertex:
		default:                    return GL_VERTEX_SHADER;
		}
	}

} // namespace nova_engine::render::gl
```

## 5.4 `Engine/render/gl/gl_context.h` / `.cpp`

**原理**：把「创建 GL 上下文」从 `Window` 里剥出来。

改动前 `Window::Init()` 里混着 GLFW 窗口创建和 GL 初始化（`window.cpp:240-246`），`window.h` 因此必须 include glad。抽出这个函数后，`window.h` 可以**完全不认识 GL**（只前置声明 `GLFWwindow`）。

```cpp
#pragma once

#include <memory>   // ★ 必须：返回 std::unique_ptr。缺了会在部分 TU 里编译失败

namespace nova_engine::render::device { class IRenderDevice; }

struct GLFWwindow;

namespace nova_engine::render::gl {

	// 让 GLFW 窗口的上下文成为当前上下文，并加载 GL 函数指针。
	// 成功后返回可用的渲染设备；失败返回 nullptr（调用方必须判空）。
	std::unique_ptr<device::IRenderDevice> CreateDevice(GLFWwindow* window);

} // namespace nova_engine::render::gl
```

```cpp
// gl_context.cpp
#include "gl_context.h"

#include "gl_device.h"

#include <log/logger.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace nova_engine::render::gl {

	std::unique_ptr<device::IRenderDevice> CreateDevice(GLFWwindow* window) {
		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
			NE_CORE_ERROR("RenderGL", "Failed to initialize GLAD");
			return nullptr;
		}

		NE_CORE_INFO("RenderGL", "OpenGL {}, GLSL {}",
			reinterpret_cast<const char*>(glGetString(GL_VERSION)),
			reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

		return std::make_unique<GLDevice>();
	}

} // namespace nova_engine::render::gl
```

## 5.5 `Engine/render/gl/gl_shader.h` / `.cpp`

**原理**：`Shader` 的移植 + **修掉 bug 2**。

改动前 `Shader::GetVarLocation` 每次都调 `glGetUniformLocation`（`shader.cpp:13-17`），而 `BlockRenderer` 对每个网格都调两次 `SetMat4`（`block_renderer.cpp:39-40`）。上千个网格就是每帧几千次字符串查找 + 驱动查询。

修法：link 成功后用 `glGetActiveUniform` **遍历一次**，把 `name → location` 存进哈希表；之后 `Location()` 只查表。

同时保留改动前那个 `assert` 的意图（uniform 名字写错要能立刻发现），但改成记录一次警告而不是每次断言 —— 因为查表失败通常是 shader 编译问题，重复报错没有价值。

```cpp
#pragma once

#include <render/core/shader_desc.h>
#include <render/core/shader_param.h>

#include <glad/glad.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace nova_engine::render::gl {

	class GLShader {
	public:
		GLShader() = default;
		~GLShader() { Destroy(); }

		GLShader(const GLShader&) = delete;
		GLShader& operator=(const GLShader&) = delete;
		GLShader(GLShader&& o) noexcept;
		GLShader& operator=(GLShader&& o) noexcept;

		bool Create(const ShaderDesc& desc);
		void Destroy();

		void Use() const { glUseProgram(program_); }

		// 查表获取 uniform 位置；未找到返回 -1（调用方静默跳过）
		GLint Location(std::string_view name) const;

		void SetParam(std::string_view name, const ShaderParam& value) const;

		GLuint id() const { return program_; }

	private:
		GLuint program_ = 0;
		std::unordered_map<std::string, GLint> locations_;

		void CacheUniformLocations();
		static bool CompileStage(const std::string& path, GLenum type, GLuint& out);
		static std::string LoadSource(const std::string& path);
	};

} // namespace nova_engine::render::gl
```

```cpp
// gl_shader.cpp
#include "gl_shader.h"

#include "gl_enums.h"

#include <log/logger.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace nova_engine::render::gl {

	GLShader::GLShader(GLShader&& o) noexcept
		: program_(o.program_), locations_(std::move(o.locations_)) {
		o.program_ = 0;
		o.locations_.clear();
	}
	GLShader& GLShader::operator=(GLShader&& o) noexcept {
		if (this != &o) {
			Destroy();
			program_   = o.program_;
			locations_ = std::move(o.locations_);
			o.program_ = 0;
			o.locations_.clear();
		}
		return *this;
	}

	bool GLShader::CompileStage(const std::string& path, GLenum type, GLuint& out) {
		const std::string src = LoadSource(path);
		if (src.empty()) {
			NE_CORE_ERROR("RenderGL", "Shader source not found: {}", path);
			return false;
		}

		const GLuint shader = glCreateShader(type);
		const char* raw = src.c_str();
		glShaderSource(shader, 1, &raw, nullptr);
		glCompileShader(shader);

		GLint ok = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			char log[1024] = {};
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			NE_CORE_ERROR("RenderGL", "Compile failed: {} ({})", path, log);
			glDeleteShader(shader);
			return false;
		}
		out = shader;
		return true;
	}

	std::string GLShader::LoadSource(const std::string& path) {
		// 与改动前 shader.cpp:168-190 的 LoadShaderSrc 等价
		std::ifstream file(path);
		if (!file.is_open()) return {};
		std::stringstream buf;
		buf << file.rdbuf();
		return buf.str();
	}

	bool GLShader::Create(const ShaderDesc& desc) {
		std::vector<GLuint> stages;
		for (const ShaderStageDesc& s : desc.stages) {
			GLuint id = 0;
			if (!CompileStage(s.path, ToGLShaderStage(s.stage), id)) {
				for (GLuint s2 : stages) glDeleteShader(s2);
				return false;
			}
			stages.push_back(id);
		}

		program_ = glCreateProgram();
		for (GLuint s : stages) glAttachShader(program_, s);
		glLinkProgram(program_);

		GLint ok = GL_FALSE;
		glGetProgramiv(program_, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[1024] = {};
			glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
			NE_CORE_ERROR("RenderGL", "Link failed: {}", log);
			for (GLuint s : stages) glDeleteShader(s);
			glDeleteProgram(program_);
			program_ = 0;
			return false;
		}

		for (GLuint s : stages) glDeleteShader(s);

		CacheUniformLocations();
		return true;
	}

	// 修掉改动前 shader.cpp:13-17 的每次查询：
	// link 后遍历一次全部活动 uniform，之后只查表。
	void GLShader::CacheUniformLocations() {
		GLint count = 0;
		glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);

		GLint maxName = 0;
		glGetProgramiv(program_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxName);

		std::string name(static_cast<std::size_t>(maxName ? maxName : 1), '\0');

		for (GLint i = 0; i < count; ++i) {
			GLsizei length = 0;
			GLint   size   = 0;
			GLenum  type   = 0;
			glGetActiveUniform(program_, static_cast<GLuint>(i),
			                   static_cast<GLsizei>(name.size()), &length,
			                   &size, &type, name.data());
			name.resize(static_cast<std::size_t>(length));

			// 数组 uniform 的名字形如 "foo[0]"，同时缓存去掉下标的版本
			locations_.emplace(name, glGetUniformLocation(program_, name.c_str()));
			if (const auto bracket = name.find('['); bracket != std::string::npos) {
				locations_.emplace(name.substr(0, bracket),
				                   glGetUniformLocation(program_, name.c_str()));
			}
			name.resize(static_cast<std::size_t>(maxName ? maxName : 1));
		}
	}

	GLint GLShader::Location(std::string_view name) const {
		const auto it = locations_.find(std::string(name));
		return it == locations_.end() ? -1 : it->second;
	}

	void GLShader::SetParam(std::string_view name, const ShaderParam& value) const {
		const GLint loc = Location(name);
		if (loc < 0) return;   // uniform 未使用或被优化掉，静默跳过

		std::visit([loc](const auto& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, int>)          glUniform1i(loc, v);
			else if constexpr (std::is_same_v<T, float>)   glUniform1f(loc, v);
			else if constexpr (std::is_same_v<T, glm::vec2>) glUniform2fv(loc, 1, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::vec3>) glUniform3fv(loc, 1, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::vec4>) glUniform4fv(loc, 1, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::mat3>) glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(v));
			else if constexpr (std::is_same_v<T, glm::mat4>) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(v));
		}, value);
	}

	void GLShader::Destroy() {
		if (program_ != 0) {
			glDeleteProgram(program_);
			program_ = 0;
		}
		locations_.clear();
	}

} // namespace nova_engine::render::gl
```

## 5.6 `Engine/render/gl/gl_texture.h` / `.cpp`

**原理**：`GLTexture` + `GLTextureArray` 合并成一个类，因为两者的差别只有 target（`GL_TEXTURE_2D` / `GL_TEXTURE_2D_ARRAY`）。

**关键**：采样状态在创建时设置一次（来自 `TextureDesc::defaultSampler`），**之后任何 Pass 都不再修改它** —— 需要不同采样的 Pass 用 `PipelineState::sampler`（采样器对象）。这就是 bug 1 的根治方案。

```cpp
#pragma once

#include <render/core/texture_desc.h>

#include <glad/glad.h>

#include <string>
#include <unordered_map>

namespace nova_engine::render::gl {

	class GLTexture {
	public:
		GLTexture() = default;
		~GLTexture() { Destroy(); }

		GLTexture(const GLTexture&) = delete;
		GLTexture& operator=(const GLTexture&) = delete;
		GLTexture(GLTexture&& o) noexcept;
		GLTexture& operator=(GLTexture&& o) noexcept;

		bool Create2D(const TextureDesc& desc, const void* pixels);
		bool CreateArray(const TextureArrayDesc& desc,
		                 const std::vector<const void*>& layerPixels);

		void Destroy();

		GLuint  id() const { return texture_; }
		GLenum  target() const { return target_; }
		bool    valid() const { return texture_ != 0; }
		int     FindLayerIndex(const std::string& name) const;

	private:
		GLuint texture_ = 0;
		GLenum target_  = GL_TEXTURE_2D;
		int    width_ = 0, height_ = 0;
		std::unordered_map<std::string, int> layerIndex_;
	};

} // namespace nova_engine::render::gl
```

```cpp
// gl_texture.cpp
#include "gl_texture.h"

#include "gl_enums.h"

#include <log/logger.h>

namespace nova_engine::render::gl {

	GLTexture::GLTexture(GLTexture&& o) noexcept
		: texture_(o.texture_), target_(o.target_),
		  width_(o.width_), height_(o.height_),
		  layerIndex_(std::move(o.layerIndex_)) {
		o.texture_ = 0;
	}
	GLTexture& GLTexture::operator=(GLTexture&& o) noexcept {
		if (this != &o) {
			Destroy();
			texture_    = o.texture_;
			target_     = o.target_;
			width_      = o.width_;
			height_     = o.height_;
			layerIndex_ = std::move(o.layerIndex_);
			o.texture_ = 0;
		}
		return *this;
	}

	namespace {
		// 对应改动前 frame_buffer.cpp:91-99 的纹理参数设置
		void ApplySampler(GLenum target, const SamplerState& s) {
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(s.minFilter));
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(s.magFilter));
			glTexParameteri(target, GL_TEXTURE_WRAP_S,     ToGLWrap(s.wrap));
			glTexParameteri(target, GL_TEXTURE_WRAP_T,     ToGLWrap(s.wrap));
		}
	} // namespace

	bool GLTexture::Create2D(const TextureDesc& desc, const void* pixels) {
		Destroy();

		const GLTexFormat fmt = ToGLTexFormat(desc.format);

		glGenTextures(1, &texture_);
		target_ = GL_TEXTURE_2D;
		glBindTexture(target_, texture_);

		glTexImage2D(target_, 0, fmt.internalFormat,
		             static_cast<GLsizei>(desc.width),
		             static_cast<GLsizei>(desc.height),
		             0, fmt.format, fmt.type, pixels);

		ApplySampler(target_, desc.defaultSampler);

		// 阴影贴图的深度比较模式，对应 frame_buffer_library.cpp:28-31
		if (desc.depthCompareMode) {
			glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, desc.borderColor);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}

		if (desc.generateMipmaps) glGenerateMipmap(target_);

		glBindTexture(target_, 0);

		width_  = static_cast<int>(desc.width);
		height_ = static_cast<int>(desc.height);
		return texture_ != 0;
	}

	bool GLTexture::CreateArray(const TextureArrayDesc& desc,
	                            const std::vector<const void*>& layerPixels) {
		Destroy();

		const GLTexFormat fmt = ToGLTexFormat(desc.format);
		const GLsizei size    = static_cast<GLsizei>(desc.tileSize);

		glGenTextures(1, &texture_);
		target_ = GL_TEXTURE_2D_ARRAY;
		glBindTexture(target_, texture_);

		// 对应改动前 gltexture_array.cpp:36-38
		glTexImage3D(target_, 0, fmt.internalFormat, size, size,
		             static_cast<GLsizei>(desc.layers), 0,
		             fmt.format, fmt.type, nullptr);

		for (std::size_t i = 0; i < layerPixels.size() && i < desc.layers; ++i) {
			if (layerPixels[i] == nullptr) continue;
			// 对应 gltexture_array.cpp:56-58
			glTexSubImage3D(target_, 0, 0, 0, static_cast<GLint>(i),
			                size, size, 1, fmt.format, fmt.type, layerPixels[i]);
		}

		ApplySampler(target_, desc.defaultSampler);

		if (desc.generateMipmaps) glGenerateMipmap(target_);   // gltexture_array.cpp:67

		glBindTexture(target_, 0);

		width_ = height_ = static_cast<int>(size);
		for (const TextureArrayLayer& layer : desc.sources) {
			layerIndex_[layer.name] = layer.index;
		}
		return texture_ != 0;
	}

	int GLTexture::FindLayerIndex(const std::string& name) const {
		const auto it = layerIndex_.find(name);
		return it == layerIndex_.end() ? 0 : it->second;
	}

	void GLTexture::Destroy() {
		if (texture_ != 0) {
			glDeleteTextures(1, &texture_);
			texture_ = 0;
		}
		layerIndex_.clear();
	}

} // namespace nova_engine::render::gl
```

## 5.7 `GLRenderTarget` 的初版设计（已被 [5.10](#510-enginerenderglgl_deviceh--cpp) 取代）

> **状态：本节保留作为推导记录，落地时请使用 5.10 的最终版。**
>
> 初版让 `GLRenderTarget` 同时管理 FBO 挂接与纹理的创建/重建/销毁，写 `GLDevice` 时发现职责过载 —— 详见 5.10 开头的分析。下面的代码**不要照抄**，但其中 `AttachAll` / `RebuildDrawBuffers` 的 GL 调用序列是最终版直接继承的，可以对照 `frame_buffer.cpp` 阅读。

**原理**：`FrameBuffer` 的移植，支持 `imported` 附件。

保留 `frame_buffer.cpp:149-171` 的 `RebuildDrawBuffers` 逻辑（`glDrawBuffers` 的构建，含纯深度时 `GL_NONE` 的分支）。这些是 GL FBO 的固有复杂度，不该外泄。

```cpp
#pragma once

#include <render/core/render_target_desc.h>

#include <glad/glad.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nova_engine::render::gl {

	class GLTexture;

	class GLRenderTarget {
	public:
		GLRenderTarget() = default;
		~GLRenderTarget() { Destroy(); }

		GLRenderTarget(const GLRenderTarget&) = delete;
		GLRenderTarget& operator=(const GLRenderTarget&) = delete;
		GLRenderTarget(GLRenderTarget&& o) noexcept;
		GLRenderTarget& operator=(GLRenderTarget&& o) noexcept;

		// imported 里给出每个附件的「已存在纹理」（未导入的为 nullptr）。
		// 与 imported 同时存在的就是 WBOIT 复用主帧深度的场景。
		bool Create(const RenderTargetDesc& desc, uint32_t width, uint32_t height,
		            const std::vector<GLTexture*>& imported);

		// 只重建存储，不改变任何附件名字与顺序 —— 保证句柄在 resize 后依然有效
		void Resize(uint32_t width, uint32_t height,
		            const std::vector<GLTexture*>& imported);

		void Destroy();
		void Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }

		GLuint fbo() const { return fbo_; }
		bool   ownsAttachment(const std::string& name) const;

	private:
		struct Attachment {
			TextureFormat format = TextureFormat::RGBA8;
			bool          isDepth = false;
			uint32_t      index = 0;
			uint32_t      fixedWidth = 0, fixedHeight = 0;
			GLuint        texture = 0;     // 仅当自有时有效
			bool          owned = true;
		};

		GLuint fbo_ = 0;
		std::unordered_map<std::string, Attachment> attachments_;
		std::vector<std::string> attachmentOrder_;

		void RebuildDrawBuffers();
		void RecreateStorage(uint32_t width, uint32_t height,
		                     const std::vector<GLTexture*>& imported);
	};

} // namespace nova_engine::render::gl
```

```cpp
// gl_render_target.cpp
#include "gl_render_target.h"

#include "gl_enums.h"
#include "gl_texture.h"

#include <log/logger.h>

#include <algorithm>

namespace nova_engine::render::gl {

	GLRenderTarget::GLRenderTarget(GLRenderTarget&& o) noexcept
		: fbo_(o.fbo_), attachments_(std::move(o.attachments_)),
		  attachmentOrder_(std::move(o.attachmentOrder_)) {
		o.fbo_ = 0;
	}
	GLRenderTarget& GLRenderTarget::operator=(GLRenderTarget&& o) noexcept {
		if (this != &o) {
			Destroy();
			fbo_             = o.fbo_;
			attachments_     = std::move(o.attachments_);
			attachmentOrder_ = std::move(o.attachmentOrder_);
			o.fbo_ = 0;
		}
		return *this;
	}

	bool GLRenderTarget::Create(const RenderTargetDesc& desc,
	                            uint32_t width, uint32_t height,
	                            const std::vector<GLTexture*>& imported) {
		Destroy();

		glGenFramebuffers(1, &fbo_);

		std::size_t i = 0;
		for (const RenderTargetAttachment& a : desc.attachments) {
			Attachment att;
			att.format      = a.format;
			att.isDepth     = a.isDepth;
			att.index       = a.index;
			att.fixedWidth  = a.fixedWidth;
			att.fixedHeight = a.fixedHeight;
			att.owned       = (i >= imported.size() || imported[i] == nullptr);
			att.texture     = att.owned ? 0 : imported[i]->id();

			attachments_[a.name] = att;
			if (!a.isDepth) attachmentOrder_.push_back(a.name);   // 颜色附件顺序
			++i;
		}

		RecreateStorage(width, height, imported);
		return fbo_ != 0;
	}

	void GLRenderTarget::Resize(uint32_t width, uint32_t height,
	                            const std::vector<GLTexture*>& imported) {
		RecreateStorage(width, height, imported);
	}

	void GLRenderTarget::RecreateStorage(uint32_t width, uint32_t height,
	                                     const std::vector<GLTexture*>& imported) {
		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

		std::size_t i = 0;
		for (auto& [name, att] : attachments_) {
			// 顺序遍历 unordered_map 不可靠，这里用名字重查顺序
			(void)name;
			(void)att;
			++i;
		}

		// 按创建顺序处理（attachmentOrder_ 只含颜色；深度单独补上）
		std::vector<std::string> all;
		all.insert(all.end(), attachmentOrder_.begin(), attachmentOrder_.end());
		for (const auto& [name, att] : attachments_) {
			if (att.isDepth) all.push_back(name);
		}

		i = 0;
		for (const std::string& name : all) {
			Attachment& att = attachments_[name];

			const uint32_t w = att.fixedWidth  ? att.fixedWidth  : width;
			const uint32_t h = att.fixedHeight ? att.fixedHeight : height;

			if (att.owned) {
				if (att.texture != 0) glDeleteTextures(1, &att.texture);

				TextureDesc td;
				td.width  = w;
				td.height = h;
				td.format = att.format;
				td.defaultSampler = { TextureFilter::Linear, TextureFilter::Linear,
				                      TextureWrap::ClampToEdge };
				// 阴影贴图的深度比较模式在 RenderResources 里由 TextureDesc 声明；
				// 这里通过固定尺寸 + Depth32F 判定（见下方说明）
				td.depthCompareMode = (att.isDepth && att.fixedWidth != 0);

				const GLTexFormat fmt = ToGLTexFormat(att.format);
				glGenTextures(1, &att.texture);
				glBindTexture(GL_TEXTURE_2D, att.texture);
				glTexImage2D(GL_TEXTURE_2D, 0, fmt.internalFormat,
				             static_cast<GLsizei>(w), static_cast<GLsizei>(h),
				             0, fmt.format, fmt.type, nullptr);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				if (td.depthCompareMode) {
					const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
					glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
				}
				glBindTexture(GL_TEXTURE_2D, 0);
			} else if (i < imported.size() && imported[i] != nullptr) {
				att.texture = imported[i]->id();
			}

			const GLenum attachment = att.isDepth
				? GL_DEPTH_ATTACHMENT
				: (GL_COLOR_ATTACHMENT0 + att.index);   // 对应 frame_buffer.cpp:105

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, att.texture, 0);
			++i;
		}

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			NE_CORE_ERROR("RenderGL", "Framebuffer incomplete");
		}

		RebuildDrawBuffers();   // 必须在 fbo_ 绑定时调用

		glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	}

	// 移植改动前 frame_buffer.cpp:149-171
	void GLRenderTarget::RebuildDrawBuffers() {
		std::vector<GLenum> buffers;
		buffers.reserve(attachmentOrder_.size());

		for (const std::string& name : attachmentOrder_) {
			const Attachment& att = attachments_[name];
			if (att.texture == 0) continue;
			buffers.push_back(GL_COLOR_ATTACHMENT0 + att.index);
		}

		if (buffers.empty()) {
			GLenum none = GL_NONE;                 // 纯深度/无颜色附件（ShadowFrame）
			glDrawBuffers(1, &none);
		} else {
			glDrawBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
		}
	}

	bool GLRenderTarget::ownsAttachment(const std::string& name) const {
		const auto it = attachments_.find(name);
		return it != attachments_.end() && it->second.owned;
	}

	void GLRenderTarget::Destroy() {
		for (auto& [name, att] : attachments_) {
			if (att.owned && att.texture != 0) {
				glDeleteTextures(1, &att.texture);
			}
		}
		attachments_.clear();
		attachmentOrder_.clear();

		if (fbo_ != 0) {
			glDeleteFramebuffers(1, &fbo_);
			fbo_ = 0;
		}
	}

} // namespace nova_engine::render::gl
```

> **实现提示**：上面 `RecreateStorage` 里对 `unordered_map` 的遍历顺序问题做了显式排序，但 `depthCompareMode` 用「固定尺寸 + 深度格式」推断是不严谨的。更干净的做法是让 `Attachment` 直接保存 `TextureDesc`（而不是拆散的字段），在 `Create` 时整体存下来。这里为了突出「引擎枚举 → GL 调用」的映射关系而写得更直白，实际落地时建议改成存 `TextureDesc`。

## 5.8 `Engine/render/gl/gl_mesh_buffers.h` / `.cpp`

**原理**：**只持有 VBO/EBO，不持有 VAO**。

这是 [5.1](#51-原理) 讲的「VAO 缓存」的另一半。改动前 `GLMesh` 里 3 个 VAO + 5 个 VBO + 1 个 EBO 一共 9 个 `GLuint`（`glmesh.h:42-44`）。拆开后：

- `GLMeshBuffers` 持有 4 个 VBO（pos/uvw/ao/normal，与改动前一致）+ 1 个 EBO
- VAO 由 `GLCommandList` 按 `(meshId, layout)` 缓存

**空网格分支**原样保留（`glmesh.cpp:98-115`）：`glBufferData(target, 0, nullptr, ...)` 在 GL 里就是释放缓冲存储的语义。

```cpp
#pragma once

#include <render/core/mesh_data.h>
#include <render/core/pipeline_state.h>

#include <glad/glad.h>

#include <array>
#include <cstdint>

namespace nova_engine::render::gl {

	class GLMeshBuffers {
	public:
		GLMeshBuffers() = default;
		~GLMeshBuffers() { Destroy(); }

		GLMeshBuffers(const GLMeshBuffers&) = delete;
		GLMeshBuffers& operator=(const GLMeshBuffers&) = delete;
		GLMeshBuffers(GLMeshBuffers&& o) noexcept;
		GLMeshBuffers& operator=(GLMeshBuffers&& o) noexcept;

		// 体素网格：上传 4 条属性流（pos3 / uvw3 / ao1 / normal3）+ 索引
		void UploadVoxel(const MeshData& data);

		// 全屏四边形：上传 2 条属性流（pos2 / uv2）+ 索引
		void UploadScreenQuad(const MeshData& data);

		void Destroy();

		GLuint   vbo(uint32_t slot) const { return vbos_[slot]; }
		GLuint   ebo() const { return ebo_; }
		uint32_t indexCount() const { return indexCount_; }
		bool     valid() const { return indexCount_ > 0; }
		bool     isScreenQuad() const { return screenQuad_; }

	private:
		std::array<GLuint, 4> vbos_{};
		GLuint   ebo_ = 0;
		uint32_t indexCount_ = 0;
		bool     screenQuad_ = false;

		static void UploadStream(GLuint vbo, const void* data, std::size_t bytes);
	};

} // namespace nova_engine::render::gl
```

```cpp
// gl_mesh_buffers.cpp
#include "gl_mesh_buffers.h"

#include <utility>

namespace nova_engine::render::gl {

	GLMeshBuffers::GLMeshBuffers(GLMeshBuffers&& o) noexcept
		: vbos_(o.vbos_), ebo_(o.ebo_),
		  indexCount_(o.indexCount_), screenQuad_(o.screenQuad_) {
		o.vbos_.fill(0);
		o.ebo_ = 0;
		o.indexCount_ = 0;
	}
	GLMeshBuffers& GLMeshBuffers::operator=(GLMeshBuffers&& o) noexcept {
		if (this != &o) {
			Destroy();
			vbos_       = o.vbos_;
			ebo_        = o.ebo_;
			indexCount_ = o.indexCount_;
			screenQuad_ = o.screenQuad_;
			o.vbos_.fill(0);
			o.ebo_ = 0;
			o.indexCount_ = 0;
		}
		return *this;
	}

	void GLMeshBuffers::UploadStream(GLuint vbo, const void* data, std::size_t bytes) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		// 对应改动前 glmesh.cpp:120-121 的 glBufferData(NULL) + glBufferSubData 两步；
		// 单次 glBufferData 少一次驱动分配
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
	}

	void GLMeshBuffers::UploadVoxel(const MeshData& data) {
		screenQuad_ = false;
		indexCount_ = static_cast<uint32_t>(data.indices.size());

		if (vbos_[0] == 0) glGenBuffers(4, vbos_.data());
		if (ebo_ == 0)     glGenBuffers(1, &ebo_);

		// 空网格：释放显存（对应改动前 glmesh.cpp:98-115）
		if (!data.isValid()) {
			for (GLuint& vbo : vbos_) {
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			}
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			indexCount_ = 0;
			return;
		}

		UploadStream(vbos_[0], data.vertices.data(), data.vertices.size() * sizeof(float));
		UploadStream(vbos_[1], data.uvws.data(),     data.uvws.size()     * sizeof(float));
		UploadStream(vbos_[2], data.aos.data(),      data.aos.size()      * sizeof(float));
		UploadStream(vbos_[3], data.normals.data(),  data.normals.size()  * sizeof(float));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             static_cast<GLsizeiptr>(data.indices.size() * sizeof(uint32_t)),
		             data.indices.data(), GL_STATIC_DRAW);
	}

	void GLMeshBuffers::UploadScreenQuad(const MeshData& data) {
		screenQuad_ = true;
		indexCount_ = static_cast<uint32_t>(data.indices.size());

		if (vbos_[0] == 0) glGenBuffers(4, vbos_.data());
		if (ebo_ == 0)     glGenBuffers(1, &ebo_);

		// ScreenQuad 布局：vertices 解释为 vec2、uvws 解释为 vec2
		UploadStream(vbos_[0], data.vertices.data(), data.vertices.size() * sizeof(float));
		UploadStream(vbos_[1], data.uvws.data(),     data.uvws.size()     * sizeof(float));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             static_cast<GLsizeiptr>(data.indices.size() * sizeof(uint32_t)),
		             data.indices.data(), GL_STATIC_DRAW);
	}

	void GLMeshBuffers::Destroy() {
		for (GLuint& vbo : vbos_) {
			if (vbo != 0) { glDeleteBuffers(1, &vbo); vbo = 0; }
		}
		if (ebo_ != 0) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
		indexCount_ = 0;
	}

} // namespace nova_engine::render::gl
```

## 5.9 `Engine/render/gl/gl_command_list.h` / `.cpp`

**原理**：`ICommandList` 的实现，同时承载 **VAO 缓存**和**采样器缓存**。

三个实现要点：

1. **VAO 缓存键** `(meshId, vertexLayout)`。缓存放在 `GLCommandList` 而不是 `GLDevice`，因为它只在提交路径上被访问；`GLDevice` 析构时统一清理（`GLCommandList` 由 `GLDevice` 拥有）。
2. **采样器对象缓存**。`PipelineState::sampler` → `glGenSamplers` + 参数设置，按 `SamplerState` 的值缓存。绑定到所有纹理单元（`glBindSampler`），这就是 bug 1 的正解。
3. **管线状态去重**。同一 Pass 内连续 N 次 `DrawMesh` 只应用一次状态。

`BlendMode::WboitAccum` 的映射是这里的重点：它展开成两次 `glBlendFunci`（对应 `semi_transparent_renderer.cpp:99-100`）。

```cpp
#pragma once

#include <render/device/command_list.h>

#include "slot_table.h"

#include <glad/glad.h>

#include <cstdint>
#include <unordered_map>

namespace nova_engine::render::gl {

	class GLDevice;

	class GLCommandList final : public device::ICommandList {
	public:
		explicit GLCommandList(GLDevice& device) : device_(device) {}
		~GLCommandList() override;

		void BeginRenderTarget(RenderTargetHandle, device::LoadOp colorLoad,
		                       device::LoadOp depthLoad,
		                       const device::ClearValue&) override;
		void EndRenderTarget() override;

		void SetViewport(const device::Rect&) override;

		void SetPipelineState(const PipelineState&) override;
		void SetShaderParam(std::string_view name, const ShaderParam&) override;
		void BindTexture(uint32_t unit, std::string_view sampler,
		                 TextureHandle,
		                 const SamplerState* samplerState = nullptr) override;

		void DrawMesh(MeshHandle, const glm::mat4& model) override;

		// GLDevice 析构时调用
		void ReleaseCaches();

	private:
		GLDevice& device_;

		RenderTargetHandle boundTarget_{};
		PipelineState      currentPipeline_{};
		bool               hasPipeline_ = false;

		std::unordered_map<uint64_t, GLuint> vaoCache_;      // (meshId << 8) | layout
		std::unordered_map<uint64_t, GLuint> samplerCache_;  // SamplerState 的哈希

		GLuint GetOrCreateVao(uint32_t meshId, VertexLayoutId layout);
		GLuint GetOrCreateSampler(const SamplerState&);

		void ApplyPipelineState(const PipelineState&);
	};

} // namespace nova_engine::render::gl
```

```cpp
// gl_command_list.cpp
#include "gl_command_list.h"

#include "gl_device.h"
#include "gl_enums.h"
#include "gl_mesh_buffers.h"
#include "gl_render_target.h"
#include "gl_shader.h"
#include "gl_texture.h"

#include <log/logger.h>

#include <functional>

namespace nova_engine::render::gl {

	namespace {
		uint64_t HashSampler(const SamplerState& s) {
			return (static_cast<uint64_t>(s.minFilter) << 16)
			     | (static_cast<uint64_t>(s.magFilter) << 8)
			     |  static_cast<uint64_t>(s.wrap)
			     | (static_cast<uint64_t>(s.depthCompareMode) << 24);   // ★ 必须包含
		}
	} // namespace

	GLCommandList::~GLCommandList() = default;

	// ---------------------------------------------------------------------------
	// 渲染目标
	// ---------------------------------------------------------------------------

	void GLCommandList::BeginRenderTarget(RenderTargetHandle handle,
	                                      device::LoadOp colorLoad,
	                                      device::LoadOp depthLoad,
	                                      const device::ClearValue& clear) {
		boundTarget_ = handle;

		const GLuint fbo = device_.FramebufferOf(handle);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// 与改动前不同：这里对每个颜色附件分别清屏，替代 glClearBufferfv
		// （render_system.cpp:106-110）
		GLenum buffers[device::ClearValue::kMaxColorAttachments] = {};
		for (uint32_t i = 0; i < device::ClearValue::kMaxColorAttachments; ++i) {
			buffers[i] = GL_COLOR_ATTACHMENT0 + i;
		}

		if (colorLoad == device::LoadOp::Clear) {
			for (uint32_t i = 0; i < device::ClearValue::kMaxColorAttachments; ++i) {
				if (!device_.HasColorAttachment(handle, i)) continue;
				glClearBufferfv(GL_COLOR, static_cast<GLint>(i), clear.color[i]);
			}
		}

		if (depthLoad == device::LoadOp::Clear) {
			glDepthMask(GL_TRUE);
			glClearBufferfi(GL_DEPTH_STENCIL, 0, clear.depth, clear.stencil);
		}

		hasPipeline_ = false;   // 新的渲染目标后需要重新应用状态
	}

	void GLCommandList::EndRenderTarget() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		boundTarget_ = {};
		hasPipeline_ = false;
	}

	void GLCommandList::SetViewport(const device::Rect& r) {
		glViewport(r.x, r.y, r.width, r.height);
	}

	// ---------------------------------------------------------------------------
	// 管线状态
	// ---------------------------------------------------------------------------

	void GLCommandList::ApplyPipelineState(const PipelineState& ps) {
		// 着色器
		if (GLShader* shader = device_.ShaderOf(ps.shader)) {
			shader->Use();
		}

		// 混合（对应 render_system.cpp:24-26 与 semi_transparent_renderer.cpp:84-100）
		switch (ps.blend) {
		case BlendMode::Opaque:
			glDisable(GL_BLEND);
			break;

		case BlendMode::AlphaBlend:
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;

		case BlendMode::Additive:
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
			break;

		case BlendMode::WboitAccum:
			// 对应 semi_transparent_renderer.cpp:99-100：
			//   glBlendFunci(0, GL_ONE, GL_ONE)
			//   glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR)
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunci(0, GL_ONE, GL_ONE);
			glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
			break;
		}

		// 深度
		if (ps.depthTest == DepthTest::None) {
			glDisable(GL_DEPTH_TEST);
		} else {
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(ToGLDepthFunc(ps.depthTest));
		}
		glDepthMask(ps.depthWrite ? GL_TRUE : GL_FALSE);

		// 颜色写入掩码（对应 semi_transparent_renderer.cpp:53）
		if (ps.colorWrite) {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		} else {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		}

		// 剔除（对应 render_system.cpp:37, 53, 78 与 semi_transparent_renderer.cpp:56-57）
		if (ps.cull == CullMode::None) {
			glDisable(GL_CULL_FACE);
		} else {
			glEnable(GL_CULL_FACE);
			glCullFace(ToGLCullFace(ps.cull));
		}

		// 采样器：绑到所有单元，取代改动前直接改纹理滤波的做法
		//（render_system.cpp:102 / 139 —— 这就是 bug 1 的根治点）
		const GLuint sampler = GetOrCreateSampler(ps.sampler);
		for (uint32_t unit = 0; unit < 8; ++unit) {
			glBindSampler(unit, sampler);
		}
	}

	void GLCommandList::SetPipelineState(const PipelineState& ps) {
		if (hasPipeline_ && currentPipeline_.shader.id == ps.shader.id
		    && currentPipeline_.blend == ps.blend
		    && currentPipeline_.depthTest == ps.depthTest
		    && currentPipeline_.depthWrite == ps.depthWrite
		    && currentPipeline_.colorWrite == ps.colorWrite
		    && currentPipeline_.cull == ps.cull
		    && currentPipeline_.sampler == ps.sampler) {
			return;   // 同一 Pass 内的重复设置直接跳过
		}

		currentPipeline_ = ps;
		hasPipeline_ = true;
		ApplyPipelineState(ps);
	}

	void GLCommandList::SetShaderParam(std::string_view name, const ShaderParam& value) {
		if (GLShader* shader = device_.ShaderOf(currentPipeline_.shader)) {
			shader->SetParam(name, value);
		}
	}

	void GLCommandList::BindTexture(uint32_t unit, std::string_view sampler,
	                                TextureHandle handle,
	                                const SamplerState* samplerState) {
		GLTexture* texture = device_.TextureOf(handle);
		if (texture == nullptr || !texture->valid()) {
			NE_CORE_ERROR("RenderGL", "BindTexture: invalid texture on unit {}", unit);
			return;
		}

		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(texture->target(), texture->id());

		// ★ 按单元重新绑采样器。
		//   glBindSampler 是 per-unit 状态，切管线时 ApplyPipelineState 只设了
		//   PipelineState::sampler 那一个；对需要不同采样状态的单元
		//   （典型：阴影贴图需要 depthCompareMode）必须在这里覆盖，
		//   否则会被管线的默认采样器盖掉，且不报任何错。
		const SamplerState& state = samplerState ? *samplerState : currentPipeline_.sampler;
		glBindSampler(unit, GetOrCreateSampler(state));

		// 让 sampler uniform 指向该槽位
		if (GLShader* shader = device_.ShaderOf(currentPipeline_.shader)) {
			shader->SetParam(sampler, static_cast<int>(unit));
		}
	}

	// ---------------------------------------------------------------------------
	// 绘制
	// ---------------------------------------------------------------------------

	GLuint GLCommandList::GetOrCreateVao(uint32_t meshId, VertexLayoutId layout) {
		const uint64_t key = (static_cast<uint64_t>(meshId) << 8)
		                   | static_cast<uint64_t>(layout);
		if (const auto it = vaoCache_.find(key); it != vaoCache_.end()) {
			return it->second;
		}

		GLMeshBuffers* mesh = device_.MeshOf(MeshHandle{ meshId });
		if (mesh == nullptr) return 0;

		GLuint vao = 0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// 对应改动前 glmesh.cpp:151-172 的三段 VAO 设置
		switch (layout) {
		case VertexLayoutId::VoxelMain:
			// 0 = pos3, 1 = uvw3, 2 = ao1, 3 = normal3
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(2));
			glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), nullptr);
			glEnableVertexAttribArray(2);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(3));
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(3);
			break;

		case VertexLayoutId::VoxelShadow:
			// 0 = pos3
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			break;

		case VertexLayoutId::VoxelLight:
			// 0 = pos3, 1 = normal3, 2 = uvw3
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(3));
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glEnableVertexAttribArray(2);
			break;

		case VertexLayoutId::ScreenQuad:
			// 0 = pos2, 1 = uv2
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(0));
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo(1));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			break;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo());
		glBindVertexArray(0);

		vaoCache_.emplace(key, vao);
		return vao;
	}

	GLuint GLCommandList::GetOrCreateSampler(const SamplerState& s) {
		// ★ 哈希必须包含 depthCompareMode，否则「开/关 compare 的两个采样状态」
		//   会命中同一个缓存项，阴影会时对时错。
		const uint64_t key = HashSampler(s);
		if (const auto it = samplerCache_.find(key); it != samplerCache_.end()) {
			return it->second;
		}

		GLuint sampler = 0;
		glGenSamplers(1, &sampler);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(s.minFilter));
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(s.magFilter));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S,     ToGLWrap(s.wrap));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T,     ToGLWrap(s.wrap));

		// ★ GL_TEXTURE_COMPARE_MODE / _FUNC 是采样器对象状态（GL 3.3+）。
		//   采样器绑定到纹理单元时会**覆盖**纹理自身的设置 ——
		//   所以阴影采样器必须在这里显式打开 compare 模式，
		//   光靠纹理上设 glTexParameteri 会被默认采样器覆盖掉。
		if (s.depthCompareMode) {
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		} else {
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}

		samplerCache_.emplace(key, sampler);
		return sampler;
	}

	void GLCommandList::DrawMesh(MeshHandle handle, const glm::mat4& model) {
		GLMeshBuffers* mesh = device_.MeshOf(handle);
		if (mesh == nullptr || !mesh->valid()) return;

		const GLuint vao = GetOrCreateVao(handle.id, currentPipeline_.vertexLayout);
		if (vao == 0) return;

		SetShaderParam("model", model);

		glBindVertexArray(vao);
		glDrawElements(ToGLTopology(currentPipeline_.topology),
		               static_cast<GLsizei>(mesh->indexCount()),
		               GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);
	}

	void GLCommandList::ReleaseCaches() {
		for (auto& [key, vao] : vaoCache_) {
			if (vao != 0) glDeleteVertexArrays(1, &vao);
		}
		vaoCache_.clear();

		for (auto& [key, sampler] : samplerCache_) {
			if (sampler != 0) glDeleteSamplers(1, &sampler);
		}
		samplerCache_.clear();
	}

} // namespace nova_engine::render::gl
```

## 5.10 `Engine/render/gl/gl_device.h` / `.cpp`

### `GLRenderTarget`：为什么要改初版设计

写 `GLDevice` 时发现 5.7 那版 `GLRenderTarget` 承担了太多职责：它既管 FBO 挂接，又管纹理的创建/重建/销毁，还要维护 `TextureDesc` 的副本。结果是 `GetAttachment()` 返回的 `TextureHandle` 无法被 `BindTexture()` 解析 —— 因为渲染目标的附件纹理不在设备的纹理槽位表里。

**简化后的职责划分**：

| 类 | 职责 |
|---|---|
| `GLRenderTarget` | **只做 FBO 挂接 + draw buffers**。接收一串 `{name, GLuint texture, isDepth, colorIndex}`，不拥有纹理 |
| `GLDevice` | 拥有全部纹理（包括渲染目标附件），负责创建与 resize 时的重新分配（**句柄不变**） |

### `GLRenderTarget` 最终版：完整实现

#### `Engine/render/gl/gl_render_target.h`

```cpp
#pragma once

#include <glad/glad.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nova_engine::render::gl {

	// 一个附件在 FBO 上的挂接描述。
	//
	// 注意：这个结构体只持有纹理的 GL id，**不拥有纹理**。
	// 纹理的创建 / 重建 / 销毁全部由 GLDevice 负责。
	struct AttachmentBinding {
		std::string name;                  // 仅用于日志与调试
		GLuint      texture    = 0;
		bool        isDepth    = false;
		uint32_t    colorIndex = 0;        // 颜色附件序号；深度附件忽略此字段
	};

	// 只负责 FBO 的挂接与 draw buffers 配置。
	//
	// 职责边界（与 5.7 初版设计的区别）：
	//   ✅ 创建 / 销毁 FBO，把附件挂上去，配置 glDrawBuffers，做完整性检查
	//   ❌ 不创建纹理、不保存 TextureDesc、不重建存储、不决定尺寸
	//
	// 这条边界让「谁拥有纹理」这个问题只有一个答案：GLDevice。
	// 于是 GetAttachment() 返回的 TextureHandle 一定能被 BindTexture() 解析。
	class GLRenderTarget {
	public:
		GLRenderTarget() = default;
		~GLRenderTarget() { Destroy(); }

		GLRenderTarget(const GLRenderTarget&) = delete;
		GLRenderTarget& operator=(const GLRenderTarget&) = delete;
		GLRenderTarget(GLRenderTarget&& other) noexcept;
		GLRenderTarget& operator=(GLRenderTarget&& other) noexcept;

		// 建 FBO 并完成首次挂接
		bool Create(const std::vector<AttachmentBinding>& bindings);

		// 重新挂接。用于 resize 之后纹理 GL id 变化的情况。
		// 若 FBO 尚未创建，等价于 Create。
		void Rebuild(const std::vector<AttachmentBinding>& bindings);

		void Destroy();

		void Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }

		GLuint fbo() const { return fbo_; }

	private:
		GLuint fbo_ = 0;

		// 保留最近一次的挂接记录。Debug 构建下可用于断言 / 打印，
		// Release 下不产生任何开销 —— 它的存在也让 Rebuild 可以省略参数重载。
		std::vector<AttachmentBinding> bindings_;

		void Apply(const std::vector<AttachmentBinding>& bindings);
		void AttachAll();
		void RebuildDrawBuffers();
	};

} // namespace nova_engine::render::gl
```

#### `Engine/render/gl/gl_render_target.cpp`

```cpp
#include "gl_render_target.h"

#include <log/logger.h>

#include <algorithm>

namespace nova_engine::render::gl {

	// ===========================================================================
	// 移动语义
	//
	// GLRenderTarget 不拥有任何纹理，只拥有一个 FBO。
	// 因此移动时只需搬运 fbo_ 与挂接记录，不做任何 glDelete*。
	// 这与改动前 FrameBuffer 的移动构造（frame_buffer.cpp:13-40）形成对比 ——
	// 那里要一起搬 textures_ 这张 GLuint 表，因为纹理归它所有。
	// ===========================================================================

	GLRenderTarget::GLRenderTarget(GLRenderTarget&& other) noexcept
		: fbo_(other.fbo_), bindings_(std::move(other.bindings_)) {
		other.fbo_ = 0;
	}

	GLRenderTarget& GLRenderTarget::operator=(GLRenderTarget&& other) noexcept {
		if (this != &other) {
			Destroy();
			fbo_       = other.fbo_;
			bindings_  = std::move(other.bindings_);
			other.fbo_ = 0;
		}
		return *this;
	}

	// ===========================================================================
	// 创建与重建
	// ===========================================================================

	// 对应改动前 frame_buffer.cpp:5-6 的 glGenFramebuffers
	bool GLRenderTarget::Create(const std::vector<AttachmentBinding>& bindings) {
		Destroy();

		glGenFramebuffers(1, &fbo_);
		if (fbo_ == 0) {
			NE_CORE_ERROR("RenderGL", "glGenFramebuffers failed");
			return false;
		}

		Apply(bindings);
		return true;
	}

	// resize 之后重新挂接。
	//
	// 前提：bindings 里的 texture（GL id）必须仍然有效。
	// 这由 GLTexture::ReallocateStorage 保证 —— 见下方「与 resize 的配合」一节。
	void GLRenderTarget::Rebuild(const std::vector<AttachmentBinding>& bindings) {
		if (fbo_ == 0) {
			Create(bindings);
			return;
		}
		Apply(bindings);
	}

	// 挂接 + draw buffers 的公共流程。
	//
	// 保存并恢复当前 FBO 绑定，与改动前 frame_buffer.cpp:101-102, 114 一致 ——
	// 这个函数会在渲染循环之外被调用（初始化、resize），不能假设当前绑定是 0。
	void GLRenderTarget::Apply(const std::vector<AttachmentBinding>& bindings) {
		bindings_ = bindings;

		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		AttachAll();
		RebuildDrawBuffers();
		glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	}

	// 对应改动前 frame_buffer.cpp:104-110
	void GLRenderTarget::AttachAll() {
		for (const AttachmentBinding& b : bindings_) {
			if (b.texture == 0) continue;   // 未初始化，跳过（不挂接也不进 draw buffers）

			const GLenum attachment = b.isDepth
				? GL_DEPTH_ATTACHMENT
				: static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + b.colorIndex);

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
			                       GL_TEXTURE_2D, b.texture, 0);
		}

		// 对应改动前 frame_buffer.cpp:107-110
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			NE_CORE_ERROR("RenderGL", "Framebuffer incomplete (status = 0x{:X})",
			              static_cast<unsigned>(status));
		}
	}

	// 移植改动前 frame_buffer.cpp:149-171
	//
	// ★ 语义要点：glDrawBuffers 的第 i 个元素对应**片元着色器的输出 location i**，
	//   而不是「颜色附件 i」。因此数组必须按 colorIndex 升序排列：
	//
	//     colorBuffers[0] = GL_COLOR_ATTACHMENT0  →  out0 (outAccum)     写入 accum
	//     colorBuffers[1] = GL_COLOR_ATTACHMENT1  →  out1 (outRevealage) 写入 reveal
	//
	//   这与 accum_pass_frag.glsl 的
	//     layout(location = 0) out vec4  outAccum;
	//     layout(location = 1) out float outRevealage;
	//   一一对应。
	//
	// 改动前是按添加顺序（colorAttachmentOrder_）压入的。对现有的四个渲染目标，
	// 添加顺序与 colorIndex 顺序恰好一致，所以两者结果完全相同。这里显式排序，
	// 是为了让正确性不依赖「谁先调用 AddTexture」这个隐含约定。
	void GLRenderTarget::RebuildDrawBuffers() {
		std::vector<const AttachmentBinding*> colors;
		colors.reserve(bindings_.size());
		for (const AttachmentBinding& b : bindings_) {
			if (!b.isDepth && b.texture != 0) colors.push_back(&b);
		}

		std::sort(colors.begin(), colors.end(),
			[](const AttachmentBinding* a, const AttachmentBinding* b) {
				return a->colorIndex < b->colorIndex;
			});

		if (colors.empty()) {
			// 纯深度 / 无颜色附件（ShadowFrame），对应 frame_buffer.cpp:162-166。
			// 不写颜色，只写深度。
			GLenum none = GL_NONE;
			glDrawBuffers(1, &none);
			return;
		}

		std::vector<GLenum> buffers;
		buffers.reserve(colors.size());
		for (const AttachmentBinding* b : colors) {
			buffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + b->colorIndex));
		}

		glDrawBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
	}

	void GLRenderTarget::Destroy() {
		bindings_.clear();

		// 只删 FBO。纹理归 GLDevice 所有，这里绝不碰。
		if (fbo_ != 0) {
			glDeleteFramebuffers(1, &fbo_);
			fbo_ = 0;
		}
	}

} // namespace nova_engine::render::gl
```

#### 与 resize 的配合（读取本文件前必须先看这一节）

`AttachmentBinding` 里存的是**裸 `GLuint`**，在 `GLDevice::CreateRenderTarget` 时捕获一次。如果 resize 时 GL 纹理对象被销毁重建，id 就会变，挂接记录随之失效。

更糟的是，`GLDevice::OnResize` 遍历的是 `std::unordered_map`，**顺序不确定**。如果 `OITFrame` 在 `MainFrame` 之前被 rebuild，它就会挂上一个已经被删除的深度纹理 id —— 画面表现为后处理通道读到垃圾深度。

**解法：让纹理的 GL id 在 resize 前后保持不变。**

给 5.6 的 `GLTexture` 补一个方法。关键点：**不删旧对象，而是在同一个纹理对象名上重新调用 `glTexImage2D`**（GL 语义：对已存在的纹理再次 `glTexImage2D` 就是重新定义该 level 的存储，纹理对象名不变）。

签名直接接收 `TextureDesc`，比逐参数传递更好 —— 不需要在 `GLTexture` 里额外存一份 `format_`。

```cpp
// 追加到 Engine/render/gl/gl_texture.h 的 public 区
	// 重新分配存储，复用同一个 GL 纹理对象（GL id 不变）。
	// 用于窗口 resize —— 保证 TextureHandle 与 GL id 在 OnResize 后都保持有效。
	// 只能在 2D 纹理上调用；数组纹理的层数固定，不需要 resize。
	bool ReallocateStorage(const TextureDesc& desc);
```

```cpp
// 追加到 Engine/render/gl/gl_texture.cpp
	bool GLTexture::ReallocateStorage(const TextureDesc& desc) {
		// 数组纹理的层数在创建时确定，窗口 resize 不改变它。
		// 这里拒绝而不是静默改掉 target_ —— 后者会让纹理对象与 target_ 记录不一致。
		if (texture_ != 0 && target_ != GL_TEXTURE_2D) {
			NE_CORE_ERROR("RenderGL", "ReallocateStorage only supports GL_TEXTURE_2D");
			return false;
		}

		if (texture_ == 0) {
			glGenTextures(1, &texture_);
			if (texture_ == 0) {
				NE_CORE_ERROR("RenderGL", "glGenTextures failed");
				return false;
			}
		}

		target_ = GL_TEXTURE_2D;
		const GLTexFormat fmt = ToGLTexFormat(desc.format);

		glBindTexture(target_, texture_);

		// ★ 是 glTexImage2D，不是 glTexSubImage2D。两者参数个数相同但含义不同，
		//   详见下方「为什么不能是 glTexSubImage2D」。
		//   glTexImage2D 的参数表：
		//     (target, level, internalformat, width, height, border, format, type, pixels)
		glTexImage2D(target_, 0, static_cast<GLint>(fmt.internalFormat),
		             static_cast<GLsizei>(desc.width),
		             static_cast<GLsizei>(desc.height),
		             0, fmt.format, fmt.type, nullptr);

		// 纹理参数（filter / wrap / compare）属于纹理对象状态，
		// glTexImage2D **不会**重置它们。这里重设一遍只是让本函数自包含，
		// 不依赖「创建时已经设过」这个前提。
		ApplySampler(target_, desc.defaultSampler);

		// 但 COMPARE_MODE 必须双向显式设置：
		// ApplySampler 不碰它，若上一轮是 GL_COMPARE_REF_TO_TEXTURE
		// 而这一轮 desc.depthCompareMode == false，不关掉就会残留。
		if (desc.depthCompareMode) {
			glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, desc.borderColor);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		} else {
			glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}

		// 尺寸变了，旧 mip 链已失效；需要时重建
		if (desc.generateMipmaps) glGenerateMipmap(target_);

		glBindTexture(target_, 0);

		width_  = static_cast<int>(desc.width);
		height_ = static_cast<int>(desc.height);
		return true;
	}
```

#### 为什么不能是 `glTexSubImage2D`

这两个函数的**参数个数相同（都是 9 个）**，且大多是整型，**编译器不会报错** —— 但含义完全不同：

| 位置 | `glTexImage2D` | `glTexSubImage2D` |
|---|---|---|
| 1 | target | target |
| 2 | level | level |
| 3 | **internalformat** | **xoffset** |
| 4 | width | yoffset |
| 5 | height | width |
| 6 | **border**（必须为 0） | **height** |
| 7 | format | format |
| 8 | type | type |
| 9 | pixels | pixels |

所以 `glTexSubImage2D(target_, 0, fmt.internalFormat, desc.width, desc.height, 0, fmt.format, fmt.type, nullptr)` 会被驱动解释成：

| 参数 | 实际收到的值 | 后果 |
|---|---|---|
| `xoffset` | `GL_RGBA16F` = 34842 | 远超纹理宽度 → `GL_INVALID_VALUE` |
| `yoffset` | `desc.width`（如 800） | 越界 |
| `width` | `desc.height`（如 600） | — |
| `height` | `0` | **零高度子图像 = 不传输任何像素** |

**结果是函数静默地什么都没做**：纹理保持旧尺寸，同时 GL 错误标志被置位。画面表现为「窗口放大后内部渲染分辨率不变、图像被拉伸」，不崩溃、不报错、极难定位。

更根本的一点：**`glTexSubImage2D` 在语义上就不可能改变纹理尺寸**。它只在已分配的 level 内更新一个子矩形，无法重新分配存储。要改变尺寸只有两条路：

- `glTexImage2D`（可变存储，**本方案采用**）
- `glTexStorage2D` + `glTexSubImage2D`（不可变存储，尺寸一次定死；resize 只能重建整个纹理对象 —— 那样 GL id 就变了，会重新引入挂接记录失效的问题，正是本节要避免的）

有了这个方法之后，`GLDevice::OnResize` 的**两阶段**顺序就成立了：

```cpp
	void GLDevice::OnResize(uint32_t width, uint32_t height) {
		width_  = width;
		height_ = height;

		// 阶段 1：重新分配所有自有附件的存储。
		//          GL id 不变 —— 所以下面的挂接记录依然有效。
		for (auto& [id, record] : targetRecords_) {
			ReallocateTarget(record, width, height);
		}

		// 阶段 2：重新挂接。
		//          此时全部纹理都已是最终尺寸，不存在「挂到旧纹理」的窗口。
		for (auto& [id, record] : targetRecords_) {
			if (GLRenderTarget* rt = targets_.Get(id)) {
				rt->Rebuild(MakeBindings(record));
			}
		}
	}
```

两阶段是必要的：**阶段 1 与阶段 2 的 `unordered_map` 遍历顺序可能不同**，如果边分配边挂接，`OITFrame` 仍有可能在 `MainFrame` 完成分配前就读取到旧的挂接列表。

这个划分还有个额外好处：**它天然支持「句柄在 resize 后保持有效」这条接口契约**（[2.2](#22-engine-renderdevicerender_deviceh)）—— `TextureHandle` 指向 `GLDevice::textures_` 里的槽位，resize 只是重新分配存储，槽位不变、GL id 不变，句柄自然不变。

#### 实现 `IRenderDevice` 时的踩坑清单

`GLDevice` 少实现一个纯虚函数（或**签名差一个 `const`**）时，编译器只会报：

```
error C2259: "GLDevice": 无法实例化抽象类
```

这行信息量极低。**处理办法**：

1. **给每个方法加 `override`** —— 编译器会从 C2259 变成 `C3668: 包含重写说明符"override"的方法没有重写任何基类方法`，**直接指到出错那一行**
2. **切到「输出」窗口**（不是「错误列表」）—— C2259 后面会跟 `note:` 逐条列出未实现的纯虚函数，错误列表会把 note 截掉
3. **打开 `/w14263`**（要求 override 说明符），把这类问题变成常规编译错误

> `note` 的数量能直接区分两类原因：
> - 列 **1 条** → 某个方法签名不匹配（绝大多数是漏了 `const`）
> - 列 **18 条** → 继承写错了，`GLDevice` 根本没继承到 `IRenderDevice`

`IRenderDevice` 一共 18 个纯虚函数，逐个核对（★ 标出的是最容易漏 `const` 的）：

```cpp
// ── 网格 ──
MeshHandle CreateMesh(const MeshData&) override;
void       UpdateMesh(MeshHandle, const MeshData&) override;
void       DestroyMesh(MeshHandle) override;

// ── 纹理 ──
TextureHandle CreateTexture(const TextureDesc&, std::span<const std::byte> pixels) override;
TextureHandle CreateTextureArray(const TextureArrayDesc&) override;
void          DestroyTexture(TextureHandle) override;

// ── 着色器 ──
ShaderHandle CreateShader(const ShaderDesc&) override;
void         DestroyShader(ShaderHandle) override;

// ── 渲染目标 ──
RenderTargetHandle CreateRenderTarget(const RenderTargetDesc&) override;
void               DestroyRenderTarget(RenderTargetHandle) override;
TextureHandle      GetAttachment(RenderTargetHandle, std::string_view) const override;  // ★ const

// ── 内建资源 ──
MeshHandle FullscreenQuad() const override;                                             // ★ const

// ── 帧 ──
void          BeginFrame(const FrameInfo&) override;
ICommandList& Commands() override;
void          Present() override;
void          OnResize(uint32_t width, uint32_t height) override;
RenderTargetHandle BackBuffer() const override;                                         // ★ const
const char*   Name() const override;                                                    // ★ const
```

**最常踩的五个**：

| # | 基类声明 | 常见错误 | 说明 |
|---|---|---|---|
| 1 | `GetAttachment(...) const` | 漏 `const` | `const` 是签名的一部分，漏了就不构成覆盖 |
| 2 | `Name() const` | 漏 `const` | 同上 |
| 3 | `FullscreenQuad() const` | 漏 `const` | 同上 |
| 4 | `BackBuffer() const` | 漏 `const` | 同上 |
| 5 | `CreateTexture(const TextureDesc&, std::span<const std::byte>)` | 写成 `const std::vector<std::byte>&` | 需要 `#include <span>` |

### `gl_device.h`

```cpp
#pragma once

#include <render/device/render_device.h>

#include "gl_command_list.h"
#include "gl_mesh_buffers.h"
#include "gl_render_target.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "slot_table.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nova_engine::render::gl {

	class GLDevice final : public device::IRenderDevice {
	public:
		GLDevice();
		~GLDevice() override;

		// ---------------- 网格 ----------------
		MeshHandle CreateMesh(const MeshData&) override;
		void       UpdateMesh(MeshHandle, const MeshData&) override;
		void       DestroyMesh(MeshHandle) override;

		// ---------------- 纹理 ----------------
		TextureHandle CreateTexture(const TextureDesc&,
		                            std::span<const std::byte> pixels) override;
		TextureHandle CreateTextureArray(const TextureArrayDesc&) override;
		void          DestroyTexture(TextureHandle) override;

		// ---------------- 着色器 ----------------
		ShaderHandle CreateShader(const ShaderDesc&) override;
		void         DestroyShader(ShaderHandle) override;

		// ---------------- 渲染目标 ----------------
		RenderTargetHandle CreateRenderTarget(const RenderTargetDesc&) override;
		void               DestroyRenderTarget(RenderTargetHandle) override;
		TextureHandle      GetAttachment(RenderTargetHandle,
		                                 std::string_view name) const override;

		// ---------------- 内建资源 ----------------
		MeshHandle FullscreenQuad() const override { return fullscreenQuad_; }

		// ---------------- 帧 ----------------
		void          BeginFrame(const FrameInfo& info) override;
		ICommandList& Commands() override { return commands_; }
		void          Present() override;

		void               OnResize(uint32_t width, uint32_t height) override;
		// 返回初始化时注册的默认帧缓冲句柄（内部 FBO 为 0），不是空句柄 ——
		// 否则调用方无法区分「默认帧缓冲」与「创建失败」
		RenderTargetHandle BackBuffer() const override { return backBuffer_; }

		const char* Name() const override { return "OpenGL"; }

		// ---- 供 GLCommandList 使用的内部访问 ----
		GLShader*      ShaderOf(ShaderHandle h)        { return shaders_.Get(h.id); }
		GLTexture*     TextureOf(TextureHandle h)      { return textures_.Get(h.id); }
		GLMeshBuffers* MeshOf(MeshHandle h)            { return meshes_.Get(h.id); }
		GLuint         FramebufferOf(RenderTargetHandle h) const;
		bool           HasColorAttachment(RenderTargetHandle, uint32_t index) const;

	private:
		// 渲染目标的附件登记。
		//
		// ★ name 必须存下来 —— GetAttachment 靠它查找。
		//   曾经用 attachments.front() 占位，导致 MainFrame 的 "DepthTex"
		//   查询返回了 "ColorTex"，把一张 RGBA8 颜色纹理挂到
		//   GL_DEPTH_ATTACHMENT 上 → GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT (0x8CD6)。
		//   而且这种错误是静默的：句柄合法、只是内容不对。
		struct AttachmentRecord {
			std::string   name;
			TextureHandle handle{};
			TextureDesc   desc{};
			bool          isDepth    = false;
			// true  = 本目标创建并拥有，析构时要销毁
			// false = imported（复用别的目标的附件），析构时不能碰
			bool          owned      = true;
			uint32_t      colorIndex = 0;
			uint32_t      fixedWidth  = 0;
			uint32_t      fixedHeight = 0;
		};
		struct TargetRecord {
			std::vector<AttachmentRecord> attachments;
			uint32_t colorAttachmentCount = 0;
			// 默认帧缓冲：附件不由设备管理，FBO 为 0
			bool     isBackBuffer = false;
		};

		SlotTable<GLShader>       shaders_;
		SlotTable<GLTexture>      textures_;
		SlotTable<GLMeshBuffers>  meshes_;
		SlotTable<GLRenderTarget> targets_;

		std::unordered_map<uint32_t, TargetRecord> targetRecords_;

		GLCommandList commands_;
		uint32_t      width_ = 0, height_ = 0;

		MeshHandle fullscreenQuad_{};

		// 默认帧缓冲的句柄，在构造函数里注册（内部 FBO 为 0）
		RenderTargetHandle backBuffer_{};

		// 把 TargetRecord 转成 GLRenderTarget 需要的挂接列表
		std::vector<AttachmentBinding> MakeBindings(const TargetRecord&) const;
		// 按固定尺寸/窗口尺寸重新分配某个渲染目标的全部附件存储（句柄不变）
		void ReallocateTarget(TargetRecord&, uint32_t width, uint32_t height);
		void CreateFullscreenQuad();
	};

} // namespace nova_engine::render::gl
```

### `gl_device.cpp`（关键部分）

```cpp
#include "gl_device.h"

#include "gl_enums.h"

#include <render/texture.h>   // render::LoadTexture / TextureData

#include <log/logger.h>

#include <stdexcept>

namespace nova_engine::render::gl {

	using namespace nova_engine::render;

	GLDevice::GLDevice() : commands_(*this) {
		// ★ 给默认帧缓冲注册一个真实句柄。
		//
		// 不要用空句柄（id == 0）代表默认帧缓冲 —— 那样「默认帧缓冲」和
		// 「CreateRenderTarget 失败」是同一个值，调用方校验资源时会误报。
		//
		// GLRenderTarget 的 fbo_ 保持 0，FramebufferOf 自然返回默认帧缓冲。
		{
			const uint32_t id = targets_.Add(GLRenderTarget{});
			TargetRecord rec;
			rec.isBackBuffer = true;
			targetRecords_[id] = std::move(rec);
			backBuffer_ = RenderTargetHandle{ id };
		}

		CreateFullscreenQuad();
	}

	GLDevice::~GLDevice() {
		// 顺序：先释放依赖设备的缓存，再释放资源
		commands_.ReleaseCaches();

		for (uint32_t id = 1; id <= meshes_.Capacity(); ++id) {
			if (GLMeshBuffers* m = meshes_.Get(id)) m->Destroy();
		}
		for (uint32_t id = 1; id <= textures_.Capacity(); ++id) {
			if (GLTexture* t = textures_.Get(id)) t->Destroy();
		}
		for (uint32_t id = 1; id <= targets_.Capacity(); ++id) {
			if (GLRenderTarget* rt = targets_.Get(id)) rt->Destroy();
		}
		for (uint32_t id = 1; id <= shaders_.Capacity(); ++id) {
			if (GLShader* s = shaders_.Get(id)) s->Destroy();
		}
	}

	// ===========================================================================
	// 网格
	// ===========================================================================

	MeshHandle GLDevice::CreateMesh(const MeshData& data) {
		GLMeshBuffers buffers;
		buffers.UploadVoxel(data);
		return MeshHandle{ meshes_.Add(std::move(buffers)) };
	}

	void GLDevice::UpdateMesh(MeshHandle handle, const MeshData& data) {
		if (GLMeshBuffers* m = meshes_.Get(handle.id)) m->UploadVoxel(data);
	}

	void GLDevice::DestroyMesh(MeshHandle handle) {
		if (GLMeshBuffers* m = meshes_.Get(handle.id)) m->Destroy();
		meshes_.Remove(handle.id);
	}

	// ===========================================================================
	// 纹理
	// ===========================================================================

	TextureHandle GLDevice::CreateTexture(const TextureDesc& desc,
	                                      std::span<const std::byte> pixels) {
		GLTexture texture;
		if (!texture.Create2D(desc, pixels.empty() ? nullptr : pixels.data())) {
			NE_CORE_ERROR("RenderGL", "CreateTexture failed ({}x{})", desc.width, desc.height);
			return {};
		}
		return TextureHandle{ textures_.Add(std::move(texture)) };
	}

	TextureHandle GLDevice::CreateTextureArray(const TextureArrayDesc& desc) {
		// 与改动前 GLTextureArray 的构造一致：按 desc.sources 逐个读文件。
		// 资产加载仍在后端是个已知折中：将来要异步加载时，
		// 把 desc.sources 换成"已解码的像素指针数组"即可，接口形状不变。
		std::vector<TextureData> loaded;
		std::vector<const void*> pixels;
		loaded.reserve(desc.sources.size());
		pixels.reserve(desc.sources.size());

		for (const TextureArrayLayer& layer : desc.sources) {
			loaded.push_back(LoadTexture(layer.path.c_str()));
			pixels.push_back(loaded.back().data());
		}

		// ★ tileSize 推导必须在设备层做。
		//
		//   TextureArrayDesc::tileSize 的注释写着「0 = 由第一张图决定」，
		//   但调用方（framework）不加载图片就不可能知道边长 ——
		//   这个推导只能在这里完成。
		//
		//   漏掉它的后果非常隐蔽：glTexImage3D(..., 0, 0, layers, ...) 是**合法**的
		//   （创建零尺寸纹理），glTexSubImage3D 尺寸为 0 也是**合法**的（不传输像素），
		//   两者都不产生 GL 错误，只是纹理数组一个像素都没有 ——
		//   所有采样变成未定义行为，画面全错但日志干净。
		TextureArrayDesc effective = desc;
		if (effective.tileSize == 0) {
			if (loaded.empty() || loaded[0].width() <= 0) {
				NE_CORE_ERROR("RenderGL",
					"CreateTextureArray: cannot derive tile size (no valid source images)");
				return {};
			}
			effective.tileSize = static_cast<uint32_t>(loaded[0].width());
			NE_CORE_INFO("RenderGL", "CreateTextureArray: tileSize {}x{} derived from '{}'",
			             effective.tileSize, loaded[0].height(), desc.sources[0].path);
		}

		// 每层尺寸必须一致，否则子图上传会错位
		for (std::size_t i = 0; i < loaded.size(); ++i) {
			if (static_cast<uint32_t>(loaded[i].width())  != effective.tileSize ||
			    static_cast<uint32_t>(loaded[i].height()) != effective.tileSize) {
				NE_CORE_ERROR("RenderGL",
					"CreateTextureArray: layer '{}' is {}x{}, expected {}x{}",
					desc.sources[i].path, loaded[i].width(), loaded[i].height(),
					effective.tileSize, effective.tileSize);
				return {};
			}
		}

		// ★ layers 与实际图片数不符时以实际为准。
		//   TextureDesc::layers 默认是 1 —— 如果调用方只填了 sources 忘了填 layers，
		//   glTexImage3D 只会分配 1 层，且 GLTexture::CreateArray 的上传循环
		//   （i < layerPixels.size() && i < desc.layers）只会处理第 0 层。
		if (effective.layers != loaded.size()) {
			NE_CORE_WARN("RenderGL",
				"CreateTextureArray: layers={} but {} images; using {}",
				effective.layers, loaded.size(), loaded.size());
			effective.layers = static_cast<uint32_t>(loaded.size());
		}

		GLTexture texture;
		if (!texture.CreateArray(effective, pixels)) {
			NE_CORE_ERROR("RenderGL", "CreateTextureArray failed");
			return {};
		}
		return TextureHandle{ textures_.Add(std::move(texture)) };
	}

	void GLDevice::DestroyTexture(TextureHandle handle) {
		if (GLTexture* t = textures_.Get(handle.id)) t->Destroy();
		textures_.Remove(handle.id);
	}

	// ===========================================================================
	// 着色器
	// ===========================================================================

	ShaderHandle GLDevice::CreateShader(const ShaderDesc& desc) {
		GLShader shader;
		if (!shader.Create(desc)) return {};
		return ShaderHandle{ shaders_.Add(std::move(shader)) };
	}

	void GLDevice::DestroyShader(ShaderHandle handle) {
		if (GLShader* s = shaders_.Get(handle.id)) s->Destroy();
		shaders_.Remove(handle.id);
	}

	// ===========================================================================
	// 渲染目标
	// ===========================================================================

	RenderTargetHandle GLDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
		TargetRecord record;

		for (const RenderTargetAttachment& a : desc.attachments) {
			AttachmentRecord att;
			att.name        = a.name;          // ★ 必须记录，GetAttachment 靠它查
			att.isDepth     = a.isDepth;
			att.colorIndex  = a.index;
			att.fixedWidth  = a.fixedWidth;
			att.fixedHeight = a.fixedHeight;

			if (a.imported.valid()) {
				// 复用已有纹理：WBOIT 的 OITFrame 复用 MainFrame 的深度附件
				if (textures_.Get(a.imported.id) == nullptr) {
					NE_CORE_ERROR("RenderGL",
						"CreateRenderTarget: imported attachment '{}' has an invalid handle",
						a.name);
					return {};
				}
				att.handle = a.imported;
				att.owned  = false;            // ★ 不归本目标所有，析构时不销毁
			} else {
				TextureDesc td;
				td.width  = a.fixedWidth  ? a.fixedWidth  : width_;
				td.height = a.fixedHeight ? a.fixedHeight : height_;
				td.format = a.format;
				td.defaultSampler = { TextureFilter::Linear, TextureFilter::Linear,
				                      TextureWrap::ClampToEdge };
				// 阴影贴图（固定尺寸的深度附件）需要深度比较模式与边框色，
				// 对应 frame_buffer_library.cpp:28-31
				td.depthCompareMode = (a.isDepth && a.fixedWidth != 0);

				// 尺寸为 0 会创建一个空纹理，挂到 FBO 上就是
				// GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT。这里提前拦住，
				// 否则错误会在几层之外才浮现。
				if (td.width == 0 || td.height == 0) {
					NE_CORE_ERROR("RenderGL",
						"CreateRenderTarget: attachment '{}' has zero size "
						"(did you forget device.OnResize before creating resources?)",
						a.name);
					return {};
				}

				att.desc   = td;
				att.handle = CreateTexture(td, {});
				att.owned  = true;

				if (!att.handle.valid()) {
					NE_CORE_ERROR("RenderGL",
						"CreateRenderTarget: failed to create texture for attachment '{}'",
						a.name);
					return {};
				}
			}

			if (!a.isDepth) ++record.colorAttachmentCount;
			record.attachments.push_back(std::move(att));
		}

		GLRenderTarget target;
		if (!target.Create(MakeBindings(record))) {
			NE_CORE_ERROR("RenderGL", "CreateRenderTarget failed");
			return {};
		}

		const uint32_t id = targets_.Add(std::move(target));
		targetRecords_[id] = std::move(record);
		return RenderTargetHandle{ id };
	}

	void GLDevice::DestroyRenderTarget(RenderTargetHandle handle) {
		const auto it = targetRecords_.find(handle.id);
		if (it != targetRecords_.end()) {
			for (const AttachmentRecord& att : it->second.attachments) {
				// ★ 用显式的 owned 判断，不要用 desc.width != 0 之类的巧合条件
				if (att.owned) DestroyTexture(att.handle);
			}
			targetRecords_.erase(it);
		}
		if (GLRenderTarget* rt = targets_.Get(handle.id)) rt->Destroy();
		targets_.Remove(handle.id);
	}

	// ★ 按名字查找。
	//
	// 这里曾经用 attachments.front() 占位，后果是：
	//   GetAttachment(mainFrame, "DepthTex") -> ColorTex
	//     → OITFrame 把 RGBA8 颜色纹理挂到 GL_DEPTH_ATTACHMENT
	//     → GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT (0x8CD6)
	//   GetAttachment(oitFrame, "RevealTex") -> AccumTex
	//     → 后处理通道把 accum 采样两次，WBOIT 合成错误（且不报任何错）
	//
	// 教训：占位实现返回「合法但错误」的句柄，比返回无效句柄危险得多 ——
	//       前者静默出错，后者会立刻炸。
	TextureHandle GLDevice::GetAttachment(RenderTargetHandle handle,
	                                      std::string_view name) const {
		const auto it = targetRecords_.find(handle.id);
		if (it == targetRecords_.end()) {
			NE_CORE_ERROR("RenderGL", "GetAttachment: render target not found");
			return {};
		}

		for (const AttachmentRecord& att : it->second.attachments) {
			if (att.name == name) return att.handle;
		}

		NE_CORE_ERROR("RenderGL", "GetAttachment: attachment '{}' not found", name);
		return {};
	}

	std::vector<AttachmentBinding> GLDevice::MakeBindings(const TargetRecord& rec) const {
		std::vector<AttachmentBinding> bindings;
		bindings.reserve(rec.attachments.size());
		for (const AttachmentRecord& a : rec.attachments) {
			GLuint id = 0;
			if (GLTexture* t = textures_.Get(a.handle.id)) id = t->id();
			bindings.push_back({ {}, id, a.isDepth, a.colorIndex });
		}
		return bindings;
	}

	GLuint GLDevice::FramebufferOf(RenderTargetHandle handle) const {
		if (!handle.valid()) return 0;                       // 默认帧缓冲
		const GLRenderTarget* rt = targets_.Get(handle.id);
		return rt ? rt->fbo() : 0;
	}

	bool GLDevice::HasColorAttachment(RenderTargetHandle handle, uint32_t index) const {
		const auto it = targetRecords_.find(handle.id);
		if (it == targetRecords_.end()) return false;

		// ★ 默认帧缓冲只有 COLOR_ATTACHMENT0。
		//   不认这个特例的话，BeginRenderTarget(backBuffer, LoadOp::Clear, ...)
		//   里的清屏循环会一次都不执行（HasColorAttachment 全返回 false），
		//   默认帧缓冲永远不被清除 —— 只是后处理的全屏四边形盖满了才没暴露。
		if (it->second.isBackBuffer) return index == 0;

		for (const AttachmentRecord& a : it->second.attachments) {
			if (!a.isDepth && a.colorIndex == index) return true;
		}
		return false;
	}

	// ===========================================================================
	// 帧
	// ===========================================================================

	void GLDevice::BeginFrame(const FrameInfo& info) {
		width_  = info.backBufferWidth;
		height_ = info.backBufferHeight;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GLDevice::Present() {
		// 交换缓冲由平台层（GLFW）负责，见第 6.6 节的 Window 参考实现。
		// 之所以不在这里 swap：设备不应该知道窗口的存在。
	}

	// 见上方「与 resize 的配合」——必须是两阶段，且必须用 ReallocateStorage。
	void GLDevice::OnResize(uint32_t width, uint32_t height) {
		width_  = width;
		height_ = height;

		// 阶段 1：重新分配所有自有附件的存储。
		// GL id 不变，所以 GLRenderTarget 里已捕获的挂接记录依然有效。
		for (auto& [id, record] : targetRecords_) {
			ReallocateTarget(record, width, height);
		}

		// 阶段 2：全部纹理就位后再统一重新挂接，
		// 避免 unordered_map 遍历顺序导致「挂到旧纹理」。
		for (auto& [id, record] : targetRecords_) {
			if (GLRenderTarget* rt = targets_.Get(id)) {
				rt->Rebuild(MakeBindings(record));
			}
		}
	}

	void GLDevice::ReallocateTarget(TargetRecord& record, uint32_t width, uint32_t height) {
		for (AttachmentRecord& att : record.attachments) {
			if (att.desc.width == 0) continue;   // imported，不归本目标管理

			// 先把最终尺寸写回 desc，ReallocateStorage 与 MakeBindings 都以它为准
			att.desc.width  = att.fixedWidth  ? att.fixedWidth  : width;
			att.desc.height = att.fixedHeight ? att.fixedHeight : height;

			if (GLTexture* t = textures_.Get(att.handle.id)) {
				// ★ 必须用 ReallocateStorage，不能用 Create2D：
				//   后者会先 Destroy() 再 glGenTextures，产生新的 GL id，
				//   使 GLRenderTarget 里已捕获的挂接记录全部失效。
				t->ReallocateStorage(att.desc);
			}
		}
	}

	// ===========================================================================
	// 内建全屏四边形
	// ===========================================================================

	void GLDevice::CreateFullscreenQuad() {
		// 布局 VertexLayoutId::ScreenQuad: slot0 = pos2, slot1 = uv2
		//
		// ⚠️ 数据必须是**紧凑的 vec2**（每顶点 2 个 float，步长 8 字节），
		//    与 ScreenQuad 布局的 glVertexAttribPointer 步长一致。
		//
		//    踩过的坑：这里曾写成 vec3（每顶点 3 个 float），
		//    而 VAO 按步长 8 字节读取 → 实际取到的顶点是：
		//      索引 0 -> (-1, 1)   索引 1 -> (0, -1)
		//      索引 2 -> ( 0, 1)   索引 3 -> (-1, 0)
		//    画出几个带尖角的三角块。症状是
		//    「改顶点数据画面会变，但形状和顶点对不上」—— 因为数据确实在被读，
		//    只是步长错了一半。
		//
		//    改动前 post_process_renderer.cpp:8-31 用的是**交错**格式
		//    （每顶点 4 个 float：x,y,u,v，步长 16 字节）+ 6 顶点无索引。
		//    改成 SoA + 索引之后，这个步长约定必须重新对齐。
		MeshData quad;
		quad.vertices = {
			-1.0f,  1.0f,     // 左上
			-1.0f, -1.0f,     // 左下
			 1.0f, -1.0f,     // 右下
			 1.0f,  1.0f,     // 右上
		};
		quad.uvws = {
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
		};
		quad.indices = { 0, 1, 2, 0, 2, 3 };

		GLMeshBuffers buffers;
		buffers.UploadScreenQuad(quad);
		fullscreenQuad_ = MeshHandle{ meshes_.Add(std::move(buffers)) };
	}

} // namespace nova_engine::render::gl
```

> **已补齐**：`GetAttachment` 现在按名字查找（`AttachmentRecord` 里存了 `name`）。
>
> 早期版本用 `attachments.front()` 占位，实测会踩到两个后果：
>
> | 调用 | 占位返回 | 应为 | 后果 |
> |---|---|---|---|
> | `GetAttachment(mainFrame, "DepthTex")` | `ColorTex` | `DepthTex` | RGBA8 颜色纹理挂到 `GL_DEPTH_ATTACHMENT` → `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT` (**0x8CD6**) |
> | `GetAttachment(oitFrame, "RevealTex")` | `AccumTex` | `RevealTex` | 后处理把 accum 采样两次 → WBOIT 合成错误，**不报任何错** |
>
> 第二行是这次最有价值的教训：**占位实现返回「合法但错误」的句柄，比返回无效句柄危险得多** —— 后者立刻炸，前者静默出错。所以现在 `GetAttachment` 找不到名字时会 `NE_CORE_ERROR` 并返回空句柄，而不是悄悄给一个别的附件。

## 5.11 `Engine/render/gl/imgui_gl_backend.h` / `.cpp`

**原理**：把 `imgui_impl_opengl3` + `imgui_impl_glfw` 从 `ui/` 挪到 `render/gl/`。

改动前 `debug_ui_manager.h:4-5` 直接 include 两个后端头文件，于是**整个 `ui/` 目录都被 GL 绑住**。抽出一个 `IUiBackend` 接口之后：

- `ui/` 只 include `imgui.h`（IMGUI 核心，不含后端）
- GL 后端实现在 `render/gl/`
- 将来加游戏 UI 时，`IUiBackend` 是现成的接入点

```cpp
#pragma once

#include <ui/ui_backend.h>

struct GLFWwindow;

namespace nova_engine::render::gl {

	// ImGui 的 OpenGL3 + GLFW 后端。
	// 这是全项目唯一 include imgui_impl_* 的地方。
	class ImguiGLBackend final : public ui::IUiBackend {
	public:
		explicit ImguiGLBackend(GLFWwindow* window) : window_(window) {}
		~ImguiGLBackend() override { Shutdown(); }

		bool Init() override;
		void Shutdown() override;
		void NewFrame() override;
		void RenderDrawData() override;

	private:
		GLFWwindow* window_ = nullptr;
		bool        initialized_ = false;
	};

} // namespace nova_engine::render::gl
```

```cpp
// imgui_gl_backend.cpp
#include "imgui_gl_backend.h"

#include <log/logger.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>

namespace nova_engine::render::gl {

	bool ImguiGLBackend::Init() {
		if (initialized_) return true;

		// glsl 版本号不再由上层传入 —— 它是 GL 后端的知识
		if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)
		    || !ImGui_ImplOpenGL3_Init("#version 330")) {
			NE_CORE_ERROR("ImguiGL", "Failed to init ImGui GL backend");
			return false;
		}

		initialized_ = true;
		return true;
	}

	void ImguiGLBackend::Shutdown() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		initialized_ = false;
	}

	void ImguiGLBackend::NewFrame() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void ImguiGLBackend::RenderDrawData() {
		if (!initialized_) return;
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

} // namespace nova_engine::render::gl
```

---

# 6. 需要修改的既有文件（参考实现）

> **本节代码不在本次交付范围内执行**，仅作为落地时的参考。每一处都标注了改动的**原因**，不涉及渲染结果的改动会明确说明。

> ## ⚠️ 本节是「改动点摘录」，不是完整文件
>
> 下面每个小节里的代码块**只包含需要改动的那部分函数**。把整段代码直接覆盖到原文件上会**丢掉未展示的函数体**，症状是一堆 `LNK2019 无法解析的外部符号` —— 因为头文件里的声明还在，实现没了。
>
> **正确用法**：
> 1. 打开原文件，对照摘录**逐函数替换**
> 2. 摘录里没出现的函数，**原样保留**
> 3. 每个小节末尾的「保留原样」清单列出了该文件里不该动的成员
>
> **链接错误自检**：改造后如果出现 `LNK2019`，符号名会直接告诉你哪个函数丢了实现 —— 这是最快的排查方式，比逐个文件肉眼比对可靠。例如：
>
> ```
> LNK2019: 无法解析的外部符号 "public: void __cdecl
>   nova_engine::render::system::RenderSystem::Shutdown(void)"
>   (?Shutdown@RenderSystem@system@render@nova_engine@@QEAAXXZ)
> ```
>
> 说明 `RenderSystem::Shutdown` 有声明、无定义。

## 6.1 新增 `Engine/voxel/chunk/dirty_region_queue.h`

**原理**：这是整个改造里**唯一一处真正的架构方向修正**（其余都是搬迁）。

改动前 `input_system.cpp:63-95` 是「玩家改了体素 → 直接调用渲染层的 `RequireMeshUpdate`」，导致 `core/` 依赖 `render/opengl/`。

正确做法是「写数据、由消费方来取」：输入系统只往 `World` 里写脏区域，渲染系统自己来读。这样：

- `core/input` 不再认识渲染层的任何类型
- 脏区域成为 `World` 的一部分，将来物理/网络/存档系统都可以读取或产生它

```cpp
#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace nova_engine::voxel::chunk {

	// 区块网格失效标记队列。
	//
	// 纯数据：只记录"哪些区块的网格需要重建"，不关心谁来消费、怎么重建。
	// 这正是 World「只存储数据不做逻辑」理念的直接应用。
	class DirtyRegionQueue {
	public:
		// radiusChunks 表示以 pos 所在区块为中心的邻域半径（0 = 只有自己）
		void MarkDirty(const glm::ivec3& pos, int radiusChunks = 0);

		// 取出并清空全部标记
		std::vector<glm::ivec3> Drain();

		bool Empty() const { return pending_.empty(); }
		std::size_t Size() const { return pending_.size(); }

	private:
		std::vector<glm::ivec3> pending_;
	};

} // namespace nova_engine::voxel::chunk
```

## 6.2 `Engine/core/world/world.h`

**改动**：加一行。

```cpp
#pragma once

#include <string>
#include <voxel/chunk/chunk_data.h>
#include <voxel/chunk/dirty_region_queue.h>   // ← 新增
#include <voxel/planet/planet_player_data.h>
#include <voxel/planet/planet.h>

namespace nova_engine::core::world {
	// world是游戏中所有核心数据的集合，只存储数据和状态，不进行任何逻辑处理，
	// 所有的逻辑处理都应该在系统中进行
	struct World {
		nova_engine::voxel::chunk::ChunkData chunkData;

		// 区块网格失效标记（纯数据）。输入系统写入，渲染系统消费。
		nova_engine::voxel::chunk::DirtyRegionQueue dirtyRegions;   // ← 新增

		glm::vec3 playerPos = {0,128,0};
		glm::vec3 playerRot = { 0,0,1 };
		// ... 其余字段完全不变
	};
}
```

## 6.3 `Engine/core/input/input_system.h` / `.cpp`

### 头文件：删掉对渲染层的依赖

```cpp
#pragma once

#include "input_state.h"
#include <core/world/world.h>
// ← 删除: #include <render/opengl/context/render_context.h>

namespace nova_engine::core::input {
	class InputSystem {
	public:
		static InputSystem& GetInstance() {
			static InputSystem instance;
			return instance;
		}

		InputState& GetState() { return state_; }

		// ← 参数从 render::opengl::context::RenderContext& 改为什么都不传
		void Update(double time, world::World& world);

		InputSystem(const InputSystem&) = delete;
		InputSystem& operator=(const InputSystem&) = delete;
	private:
		InputState state_;
		InputSystem() = default;
		~InputSystem() = default;
	};
}
```

### 实现：把两处 `RequireMeshUpdate` 换成写脏区域

```cpp
// input_system.cpp —— 关键片段
// ← 删除: #include <render/opengl/library/camera_library.h>
// ← 删除: using namespace nova_engine::render::opengl;

void InputSystem::Update(double time, world::World& world) {

	// ... 移动、朝向、射线检测（完全不变） ...

			if (voxel < 0.0f) {
				world.selectedBlock = { pos.x, pos.y, pos.z };

				if (state_.destroyBlock) {
					chunk->SetVoxel(pos.x & 31, pos.y & 31, pos.z & 31,
					                utils::assist::Pack(1.0, 0));
					state_.destroyBlock = false;

					// ← 替换原来的：
					//   auto layer = context.chunkMesh.GetLayer(Default);
					//   if (layer) { for nx,ny,nz ... layer->RequireMeshUpdate(...); }
					world.dirtyRegions.MarkDirty(pos, /*radiusChunks=*/1);
				}

				if (state_.buildBlock) {
					// ... SetVoxel（完全不变）...
					world.dirtyRegions.MarkDirty(pos, /*radiusChunks=*/1);
				}

				// ... 其余不变 ...
			}

	state_.buildBlock = false;
	state_.destroyBlock = false;
}
```

> **语义等价性**：改动前 `input_system.cpp:66-70` 的循环是 `pos + n*2`，`n ∈ {-1,0,1}`，落在三个轴上 → 影响 `pos + {-2,0,2}³` 这 27 个偏移。由于 `RequireMeshUpdate` 内部会按 `CHUNK_BIT1` 对齐（`chunk_mesh_cache.h:35-38`），偏移 2 与偏移 1 在对齐后可能落在同一区块或相邻区块。`MarkDirty(pos, radius=1)` 的消费端需要复刻同样的偏移集合，见 6.4。

> ⚠️ 如果你希望**绝对零行为变化**，`MarkDirty` 的消费端应当直接照抄原来的 `pos + n*2` 三重循环。这里把偏移语义抽象成 `radiusChunks` 是为了让接口干净；落地时请先验证画面一致，再考虑是否简化。

## 6.4 `Engine/render/scene/chunk_mesh_registry.h` / `.cpp`（增补）

在 3.3 的基础上，增加一个从 `World` 消费脏区域的入口：

```cpp
// chunk_mesh_registry.h 追加
class ChunkMeshRegistry {
public:
	// ... 3.3 的全部成员 ...

	// 从 World 的脏区域队列拉取标记并转化为网格重建请求。
	// 渲染侧主动来取，而不是输入系统推过来 —— 依赖方向由此掰正。
	void ConsumeDirty(core::world::World& world);
};
```

```cpp
// chunk_mesh_registry.cpp 追加
#include <core/world/world.h>

void ChunkMeshRegistry::ConsumeDirty(core::world::World& world) {
	for (const glm::ivec3& p : world.dirtyRegions.Drain()) {
		// 复刻改动前 input_system.cpp:66-70 的 pos + n*2 三重循环，
		// 保证网格重建的覆盖范围与改造前完全一致
		for (int nx = -1; nx <= 1; ++nx)
			for (int ny = -1; ny <= 1; ++ny)
				for (int nz = -1; nz <= 1; ++nz)
					RequireMeshUpdate(p.x + nx * 2, p.y + ny * 2, p.z + nz * 2);
	}
}
```

## 6.5 `Engine/render/system/render_system.h` / `.cpp`

这是改动量最大的一个文件：**`Render()` 整体删除，`Update()` 改名 `BuildView()`**。

### 头文件

```cpp
#pragma once

#include <core/world/world.h>
#include <render/core/camera.h>
#include <render/core/render_settings.h>
#include <render/scene/chunk_mesh_registry.h>
#include <render/scene/lod_mesh_registry.h>
#include <render/scene/render_view.h>

#include <core/concurrency/worker.h>
#include "lod_system.h"

#include <memory>
#include <unordered_set>

namespace nova_engine::render::device { class IRenderDevice; }

namespace nova_engine::render::system {

	struct MeshBuildResult {
		int x = 0, y = 0, z = 0;
		MeshData value;
	};

	class RenderSystem {
	public:
		static RenderSystem& GetInstance() {
			static RenderSystem instance;
			return instance;
		}

		// 构造不再需要 GL 上下文。设备由外部注入（Window 是组装点）。
		void Init(device::IRenderDevice& device);
		void Shutdown();

		~RenderSystem() { meshBuildWorker_.Stop(); }

		// ← 取代原来的 Update(world, context) + Render(context)
		// 只做「逻辑更新 + 产出渲染视图」，一个 gl* 都没有。
		scene::RenderView BuildView(core::world::World& world,
		                            const RenderSettings& settings);

		const LodSystem& GetLodSystem() const { return lodSystem_; }

	private:
		core::concurrency::Worker<MeshBuildResult> meshBuildWorker_{"MeshBuildWorker", 512, 512};
		std::unordered_set<glm::ivec3> existTasks_;

		std::unique_ptr<scene::ChunkMeshSet>     chunkMesh_;
		std::unique_ptr<scene::LodMeshRegistry>  lodMesh_;

		// 取代改动前 CameraLibrary 的 "MainCamera" / "ShadowCamera" 两个字符串键条目
		Camera mainCamera_;
		Camera shadowCamera_;

		scene::RenderView view_{};

		RenderSystem() { meshBuildWorker_.Start(); }

		void SubmitMeshRequire(voxel::chunk::Chunk<float>*,
			const std::array<const nova_engine::voxel::chunk::Chunk<float>*, 7>& nei,
			int cx, int cy, int cz);

		void UpdateShadowCamera(const core::world::World& world,
		                        const RenderSettings& settings);

		LodSystem lodSystem_;
	};
}
```

### 实现：`Render()` 删除，`Update()` 改名

改动前 `render_system.cpp:13-149` 那 137 行整体删除（内容已经变成 5 个 Pass）。

```cpp
// render_system.cpp —— BuildView 的骨架（省略与改动前完全一致的部分）

scene::RenderView RenderSystem::BuildView(core::world::World& world,
                                          const RenderSettings& settings) {
	// ---- 1. 消费脏区域（取代 input 系统反向调用 RequireMeshUpdate）----
	scene::ChunkMeshRegistry* meshCache  = chunkMesh_->Layer(scene::MeshLayer::Default);
	scene::ChunkMeshRegistry* meshCache2 = chunkMesh_->Layer(scene::MeshLayer::Lucency);
	if (!meshCache || !meshCache2) return view_;
	meshCache->ConsumeDirty(world);

	// ---- 2. 远处的网格回收（逻辑与改动前 render_system.cpp:165-173 一致）----
	glm::ivec3 playerPos = world.playerPos;
	auto& meshs = const_cast<std::vector<scene::MeshInstance>&>(meshCache->GetAllMesh());
	for (const auto& mesh : meshs) {
		auto chunk = world.planet.chunkData.FindChunk(
			(int)mesh.position.x, (int)mesh.position.y, (int)mesh.position.z);
		int dis = glm::length2(mesh.position - (glm::vec3)playerPos);
		if (dis > 192 * 192 || !chunk) {
			meshCache->FreeMesh((int)mesh.position.x, (int)mesh.position.y, (int)mesh.position.z);
			meshCache2->FreeMesh((int)mesh.position.x, (int)mesh.position.y, (int)mesh.position.z);
		}
	}

	// ---- 3. 提交网格构建任务（逻辑不变，仅类型替换）----
	//     原 render_system.cpp:174-226，完全保留

	// ---- 4. 回收构建结果并上传 ----
	core::concurrency::Result<MeshBuildResult> out;
	while (meshBuildWorker_.TryPopResult(out)) {
		const int cx = out.value.x, cy = out.value.y, cz = out.value.z;
		meshCache->SubmitMesh(cx, cy, cz, out.value.value);   // ← 内部走 IRenderDevice
		existTasks_.erase({cx, cy, cz});
	}

	// ---- 5. 更新相机（逻辑与改动前 render_system.cpp:246-275 一致）----
	mainCamera_.position = world.playerPos;
	mainCamera_.rotation = world.playerRot;
	UpdateShadowCamera(world, settings);

	// ---- 6. 产出渲染视图 ----
	view_.camera        = &mainCamera_;
	view_.shadowCamera  = &shadowCamera_;
	view_.settings      = &settings;
	view_.chunkMesh     = chunkMesh_.get();
	view_.lodMesh       = settings.useLod ? lodMesh_.get() : nullptr;

	view_.mainOrigin    = mainCamera_.position;
	view_.shadowOrigin  = shadowCamera_.position;

	// 两个矩阵语义不同，分别对应改动前两条代码路径，见 3.5 节
	view_.shadowViewProj     = shadowCamera_.GetOrthoMatrix(32.0f)
	                         * shadowCamera_.GetRelativeViewMatrix();
	view_.shadowSampleMatrix = shadowCamera_.GetOrthoMatrix(32.0f)
	                         * shadowCamera_.GetRelativeViewMatrix(mainCamera_.position);
	return view_;
}
```

#### `Init` / `Shutdown` / `UpdateShadowCamera`（最容易漏掉的部分）

头文件里声明了但上面的 `BuildView` 骨架里没出现 —— 只贴声明不贴定义会产生 `LNK2019`。完整实现如下。

```cpp
// render_system.cpp

void RenderSystem::Init(device::IRenderDevice& device) {
	if (chunkMesh_) return;   // 幂等：重复调用不重建

	chunkMesh_ = std::make_unique<scene::ChunkMeshSet>(
		device, static_cast<uint32_t>(scene::MeshLayer::Count));
	lodMesh_ = std::make_unique<scene::LodMeshRegistry>(device);

	// LodSystem 本身不需要 device —— 它只产出 MeshData，
	// 真正的 GPU 上传由 LodMeshRegistry 内部通过 device 完成。
	NE_CORE_INFO("RenderSystem", "Initialized on {}", device.Name());
}

void RenderSystem::Shutdown() {
	lodMesh_.reset();
	chunkMesh_.reset();
	// meshBuildWorker_ 的生命周期由 RenderSystem 的构造/析构管理，这里不动。
}

// 逐行对应改动前 render_system.cpp:252-275 的阴影相机更新。
// 所有魔法数字改为从 RenderSettings 读取。
void RenderSystem::UpdateShadowCamera(const core::world::World& world,
                                      const RenderSettings& settings) {
	const float orthoSize     = settings.shadowOrthoSize;
	const int   shadowRes     = static_cast<int>(settings.shadowResolution);
	const float texelWorldPos = (2.0f * orthoSize) / static_cast<float>(shadowRes);

	const glm::vec3 lightDir = glm::normalize(settings.lightDirection);
	const float     distance = settings.shadowDistance;

	const glm::vec3 lightPos  = world.playerPos + lightDir * distance;
	const glm::mat4 lightView = glm::lookAt(lightPos, world.playerPos,
	                                        glm::vec3(0.0f, 1.0f, 0.0f));

	// 纹素对齐：把玩家位置吸附到光空间的纹素网格上，消除相机移动时的阴影抖动。
	// 与改动前一致，只对齐 x / y（光空间的横纵轴），不对齐 z（光轴方向）。
	glm::vec4 playerInLightSpace = lightView * glm::vec4(world.playerPos, 1.0f);
	playerInLightSpace.x = glm::floor(playerInLightSpace.x / texelWorldPos) * texelWorldPos;
	playerInLightSpace.y = glm::floor(playerInLightSpace.y / texelWorldPos) * texelWorldPos;

	const glm::mat4 invLightView  = glm::inverse(lightView);
	const glm::vec4 alignedPlayer = invLightView * playerInLightSpace;
	const glm::vec3 alignedTarget = glm::vec3(alignedPlayer);

	shadowCamera_.position = alignedTarget + lightDir * distance;
	shadowCamera_.rotation = -lightDir;   // 与改动前一致：朝向取反
}
```

`BuildView` 里第 5 步调用它，并把两个光空间矩阵算出来：

```cpp
	// BuildView 内部的第 5 步（对应上面 6.5 骨架的第 5 段）
	mainCamera_.position = world.playerPos;
	mainCamera_.rotation = world.playerRot;

	UpdateShadowCamera(world, settings);   // ★ 上面这个函数

	view_.camera       = &mainCamera_;
	view_.shadowCamera = &shadowCamera_;
	view_.settings     = &settings;
	view_.chunkMesh    = chunkMesh_.get();
	view_.lodMesh      = settings.useLod ? lodMesh_.get() : nullptr;

	view_.mainOrigin   = mainCamera_.position;
	view_.shadowOrigin = shadowCamera_.position;

	// 两个矩阵语义不同，见 3.5 节
	const glm::mat4 shadowOrtho = shadowCamera_.GetOrthoMatrix(32.0f);
	view_.shadowViewProj     = shadowOrtho * shadowCamera_.GetRelativeViewMatrix();
	view_.shadowSampleMatrix = shadowOrtho
	                         * shadowCamera_.GetRelativeViewMatrix(mainCamera_.position);
```

**`RenderSystem` 里保留原样的部分**：

| 成员 | 处理 |
|---|---|
| `Render()` | ❌ **整个删除** —— 内容已拆成 5 个 Pass |
| `Update()` | 🔁 改名 `BuildView()`，去掉 `RenderContext&` 参数 |
| `SubmitMeshRequire()` | ✅ 原样保留 —— 只用到 `MeshData` 与 Worker，不含 GL |
| `GetLodSystem()` | ✅ 原样保留 |
| `RenderSystem()` / `~RenderSystem()` | ✅ 原样保留 |
| `meshBuildWorker_` / `existTasks_` / `lodSystem_` | ✅ 原样保留 |
| 文件级 `chunksPos` 静态向量、`firstLod` 变量 | ✅ 原样保留（`chunksPos` 被 `BuildView` 使用） |

**`Window::Update` 里的调用**：

```cpp
// 改动前
render::system::RenderSystem::GetInstance().Update(world, realContext);

// 改动后
view = render::system::RenderSystem::GetInstance().BuildView(world, settings_);
```

**视口尺寸**由 `Window` 在调用前写入（`view.viewportWidth/Height`），或者由 `FrameRenderer::Execute` 从 `RenderResources` 补上。

## 6.6 `Engine/window/window.h` / `.cpp`

### 头文件：完全不认识 GL

```cpp
#pragma once

#include <render/core/render_settings.h>
#include <render/framework/frame_renderer.h>
#include <render/framework/render_resources.h>
#include <render/scene/render_view.h>

#include <core/world/world.h>

#include <memory>

struct GLFWwindow;   // ← 前置声明即可，不再 include glad / glfw3.h

namespace nova_engine::render::device  { class IRenderDevice; }
namespace nova_engine::render::gl      { class ImguiGLBackend; }

namespace nova_engine::window {
	class Window {
	public:
		// ★ 定义在 window.cpp：函数内 static 的析构注册同样需要 ~Window 的定义
		static Window& GetInstance();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void Run();

	private:
		GLFWwindow* window_ = nullptr;

		// ★ 这两个类型在本头文件里是**不完整类型**（只前置声明），
		//   所以 ~Window() 必须在 window.cpp 里定义，不能在这里写 = default。
		//   否则任何 include 本头文件的 TU 都会实例化
		//   std::default_delete<T>::operator()，触发 C2027（见下方说明）。
		std::unique_ptr<render::device::IRenderDevice> device_;
		std::unique_ptr<render::gl::ImguiGLBackend>    uiBackend_;

		render::framework::RenderResources resources_{};
		render::framework::FrameRenderer   frameRenderer_;
		render::RenderSettings             settings_;

		core::world::World world_;
		render::scene::RenderView view_{};

		Window();
		~Window();                      // ★ 只声明，不要 = default

		void Init();
		void Load();
		void Render(double time);
		void Update(double time);
		void Unload();

		// GLFW 回调（签名不变）
		static void framebuffer_size_callback(GLFWwindow*, int w, int h);
		static void keyCallback(GLFWwindow*, int key, int, int action, int mods);
		static void mouseMoveCallBack(GLFWwindow*, double x, double y);
		static void mouseButtonCallBack(GLFWwindow*, int button, int action, int);
		static void scrollCallBack(GLFWwindow*, double dx, double dy);
		void processInput(GLFWwindow*);
	};
}
```

#### 为什么 `~Window()` 不能内联（C2027）

报错形式：

```
错误 C2027  使用了未定义类型"nova_engine::render::gl::ImguiGLBackend"
            文件 ...\VC\Tools\MSVC\...\include\memory    行 3337
```

关键线索是**报错文件是 `<memory>` 而不是 `window.h`**。原因链：

1. `window.h` 里 `IRenderDevice` 和 `ImguiGLBackend` 都只是**前置声明**（不完整类型）
2. `~Window() = default;` 写在类体里 → 隐式 inline → **每个包含 `window.h` 的 TU 都要实例化它**
3. 实例化 `~Window()` 要销毁 `std::unique_ptr<ImguiGLBackend>` → 实例化 `std::default_delete<T>::operator()`
4. 那里有 `static_assert(sizeof(T) > 0, ...)`（MSVC `<memory>` 第 3337 行）→ 类型不完整 → C2027
5. `main.cpp` 只 include `window.h`（为了调 `Window::GetInstance().Run()`），它不认识这两个类型

**这就是为什么报错位置（`<memory>`）和真正原因（`window.h` 的 `= default`）看起来毫无关系。**

修复只需一条规则：

> **任何持有 `std::unique_ptr<T>` 而 `T` 只被前置声明的类，其析构函数（以及任何会销毁该成员的隐式特殊成员函数）必须在 `.cpp` 中定义。**

判断方法：头文件里出现 `namespace X { class T; }` 且成员有 `std::unique_ptr<X::T>`，就不能在该头文件里写 `~Klass() = default;`。

`GetInstance()` 内部有函数内 static，其析构注册同样需要 `~Window()` 的定义，所以一并移到 `.cpp`。

> **本项目里只有 `Window` 命中这条规则**，其余持有 `unique_ptr` 的类都满足条件：
>
> | 类 | 成员 | 是否完整 |
> |---|---|---|
> | `RenderSystem` | `unique_ptr<scene::ChunkMeshSet>` / `LodMeshRegistry` | ✅ 头文件已 include 完整定义 |
> | `ChunkMeshSet` | `vector<unique_ptr<ChunkMeshRegistry>>` | ✅ `ChunkMeshRegistry` 在同文件上方定义 |
> | `GLDevice` | `SlotTable<GLShader>` 等 | ✅ 相关头文件已 include |
> | `FrameRenderer` | 5 个 Pass 按值持有 | ✅ 已 include |
> | **`Window`** | **`unique_ptr<IRenderDevice>` / `ImguiGLBackend`** | ❌ **只前置声明 → 必须外移析构** |

### 实现的关键片段

```cpp
// window.cpp
#include "window.h"

#include <render/gl/gl_context.h>          // CreateDevice
#include <render/device/render_device.h>   // ★ IRenderDevice 完整定义
#include <render/gl/imgui_gl_backend.h>    // ★ ImguiGLBackend 完整定义
#include <render/system/render_system.h>
#include <ui/debug_ui/debug_ui_manager.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// ← 删除全部: #include <render/opengl/...>

namespace nova_engine::window {

	namespace rd  = nova_engine::render;
	namespace rdd = nova_engine::render::device;

	// ★ 必须在这里（而不是 window.h）定义：
	//   此刻 device::IRenderDevice 与 gl::ImguiGLBackend 都已是完整类型，
	//   由上面的两个 include 提供。写在头文件里会触发 C2027。
	Window::Window() = default;
	Window::~Window() = default;

	Window& Window::GetInstance() {
		static Window instance;
		return instance;
	}

	void Window::Init() {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, 0);

		window_ = glfwCreateWindow(800, 600, "NovaEngine", nullptr, nullptr);
		if (window_ == nullptr) {
			NE_CORE_ERROR("GameCore", "Failed to create window");
			glfwTerminate();
			return;
		}

		// ← 取代原来的 glfwMakeContextCurrent + gladLoadGLLoader
		//    （window.cpp:240-246），GL 相关代码全部收进 render/gl
		device_ = rd::gl::CreateDevice(window_);
		if (!device_) {
			NE_CORE_ERROR("GameCore", "Failed to create render device");
			return;
		}

		glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
		glfwSetKeyCallback(window_, keyCallback);
		glfwSetCursorPosCallback(window_, mouseMoveCallBack);
		glfwSetMouseButtonCallback(window_, mouseButtonCallBack);
		glfwSetScrollCallback(window_, scrollCallBack);

		glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window_, 400, 300);
		glfwSwapInterval(0);
		glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		// ← 删除 glEnable(GL_DEPTH_TEST) / glDepthMask / glEnable(GL_CULL_FACE)
		//    / glCullFace / glEnable(GL_MULTISAMPLE)（window.cpp:259-263）
		//    这些都已成为 PipelineState 的一部分

		// ImGui 初始化：先建核心上下文，再交给后端
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();

		uiBackend_ = std::make_unique<rd::gl::ImguiGLBackend>(window_);
		if (!uiBackend_->Init()) {
			NE_CORE_ERROR("GameCore", "Failed to init UI backend");
		}
		ui::debug_ui::DebugUIManager::GetInstance().Init(*uiBackend_);
	}

	// 改动前 window.cpp:26-42：手工 fan-out 给 5 个 renderer + FrameBufferLibrary
	// 改动后是一次 OnResize 调用 —— 但必须判空。
	//
	// ★ 这个回调有可能在 device_ 赋值之前被窗口系统触发
	//   （glfwCreateWindow 之后、CreateDevice 返回之前的任何一次窗口调整，
	//    或者 CreateDevice 失败返回 nullptr）。
	//   直接写 Window::GetInstance().device_->OnResize(...) 就是对空指针调成员函数。
	void Window::framebuffer_size_callback(GLFWwindow*, int width, int height) {
		if (width == 0 || height == 0) return;

		Window& self = Window::GetInstance();   // 先取单例
		if (!self.device_) return;              // 再判空

		self.width_  = static_cast<uint32_t>(width);
		self.height_ = static_cast<uint32_t>(height);
		self.device_->OnResize(self.width_, self.height_);
	}

	// ★ 尺寸存成成员：viewport 是「窗口状态」而不是「场景状态」。
	//   在 Window 的私有区加：uint32_t width_ = 800, height_ = 600;
	//   若由 BuildView 决定 viewport，首帧就会是 0×0，
	//   glm::perspective(fov, 0.0f / 0, ...) 会产出 NaN 投影矩阵。

	void Window::Load() {
		// ← 删除 glViewport / glClearColor（window.cpp:281-282）
		//    清屏色现在是 RenderSettings::clearColor

		device_->OnResize(width_, height_);
		resources_ = rd::framework::CreateRenderResources(*device_, width_, height_);

		rd::system::RenderSystem::GetInstance().Init(*device_);

		// ★ 关键：进入主循环前先填充一次 view_。
		//
		//   Window::Run 的固定步长循环形如
		//       while (deltaTime >= FIXED_STEP_MS && loopCount < 5) Update(...);
		//       Render(alpha);          // ← 无条件执行
		//   首帧若 deltaTime 不足一个步长，Update() 整段跳过，
		//   此时 view_ 仍是默认构造 —— camera / settings / chunkMesh 全为 nullptr，
		//   Render() 会崩在 TransparentPass::Enabled 的 settings->useLucency，
		//   报「读取位置 0x0000000000000001」（useLucency 在 RenderSettings 偏移 1）。
		view_ = rd::system::RenderSystem::GetInstance().BuildView(world_, settings_);

		// ... 调试面板注册（不变）...
	}

	void Window::Update(double time) {
		core::input::InputSystem::GetInstance().Update(time, world_);   // ← 少一个参数
		voxel::chunk::ChunkSystem::GetInstance().Update(world_);

		view_ = rd::system::RenderSystem::GetInstance().BuildView(world_, settings_);
		// viewport 不在这里设 —— 见 Render()
	}

	void Window::Render(double /*alpha*/) {
		// viewport 每帧从窗口尺寸同步，不依赖 Update() 是否执行过
		view_.viewportWidth  = width_;
		view_.viewportHeight = height_;

		auto& cmd = device_->Commands();
		device_->BeginFrame({ width_, height_, 0.0 });

		frameRenderer_.Execute(view_, resources_, cmd);   // 内部有 IsValid() 守卫

		if (settings_.debugUI) {
			ui::debug_ui::DebugUIManager::GetInstance().BeginFrame();
			ui::debug_ui::DebugUIManager::GetInstance().EndFrame();
		}

		glfwSwapBuffers(window_);   // ← 唯一的 platform 调用
	}

	void Window::Unload() {
		rd::system::RenderSystem::GetInstance().Shutdown();
		rd::framework::DestroyRenderResources(*device_, resources_);
		ui::debug_ui::DebugUIManager::GetInstance().Shutdown();
		ImGui::DestroyContext();
		uiBackend_.reset();
		device_.reset();
		glfwTerminate();
	}
}
```

#### ⚠️ 主循环里**不能丢**的两行

改写 `Window::Render` 时极易漏掉这两行 —— 它们原来在 `window.cpp:366, 368`，而且**漏掉后不会报错，只会让输入和面板数据变成死的**。

```cpp
void Window::Run() {
	Init();
	Load();

	// ★ 进入主循环前先填充一次 view_（见 Load()）

	double pastTime = glfwGetTime() * 1000.0;
	double deltaTime = 0;

	while (!glfwWindowShouldClose(window_)) {
		glfwPollEvents();          // ★★ 不能丢：不调它，所有回调都不触发
		processInput(window_);

		double currentTime = glfwGetTime() * 1000.0;
		double frameDelta  = currentTime - pastTime;
		pastTime = currentTime;
		if (frameDelta > MAX_ACCUM_MS) frameDelta = MAX_ACCUM_MS;
		deltaTime += frameDelta;

		int loopCount = 0;
		while (deltaTime >= FIXED_STEP_MS && loopCount < 5) {
			Update(FIXED_STEP_MS);
			deltaTime -= FIXED_STEP_MS;
			++loopCount;
		}

		Render(static_cast<float>(deltaTime / FIXED_STEP_MS));
	}
	Unload();
}

void Window::Render(double /*alpha*/) {
	fps = ImGui::GetIO().Framerate;   // ★★ 不能丢：调试面板的帧率来源

	view_.viewportWidth  = width_;
	view_.viewportHeight = height_;
	// ...
}
```

**丢掉 `glfwPollEvents()` 的症状**（都是静默的）：

| 症状 | 原因 |
|---|---|
| 玩家不动，`playerPos` 停在初值 | `keyCallback` 永不触发 → `InputState` 全 false |
| 面板显示的调试数据一成不变 | 输入状态不变，世界状态自然不变 |
| 窗口标题栏显示「无响应」，Esc 关不掉 | `glfwGetKey` 返回陈旧状态 |
| ImGui 面板鼠标无响应 | `ImGui_ImplGlfw_NewFrame` 收不到事件 |

**丢掉 `fps = ImGui::GetIO().Framerate;` 的症状**：面板永远显示 `1000.000 ms / frame (1.0 FPS)`（文件级 `float fps = 1.0f;` 的初值）。

> 这两行在原始 `window.cpp` 里是有的（`glfwPollEvents()` 在 `Render` 开头、`fps` 赋值紧随其后）。本方案把 `Render` 改写成了 Pass 提交，很容易顺手删掉。**建议把 `glfwPollEvents()` 移到 `Run()` 的循环开头** —— 它本来就该在 `processInput` / `Update` 之前。

#### `Window` 完整成员对照表

`Window` 是改动最彻底的文件（近乎重写），因此逐成员列出处理方式 —— 上面代码块里没出现的就是「原样保留」。

| 成员 | 处理 |
|---|---|
| `Run()` | 🔁 **循环体内必须加 `glfwPollEvents()`**（原在 `Render` 里，见上方警告）；其余逻辑（固定步长、Catch-up、Alpha 插值）原样保留 |
| `Init()` | 🔁 去 GL：`glfwMakeContextCurrent` + `gladLoadGLLoader` → `gl::CreateDevice`；删除 5 行 `glEnable`/`glCullFace`；ImGui 核心上下文（`CreateContext` / `StyleColorsDark`）移到这里 |
| `Load()` | 🔁 删 `glViewport` / `glClearColor`；加 `CreateRenderResources` + `RenderSystem::Init` |
| `Render()` | 🔁 删 `glClear`；改为 `BeginFrame` → `FrameRenderer::Execute` → UI → `glfwSwapBuffers`。**保留 `fps = ImGui::GetIO().Framerate;`** —— 调试面板靠它，见上方警告 |
| `Update()` | 🔁 `InputSystem::Update` 少一个参数；`RenderSystem::Update` → `BuildView` |
| `Unload()` | 🔁 加资源释放顺序；`ImGui::DestroyContext` 移到这里 |
| `framebuffer_size_callback` | 🔁 7 行 fan-out → 一行 `device_->OnResize()` |
| `keyCallback` | ⚠️ WASD / Space / Shift 部分**原样保留**；F1~F4 改的是 `settings_` 字段而非 `context->` |
| `mouseMoveCallBack` | ✅ **原样保留** |
| `mouseButtonCallBack` | ✅ **原样保留** |
| `scrollCallBack` | ✅ **原样保留** |
| `processInput` | ✅ **原样保留** |
| 文件级 `world` | 🔁 变成成员 `world_` |
| 文件级 `context` | ❌ **删除** —— 由 `RenderResources` + `RenderView` 取代 |
| 文件级 `fps` | ✅ 保留（`Render` 里的 `ImGui::GetIO().Framerate` 写入） |
| 文件级 `mesh1` | ❌ **删除** —— 测试用的 `GLMesh`，渲染调用早已注释掉 |
| 文件级 `randInt` | ❌ 可删 —— 唯一引用在 `Load()` 的注释里 |
| `FIXED_STEP_MS` / `MAX_ACCUM_MS` | ✅ **原样保留** |
| `main.cpp` | ✅ **原样保留** —— `Window::GetInstance().Run()` 不变 |

> **F1~F4 按键**：改动前改的是 `context->useShadow` 等（`window.cpp:74-97`），现在改的是 `settings_.useShadow` 等，一行一个字段替换。语义不变。

> **组装点说明**：`window.cpp` 是唯一 include `render/gl/*` 的地方，这是**故意的** —— 它是 composition root，必须知道用哪个后端。这不违反分层：`render/gl` 的头文件本身不泄露 GL 类型（`gl_context.h` 只返回 `unique_ptr<IRenderDevice>`），所以 `window.cpp` 里依然**一个 `gl*` 都没有**。

## 6.7 `Engine/ui/ui_backend.h` + `debug_ui_manager`

### 新增 `Engine/ui/ui_backend.h`

```cpp
#pragma once

namespace nova_engine::ui {

	// UI 后端接口。
	//
	// 存在的意义：让 ui/ 目录只依赖 ImGui 核心，不依赖任何 imgui_impl_* 后端。
	// 实现见 render/gl/imgui_gl_backend.h。
	class IUiBackend {
	public:
		virtual ~IUiBackend() = default;

		// nativeWindow 是平台窗口句柄（GLFWwindow* 等），由具体后端解释
		virtual bool Init() = 0;
		virtual void Shutdown() = 0;

		// 每帧开始前的后端准备（对应 ImGui_ImplOpenGL3_NewFrame + ImGui_ImplGlfw_NewFrame）
		virtual void NewFrame() = 0;

		// ImGui::Render() 之后提交绘制数据
		virtual void RenderDrawData() = 0;
	};

} // namespace nova_engine::ui
```

### `debug_ui_manager.h` / `.cpp`

```cpp
// debug_ui_manager.h
#pragma once

#include <imgui/imgui.h>
// ← 删除: #include <imgui/backends/imgui_impl_opengl3.h>
// ← 删除: #include <imgui/backends/imgui_impl_glfw.h>
#include <ui/ui_backend.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nova_engine::ui::debug_ui {
	class DebugUIManager {
	public:
		static DebugUIManager& GetInstance() {
			static DebugUIManager instance;
			return instance;
		}

		// ← 参数从 (GLFWwindow*, const char* glslVersion) 改为 IUiBackend&
		bool Init(IUiBackend& backend);
		void Shutdown();

		void BeginFrame();
		void EndFrame();

		void RegisterPanel(const std::string& name, std::function<void()> callback,
		                   bool visible = true);
		void TogglePanel(const std::string& name);
		bool IsPanelVisable(const std::string& name) const;

	private:
		DebugUIManager() = default;
		~DebugUIManager() = default;

		void DrawAllPanels();

		IUiBackend* backend_ = nullptr;   // ← 新增：非拥有
		bool show_ = false;
		bool initialized_ = false;
		std::vector<std::pair<std::string, std::function<void()>>> panels_;
		std::unordered_map<std::string, bool> visibility_;
	};
}
```

```cpp
// debug_ui_manager.cpp —— 三个函数的实现变化

bool DebugUIManager::Init(IUiBackend& backend) {
	if (initialized_) return true;
	backend_ = &backend;

	// ← ImGui::CreateContext() / StyleColorsDark() 移到了 Window::Init，
	//    因为它们是 ImGui 核心，不属于后端；这里只记录后端引用
	initialized_ = true;
	NE_CORE_INFO("DebugUISystem", "Init UI successfully!");
	return true;
}

void DebugUIManager::Shutdown() {
	if (!initialized_) return;
	// ← ImGui::DestroyContext() 移到 Window::Unload
	if (backend_) backend_->Shutdown();
	backend_ = nullptr;
	initialized_ = false;
	panels_.clear();
	visibility_.clear();
}

void DebugUIManager::BeginFrame() {
	if (!initialized_) return;
	// ← 取代 ImGui_ImplOpenGL3_NewFrame() + ImGui_ImplGlfw_NewFrame()
	if (backend_) backend_->NewFrame();
	ImGui::NewFrame();
}

void DebugUIManager::EndFrame() {
	if (!initialized_) return;
	DrawAllPanels();
	if (show_) ImGui::ShowDemoWindow();
	ImGui::Render();
	// ← 取代 ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData())
	if (backend_) backend_->RenderDrawData();
}
```

#### 必须原样保留的四个函数

`DebugUIManager` 一共 8 个成员函数，上面只列了**会变的 4 个**（`Init` / `Shutdown` / `BeginFrame` / `EndFrame`）。下面这 4 个**不涉及后端，一行都不用改** —— 但如果整段覆盖文件就会一起丢掉，症状是 `LNK2019`：

```cpp
// debug_ui_manager.cpp —— 以下四个函数保持原样，不要删除

	void DebugUIManager::DrawAllPanels() {
		for (auto& [name, callback] : panels_) {
			if (IsPanelVisable(name)) {
				ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
				if (ImGui::Begin(name.c_str()))
					callback();
				ImGui::End();
			}
		}
	}

	void DebugUIManager::RegisterPanel(const std::string& name,
	                                   std::function<void()> callback, bool visible) {
		if (!callback) return;

		auto it = std::find_if(panels_.begin(), panels_.end(),
			[&name](const std::pair<std::string, std::function<void()>>& entry) {
				return entry.first == name;
			});

		if (it != panels_.end()) {
			it->second = callback;
		}
		else {
			panels_.push_back({ name, callback });

			auto vis_it = visibility_.find(name);
			if (vis_it == visibility_.end()) {
				visibility_[name] = visible;
			}
		}

		NE_CORE_INFO("DebugUISystem", "Registe ui panel for name: '{}'", name);
	}

	void DebugUIManager::TogglePanel(const std::string& name) {
		auto it = visibility_.find(name);
		if (it != visibility_.end()) {
			it->second = !it->second;
		}
	}

	bool DebugUIManager::IsPanelVisable(const std::string& name) const {
		auto it = visibility_.find(name);
		if (it != visibility_.end()) {
			return it->second;
		}
		return false;
	}
```

> **一处可选的小修**：原文 `RegisterPanel` 的 `find_if` 谓词按**值**接收 `std::pair<...>`（`debug_ui_manager.cpp:69`），每次比较都会拷贝一个 `std::string` 加一个 `std::function`。改成 `const std::pair<...>&` 即可。这不属于本次抽象范围，但既然要重写这个文件，顺手改掉没有风险。
>
> 另外记得 `#include <algorithm>`（`std::find_if`）。

#### `DebugUIManager` 完整成员对照表

| 成员 | 处理 |
|---|---|
| `Init` | 🔁 参数 `(GLFWwindow*, const char*)` → `(IUiBackend&)` |
| `Shutdown` | 🔁 不再调 `ImGui_ImplOpenGL3_Shutdown` / `ImGui::DestroyContext` |
| `BeginFrame` | 🔁 改调 `backend_->NewFrame()` |
| `EndFrame` | 🔁 改调 `backend_->RenderDrawData()` |
| `RegisterPanel` | ✅ **原样保留** |
| `TogglePanel` | ✅ **原样保留** |
| `IsPanelVisable` | ✅ **原样保留** |
| `DrawAllPanels` | ✅ **原样保留** |
| 成员 `backend_` | 🆕 新增（非拥有指针） |
| 成员 `show_` / `initialized_` / `panels_` / `visibility_` | ✅ 原样保留 |

**收益验证**：`grep -r "imgui_impl" Engine/ui` → **0 命中**。

## 6.8 `Engine/utils/loaders/model_loader.h` / `.cpp`

```cpp
// model_loader.h
#pragma once

#include <vector>
#include <array>
#include <string>
#include <json/json.hpp>
#include <voxel/block/block_mesh.h>
// ← 删除: #include <render/opengl/library/texture_array_library.h>

namespace nova_engine::utils::loader {

	struct OriginalModelData {
		std::vector<std::vector<std::array<float, 3>>> vertices;
		std::vector<std::vector<std::array<float, 2>>> uvs;
		std::vector<int> orientation;
		std::vector<std::string> textures;
		int type;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(OriginalModelData, vertices, uvs, orientation, textures, type);
	};

	class ModelLoader {
	public:
		// ← 去掉 const render::opengl::GLTextureArray* 参数
		//    BlockMesh 里的 uv 已经是 [0,1] 归一化坐标，
		//    纹理层索引由上层用 BlockLibrary 的名字映射完成
		static nova_engine::voxel::block::BlockMesh LoadBlockModelFromJsonPath(const char* path);
	};
}
```

> **为什么加载器不该认识 `GLTextureArray`**：改动前它需要一个具体的 GPU 纹理对象，只是为了把方块名换成纹理层索引 —— 这是**名字映射**，不是 GPU 操作。解耦后，这个映射可以在 `BlockLibrary` 里用一份 `name → UV 偏移` 的表完成，加载器变成一个纯 CPU 的函数，可以放进工作线程、可以被单测直接调用（`Tests/test_file/model_loader_test.cpp` 已经在测它了）。

---

# 7. 构建系统

## 7.1 原理

**光靠目录约定守不住边界。** 半年后（或者三天后）你自己就会在 `render_system.cpp` 里再写一句 `glBindFramebuffer` —— 因为能写、能编译、能跑。

让建设施物理性地挡住这件事，需要两步：

1. **删掉能让 glad 被随手 include 的路径** —— 这是根因
2. **加一个自动检查** —— 这是保险

## 7.2 根因：`glm` 泄露了整个第三方头目录

改动前 `CMake/Dependencies.cmake:23-25`：

```cmake
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${NE_THIRD_PARTY_INCLUDE_DIR}      # ← Linking/include，里面有 glad/ GLFW/ imgui/
)
```

后果：**任何链接了 `glm` 的目标都能 `#include <glad/glad.h>`**。而 `Engine/CMakeLists.txt:22` 又把 `glm`（连同 glad/glfw/imgui）作为 PUBLIC 链接到 `NovaEngineCore` —— 于是全项目畅通无阻。

`Linking/include` 的实际内容（见 README 目录结构）：

```
Linking/include/
├── GLFW/        ← 平台
├── glad/        ← 图形 API
├── khrplatform/
├── glm/         ← 数学
├── imgui/       ← UI（含 backends/）
├── spdlog/
├── json/
├── stb_image/
└── googletext/
```

一个 `glm` target 暴露九个库的头文件，这是**依赖边界失效的根本原因**。

## 7.3 `CMake/Dependencies.cmake`（改造后）

```cmake
set(NE_THIRD_PARTY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Linking/include)
set(NE_THIRD_PARTY_LIB_DIR     ${CMAKE_SOURCE_DIR}/Linking/lib)
set(NE_THIRD_PARTY_SRC_DIR     ${CMAKE_SOURCE_DIR}/Linking/src)

# ===========================================================================
# 数学：只暴露 glm 自己的头文件
# ===========================================================================
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${NE_THIRD_PARTY_INCLUDE_DIR}/glm)          # ← 关键：不再暴露整个 include 目录

# ===========================================================================
# 图形 API 头文件：只有 render/gl 可以链接
# ===========================================================================
add_library(ne_gl_headers INTERFACE)
target_include_directories(ne_gl_headers INTERFACE
    ${NE_THIRD_PARTY_INCLUDE_DIR}/glad
    ${NE_THIRD_PARTY_INCLUDE_DIR}/GLFW
    ${NE_THIRD_PARTY_INCLUDE_DIR}/khrplatform)

# ===========================================================================
# 平台：GLFW 预编译库
# ===========================================================================
add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${NE_THIRD_PARTY_LIB_DIR}/GLFW/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${NE_THIRD_PARTY_INCLUDE_DIR}/GLFW")

add_library(glad STATIC ${NE_THIRD_PARTY_SRC_DIR}/glad/glad.c)
target_include_directories(glad PRIVATE ${NE_THIRD_PARTY_INCLUDE_DIR}/glad)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)

# ===========================================================================
# ImGui 拆成两半：核心 / 后端
# ===========================================================================
file(GLOB IMGUI_CORE_SOURCES ${NE_THIRD_PARTY_SRC_DIR}/imgui/*.cpp)
add_library(imgui_core STATIC ${IMGUI_CORE_SOURCES})
target_include_directories(imgui_core PUBLIC
    ${NE_THIRD_PARTY_INCLUDE_DIR}/imgui)        # ← 不含 backends/

add_library(imgui_gl STATIC
    ${NE_THIRD_PARTY_SRC_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${NE_THIRD_PARTY_SRC_DIR}/imgui/backends/imgui_impl_glfw.cpp)
target_include_directories(imgui_gl PUBLIC
    ${NE_THIRD_PARTY_INCLUDE_DIR}/imgui/backends)
target_link_libraries(imgui_gl PUBLIC imgui_core glfw ne_gl_headers glad)

# ===========================================================================
# 其余第三方
# ===========================================================================
add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE ${NE_THIRD_PARTY_INCLUDE_DIR}/spdlog)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${NE_THIRD_PARTY_INCLUDE_DIR}/json)
```

> **注意**：`Linking/include/glm` 下是 glm 的头文件本体（`glm.hpp`、`vec3.hpp` …）。改动前用 `#include <glm/glm.hpp>` 能工作，是因为 `Linking/include` 在搜索路径里。收窄成 `Linking/include/glm` 之后，`#include <glm/glm.hpp>` **会失效** —— 除非把 include 目录设为 `Linking/include`。
>
> 两个选择：
> - **A**：`target_include_directories(glm INTERFACE ${NE_THIRD_PARTY_INCLUDE_DIR})`，但把 glad/GLFW/imgui 从 `Linking/include` 移到 `Linking/include_gl/`。改动第三方目录结构。
> - **B**：保持 `Linking/include` 的布局，改为依赖**守卫测试**（7.5）而不是包含路径来强制边界。
>
> **推荐 B**：改动最小，且守卫测试能给出明确的错误信息（「哪一行、哪个文件调了 gl」），比「找不到头文件」更容易排查。7.2 的分析解释了**为什么**会漏，7.5 的守卫解决**怎么防**。

## 7.4 `Engine/CMakeLists.txt`（改造后）

```cmake
cmake_minimum_required(VERSION 3.10)

set(NE_ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ===========================================================================
# 收集各层源文件
# ===========================================================================
function(ne_collect out_var)
    file(GLOB_RECURSE srcs CMAKE_CONFIGURE_DEPENDS ${ARGN})
    set(${out_var} ${srcs} PARENT_SCOPE)
endfunction()

ne_collect(RENDER_CORE_SRC
    "${NE_ENGINE_DIR}/render/core/*.cpp" "${NE_ENGINE_DIR}/render/core/*.h")
ne_collect(RENDER_SCENE_SRC
    "${NE_ENGINE_DIR}/render/scene/*.cpp" "${NE_ENGINE_DIR}/render/scene/*.h")
ne_collect(RENDER_FRAMEWORK_SRC
    "${NE_ENGINE_DIR}/render/framework/*.cpp" "${NE_ENGINE_DIR}/render/framework/*.h")
ne_collect(RENDER_GL_SRC
    "${NE_ENGINE_DIR}/render/gl/*.cpp" "${NE_ENGINE_DIR}/render/gl/*.h")
ne_collect(PLATFORM_SRC
    "${NE_ENGINE_DIR}/window/*.cpp" "${NE_ENGINE_DIR}/window/*.h")
ne_collect(RENDER_SYSTEM_SRC
    "${NE_ENGINE_DIR}/render/system/*.cpp" "${NE_ENGINE_DIR}/render/system/*.h")
ne_collect(RENDER_UTILS_SRC
    "${NE_ENGINE_DIR}/render/utils/*.cpp" "${NE_ENGINE_DIR}/render/utils/*.h"
    "${NE_ENGINE_DIR}/render/*.cpp"       "${NE_ENGINE_DIR}/render/*.h")
ne_collect(ENGINE_CORE_SRC
    "${NE_ENGINE_DIR}/core/*.cpp"    "${NE_ENGINE_DIR}/core/*.h"
    "${NE_ENGINE_DIR}/voxel/*.cpp"   "${NE_ENGINE_DIR}/voxel/*.h"
    "${NE_ENGINE_DIR}/voxel/*.tpp"
    "${NE_ENGINE_DIR}/utils/*.cpp"   "${NE_ENGINE_DIR}/utils/*.h"
    "${NE_ENGINE_DIR}/collection/*.cpp" "${NE_ENGINE_DIR}/collection/*.h"
    "${NE_ENGINE_DIR}/log/*.cpp"     "${NE_ENGINE_DIR}/log/*.h")
ne_collect(UI_SRC
    "${NE_ENGINE_DIR}/ui/*.cpp" "${NE_ENGINE_DIR}/ui/*.h")

# render/system 与 render/utils 归入 framework 之上一层（它们依赖 scene）
# 这里挂在 NovaRenderFramework 里，因为它们不含 GL 且只依赖 core/scene/device
list(APPEND RENDER_FRAMEWORK_SRC ${RENDER_SYSTEM_SRC} ${RENDER_UTILS_SRC})

# ===========================================================================
# L0: render/core —— 只依赖 glm
# ===========================================================================
add_library(NovaRenderCore STATIC ${RENDER_CORE_SRC})
target_include_directories(NovaRenderCore PUBLIC ${NE_ENGINE_DIR})
target_link_libraries(NovaRenderCore PUBLIC glm)
target_compile_options(NovaRenderCore PRIVATE "/utf-8")
target_compile_features(NovaRenderCore PUBLIC cxx_std_23)

# ===========================================================================
# L1: render/device —— 纯虚接口（header-only）
# ===========================================================================
add_library(NovaRenderBackend INTERFACE)
target_link_libraries(NovaRenderBackend INTERFACE NovaRenderCore)

# ===========================================================================
# L2: render/scene —— 账本 + 变换（依赖 core + device 接口）
# ===========================================================================
add_library(NovaRenderScene STATIC ${RENDER_SCENE_SRC})
target_link_libraries(NovaRenderScene PUBLIC NovaRenderBackend)
target_compile_options(NovaRenderScene PRIVATE "/utf-8")

# ===========================================================================
# L3: render/framework + render/system —— 流程编排（不含 GL）
# ===========================================================================
add_library(NovaRenderFramework STATIC ${RENDER_FRAMEWORK_SRC})
target_link_libraries(NovaRenderFramework PUBLIC
    NovaRenderScene spdlog nlohmann_json)
target_compile_options(NovaRenderFramework PRIVATE "/utf-8")

# ===========================================================================
# L4: render/gl —— 唯一后端
# ===========================================================================
add_library(NovaRenderGL STATIC ${RENDER_GL_SRC})
target_link_libraries(NovaRenderGL PUBLIC
    NovaRenderBackend ne_gl_headers glad imgui_gl)
target_compile_options(NovaRenderGL PRIVATE "/utf-8")

# ===========================================================================
# L5: 平台层（窗口 + 输入）
# ===========================================================================
add_library(NovaPlatform STATIC ${PLATFORM_SRC})
target_link_libraries(NovaPlatform PUBLIC
    NovaRenderBackend NovaRenderFramework glfw imgui_core)
target_compile_options(NovaPlatform PRIVATE "/utf-8")

# ===========================================================================
# L6: 游戏逻辑（core / voxel / utils / collection / log / ui）
#     ★ 无 glad / glfw
# ===========================================================================
add_library(NovaEngineCore STATIC ${ENGINE_CORE_SRC} ${UI_SRC})
target_link_libraries(NovaEngineCore PUBLIC
    NovaRenderCore NovaRenderScene NovaRenderFramework
    glm spdlog nlohmann_json imgui_core)          # ← 没有 glad / glfw
target_compile_options(NovaEngineCore PRIVATE "/utf-8")

# ===========================================================================
# 可执行文件 —— 唯一的组装点
# ===========================================================================
add_executable(NovaEngine "main.cpp")
target_compile_options(NovaEngine PRIVATE "/utf-8")
target_link_libraries(NovaEngine PRIVATE
    NovaEngineCore NovaPlatform NovaRenderGL)

# ---------- 着色器与资源拷贝（保持原样）----------
set(SHADER_SRC_DIR "${NE_ENGINE_DIR}/render/gl/shaders")
add_custom_command(TARGET NovaEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SHADER_SRC_DIR}" "$<TARGET_FILE_DIR:NovaEngine>/shaders"
    COMMENT "Copying shaders")
add_custom_command(TARGET NovaEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:NovaEngine>/assets"
    COMMENT "Copying assets")
```

**依赖链自检**（用 `cmake --graphviz` 或肉眼看）：

```
NovaRenderCore      → glm
NovaRenderBackend   → NovaRenderCore
NovaRenderScene     → NovaRenderBackend
NovaRenderFramework → NovaRenderScene, spdlog, nlohmann_json
NovaRenderGL        → NovaRenderBackend, glad, ne_gl_headers, imgui_gl
NovaPlatform        → NovaRenderBackend, NovaRenderFramework, glfw, imgui_core
NovaEngineCore      → NovaRenderCore, NovaRenderScene, NovaRenderFramework,
                      glm, spdlog, nlohmann_json, imgui_core        ★ 无 glad/glfw
NovaEngine          → 全部
```

`NovaEngineCore` 的链接列表里出现 `glad` 或 `glfw` = 边界已破。

## 7.5 `cmake/CheckLayering.cmake`

**原理**：CMake 的 `-P` 脚本模式可以在 configure/test 阶段跑任意逻辑。用 `file(READ)` + 正则做静态检查，**不依赖编译器，不需要构建**，因此可以作为 CTest 的一员。

三条检查规则，每条对应一个具体的退化模式：

| 规则 | 拦住的退化 |
|---|---|
| 禁止 `#include <glad/…>` / `<GLFW/…>` | 有人为了用一个 `GLuint` 把 glad 引进高层 |
| 禁止 `gl[A-Z]…(` | 有人为了「就这一行」把 `glBindFramebuffer` 写进 Pass |
| 禁止 `GL_[A-Z_]+` | 有人把 `GL_TRIANGLES` 当常量传进枚举参数 |

```cmake
# cmake/CheckLayering.cmake
#
# 用法：
#   cmake -DNE_ROOT=<repo-root> -P cmake/CheckLayering.cmake
#
# 检查指定目录中不出现任何图形 API 依赖。

if(NOT DEFINED NE_ROOT)
    message(FATAL_ERROR "NE_ROOT is required")
endif()

set(NE_ENGINE "${NE_ROOT}/Engine")

# 禁止出现图形 API 的目录（相对 Engine/）
set(FORBIDDEN_DIRS
    core
    voxel
    utils
    collection
    log
    ui
    window
    render/core
    render/scene
    render/device
    render/framework
    render/system
)

set(VIOLATIONS "")

foreach(dir IN LISTS FORBIDDEN_DIRS)
    file(GLOB_RECURSE sources
        "${NE_ENGINE}/${dir}/*.cpp"
        "${NE_ENGINE}/${dir}/*.h"
        "${NE_ENGINE}/${dir}/*.hpp"
        "${NE_ENGINE}/${dir}/*.tpp")

    foreach(src IN LISTS sources)
        file(READ "${src}" content)

        # 规则 1：禁止包含图形 API 头文件
        # 注意 glm 不匹配（小写 m，且模式要求 GLFW 全大写或 glad）
        if(content MATCHES "#[ \t]*include[ \t]*[<\"](glad|GLFW|khrplatform)/")
            list(APPEND VIOLATIONS "${src}: 包含图形 API 头文件")
        endif()

        # 规则 2：禁止调用 gl* 函数
        # gl 后必须跟大写字母再接 '(' —— 因此 glm:: 与 Google 都不会误报
        if(content MATCHES "(^|[^A-Za-z0-9_])gl[A-Z][A-Za-z0-9_]*[ \t]*\\(")
            list(APPEND VIOLATIONS "${src}: 调用 gl* 函数")
        endif()

        # 规则 3：禁止引用 GL_ 常量
        if(content MATCHES "(^|[^A-Za-z0-9_])GL_[A-Z0-9_]+")
            list(APPEND VIOLATIONS "${src}: 引用 GL_* 常量")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    list(LENGTH VIOLATIONS count)
    foreach(v IN LISTS VIOLATIONS)
        message(SEND_ERROR "分层违规: ${v}")
    endforeach()
    message(FATAL_ERROR
        "分层检查失败：${count} 处违规。\n"
        "图形 API 调用只允许出现在 Engine/render/gl/ 目录下。\n"
        "详见 docs/render-abstraction.md")
else()
    message(STATUS "分层检查通过：高层目录无图形 API 依赖")
endif()
```

**注册为 CTest**（加到根 `CMakeLists.txt`）：

```cmake
enable_testing()

add_test(NAME layering_guard
         COMMAND ${CMAKE_COMMAND}
                 -DNE_ROOT=${CMAKE_SOURCE_DIR}
                 -P ${CMAKE_SOURCE_DIR}/cmake/CheckLayering.cmake)
```

**误报排查**（这三条正则都特意设计过）：

| 代码 | 规则 2 是否误报 | 原因 |
|---|---|---|
| `glm::vec3 v;` | 否 | `gl` 后是 `m`（小写），模式要求 `[A-Z]` |
| `#include <glm/vec3.hpp>` | 否 | 规则 1 只匹配 `glad` / `GLFW` / `khrplatform` |
| `GoogleTest` | 否 | `gl` 后是 `e`（小写） |
| `Google(1)` | 否 | 同上 |
| `glBindFramebuffer(` | **是** | 正确命中 |
| `GL_TRIANGLES` | —— | 由规则 3 命中 |
| `// 注释里写了 glClear` | **是** | 保守策略：宁可误报也不漏报。如确需在注释里提，写作 `gl-Clear` 或 `gl*` |

> 规则 2 对注释也会命中。这是**有意的保守策略** —— 漏报的代价是边界悄悄破掉，误报的代价是改一行注释。如果某个文件确实需要在注释里讨论 GL，把该文件加进 `ALLOWED_EXCEPTIONS` 列表并在清单里写明理由。

---

# 8. 设计决策的原理说明

## 8.1 为什么是 `PipelineState` 而不是 `Material`

常见做法是引入 `Material`：着色器 + 纹理 + 参数三合一，然后 `cmd.Draw(mesh, material)`。

**现在不做的理由是「纹理绑定还没到需要抽象的程度」**：

- 现在只有 6 个着色器、1 个纹理数组，每个 Pass 的纹理绑定是固定的
- `Material` 的价值在于「同一个网格用不同材质画多次」——现在没有这个场景
- 引入 `Material` 就要引入 `MaterialHandle`、材质缓存、参数反射 —— 全是**为不存在的需求付的成本**

**但接口是留好的**：`PipelineState` 已经是值类型，`SetPipelineState` 已经是唯一的管线切换入口。将来加 `Material` 时：

```cpp
// 未来
struct Material {
    PipelineState pipeline;              // ← 现在这个结构体直接变成成员
    std::vector<TextureHandle> textures;
    ParameterBlock params;
};
cmd.SetMaterial(material);
```

`PipelineState` 从「Pass 的成员」变成「材质的成员」，**Pass 的 `Record` 函数体只改一行**。这就是「预留入口」的正确形态 —— 不是空接口，是一个位置正确的数据结构。

## 8.2 为什么 `VertexLayoutId` 是枚举而不是完整描述符

一个「完整的」顶点布局描述符长这样：

```cpp
struct VertexAttribute { Semantic semantic; VertexFormat format; uint16_t offset; };
struct VertexLayout { uint16_t stride; std::vector<VertexAttribute> attributes; };
```

**现在不做的理由**：四个布局是**固定且已知的**，它们的定义写在 4 个 glsl 文件里，改一个就要改另一个。用枚举表达「用第几号布局」，后端用 `switch` 展开成 `glVertexAttribPointer` 调用 —— 这是**编译期可知**的信息，用运行时描述符表达反而引入了解析开销和出错空间。

**但入口是留好的**：

1. `PipelineState::vertexLayout` 已经是一个**值**，不是 `GLMesh` 的私有状态
2. 后端已经按 `(meshId, layoutId)` 缓存 VAO —— 这个缓存机制与 layout 怎么表达无关
3. 加新布局 = 枚举加一个值 + `switch` 加一个 case

**升级触发条件**（写在这里，将来不用重新决策）：当布局数量超过 8 个，或者布局需要从文件（glTF 等）动态读取时，把枚举换成描述符 —— 那时 `GetOrCreateVao` 的签名从 `VertexLayoutId` 换成 `const VertexLayout&`，缓存键从 `layoutId` 换成 `layout.Hash()`，调用点**一行不改**（因为 `PipelineState` 已经是值传递）。

## 8.3 为什么 `render/scene` 依赖 `render/device` 接口

这是对初版方案的修正（[0.3](#03-与初版方案的四处修正) 第 1 条）。

初版方案说「`render/scene` 是纯数据，不依赖 device」。写实现时发现：`ChunkMeshRegistry::SubmitMesh(x, y, z, data)` 拿到 CPU 顶点数据后**必须**上传到 GPU —— 改动前这个动作发生在 `GLMesh::GetOrUpdateData` 里（`glmesh.cpp:84-179`），是不可避免的。

三个选择：

| 方案 | 代价 |
|---|---|
| **A. 注入 `IRenderDevice&`** | `render/scene` 依赖一个纯虚接口 |
| B. 定义窄接口 `IMeshUploader {CreateMesh, UpdateMesh, DestroyMesh}`，让 `IRenderDevice` 适配它 | 多一个类型、多一层间接，换来「scene 不依赖 device」这个名义结论 |
| C. 账本只存 `MeshData`，上传由 framework 层单独做 | 需要保留所有区块的 CPU 顶点副本 —— 内存翻倍，且需要额外的脏上传队列 |

**选 A。** 理由：依赖纯虚接口本身没有成本 —— `render/scene` 依然不知道 OpenGL 存在，它甚至不知道「GPU」这个概念，只知道「有个东西能接收我的顶点数据」。而且这让单测可以直接塞一个 `FakeDevice`（`Tests/` 目录已经有 GoogleTest 基础设施）。

**判据**：如果换成 B 或 C，`render/scene` 依然无法独立于图形 API 存在（它总要调用某处的上传接口）。A 是唯一同时满足「简单」和「可测」的方案。

## 8.4 为什么 `FullscreenQuad` 由后端内建

后处理需要一个全屏四边形，它的顶点布局是 `pos2 + uv2`，与体素网格的 `pos3 + uvw3 + ao1 + normal3` 完全不同。

三个选择：

| 方案 | 代价 |
|---|---|
| A. **后端内建**（采纳） | `IRenderDevice` 多一个方法，后端多 15 行 |
| B. 上层用 `device.CreateMesh(quadData)` 创建 | `MeshData` 必须能表达两种布局 → 要么加 `SubMesh`/布局字段，要么把 pos2 硬塞进 `vertices` 的前两个分量（语义混乱） |
| C. 后端提供 `DrawFullscreen()` 专用方法 | 把「后处理」这个具体用途焊进设备接口，未来任何全屏效果都要走这个特例 |

**选 A。** `FullscreenQuad()` 返回的是一个**普通的 `MeshHandle`** —— 它走完全相同的 `DrawMesh` 路径，只是由后端负责它的顶点数据。这既避免了 B 的数据结构污染，也避免了 C 的特例化。

**未来的扩展点**：如果将来需要多个内建网格（全屏三角形、立方体用于天空盒、点精灵用于粒子），把 `FullscreenQuad()` 泛化成 `BuiltinMesh(BuiltinMeshKind)` 即可，调用点不用改。

## 8.5 为什么用 `SamplerState` 而不是在 `TextureDesc` 上设滤波

改动前 `render_system.cpp:98-103` 和 `:137-140` 对**同一个**方块纹理数组设了两种 `GL_TEXTURE_MIN_FILTER`：

- 透明通道（WBOIT）要求 `GL_LINEAR`
- 后处理通道前恢复成 `GL_NEAREST_MIPMAP_LINEAR`

这是「同一个纹理在不同 Pass 需要不同采样状态」的**真实冲突**。把滤波设进 `TextureDesc`（创建时一次）根本无法表达它 —— 除非创建两份纹理，那要多占一份显存。

**采样器对象是唯一正确的答案**：采样状态属于「怎么读纹理」（管线状态），不属于「纹理是什么」（资源描述）。GL 有 `glBindSampler`、D3D11 有独立的 `ID3D11SamplerState`、Vulkan 有 `VkSampler` —— 三个 API 都是这个划分。

**顺带修掉的 bug**：改动前那两处 `glTexParameteri` 直接改了 `TextureArrayLibrary` 拥有的纹理对象状态，而且改完没有恢复原始意图。任何一次调用顺序调整都会导致滤波状态错乱 —— 表现为「某些角度纹理突然变糊」，极难排查。用采样器之后，纹理对象状态从头到尾不被修改。

## 8.6 为什么不做帧图

量化对比（5 个 Pass、4 个渲染目标、固定顺序的现状）：

| | 手写 Pass 列表 | 帧图 |
|---|---|---|
| 代码量 | ~80 行 | ~400 行 |
| 自动 RT 池化 | ❌（只有 4 个 RT，手工管理成本极低） | ✅ |
| 自动依赖排序 | ❌（顺序固定，不需要排序） | ✅ |
| 自动 resize | ✅（`device->OnResize` 已解决） | ✅ |
| 未来上 Vulkan 自动插 barrier | ❌ | ✅ |

**没有任何一条当前需求指向帧图。** 帧图解决的是「Pass 数量增长到几十个、资源依赖变得难以手工维护」的问题 —— 现在 5 个 Pass。

**但入口是留好的**（这条很关键）：`RenderPass::Record` 的契约写明「Pass 不拥有渲染目标，只使用 `RenderResources` 里的句柄」。将来上帧图时：

1. `RenderResources` 的 `RenderTargetHandle` 字段换成帧图的 `RGResourceHandle`
2. 加一个 `RenderGraph::Compile()`
3. **5 个 Pass 的 `Record` 函数体一行都不用改** —— 它们只调用 `cmd.BeginRenderTarget(res.mainFrame, ...)`，不关心这个句柄从哪来

## 8.7 为什么 `IRenderDevice` 是立即模式而不是命令缓冲

一个常见的误解：抽象图形 API 就要做成 Vulkan 那样 —— 录制命令缓冲、显式 barrier、描述符集。这是**照抄了某个后端的形状，而不是抽出所有后端的共性**。

实际的取舍：

| 方案 | 迁移现有代码成本 | 换 D3D11 | 换 Vulkan | 额外间接层 |
|---|---|---|---|---|
| **立即模式（本方案）** | 极低 | 可平移 | 需重做 | 0 |
| 命令缓冲 + 帧图 | 高 | 可平移 | 可平移 | 1 层 |

对单人项目，**用 20% 的成本拿到 80% 的收益**是正确取舍。

真正需要被抽象掉的是三件事，而不是「命令缓冲」：

1. **调用里不出现 `GLenum` / `GLuint`** —— `ICommandList` 的 9 个方法签名里一个 GL 类型都没有
2. **不依赖隐式全局状态** —— `BeginRenderTarget`/`EndRenderTarget` 成对出现，替代散落的 `glBindFramebuffer`
3. **资源生命周期显式** —— `CreateXxx`/`DestroyXxx`，而不是靠析构函数里偷偷调 GL

这三条做到了，就已经「彻底抽离」。命令缓冲和多线程录制是**性能优化**，不是抽象 —— 现在没有这个性能需求（单线程、几百到几千个 draw call）。

## 8.8 为什么 `MeshInstance` 持有 device 反向指针

见 [3.1](#31-原理) 的完整分析。核心矛盾：

改动前 `GLMesh` 的析构函数归还 GPU 资源（`glmesh.cpp:20-22`），而 `AdaptiveLodOctree` 内部用 `std::vector<std::unique_ptr<T>>` 存数据，LOD 细分/合并时会**大量替换和销毁**这些对象（`adaptive_lod_octree.h:130, 137, 179, 186, 196`）。

如果 `MeshInstance` 不负责归还：

- LOD 每帧的细分合并会**持续泄漏 GPU 网格** —— 这不是「暂时不管」，是无界泄漏
- 或者需要给 `AdaptiveLodOctree` 模板化 deleter，签名变复杂

一个非拥有指针换 RAII 语义 + 零改动既有数据结构，是划算的。**唯一的代价是 `MeshInstance` 不再是 POD** —— 需要显式写移动构造/赋值（已在 3.2 给出）。

## 8.9 为什么资源装配在 `framework` 而不是 `gl`

这是对初版方案的修正（[0.3](#03-与初版方案的四处修正) 第 2 条）。

初版方案的目录里有一个 `render/gl/gl_resource_cache.{h,cpp}`，负责创建所有着色器和渲染目标。写实现时发现这会造成**依赖倒置**：`RenderResources` 定义在 `render/framework`，如果由 `render/gl` 创建它，那 `render/gl` 就要 include `render/framework`。

而 [0.2](#02-依赖规则表) 的第 5 条明确要求 **`render/gl` 不认识 `scene` 和 `framework`** —— 这是「换后端只重写一个目录」的保证。

**正确划分**：

| 问题 | 归属 | 理由 |
|---|---|---|
| 「需要哪几个 RT、什么格式、哪些 shader」 | `render/framework/render_resources.cpp` | 这是**资产声明** |
| 「怎么把 RGBA16F 变成 `glTexImage2D` 调用」 | `render/gl` | 这是**后端实现** |

`CreateRenderResources(IRenderDevice&)` 只使用接口，不碰 GL。`render/gl` 侧完全不认识 `RenderResources` 这个类型。这样：

- 换后端 → 只重写 `render/gl`，`render_resources.cpp` 一字不改
- 加一个渲染目标 → 只改 `render_resources.cpp`，后端一字不改

## 8.10 为什么不把 `IRenderer` 改成模板参数

有人会想：「`IRenderer::Render(const GLMesh&)` 的问题是参数类型，那把它泛化成 `template <typename MeshT> Render(const MeshT&)` 不就行了？」

**不行，因为问题不在参数类型，在于「一个渲染特性 = 一个渲染器类」这个模式本身。**

数一下加一个粒子系统需要改什么：

| 步骤 | 改动前 | 改造后 |
|---|---|---|
| 新增渲染器类 | `ParticleRenderer : IRenderer` | ❌ 不需要 |
| 注册到资源库 | `ShaderLibrary` 加一行 | ❌ 不需要（`ShaderDesc` 加一条数据） |
| 加进上下文 | `RenderContext` 加一个成员 | ❌ 不需要 |
| 加进 resize 扇出 | `window.cpp:32-39` 加一行 | ❌ 不需要（`device->OnResize` 统一处理） |
| 加进渲染流程 | `render_system.cpp` 加一段 gl 代码 | 加一个 `ParticlePass` 类 |
| 设置粒子材质 | （无此概念） | 填一个 `PipelineState` |

改造后**唯一需要写 C++ 的地方是一个 Pass 类**，其余全是数据。

这正是抽象的判据：**如果一个新特性需要新增的 C++ 类型数量不随特性数量增长，那抽象是对的。** 改造前是「线性增长」（每个特性 1 个渲染器 + 3 处注册），改造后是「常数」（1 个 Pass，且 Pass 之间高度相似）。

---

# 9. 落地检查清单

## 9.1 分阶段落地顺序

每一步都能编译通过、都有可自动验证的不变式。

### S1 — 建目录 + 挪文件（零行为变化）

**新增**
- [ ] `Engine/render/core/`：`handles.h` `enums.h` `sampler_state.h` `pipeline_state.h` `render_settings.h` `mesh_data.h` `transform.h` `shader_desc.h` `texture_desc.h` `render_target_desc.h` `shader_param.h`
- [ ] `Engine/render/core/camera.h`（从 `render/opengl/camera.h` 迁入，改命名空间）
- [ ] `Engine/cmake/CheckLayering.cmake`

**修改**
- [ ] `CMake/Dependencies.cmake`：拆出 `ne_gl_headers`，拆 `imgui_core` / `imgui_gl`
- [ ] 根 `CMakeLists.txt`：注册 `layering_guard` 测试
- [ ] 全局替换 `render::opengl::Camera` → `render::Camera`，`render::opengl::MeshData` → `render::MeshData`

**验证不变式**
```bash
# 画面与改造前逐像素一致
# NovaRenderCore 只链 glm
cmake --build out/build/x64-debug
ctest --test-dir out/build/x64-debug -R EngineCoreTests
```

**风险**：极低（纯搬迁）。

---

### S2 — 依赖方向掰正

**新增**
- [ ] `Engine/voxel/chunk/dirty_region_queue.h`
- [ ] `Engine/render/scene/`（`mesh_instance.h` `chunk_mesh_registry.{h,cpp}` `lod_mesh_registry.{h,cpp}` `render_view.h`）
- [ ] `Engine/render/device/`（`render_device.h` `command_list.h`）
- [ ] `Engine/render/gl/`（`slot_table.h` `gl_enums.h` `gl_context.{h,cpp}` `gl_shader.{h,cpp}` `gl_texture.{h,cpp}` `gl_render_target.{h,cpp}` `gl_mesh_buffers.{h,cpp}` `gl_command_list.{h,cpp}` `gl_device.{h,cpp}` `imgui_gl_backend.{h,cpp}`）

**修改**
- [ ] `core/world/world.h`：加 `dirtyRegions`
- [ ] `core/input/input_system.{h,cpp}`：去掉 `RenderContext&` 参数
- [ ] `utils/loaders/model_loader.{h,cpp}`：去掉 `GLTextureArray*` 参数

**验证不变式**
```bash
grep -rE "glad|GLFW|gl[A-Z]" Engine/core Engine/voxel Engine/utils
# → 0 命中
```
- [ ] 放置/破坏方块后区块网格正常更新
- [ ] `Tests/test_file/model_loader_test.cpp` 通过（**不需要 GL 上下文**）

**风险**：低。注意 `MarkDirty` 的邻域偏移必须与 `input_system.cpp:66-70` 的 `pos + n*2` 一致（见 6.3 的警告）。

---

### S3 — `RenderContext` 三分

**修改**
- [ ] `render/system/render_system.{h,cpp}`：`Render()` 删除，`Update()` → `BuildView()`
- [ ] `render/system/lod_system.{h,cpp}`：参数换成 `ChunkMeshRegistry&` / `LodMeshRegistry&`
- [ ] 删除 `render/opengl/context/render_context.h`、`static_mesh.h`、`chunk_mesh_cache.*`、`lod_mesh_cache.*`

**验证不变式**
```bash
grep -E "gl[A-Z]|glad" Engine/render/system/render_system.cpp
# → 0 命中
```
- [ ] `NovaRenderFramework` 可在不链 glad 的 target 里编译
- [ ] 画面一致（LOD、阴影、半透明全部正常）

**风险**：中。第一次真正改数据结构，但改动机械（类型替换）。

---

### S4 — Device 接线

**修改**
- [ ] `ui/ui_backend.h`（新增）+ `ui/debug_ui/debug_ui_manager.{h,cpp}`
- [ ] `window/window.{h,cpp}`：去 glad，持 `unique_ptr<IRenderDevice>`
- [ ] `render/framework/`：`render_pass.h` `render_resources.{h,cpp}` `frame_renderer.{h,cpp}` `passes/*.{h,cpp}`
- [ ] 删除 `render/opengl/renderer/`（`IRenderer` + 5 个子类）、`render/opengl/library/*`、`render/opengl/frame_buffer.*`、`render/opengl/glmesh.*`、`render/opengl/shader.*`、`render/opengl/texture/`

**验证不变式**
```bash
grep -rE "gl[A-Z]|glad|GLFW_" Engine/window Engine/render/system Engine/render/framework Engine/ui
# → 0 命中

grep -rlE "gl[A-Z]|glad" Engine --include=*.cpp --include=*.h
# → 只应出现 Engine/render/gl/**
```
- [ ] 窗口 resize 正常
- [ ] F1~F4 开关正常
- [ ] ImGui 面板正常显示
- [ ] `ctest` 全部通过（含 `layering_guard`）

**风险**：中高。建议单独一个提交。

---

### S5 — 清理

- [ ] 删除 `Engine/render/opengl/` 整个目录（把它下面的 `shaders/` 移到 `Engine/render/gl/shaders/`）
- [ ] 更新 `README.md` 的目录结构与模块说明表
- [ ] 更新 `Tests/CMakeLists.txt` 的链接目标

---

## 9.2 逐条对照：改动前后行为差异清单

移植时**唯一需要留意的行为差异**都在这里，其余应当逐像素一致：

| # | 差异 | 影响 | 是否建议改 |
|---|---|---|---|
| 1 | WBOIT 深度预通道与累加阶段的顺序（4.8 变化 1） | 半透明遮挡关系更正确 | ✅ 建议 |
| 2 | WBOIT 排序比较函数补上返回值（4.8 变化 2） | 消除 UB，实际结果不变 | ✅ 建议 |
| 3 | `LightPass` 显式绑定方块纹理（4.7） | 消除跨 renderer 的隐式顺序依赖 | ✅ 建议 |
| 4 | 纹理滤波改用采样器对象（8.5） | 修掉共享纹理状态被改的 bug | ✅ 建议 |
| 5 | `LightPass` 不再保存/恢复 `glClearColor` | 清屏色成为参数，无全局状态 | ✅ 必然 |
| 6 | 每帧显式设置 viewport | 消除 `needResizeFBO_` 遗漏风险 | ✅ 必然 |
| 7 | 删除 `Render(emptyMesh)` 调用 | 少一次 0 元素的 `glDrawElements` | ✅ 必然 |
| 8 | uniform 位置改为查表 | 性能改善，结果不变 | ✅ 必然 |
| 9 | 索引类型 `int` → `uint32_t` | 语义修正，二进制布局不变 | ✅ 必然 |
| 10 | 阴影通道的相对原点用 `shadowOrigin`（3.5） | **保持原样**，不要"顺手修正" | ⚠️ 保留 |
| 11 | 后处理采样阴影用 `shadowSampleMatrix`（3.5） | **保持原样**，与渲染用的矩阵不同 | ⚠️ 保留 |
| 12 | `LodMeshRegistry::TryDeleteOctree` 的反向逻辑（3.4） | **保持原样**，`lod_system.cpp:116-130` 依赖它 | ⚠️ 保留 |

第 10~12 条是**故意保留的既有怪异行为**。它们看起来像 bug，但改动会导致画面变化，且不在本次「抽象」的范围内。建议在代码里留注释标注，作为后续独立任务的输入。

## 9.3 最终守则

改造完成后，这四条应当成为团队（或者你自己的）硬规则：

1. **`Engine/render/gl/` 之外，任何文件不得出现 `gl`、`GL_`、`GLuint`、`#include <glad/...>`。**
   —— 由 `cmake/CheckLayering.cmake` 自动保证。

2. **`Engine/core`、`Engine/voxel`、`Engine/utils`、`Engine/ui` 不得 include `render/device/`。**
   —— 游戏逻辑不认识设备。这条靠 code review 保证（守卫脚本可以扩展到检查 include 路径）。

3. **新增渲染特性 = 新增一个 `RenderPass` + 若干 `PipelineState` 数据，不新增 renderer 类。**
   —— 这是 8.10 的判据：新特性的 C++ 类型数量必须是常数。

4. **头文件里前置声明了 `T` 又持有 `std::unique_ptr<T>` 时，析构函数必须在 `.cpp` 里定义。**
   —— 否则每个 include 该头文件的 TU 都会实例化 `std::default_delete<T>`，触发 C2027，且报错落在 `<memory>` 里，看不出和头文件的关系。详见 [6.6](#66-enginewindowwindowh--cpp)。

5. **`IRenderDevice` 的生命周期必须长于所有网格注册表。**
   —— `MeshInstance::~MeshInstance` 会通过 `owner` 反向指针调用 `device->DestroyMesh()`（见 [3.2](#32-engineerenderscenemesh_instanceh)），而 `ChunkMeshSet` / `LodMeshRegistry` 的析构又依赖 `MeshInstance` 的析构。销毁顺序必须是：

   ```
   RenderSystem::Shutdown()        // 先释放注册表 → 其中的 MeshInstance 归还网格
   DestroyRenderResources(device)  // 再释放着色器 / 纹理 / 渲染目标
   device_.reset()                 // 最后才是设备本身
   ```

   顺序颠倒**不会立刻崩**，而是在进程退出时才炸 —— 属于最难查的一类。`Window` 与 `RenderSystem` 都是函数内 static 单例，构造顺序（`Window` 先、`RenderSystem` 后）让静态析构顺序恰好正确，但 `Window::Unload()` 里的**显式**顺序仍必须写对，否则提前返回/异常路径会踩到。

6. **首帧进入渲染循环前，必须保证 `RenderView` 已被填充一次。**
   —— `Window::Run` 的固定步长循环允许首帧不执行 `Update()`，而 `Render()` 是无条件执行的。默认构造的 `RenderView` 里 `settings` / `camera` / `chunkMesh` 全是 `nullptr` → 崩溃信息是「读取位置 `0x0000000000000001`」（`RenderSettings::useLucency` 在偏移 1）。防御见 [6.6](#66-enginewindowwindowh--cpp) 与 [4.10](#410-engine-renderframeworkframe_rendererh--cpp)。

---

## 附：实施工作量估算

| 阶段 | 新增行数 | 删除行数 | 风险 | 可否独立提交 |
|---|---|---|---|---|
| S1 建目录 | ~450 | ~40 | 极低 | ✅ |
| S2 掰正方向 + 建 scene/device/gl | ~1600 | ~250 | 低 | ✅ |
| S3 三分 RenderContext | ~300 | ~400 | 中 | ✅ |
| S4 Device 接线 + Pass 拆分 | ~700 | ~500 | 中高 | ✅ |
| S5 清理 | ~0 | ~600 | 极低 | ✅ |
| **合计** | **~3050** | **~1790** | | |

净增约 1260 行，但分布从「散落 6 个目录、每个目录都有 GL」收敛到「`render/gl` 一个目录」。

真正的工作量不在写新代码，而在 **S2/S3 的机械替换**（`GLMesh` → `MeshInstance`、`RenderContext&` → `Registry&`）。这部分建议用 IDE 的全局重命名 + 编译器错误驱动完成，不要手工逐个文件改。

