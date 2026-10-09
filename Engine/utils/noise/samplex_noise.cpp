#include "samplex_noise.h"
#include <algorithm>
#include <cstdint>

namespace nova_engine::utils::noise {
	double SamplexNoise::Noise(double x, double y, double z) const {
		constexpr double F3 = 1.0 / 3.0;
		constexpr double G3 = 1.0 / 6.0;
		double s = (x + y + z) * F3;
		int i = FastFloor(x + s);
		int j = FastFloor(y + s);
		int k = FastFloor(z + s);
		double t = (i + j + k) * G3;
		double X0 = i - t;
		double Y0 = j - t;
		double Z0 = k - t;
		double x0 = x - X0;
		double y0 = y - Y0;
		double z0 = z - Z0;
		int i1, j1, k1;
		int i2, j2, k2;
		if (x0 >= y0) {
			if (y0 >= z0) {
				i1 = 1; j1 = 0; k1 = 0;
				i2 = 1; j2 = 1; k2 = 0;
			}
			else if (x0 >= z0) {
				i1 = 1; j1 = 0; k1 = 0;
				i2 = 1; j2 = 0; k2 = 1;
			}
			else {
				i1 = 0; j1 = 0; k1 = 1;
				i2 = 1; j2 = 0; k2 = 1;
			}
		}
		else {
			if (y0 < z0) {
				i1 = 0; j1 = 0; k1 = 1;
				i2 = 0; j2 = 1; k2 = 1;
			}
			else if (x0 < z0) {
				i1 = 0; j1 = 1; k1 = 0;
				i2 = 0; j2 = 1; k2 = 1;
			}
			else {
				i1 = 0; j1 = 1; k1 = 0;
				i2 = 1; j2 = 1; k2 = 0;
			}
		}
		double x1 = x0 - i1 + G3;
		double y1 = y0 - j1 + G3;
		double z1 = z0 - k1 + G3;
		double x2 = x0 - i2 + 2.0 * G3;
		double y2 = y0 - j2 + 2.0 * G3;
		double z2 = z0 - k2 + 2.0 * G3;
		double x3 = x0 - 1.0 + 3.0 * G3;
		double y3 = y0 - 1.0 + 3.0 * G3;
		double z3 = z0 - 1.0 + 3.0 * G3;

		int ii = i & 255;
		int jj = j & 255;
		int kk = k & 255;

		double n0 = 0.0, n1 = 0.0, n2 = 0.0, n3 = 0.0;
		double t0 = 0.6 - x0 * x0 - y0 * y0 - z0 * z0;
		if (t0 > 0.0) {
			t0 *= t0;
			n0 = t0 * t0 * Dot(grad3_[perm_[ii + perm_[jj + perm_[kk]]] % 12], x0, y0, z0);
		}
		double t1 = 0.6 - x1 * x1 - y1 * y1 - z1 * z1;
		if (t1 > 0.0) {
			t1 *= t1;
			n1 = t1 * t1 * Dot(grad3_[perm_[ii + i1 + perm_[jj + j1 + perm_[kk + k1]]] % 12], x1, y1, z1);
		}
		double t2 = 0.6 - x2 * x2 - y2 * y2 - z2 * z2;
		if (t2 > 0.0) {
			t2 *= t2;
			n2 = t2 * t2 * Dot(grad3_[perm_[ii + i2 + perm_[jj + j2 + perm_[kk + k2]]] % 12], x2, y2, z2);
		}
		double t3 = 0.6 - x3 * x3 - y3 * y3 - z3 * z3;
		if (t3 > 0.0) {
			t3 *= t3;
			n3 = t3 * t3 * Dot(grad3_[perm_[ii + 1 + perm_[jj + 1 + perm_[kk + 1]]] % 12], x3, y3, z3);
		}
		return 32.0 * (n0 + n1 + n2 + n3);
	}

}