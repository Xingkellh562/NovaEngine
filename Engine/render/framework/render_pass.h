#pragma once

#include <string_view>

namespace nova_engine::render::device { class ICommandList; }
namespace nova_engine::render::scene { class RenderView; }

namespace nova_engine::render::framework {
	struct RenderResources;

	//一个渲染阶段
	//Record只负责提交
	//Record不拥有渲染目标，只使用RenderResources里的句柄
	class RenderPass {
	public:
		virtual ~RenderPass() = default;
		virtual std::string_view Name() const = 0;

		//返回false表示直接跳过这一帧什么都不做
		virtual bool Enabled(const scene::RenderView&) const { return true; }

		virtual void Record(const scene::RenderView& view,
			const RenderResources& resources,
			device::ICommandList& cmd) = 0;

	protected:
		RenderPass() = default;
	};
}