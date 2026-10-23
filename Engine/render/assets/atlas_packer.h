#pragma once

#include <cstdint>
#include <vector>

namespace nova_engine::render::assets {

	// Skyline（Bottom-Left）矩形打包器。
	//
	// 维护一条"天际线" —— 一串水平线段，表示当前已占用的上边界。
	// 放新矩形时，从左到右找"能放下且落点最低"的位置（同样低时选最左），
	// 然后把天际线被覆盖的那段替换成矩形的新上边界。
	//
	// 不处理旋转。如果需要 90° 旋转，调用方在打包前把宽高互换即可，
	// 但那要求 TextureRegion 额外携带旋转标记 —— 见第 11 节。
	class SkylinePacker {
	public:
		struct Rect {
			uint32_t x = 0, y = 0, w = 0, h = 0;
		};

		SkylinePacker(uint32_t width, uint32_t height);

		// 放置一个 w×h 的矩形。放不下返回 false。
		bool Pack(uint32_t w, uint32_t h, Rect& out);

		// 当前已用到的最大高度（可用于紧裁单页尺寸）
		uint32_t UsedHeight() const;

		uint32_t width()  const { return width_; }
		uint32_t height() const { return height_; }

	private:
		struct Node {
			uint32_t x = 0;      // 线段左端
			uint32_t y = 0;      // 该段的上边界高度
			uint32_t width = 0;
		};

		uint32_t width_ = 0;
		uint32_t height_ = 0;
		std::vector<Node> skyline_;
	};

}