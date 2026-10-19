# NovaEngine

NovaEngine 是一个使用 C++23 编写的体素（Voxel）游戏引擎，以 OpenGL 4.5 Core 作为图形后端。

引擎目前处于**渲染骨架迁移后的中间阶段**：图形 API 已完成收口，但内容管线（方块系统、LOD、半透明）正在重做。下面「当前状态」一节说明了哪些是已完成、哪些是进行中。

---

## 当前状态

### 已完成

- **渲染分层**：图形 API 调用已完全收口到 `render/gl/` 一个目录，`core` / `voxel` / `utils` / `ui` / `window` / `render/system` / `render/framework` / `render/scene` 中不再出现任何 `gl*` 调用或 glad/GLFW 头文件。
- **渲染管线**：`IRenderDevice` / `ICommandList` 抽象 + 固定 5 个 Pass（阴影 → 不透明 → 光照系数 → 半透明 → 后处理），支持平行光阴影映射、多渲染目标（FBO）、采样器状态与 VAO 缓存。
- **世界与网格**：32³ 区块 + 对象池 + 八叉树索引；基于 **Surface Nets** 的平滑密度场网格生成，取代了早期的 MC 风格面剔除网格。
- **多线程**：无锁 SPSC 任务/结果队列 + 工作线程，区块生成与网格构建在子线程完成，**GPU 上传始终在主线程**。
- **基础设施**：GLFW 窗口与固定步长（60 Hz + Catch-up + Alpha 插值）游戏循环、基于 spdlog 的异步分级日志、基于 ImGui 的运行时调试面板、WASD/鼠标输入与射线拾取。

### 进行中 / 暂未接入

| 模块 | 状态 |
| --- | --- |
| **方块系统** | 正在重做。`assets/blocks/<方块>/{block.json,model.json,components.json}` 的目录格式与 JSON 结构仍在，`BlockLibrary` 目前只读取 `block.json` 的元数据（id / 名称），`model.json` 的几何与纹理尚未接入 —— 因此方块的自定义形状与逐面贴图暂不生效。旧的 `ModelLoader` 已归档为 `.txt`。 |
| **LOD 系统** | 正在重做。`AdaptiveLodOctree` 与 `LodMeshRegistry` 已就位，但细分/合并逻辑尚未接线，`LodSystem` 已归档为 `.txt`，当前渲染不产生 LOD 网格。 |
| **半透明渲染（WBOIT）** | 管线与 Pass 已完整实现（累加/显现双附件 + 独立深度预通道），但**当前没有任何几何提交到 Lucency 层**，因此该 Pass 不会执行。等新方块系统接入后恢复。 |
| **星球地形** | `PlanetTerrainGenerator` 是**临时验证代码**，用 3D 噪声密度场生成一个简单球壳，用于确认程序能跑通渲染；它忽略了传入的星球中心与半径参数，不代表最终地形方案。 |

---

## 目录结构

```
NovaEngine/
├── CMake/                 # CMake 依赖配置
├── CMakeLists.txt         # 根构建脚本
├── CMakePresets.json      # 各平台构建预设
├── Linking/               # 第三方库（头文件 / 预编译库 / 源码）
├── assets/                # 运行时资源
│   ├── blocks/            # 方块定义（block.json / model.json / components.json）
│   ├── block_textures/    # 方块纹理（启动时按文件名字母序打包为纹理数组）
│   └── grass.png
├── Engine/
│   ├── main.cpp           # 程序入口
│   ├── collection/        # 数据结构：octree、adaptive_lod_octree、object_pool
│   ├── core/
│   │   ├── concurrency/   # spsc_queue、task、worker
│   │   ├── config/        # 全局常量（区块尺寸等）
│   │   ├── input/         # 输入状态与行为
│   │   └── world/         # World 数据聚合
│   ├── log/               # 日志系统
│   ├── render/
│   │   ├── core/          # 只依赖 glm：Camera、MeshData、句柄、枚举、管线状态、描述符
│   │   ├── device/        # 纯虚接口：IRenderDevice、ICommandList
│   │   ├── framework/     # 无 GL：FrameRenderer、RenderResources、5 个 Pass
│   │   ├── scene/         # 纯数据 + 句柄：网格注册表、RenderView
│   │   ├── gl/            # ★ 全项目唯一允许出现 gl* 的目录
│   │   ├── system/        # RenderSystem（产出 RenderView）
│   │   └── utils/         # 网格构建：smooth_mesher、mesh_builder
│   ├── ui/                # 调试 UI（IUiBackend + DebugUIManager）
│   ├── utils/             # 噪声、空间工具、位打包
│   ├── voxel/             # 方块、区块、星球、地形
│   └── window/            # 窗口与主循环（唯一的 GLFW 组装点）
├── Tests/                 # GoogleTest 单元测试
└── out/                   # 构建输出（不入库）
```

---

## 渲染分层

依赖方向严格单向，箭头只能向下：

```
core/ voxel/ utils/ ui/                    游戏逻辑，看不见图形 API
        ↓
render/scene/     MeshInstance / ChunkMeshRegistry / LodMeshRegistry / RenderView
        ↓
render/framework/ 5 个 Pass + FrameRenderer + RenderResources        无任何 gl*
        ↓
render/device/    IRenderDevice / ICommandList（纯虚接口）            只有引擎自己的枚举
        ↓
render/gl/        GLDevice / GLCommandList / GLShader / GLTexture    ★ 唯一碰 gl 的地方
        ↓
render/core/      Camera / MeshData / Handle / PipelineState         只依赖 glm
```

