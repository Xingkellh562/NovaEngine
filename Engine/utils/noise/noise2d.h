#pragma once

#include <array>

namespace nova_engine::utils::noise {
	class Noise2D {
	public:
		Noise2D(int seed);
		~Noise2D() = default;

		Noise2D(const Noise2D&) = delete;
		Noise2D& operator=(const Noise2D) = delete;

		double Noise(double x,double y) const;
	private:
		std::array<int, 512> p_;

		const std::array<std::array<int, 2>, 8> 
			grad3_ = {
				{{1,1}, {-1,1}, {1,-1}, {-1,-1},
				{1,0}, {-1,0}, {0,1}, {0,-1}}
			};

		inline double Fade(double t) const {
			return t * t * (3 - 2 * t);
		}

		inline double Lerp(double a,double b,double t) const {
			return a + t * (b - a);
		}

		inline double Grad(int hash,double x,double y) const;
	};
}