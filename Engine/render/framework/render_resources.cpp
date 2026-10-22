#include "render_resources.h"

#include <render/device/render_device.h>

#include <log/logger.h>

#include <filesystem>

namespace nova_engine::render::framework {

	using namespace nova_engine::render;

	namespace {

		// 呼应 gltexture_array.cpp:42-43 的纹理数组默认采样状态
		//   GL_TEXTURE_MIN_FILTER = GL_NEAREST_MIPMAP_LINEAR
		//   GL_TEXTURE_MAG_FILTER = GL_NEAREST
		core::SamplerState BlockArraySampler() {
			return core::SamplerState{ core::TextureFilter::NearestMipmapLinear,
								 core::TextureFilter::Nearest,
								 core::TextureWrap::ClampToEdge };
		}

		// 呼应 render_system.cpp:102 在 WBOIT 通道前改成的 GL_LINEAR
		core::SamplerState LinearSampler() {
			return core::SamplerState{ core::TextureFilter::Linear,
								 core::TextureFilter::Linear,
								 core::TextureWrap::ClampToEdge };
		}

		core::ShaderHandle MakeShader(device::IRenderDevice& device,
			std::initializer_list<core::ShaderStageDesc> stages) {
			core::ShaderDesc desc;
			desc.stages.assign(stages.begin(), stages.end());
			return device.CreateShader(desc);
		}

		core::TextureArrayDesc MakeBlockTextureArrayDesc(std::string_view path) {
			namespace fs = std::filesystem;

			std::vector<fs::path> filePaths;
			for (const auto& entry : fs::directory_iterator(path)) {
				if (entry.is_regular_file() && entry.path().extension() == ".png") {
					filePaths.push_back(entry.path());
				}
			}

			std::sort(filePaths.begin(), filePaths.end());
			core::TextureArrayDesc desc{};
			desc.defaultSampler.minFilter = core::TextureFilter::LinearMipmapLinear;
			desc.defaultSampler.magFilter = core::TextureFilter::Linear;
			desc.defaultSampler.wrap = core::TextureWrap::Repeat;
			desc.generateMipmaps = true;
			if (filePaths.empty()) {
				NE_CORE_ERROR("RenderSystem", "Create texture array failed,because the directory is empty.");
				return desc;
			}
			for (int i = 0;i < filePaths.size();i++) {
				core::TextureArrayLayer layer{};
				layer.name = filePaths[i].stem().string();
				layer.path = filePaths[i].string();
				layer.index = i;

				desc.sources.push_back(layer);
			}
			desc.layers = static_cast<uint32_t>(filePaths.size());
			return desc;
		}

	} // namespace