**不变式**：`gl*` 调用与 glad/GLFW 头文件只允许出现在 `Engine/render/gl/` 与 `Engine/window/`（后者是唯一的组装点）。

### 帧管线

`Window::Render` → `FrameRenderer::Execute`，固定顺序：

| Pass | 目标 | 说明 |
| --- | --- | --- |
| `ShadowPass` | ShadowMap 2048²（仅深度） | 写入平行光深度 |
| `OpaquePass` | MainFrame（RGBA8 + Depth32F） | 采样方块纹理数组 |
| `LightPass` | LightFrame（RGB16F + 独立深度） | 同批几何二次光栅化，生成光照系数图 |
| `TransparentPass` | MainFrame 深度 → OITFrame（RGBA16F + R16F） | WBOIT 权重累加；**当前无几何提交，不执行** |
| `PostProcessPass` | BackBuffer | 合成颜色/阴影/光照/半透明，输出最终画面 |

---

## 关键模块

| 模块 | 说明 |
| --- | --- |
| `core/world` | `World` 结构体：区块数据、玩家状态、脏区域队列，只存数据不做逻辑 |
| `core/concurrency` | `SpscQueue`（无锁单生产者单消费者）、`Worker`、`Task` / `CancelToken` |
| `voxel/chunk` | `Chunk<T>`（按位运算索引）、`ChunkData`（区块集合） |
| `voxel/planet` | `PlanetChunkData` / `PlanetChunkSystem`（当前实际使用的区块路径） |
| `voxel/block` | `Block`、`BlockLibrary`（从 JSON 加载方块元数据） |
| `voxel/terrain` | `TerrainGenerator`（2D 噪声高度图）、`PlanetTerrainGenerator`（3D 噪声密度场，临时） |
| `render/utils/smooth_mesher` | Surface Nets 平滑网格生成，输入 `Chunk<float>` 密度场，输出 `MeshData` |
| `render/system/render_system` | 区块网格的提交与回收，产出 `RenderView` |
| `collection/data_structure` | `Octree`、`AdaptiveLodOctree`、`ObjectPool` |
| `log` | 异步 spdlog，输出到控制台与 `logs/`（10 MB × 5 轮转） |

---

## 依赖

| 库 | 形式 | 用途 |
| --- | --- | --- |
| [GLFW](https://www.glfw.org/) | 预编译库 | 窗口与输入 |
| [GLAD](https://glad.dav1d.de/) | 源码编译 | OpenGL 加载器 |
| [GLM](https://github.com/g-truc/glm) | 头文件 | 数学库 |
| [Dear ImGui](https://github.com/ocornut/imgui) | 源码编译 | 调试 UI |
| [spdlog](https://github.com/gabime/spdlog) | 头文件 | 日志 |
| [nlohmann/json](https://github.com/nlohmann/json) | 头文件 | JSON 解析 |
| [stb_image](https://github.com/nothings/stb) | 头文件 | 纹理加载 |
| [GoogleTest](https://github.com/google/googletest) | 源码编译 | 单元测试 |

全部第三方库已置于 `Linking/`，无需额外安装。

---

## 构建

### 环境要求

- CMake ≥ 3.10
- 支持 C++23 的编译器（MSVC / GCC / Clang）
- 支持 OpenGL 4.5 的显卡与驱动
- Windows 预设使用 Ninja 生成器，需要 `cl.exe` 在 PATH 中（或在 VS 开发者命令行下执行）

### Windows

```bash
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

可用预设：`x64-debug`、`x64-release`、`x86-debug`、`x86-release`。

### Linux / macOS

```bash
cmake --preset linux-debug  && cmake --build out/build/linux-debug
cmake --preset macos-debug  && cmake --build out/build/macos-debug
```

### 运行

```bash
./out/build/x64-debug/Engine/NovaEngine
```

> **注意**：着色器（`shaders/`）与资源（`assets/`）通过构建后步骤拷贝到可执行文件目录，且代码按**相对当前工作目录**的路径加载它们。请从可执行文件所在目录运行，否则会因找不到 `assets/` 或 `shaders/` 而失败。

---

## 操作

| 按键 | 功能 |
| --- | --- |
| `W` / `A` / `S` / `D` | 前后左右移动 |
| `Space` / `Left Shift` | 上升 / 下降 |
| 鼠标移动 | 视角旋转 |
| 鼠标左键 | 破坏方块 |
| 鼠标右键 | 放置方块 |
| 滚轮 | 切换手持方块 |
| `F1` | 阴影开关 |
| `F2` | 半透明开关 |
| `F3` | LOD 开关（LOD 尚未接入，暂无效果） |
| `F4` | 调试面板开关 |
| `Esc` | 退出 |

---

## 测试

测试使用 GoogleTest 编写。

```bash
cmake --build out/build/x64-debug --target EngineCoreTests
./out/build/x64-debug/Tests/EngineCoreTests
```

> **已知问题**：`Tests/CMakeLists.txt` 目前只编译 `main_test.cpp`，`Tests/test_file/` 下的测试源文件尚未纳入构建目标（其中若干引用了已归档的头文件）。在修好之前，上面的命令不会实际运行任何用例。

---

## 设计说明

- **数据与逻辑分离**：`World` 只存储数据与状态，逻辑处理交由各 System 完成，便于后续扩展实体、物理等。
- **图形 API 收口**：所有 GPU 资源都通过 `IRenderDevice` 创建并返回类型化句柄，上层代码不接触 `GLuint`，也不 include glad。
- **命名空间**：所有代码位于 `nova_engine` 命名空间下，按 `nova_engine::<模块>::<子模块>` 分层组织。
