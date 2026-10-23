#include "atlas_packer.h"

#include <algorithm>
#include <limits>

namespace nova_engine::render::assets {

	SkylinePacker::SkylinePacker(uint32_t width, uint32_t height)
		: width_(width), height_(height) {
		// 初始天际线：一整条位于 y=0 的线段
		skyline_.push_back(Node{ 0u, 0u, width });
	}

	uint32_t SkylinePacker::UsedHeight() const {
		uint32_t h = 0;
		for (const Node& n : skyline_) h = std::max(h, n.y);
		return h;
	}

	bool SkylinePacker::Pack(uint32_t w, uint32_t h, Rect& out) {
		if (w == 0 || h == 0) return false;
		if (w > width_ || h > height_) return false;

		constexpr uint32_t kInf = std::numeric_limits<uint32_t>::max();

		uint32_t bestY = kInf;
		uint32_t bestX = 0;
		std::size_t bestI = 0;

		// ---- 1. 找落点：遍历每个可能的左端 ----
		for (std::size_t i = 0; i < skyline_.size(); ++i) {
			const uint32_t x = skyline_[i].x;
			if (x + w > width_) continue;

			// 向右跨越若干线段，累计宽度直到够 w；高度取沿途最大值
			uint32_t maxY = 0;
			uint32_t covered = 0;
			for (std::size_t j = i; j < skyline_.size() && covered < w; ++j) {
				maxY = std::max(maxY, skyline_[j].y);
				covered += skyline_[j].width;
			}
			if (covered < w) continue;             // 右侧放不下
			if (maxY + h > height_) continue;       // 超出页高

			// 最低优先，同低选最左
			if (maxY < bestY || (maxY == bestY && x < bestX)) {
				bestY = maxY;
				bestX = x;
				bestI = i;
			}
		}

		if (bestY == kInf) return false;

		out = Rect{ bestX, bestY, w, h };

		// ---- 2. 更新天际线 ----
		// 找出覆盖 [bestX, bestX+w) 的最后一个节点
		std::size_t last = bestI;
		uint32_t covered2 = 0;
		for (; last < skyline_.size() && covered2 < w; ++last) {
			covered2 += skyline_[last].width;
		}
		// 此时 last 指向「第一个未被完全覆盖」的节点
		const Node& tail = skyline_[last - 1];
		const uint32_t restRight = tail.x + tail.width;
		const uint32_t restY = tail.y;

		std::vector<Node> replacement;
		replacement.reserve(2);
		replacement.push_back(Node{ bestX, bestY + h, w });
		if (restRight > bestX + w) {
			// 最后一个节点被截断，右半部分保留原来的高度
			replacement.push_back(Node{ bestX + w, restY, restRight - (bestX + w) });
		}

		skyline_.erase(skyline_.begin() + bestI, skyline_.begin() + last);
		skyline_.insert(skyline_.begin() + bestI,
			replacement.begin(), replacement.end());

		return true;
	}

}