	RenderResources CreateRenderResources(device::IRenderDevice& device,
		uint32_t width, uint32_t height) {
		RenderResources res;

		// ================= 着色器 =================
		// 对应改动前 shader_library.cpp:6-28
		res.baseShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/frag.glsl" },
			});
		res.lodShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/lod_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/lod_frag.glsl" },
			});
		res.shadowShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/para_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/para_frag.glsl" },
			});
		res.postProcessShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/post_process_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/post_process_frag.glsl" },
			});
		res.depthPrepassShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/depth_prepass_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/depth_prepass_frag.glsl" },
			});
		res.accumPassShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/accum_pass_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/accum_pass_frag.glsl" },
			});
		res.lightShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/light_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/light_frag.glsl" },
			});
		res.lodLightShader = MakeShader(device, {
			{ core::ShaderStage::Vertex,   "shaders/lod_light_vert.glsl" },
			{ core::ShaderStage::Fragment, "shaders/lod_light_frag.glsl" },
			});

		// ================= 渲染目标 =================
		// MainFrame: ColorTex(GL_RGBA8) + DepthTex(GL_DEPTH_COMPONENT32F)，跟随窗口
		{
			core::RenderTargetDesc desc;
			desc.attachments.push_back({ "ColorTex", core::TextureFormat::RGBA8,  false, 0, 0, 0 });
			desc.attachments.push_back({ "DepthTex", core::TextureFormat::Depth32F, true, 0, 0, 0 });
			res.mainFrame = device.CreateRenderTarget(desc);
		}

		// LightFrame: LightTex(GL_RGB16F) + DepthTex(GL_DEPTH_COMPONENT32F)，跟随窗口
		{
			core::RenderTargetDesc desc;
			desc.attachments.push_back({ "LightTex", core::TextureFormat::RGB16F,  false, 0, 0, 0 });
			desc.attachments.push_back({ "DepthTex", core::TextureFormat::Depth32F, true, 0, 0, 0 });
			res.lightFrame = device.CreateRenderTarget(desc);
		}

		// ShadowFrame: ShadowMap(GL_DEPTH_COMPONENT32F)，固定 2048x2048
		// 深度比较模式 + 边框色 1.0，对应 frame_buffer_library.cpp:24-33
		{
			core::RenderTargetDesc desc;
			core::RenderTargetAttachment sm;
			sm.name = "ShadowMap";
			sm.format = core::TextureFormat::Depth32F;
			sm.isDepth = true;
			sm.fixedWidth = 2048;
			sm.fixedHeight = 2048;
			desc.attachments.push_back(sm);
			res.shadowFrame = device.CreateRenderTarget(desc);
			res.shadowResolution = 2048;
		}

		// OITFrame: AccumTex(GL_RGBA16F) + RevealTex(GL_R16F)，深度复用 MainFrame
		{
			res.mainDepth = device.GetAttachment(res.mainFrame, "DepthTex");

			core::RenderTargetDesc desc;
			desc.attachments.push_back({ "AccumTex",  core::TextureFormat::RGBA16F, false, 0, 0, 0 });
			desc.attachments.push_back({ "RevealTex", core::TextureFormat::R16F,    false, 1, 0, 0 });

			core::RenderTargetAttachment depthAlias;
			depthAlias.name = "DepthTex";
			depthAlias.isDepth = true;
			depthAlias.imported = res.mainDepth;   // 复用主帧深度
			desc.attachments.push_back(depthAlias);

			res.oitFrame = device.CreateRenderTarget(desc);
		}

		//采样器种类
		res.blockSampler = BlockArraySampler();
		res.linearSampler = LinearSampler();
		res.shadowSampler = LinearSampler();
		res.shadowSampler.depthCompareMode = true;

		// ================= 附件句柄 =================
		res.backBuffer = device.BackBuffer();
		res.mainColor = device.GetAttachment(res.mainFrame, "ColorTex");
		res.lightTex = device.GetAttachment(res.lightFrame, "LightTex");
		res.shadowMap = device.GetAttachment(res.shadowFrame, "ShadowMap");
		res.accumTex = device.GetAttachment(res.oitFrame, "AccumTex");
		res.revealTex = device.GetAttachment(res.oitFrame, "RevealTex");

		// ================= 方块纹理数组 =================
		res.blockTextures = device.CreateTextureArray(MakeBlockTextureArrayDesc("assets/block_textures"));

		// ================= 内建网格 =================
		res.fullscreenQuad = device.FullscreenQuad();

		// ================= 管线状态 =================

		// 阴影
		res.shadowPipeline.shader = res.shadowShader;
		res.shadowPipeline.blendMode = core::BlendMode::Opaque;
		res.shadowPipeline.depthTest = core::DepthTest::Less;
		res.shadowPipeline.depthWrite = true;
		res.shadowPipeline.colorWrite = true;
		res.shadowPipeline.cullMode = core::CullMode::Back;
		res.shadowPipeline.colorTargets = 1;
		res.shadowPipeline.vertexLayout = core::VertexLayoutId::VoxelShadow;
		res.shadowPipeline.samplerState = res.linearSampler;

		// 不透明
		res.opaquePipeline.shader = res.baseShader;
		res.opaquePipeline.blendMode = core::BlendMode::Opaque;
		res.opaquePipeline.depthTest = core::DepthTest::Less;
		res.opaquePipeline.depthWrite = true;
		res.opaquePipeline.cullMode = core::CullMode::Back;
		res.opaquePipeline.colorTargets = 1;
		res.opaquePipeline.vertexLayout = core::VertexLayoutId::VoxelMain;
		res.opaquePipeline.samplerState = res.blockSampler;

		// Lod
		res.lodPipeline = res.opaquePipeline;
		res.lodPipeline.shader = res.lodShader;

		// 冯氏光照
		res.lightPipeline = res.opaquePipeline;
		res.lightPipeline.shader = res.lightShader;
		res.lightPipeline.vertexLayout = core::VertexLayoutId::VoxelLight;

		res.lodLightPipeline = res.lightPipeline;
		res.lodLightPipeline.shader = res.lodLightShader;

		// WBOIT 深度预通道
		// 关键：colorWrite = false 对应 glColorMask(FALSE,FALSE,FALSE,FALSE)
		res.wboitDepthPipeline = res.opaquePipeline;
		res.wboitDepthPipeline.shader = res.depthPrepassShader;
		res.wboitDepthPipeline.blendMode = core::BlendMode::Opaque;
		res.wboitDepthPipeline.depthTest = core::DepthTest::LessEqual;
		res.wboitDepthPipeline.depthWrite = true;
		res.wboitDepthPipeline.colorWrite = false;
		res.wboitDepthPipeline.cullMode = core::CullMode::Back;
		res.wboitDepthPipeline.samplerState = res.shadowSampler;

		// WBOIT 累加
		// 关键：colorTargets = 2 且 blend = WboitAccum（按附件分别设置混合函数）
		res.wboitAccumPipeline = res.wboitDepthPipeline;
		res.wboitAccumPipeline.shader = res.accumPassShader;
		res.wboitAccumPipeline.blendMode = core::BlendMode::WboitAccum;
		res.wboitAccumPipeline.depthWrite = false;
		res.wboitAccumPipeline.colorWrite = true;
		res.wboitAccumPipeline.colorTargets = 2;

		// 后处理
		res.postProcessPipeline.shader = res.postProcessShader;
		res.postProcessPipeline.blendMode = core::BlendMode::Opaque;
		res.postProcessPipeline.depthTest = core::DepthTest::None;
		res.postProcessPipeline.depthWrite = false;
		res.postProcessPipeline.cullMode = core::CullMode::None;
		res.postProcessPipeline.colorTargets = 1;
		res.postProcessPipeline.vertexLayout = core::VertexLayoutId::ScreenQuad;
		res.postProcessPipeline.samplerState = res.linearSampler;

		NE_CORE_INFO("RenderFramework", "Render resources created on {}", device.Name());

		std::vector< core::RenderTargetHandle> targets = {
			res.mainFrame, res.lightFrame,res.shadowFrame,res.oitFrame,res.backBuffer
		};
		/*for (const auto& f : targets)
			if (!f.Valid()) NE_CORE_ERROR("RenderFramework", "RenderTarget ID : {} creation FAILED", f.id);*/

		for (int id = 0;id < targets.size();id++) {
			if (!targets[id].Valid()) NE_CORE_ERROR("RenderFramework", "RenderTarget ID : {} creation FAILED", id);
		}
		// textures / shaders 同理

		// 附件句柄必须互不相同 —— 这正是 GetAttachment 返回错附件的症状
		if (res.mainColor.id == res.mainDepth.id)
			NE_CORE_ERROR("RenderFramework", "mainColor == mainDepth (GetAttachment 查错了)");
		if (res.accumTex.id == res.revealTex.id)
			NE_CORE_ERROR("RenderFramework", "accumTex == revealTex (GetAttachment 查错了)");

		NE_CORE_INFO("RenderFramework",
			"Resources: main={} light={} shadow={} oit={} | color={} depth={} "
			"lightTex={} shadowMap={} accum={} reveal={} blocks={}",
			res.mainFrame.id, res.lightFrame.id, res.shadowFrame.id, res.oitFrame.id,
			res.mainColor.id, res.mainDepth.id, res.lightTex.id, res.shadowMap.id,
			res.accumTex.id, res.revealTex.id, res.blockTextures.id);
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

}