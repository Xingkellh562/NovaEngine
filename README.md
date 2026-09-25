# NovaEngine

NovaEngine 是一个使用 C++23 编写的体素（Voxel）游戏引擎，采用 OpenGL 作为渲染后端，借鉴 Minecraft 风格的方块世界。引擎以「数据与逻辑分离」为核心设计理念：世界数据（`World`）只负责存储状态，所有逻辑处理交由各个系统（System）完成。

## 特性

- **体素世界**：以 32×32×32 的区块（Chunk）为基本单位构建世界，支持区块的动态加载与卸载
- **空间索引**：基于模板化的 **八叉树（Octree）** 管理区块数据，配合 **对象池（ObjectPool）** 复用区块内存，减少频繁分配开销
- **地形生成**：使用 2D 噪声（Perlin 风格）程序化生成地形
- **方块系统**：通过 JSON 文件定义方块元数据与模型，支持多种方块类型（草、泥土、石头、圆石、木头、原木、台阶、玻璃等）
- **现代渲染管线**：
  - 平行光阴影映射（Shadow Mapping）
  - 环境光遮蔽（逐顶点AO）
  - 半透明物体渲染（WBOIT，基于权重的顺序无关透明）
  - 延迟/多阶段渲染与帧缓冲（FBO）封装
- **网格构建**：自动进行面剔除与基于顶点的环境光遮蔽（Mesh Builder）
- **输入系统**：WASD 移动、鼠标视角、放置/破坏方块、滚轮切换方块
- **调试 UI**：基于 ImGui 的运行时调试面板（FPS、区块数、对象池状态等）
- **日志系统**：基于 spdlog 的分级日志，提供 `NE_CORE_*` / `NE_CLIENT_*` 系列宏
- **固定步长游戏循环**：物理/逻辑更新采用固定步长 + Catch-up 机制，渲染使用 Alpha 插值

## 目录结构

```
NovaEngine/
├── CMake/                 # CMake 依赖配置（第三方库导入）
├── CMakeLists.txt         # 根 CMake 构建脚本
├── CMakePresets.json      # 各平台构建预设
├── Linking/               # 第三方库（头文件 / 预编译库 / 源码）
│   ├── include/           # GLFW、GLAD、GLM、ImGui、spdlog、json、stb_image、GoogleTest
│   ├── lib/               # GLFW 预编译库
│   └── src/               # GLAD、ImGui、GoogleTest 源码
├── assets/                # 运行时资源（方块模型 / 纹理）
│   ├── blocks/            # 方块定义（block.json + model.json）
│   └── block_textures/    # 方块纹理
├── Engine/                # 引擎核心（编译为 NovaEngineCore 静态库）
│   ├── main.cpp           # 程序入口
│   ├── collection/        # 数据结构（八叉树、对象池）
│   ├── core/              # 配置、输入、世界
│   ├── log/               # 日志系统
│   ├── render/            # 渲染系统（OpenGL 后端、网格、纹理、着色器）
│   ├── ui/                # 调试 UI
│   ├── utils/             # 工具（模型加载、噪声、空间分析）
│   ├── voxel/             # 体素（方块、区块、行星、地形）
│   └── window/            # 窗口与游戏主循环
└── Tests/                 # GoogleTest 单元测试
```

### 引擎核心模块说明

| 模块 | 说明 |
| --- | --- |
| `core/world` | `World` 结构体，世界所有核心数据（区块数据、玩家状态）的集合，只存储数据不做逻辑 |
| `core/input` | `InputSystem`，处理玩家输入，更新世界状态 |
| `voxel/chunk` | `Chunk`（单个区块体素数据）、`ChunkData`（区块集合，由八叉树索引）、`ChunkSystem`（区块加载/卸载逻辑） |
| `voxel/block` | `Block`、`BlockLibrary`（从 JSON 加载方块定义） |
| `voxel/terrain` | `TerrainGenerator`，基于 2D 噪声生成地形 |
| `render/system` | `RenderSystem`，负责渲染调度与 LOD 网格更新 |
| `render/opengl` | OpenGL 后端实现：渲染器、相机、着色器、帧缓冲、纹理、网格等 |
| `window` | `Window` 单例，负责 GLFW 窗口初始化与固定步长主循环 |
| `log` | `LogManager` 与日志宏，基于 spdlog |

## 依赖

| 库 | 用途 |
| --- | --- |
| [GLFW](https://www.glfw.org/) | 窗口创建与输入管理（预编译库） |
| [GLAD](https://glad.dav1d.de/) | OpenGL 加载器 |
| [GLM](https://github.com/g-truc/glm) | 数学库（头文件） |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试 UI |
| [spdlog](https://github.com/gabime/spdlog) | 日志库 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析（方块模型） |
| [stb_image](https://github.com/nothings/stb) | 纹理加载 |
| [GoogleTest](https://github.com/google/googletest) | 单元测试 |

## 构建

### 环境要求

- CMake ≥ 3.10
- 支持 C++23 的编译器（MSVC / GCC / Clang）
- OpenGL 4.5 及以上
- 第三方库已置于 `Linking/` 目录，无需额外安装

### Windows（Visual Studio / Ninja）

项目提供了 CMake 预设，可直接选择对应的构建预设：

```bash
# 使用 CMake 预设配置（x64 Debug）
cmake --preset x64-debug

# 构建
cmake --build out/build/x64-debug
```

可用的 Windows 预设：`x64-debug`、`x64-release`、`x86-debug`、`x86-release`。

### Linux / macOS

```bash
# Linux
cmake --preset linux-debug
cmake --build out/build/linux-debug

# macOS
cmake --preset macos-debug
cmake --build out/build/macos-debug
```

构建完成后，着色器与资源会通过后置构建步骤自动拷贝到可执行文件目录下（`shaders/` 与 `assets/`）。

## 运行与控制

运行生成的可执行文件 `NovaEngine` 后即可进入世界。

| 按键 | 功能 |
| --- | --- |
| `W` / `A` / `S` / `D` | 前后左右移动 |
| `Space` | 上升 |
| `Left Shift` | 下降 |
| 鼠标移动 | 视角旋转 |
| 鼠标左键 | 破坏方块 |
| 鼠标右键 | 放置方块 |
| 滚轮 | 切换手持方块 |
| `F1` | 切换阴影开关 |
| `F2` | 切换半透明网格开关 |
| `Esc` | 退出程序 |

## 测试

测试使用 GoogleTest 编写，位于 `Tests/` 目录，覆盖八叉树、区块数据、模型加载、网格缓存等模块。

```bash
# 配置后构建测试目标
cmake --build out/build/<preset> --target EngineCoreTests

# 运行测试
./out/build/<preset>/Tests/EngineCoreTests
```

## 设计说明

- **数据与逻辑分离**：`World` 仅存储数据与状态，所有逻辑处理均在对应的 System 中进行，便于后续扩展（实体、物理等）。
- **命名空间**：所有代码位于 `nova_engine` 命名空间下，按 `nova_engine::<模块>::<子模块>` 层级组织。
