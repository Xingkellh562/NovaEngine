#pragma once

#include <array>
#include <numeric>
#include <random>

namespace nova_engine::utils::noise {
	class SamplexNoise {
	public:
		explicit SamplexNoise(size_t seed = 0) {
			std::iota(perm_.begin(), perm_.begin() + 256, 0);
			std::mt19937 rng(seed);
			std::shuffle(perm_.begin(), perm_.begin() + 256, rng);

			for (int i = 0;i < 256; ++i) {
				perm_[256 + i] = perm_[i];
			}
		}

		double Noise(double x, double y, double z) const;

	private:
		std::array<int, 512> perm_;

		static constexpr std::array<std::array<int,3>,12> grad3_ = {{
			{1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
			{1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
			{0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1}
		} };

		static int FastFloor(double x) {
			return x > 0 ? static_cast<int>(x) : static_cast<int>(x) - 1;
		}

		static double Dot(const std::array<int,3>& g, double x, double y, double z) {
			return g[0] * x + g[1] * y + g[2] * z;
		}
	};
}