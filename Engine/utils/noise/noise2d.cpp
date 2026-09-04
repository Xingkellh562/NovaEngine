#include "noise2d.h"
#include <random>
#include <numeric>

namespace nova_engine::utils::noise {
	Noise2D::Noise2D(int seed){
		std::iota(p_.begin(), p_.begin() + 256,0);
		std::mt19937 gen(seed);
		std::shuffle(p_.begin(), p_.begin() + 256,gen);

		//填充两次防止溢出
		for (int i = 256;i < 512;i++)
			p_[i] = p_[i & 255];
	}

	double Noise2D::Grad(int hash, double x, double y) const {
		int h = hash & 7;
		double u = h < 4 ? x : y;
		double v = h < 4 ? y : x;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}

	double Noise2D::Noise(double x, double y) const {
		int xi = static_cast<int>(std::floor(x)) & 255;
		int yi = static_cast<int>(std::floor(y)) & 255;
		double fx = x - std::floor(x);
		double fy = y - std::floor(y);

		double u = Fade(fx);
		double v = Fade(fy);

		int aa = p_[p_[xi] + yi];
		int ab = p_[p_[xi] + yi + 1];
		int ba = p_[p_[xi + 1] + yi];
		int bb = p_[p_[xi + 1] + yi + 1];

		double x1 = Lerp(Grad(aa, fx, fy), Grad(ba, fx - 1, fy), u);
		double x2 = Lerp(Grad(ab, fx, fy - 1), Grad(bb, fx - 1, fy - 1), u);

		double result = Lerp(x1, x2, v);

		// 结果范围大约 [-0.8,0.8]，映射到 [-1,1]
		return result / 0.8;
	}
}