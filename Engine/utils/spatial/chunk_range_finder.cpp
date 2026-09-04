#include "chunk_range_finder.h"
#include <algorithm>

namespace nova_engine::utils::spatial {
	const std::vector<glm::ivec3> FindChunkInRange(int range) {
		std::vector<glm::ivec3> result;

		int minR = (-range) & ~31;
		int maxR = ((range - 1) & ~31) + 32;

		for(int x = minR;x <= maxR;x += 32)
			for (int y = minR;y <= maxR;y += 32)
				for (int z = minR;z <= maxR;z += 32) {
					int dx = x + 16, dy = y + 16, dz = z + 16;
					int distance = dx * dx + dy * dy + dz * dz;
					if (distance <= range * range) {
						result.push_back({ x,y,z });
					}
				}

		std::sort(result.begin(), result.end(),
			[](const glm::ivec3& a, const glm::ivec3& b) {
				long long distA = 1LL * a.x * a.x + 1LL * a.y * a.y + 1LL * a.z * a.z;
				long long distB = 1LL * b.x * b.x + 1LL * b.y * b.y + 1LL * b.z * b.z;
				if (distA != distB) return distA < distB; // 按距离升序

				// 距离相等时，按坐标字典序升序
				if (a.x != b.x) return a.x < b.x;
				if (a.y != b.y) return a.y < b.y;
				return a.z < b.z;
			});

		return result;
	}
}