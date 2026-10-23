#pragma once

#include <render/core/camera.h>
#include <render/core/render_settings.h>

#include "chunk_mesh_registry.h"
#include "lod_mesh_registry.h"
#include "ui_mesh_registry.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>


#include <cstdint>

namespace nova_engine::render::scene {

	// 一帧渲染所需的全部输入。由 RenderSystem::BuildView 产出。
	//
	// 这是高层与渲染层之间唯一的桥梁：
	//   - 游戏逻辑 / RenderSystem 只负责填这个结构
	//   - Pass 只负责读这个结构
	struct RenderView {
		const core::Camera* camera = nullptr;
		const core::Camera* shadowCamera = nullptr;
		const RenderSettings* settings = nullptr;

		const ChunkMeshSet* chunkMesh = nullptr;
		const LodMeshRegistry* lodMesh = nullptr;
		const UIMeshRegistry* uiMesh = nullptr;


		// 渲染相对原点。
		//
		// 需要两个是**继承自现有行为**：改动前 BlockRenderer / LightRenderer /
		// SemiTransparentRenderer 用 MainCamera->position 作相对原点
		// （block_renderer.cpp:40、light_renderer.cpp:9、semi_transparent_renderer.cpp:63），
		// 而 ParallelLightRenderer 用的是 ShadowCamera->position
		// （parallel_light_renderer.cpp:39）。这是既有行为，平移时必须保持一致。
		glm::vec3 mainOrigin{ 0.0f };      // 主相机位置，用于 opaque / light / transparent
		glm::vec3 shadowOrigin{ 0.0f };    // 阴影相机位置，用于 shadow

		// 视口尺寸（原 MainFrame 的分辨率）
		uint32_t viewportWidth = 0;
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
		glm::mat4 shadowViewProj{ 1.0f };      // 阴影 Pass 使用
		glm::mat4 shadowSampleMatrix{ 1.0f };  // 后处理通道采样阴影使用

		bool IsValid() const {
			return camera && shadowCamera && settings && chunkMesh
				&& viewportWidth > 0 && viewportHeight > 0;
		}
	};

}