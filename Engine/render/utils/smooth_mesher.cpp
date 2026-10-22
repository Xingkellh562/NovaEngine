#include "smooth_mesher.h"
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>
#include <core/config/config.h>
#include <glm/glm.hpp>
#include <memory>
#include <utility>
#include <mutex>
#include <utils/assist/des_packer.h>

namespace nova_engine::render::utils {
	//Marching Cubes查找表。表格本身是从标准表重新生成的，
		//生成时验证过：文档中的示例配置(cubeindex 8/9)、边表与"两端角点分类不同"的棱完全一致、
		//三角形表用到的棱都是该配置下被切割的棱。
		//角点编号采用立方体的环绕式编号，bit = (z << 2) | (y << 1) | x：
		//  0:(0,0,0) 1:(1,0,0) 2:(1,1,0) 3:(0,1,0) 4:(0,0,1) 5:(1,0,1) 6:(1,1,1) 7:(0,1,1)
		//棱编号 e = 轴 * 4 + 面内位，棱的两端在该轴上坐标分别为0和1：
		//  0-3: x方向  4-7: y方向  8-11: z方向
		//12条棱的相交掩码：第e位为1表示棱e与等值面相交
	static constexpr std::array<std::uint16_t, 256> edgeTable = {
		0x000, 0x111, 0x221, 0x330, 0x822, 0x933, 0xa03, 0xb12,
		0x412, 0x503, 0x633, 0x722, 0xc30, 0xd21, 0xe11, 0xf00,
		0x144, 0x055, 0x365, 0x274, 0x966, 0x877, 0xb47, 0xa56,
		0x556, 0x447, 0x777, 0x666, 0xd74, 0xc65, 0xf55, 0xe44,
		0x284, 0x395, 0x0a5, 0x1b4, 0xaa6, 0xbb7, 0x887, 0x996,
		0x696, 0x787, 0x4b7, 0x5a6, 0xeb4, 0xfa5, 0xc95, 0xd84,
		0x3c0, 0x2d1, 0x1e1, 0x0f0, 0xbe2, 0xaf3, 0x9c3, 0x8d2,
		0x7d2, 0x6c3, 0x5f3, 0x4e2, 0xff0, 0xee1, 0xdd1, 0xcc0,
		0x888, 0x999, 0xaa9, 0xbb8, 0x0aa, 0x1bb, 0x28b, 0x39a,
		0xc9a, 0xd8b, 0xebb, 0xfaa, 0x4b8, 0x5a9, 0x699, 0x788,
		0x9cc, 0x8dd, 0xbed, 0xafc, 0x1ee, 0x0ff, 0x3cf, 0x2de,
		0xdde, 0xccf, 0xfff, 0xeee, 0x5fc, 0x4ed, 0x7dd, 0x6cc,
		0xa0c, 0xb1d, 0x82d, 0x93c, 0x22e, 0x33f, 0x00f, 0x11e,
		0xe1e, 0xf0f, 0xc3f, 0xd2e, 0x63c, 0x72d, 0x41d, 0x50c,
		0xb48, 0xa59, 0x969, 0x878, 0x36a, 0x27b, 0x14b, 0x05a,
		0xf5a, 0xe4b, 0xd7b, 0xc6a, 0x778, 0x669, 0x559, 0x448,
		0x448, 0x559, 0x669, 0x778, 0xc6a, 0xd7b, 0xe4b, 0xf5a,
		0x05a, 0x14b, 0x27b, 0x36a, 0x878, 0x969, 0xa59, 0xb48,
		0x50c, 0x41d, 0x72d, 0x63c, 0xd2e, 0xc3f, 0xf0f, 0xe1e,
		0x11e, 0x00f, 0x33f, 0x22e, 0x93c, 0x82d, 0xb1d, 0xa0c,
		0x6cc, 0x7dd, 0x4ed, 0x5fc, 0xeee, 0xfff, 0xccf, 0xdde,
		0x2de, 0x3cf, 0x0ff, 0x1ee, 0xafc, 0xbed, 0x8dd, 0x9cc,
		0x788, 0x699, 0x5a9, 0x4b8, 0xfaa, 0xebb, 0xd8b, 0xc9a,
		0x39a, 0x28b, 0x1bb, 0x0aa, 0xbb8, 0xaa9, 0x999, 0x888,
		0xcc0, 0xdd1, 0xee1, 0xff0, 0x4e2, 0x5f3, 0x6c3, 0x7d2,
		0x8d2, 0x9c3, 0xaf3, 0xbe2, 0x0f0, 0x1e1, 0x2d1, 0x3c0,
		0xd84, 0xc95, 0xfa5, 0xeb4, 0x5a6, 0x4b7, 0x787, 0x696,
		0x996, 0x887, 0xbb7, 0xaa6, 0x1b4, 0x0a5, 0x395, 0x284,
		0xe44, 0xf55, 0xc65, 0xd74, 0x666, 0x777, 0x447, 0x556,
		0xa56, 0xb47, 0x877, 0x966, 0x274, 0x365, 0x055, 0x144,
		0xf00, 0xe11, 0xd21, 0xc30, 0x722, 0x633, 0x503, 0x412,
		0xb12, 0xa03, 0x933, 0x822, 0x330, 0x221, 0x111, 0x000,
	};

	//每种角点配置对应的三角形：每3个数字是一个三角形的3条棱，-1结束
	static constexpr std::array<std::array<std::int8_t, 16>, 256> triTable = { {
		{{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  5,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  8,  4,  9,  8,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  4,  5,  1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  1, 11,  0,  1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  8,  4,  1, 11,  8, 11,  9,  8, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4, 10,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0, 10,  1,  8, 10,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  9,  0,  1,  4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5, 10,  1,  5,  9, 10,  9,  8, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4, 11,  5, 10, 11,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0, 11,  5,  0,  8, 11,  8, 10, 11, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  9,  0,  4, 10,  9, 10, 11,  9, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  8, 11, 11,  8, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  6,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  4,  0,  6,  4,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  5,  9,  8,  2,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  5,  9,  2,  6,  5,  6,  4,  5, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 11,  8,  2,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  2,  6,  4,  0,  2,  5,  1, 11, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  1, 11,  9,  0,  1,  8,  2,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1, 11,  9,  1,  9,  6,  1,  6,  4,  6,  9,  2, -1, -1, -1, -1 }},
		{{  8,  2,  6,  4, 10,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 10,  2,  6, 10,  1,  2,  1,  0,  2, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  0,  5,  8,  2,  6,  1,  4, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  6, 10,  9,  2, 10,  9, 10,  1,  9,  1,  5, -1, -1, -1, -1 }},
		{{  4, 11,  5,  4, 10, 11,  6,  8,  2, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5, 10, 11,  5,  2, 10,  5,  0,  2,  6, 10,  2, -1, -1, -1, -1 }},
		{{  2,  6,  8,  9,  0, 10,  9, 10, 11, 10,  0,  4, -1, -1, -1, -1 }},
		{{  2,  6, 10,  2, 10,  9,  9, 10, 11, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  7,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  7,  2,  0,  8,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  7,  2,  5,  7,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  7,  2,  8,  4,  7,  4,  5,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 11,  9,  7,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  0,  8,  5,  1, 11,  2,  9,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7,  1, 11,  7,  2,  1,  2,  0,  1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1, 11,  7,  4,  1,  7,  4,  7,  2,  4,  2,  8, -1, -1, -1, -1 }},
		{{  9,  7,  2,  1,  4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0, 10,  1,  0,  8, 10,  2,  9,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  7,  2,  0,  5,  7,  1,  4, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  5,  7,  1,  7,  8,  1,  8, 10,  2,  8,  7, -1, -1, -1, -1 }},
		{{ 11,  4, 10, 11,  5,  4,  9,  7,  2, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  9,  7,  0,  8,  5,  8, 11,  5,  8, 10, 11, -1, -1, -1, -1 }},
		{{  7,  2,  0,  7,  0, 10,  7, 10, 11, 10,  0,  4, -1, -1, -1, -1 }},
		{{  7,  2,  8,  7,  8, 11, 11,  8, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  6,  8,  7,  6,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  4,  0,  9,  7,  4,  7,  6,  4, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  6,  8,  0,  5,  6,  5,  7,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  7,  4,  4,  7,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  6,  8,  9,  7,  6, 11,  5,  1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  5,  1,  9,  7,  0,  7,  4,  0,  7,  6,  4, -1, -1, -1, -1 }},
		{{  8,  0,  1,  8,  1,  7,  8,  7,  6, 11,  7,  1, -1, -1, -1, -1 }},
		{{  1, 11,  7,  1,  7,  4,  4,  7,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{  6,  9,  7,  6,  8,  9,  4, 10,  1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  7,  6,  9,  6,  1,  9,  1,  0,  1,  6, 10, -1, -1, -1, -1 }},
		{{  1,  4, 10,  0,  5,  8,  5,  6,  8,  5,  7,  6, -1, -1, -1, -1 }},
		{{ 10,  1,  5, 10,  5,  6,  6,  5,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  7,  8,  8,  7,  6, 11,  5,  4, 11,  4, 10, -1, -1, -1, -1 }},
		{{  7,  6,  0,  7,  0,  9,  6, 10,  0,  5,  0, 11, 10, 11,  0, -1 }},
		{{ 10, 11,  0, 10,  0,  4, 11,  7,  0,  8,  0,  6,  7,  6,  0, -1 }},
		{{ 10, 11,  7,  6, 10,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  3,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  4,  7, 11,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  0,  5,  7, 11,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  8,  4,  5,  9,  8,  7, 11,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  3,  7,  1,  3,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  3,  7,  5,  1,  3,  4,  0,  8, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  3,  7,  9,  0,  3,  0,  1,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7,  9,  8,  7,  8,  1,  7,  1,  3,  4,  1,  8, -1, -1, -1, -1 }},
		{{  1,  4, 10, 11,  3,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 10,  0,  8, 10,  1,  0, 11,  3,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  5,  9,  1,  4, 10,  7, 11,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7, 11,  3,  5,  9,  1,  9, 10,  1,  9,  8, 10, -1, -1, -1, -1 }},
		{{  3,  4, 10,  3,  7,  4,  7,  5,  4, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8, 10,  0, 10,  7,  0,  7,  5,  7, 10,  3, -1, -1, -1, -1 }},
		{{  4, 10,  3,  0,  4,  3,  0,  3,  7,  0,  7,  9, -1, -1, -1, -1 }},
		{{  3,  7,  9,  3,  9, 10, 10,  9,  8, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7, 11,  3,  2,  6,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  4,  0,  2,  6,  4,  3,  7, 11, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  9,  0,  7, 11,  3,  8,  2,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  3,  7,  5,  9,  6,  5,  6,  4,  6,  9,  2, -1, -1, -1, -1 }},
		{{  3,  5,  1,  3,  7,  5,  2,  6,  8, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1,  7,  7,  1,  3,  4,  0,  2,  4,  2,  6, -1, -1, -1, -1 }},
		{{  8,  2,  6,  9,  0,  7,  0,  3,  7,  0,  1,  3, -1, -1, -1, -1 }},
		{{  6,  4,  9,  6,  9,  2,  4,  1,  9,  7,  9,  3,  1,  3,  9, -1 }},
		{{  4, 10,  1,  6,  8,  2, 11,  3,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7, 11,  3,  2,  6,  1,  2,  1,  0,  1,  6, 10, -1, -1, -1, -1 }},
		{{  0,  5,  9,  2,  6,  8,  1,  4, 10,  7, 11,  3, -1, -1, -1, -1 }},
		{{  9,  1,  5,  9, 10,  1,  9,  2, 10,  6, 10,  2,  7, 11,  3, -1 }},
		{{  8,  2,  6,  4, 10,  7,  4,  7,  5,  7, 10,  3, -1, -1, -1, -1 }},
		{{  7,  5, 10,  7, 10,  3,  5,  0, 10,  6, 10,  2,  0,  2, 10, -1 }},
		{{  0,  7,  9,  0,  3,  7,  0,  4,  3, 10,  3,  4,  8,  2,  6, -1 }},
		{{  3,  7,  9,  3,  9, 10,  2,  6,  9,  6, 10,  9, -1, -1, -1, -1 }},
		{{ 11,  2,  9,  3,  2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2, 11,  3,  2,  9, 11,  0,  8,  4, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  0,  5, 11,  3,  0,  3,  2,  0, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  4,  5,  8,  5,  3,  8,  3,  2,  3,  5, 11, -1, -1, -1, -1 }},
		{{  5,  2,  9,  5,  1,  2,  1,  3,  2, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  0,  8,  5,  1,  9,  1,  2,  9,  1,  3,  2, -1, -1, -1, -1 }},
		{{  0,  1,  2,  2,  1,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  4,  1,  8,  1,  2,  2,  1,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  2,  9, 11,  3,  2, 10,  1,  4, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  1,  1,  8, 10,  2,  9, 11,  2, 11,  3, -1, -1, -1, -1 }},
		{{  4, 10,  1,  0,  5,  3,  0,  3,  2,  3,  5, 11, -1, -1, -1, -1 }},
		{{  3,  2,  5,  3,  5, 11,  2,  8,  5,  1,  5, 10,  8, 10,  5, -1 }},
		{{  9,  3,  2,  9,  4,  3,  9,  5,  4, 10,  3,  4, -1, -1, -1, -1 }},
		{{  8, 10,  5,  8,  5,  0, 10,  3,  5,  9,  5,  2,  3,  2,  5, -1 }},
		{{  4, 10,  3,  4,  3,  0,  0,  3,  2, -1, -1, -1, -1, -1, -1, -1 }},
		{{  3,  2,  8, 10,  3,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  6, 11,  3,  6,  8, 11,  8,  9, 11, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  6,  4,  0, 11,  6,  0,  9, 11,  3,  6, 11, -1, -1, -1, -1 }},
		{{ 11,  3,  6,  5, 11,  6,  5,  6,  8,  5,  8,  0, -1, -1, -1, -1 }},
		{{ 11,  3,  6, 11,  6,  5,  5,  6,  4, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1,  3,  5,  3,  8,  5,  8,  9,  8,  3,  6, -1, -1, -1, -1 }},
		{{  1,  3,  9,  1,  9,  5,  3,  6,  9,  0,  9,  4,  6,  4,  9, -1 }},
		{{  6,  8,  0,  6,  0,  3,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  6,  4,  1,  3,  6,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  4, 10, 11,  3,  8, 11,  8,  9,  8,  3,  6, -1, -1, -1, -1 }},
		{{  1,  0,  6,  1,  6, 10,  0,  9,  6,  3,  6, 11,  9, 11,  6, -1 }},
		{{  5,  8,  0,  5,  6,  8,  5, 11,  6,  3,  6, 11,  1,  4, 10, -1 }},
		{{ 10,  1,  5, 10,  5,  6, 11,  3,  5,  3,  6,  5, -1, -1, -1, -1 }},
		{{  8,  9,  3,  8,  3,  6,  9,  5,  3, 10,  3,  4,  5,  4,  3, -1 }},
		{{  0,  9,  5, 10,  3,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  6,  8,  0,  6,  0,  3,  4, 10,  0, 10,  3,  0, -1, -1, -1, -1 }},
		{{  6, 10,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  6,  3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  0,  8, 10,  6,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  5,  9, 10,  6,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  5,  9,  8,  4,  5, 10,  6,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  5,  1,  3, 10,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 11,  4,  0,  8,  3, 10,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  9,  0,  1, 11,  9,  3, 10,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{  3, 10,  6,  1, 11,  4, 11,  8,  4, 11,  9,  8, -1, -1, -1, -1 }},
		{{  6,  1,  4,  3,  1,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  6,  0,  8,  6,  3,  0,  3,  1,  0, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  6,  3,  1,  4,  6,  0,  5,  9, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  3,  1,  5,  8,  3,  5,  9,  8,  8,  6,  3, -1, -1, -1, -1 }},
		{{ 11,  6,  3, 11,  5,  6,  5,  4,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  6,  3,  5,  6, 11,  5,  8,  6,  5,  0,  8, -1, -1, -1, -1 }},
		{{  0,  4,  6,  0,  6, 11,  0, 11,  9,  3, 11,  6, -1, -1, -1, -1 }},
		{{  6,  3, 11,  6, 11,  8,  8, 11,  9, -1, -1, -1, -1, -1, -1, -1 }},
		{{  3,  8,  2, 10,  8,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  3, 10,  4,  0,  3,  0,  2,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  3, 10,  8,  2,  3,  9,  0,  5, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  2,  3,  9,  3,  4,  9,  4,  5, 10,  4,  3, -1, -1, -1, -1 }},
		{{  3,  8,  2,  3, 10,  8,  1, 11,  5, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 11,  4,  0, 10,  0,  3, 10,  0,  2,  3, -1, -1, -1, -1 }},
		{{  2, 10,  8,  2,  3, 10,  0,  1,  9,  1, 11,  9, -1, -1, -1, -1 }},
		{{ 11,  9,  4, 11,  4,  1,  9,  2,  4, 10,  4,  3,  2,  3,  4, -1 }},
		{{  8,  1,  4,  8,  2,  1,  2,  3,  1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  2,  1,  2,  3,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  9,  0,  1,  4,  2,  1,  2,  3,  2,  4,  8, -1, -1, -1, -1 }},
		{{  5,  9,  2,  5,  2,  1,  1,  2,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  5,  4,  8,  3,  5,  8,  2,  3,  3, 11,  5, -1, -1, -1, -1 }},
		{{ 11,  5,  0, 11,  0,  3,  3,  0,  2, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  3,  4,  2,  4,  8,  3, 11,  4,  0,  4,  9, 11,  9,  4, -1 }},
		{{ 11,  9,  2,  3, 11,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  9,  7,  6,  3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  4,  2,  9,  7, 10,  6,  3, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7,  0,  5,  7,  2,  0,  6,  3, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 10,  6,  3,  8,  4,  2,  4,  7,  2,  4,  5,  7, -1, -1, -1, -1 }},
		{{  9,  7,  2, 11,  5,  1,  6,  3, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  3, 10,  6,  5,  1, 11,  0,  8,  4,  2,  9,  7, -1, -1, -1, -1 }},
		{{  6,  3, 10,  7,  2, 11,  2,  1, 11,  2,  0,  1, -1, -1, -1, -1 }},
		{{  4,  2,  8,  4,  7,  2,  4,  1,  7, 11,  7,  1, 10,  6,  3, -1 }},
		{{  6,  1,  4,  6,  3,  1,  7,  2,  9, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  7,  2,  0,  8,  3,  0,  3,  1,  3,  8,  6, -1, -1, -1, -1 }},
		{{  4,  3,  1,  4,  6,  3,  5,  7,  0,  7,  2,  0, -1, -1, -1, -1 }},
		{{  3,  1,  8,  3,  8,  6,  1,  5,  8,  2,  8,  7,  5,  7,  8, -1 }},
		{{  9,  7,  2, 11,  5,  3,  5,  6,  3,  5,  4,  6, -1, -1, -1, -1 }},
		{{  5,  3, 11,  5,  6,  3,  5,  0,  6,  8,  6,  0,  9,  7,  2, -1 }},
		{{  2,  0, 11,  2, 11,  7,  0,  4, 11,  3, 11,  6,  4,  6, 11, -1 }},
		{{  6,  3, 11,  6, 11,  8,  7,  2, 11,  2,  8, 11, -1, -1, -1, -1 }},
		{{  3,  9,  7,  3, 10,  9, 10,  8,  9, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  3, 10,  0,  3,  4,  0,  7,  3,  0,  9,  7, -1, -1, -1, -1 }},
		{{  0, 10,  8,  0,  7, 10,  0,  5,  7,  7,  3, 10, -1, -1, -1, -1 }},
		{{  3, 10,  4,  3,  4,  7,  7,  4,  5, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 11,  9,  7, 10,  9, 10,  8, 10,  7,  3, -1, -1, -1, -1 }},
		{{  0, 10,  4,  0,  3, 10,  0,  9,  3,  7,  3,  9,  5,  1, 11, -1 }},
		{{ 10,  8,  7, 10,  7,  3,  8,  0,  7, 11,  7,  1,  0,  1,  7, -1 }},
		{{  3, 10,  4,  3,  4,  7,  1, 11,  4, 11,  7,  4, -1, -1, -1, -1 }},
		{{  7,  8,  9,  7,  1,  8,  7,  3,  1,  4,  8,  1, -1, -1, -1, -1 }},
		{{  9,  7,  3,  9,  3,  0,  0,  3,  1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  7,  8,  5,  8,  0,  7,  3,  8,  4,  8,  1,  3,  1,  8, -1 }},
		{{  5,  7,  3,  1,  5,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  4,  3,  5,  3, 11,  4,  8,  3,  7,  3,  9,  8,  9,  3, -1 }},
		{{ 11,  5,  0, 11,  0,  3,  9,  7,  0,  7,  3,  0, -1, -1, -1, -1 }},
		{{  0,  4,  8,  7,  3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  7,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 10,  7, 11,  6,  7, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 10,  7, 11, 10,  6,  7,  8,  4,  0, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7, 10,  6,  7, 11, 10,  5,  9,  0, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 11,  6,  7, 11, 10,  6,  9,  8,  5,  8,  4,  5, -1, -1, -1, -1 }},
		{{ 10,  5,  1, 10,  6,  5,  6,  7,  5, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  4,  5,  1,  6,  5,  6,  7,  6,  1, 10, -1, -1, -1, -1 }},
		{{  9,  6,  7,  9,  1,  6,  9,  0,  1,  1, 10,  6, -1, -1, -1, -1 }},
		{{  6,  7,  1,  6,  1, 10,  7,  9,  1,  4,  1,  8,  9,  8,  1, -1 }},
		{{  1,  7, 11,  1,  4,  7,  4,  6,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  1,  0,  8,  7,  1,  8,  6,  7, 11,  1,  7, -1, -1, -1, -1 }},
		{{  9,  0,  5,  7, 11,  4,  7,  4,  6,  4, 11,  1, -1, -1, -1, -1 }},
		{{  9,  8,  1,  9,  1,  5,  8,  6,  1, 11,  1,  7,  6,  7,  1, -1 }},
		{{  5,  4,  7,  4,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  6,  0,  6,  5,  5,  6,  7, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  0,  4,  9,  4,  7,  7,  4,  6, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9,  8,  6,  7,  9,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7,  8,  2,  7, 11,  8, 11, 10,  8, -1, -1, -1, -1, -1, -1, -1 }},
		{{  7,  0,  2,  7, 10,  0,  7, 11, 10, 10,  4,  0, -1, -1, -1, -1 }},
		{{  0,  5,  9,  8,  2, 11,  8, 11, 10, 11,  2,  7, -1, -1, -1, -1 }},
		{{ 11, 10,  2, 11,  2,  7, 10,  4,  2,  9,  2,  5,  4,  5,  2, -1 }},
		{{  1,  7,  5,  1,  8,  7,  1, 10,  8,  2,  7,  8, -1, -1, -1, -1 }},
		{{  0,  2, 10,  0, 10,  4,  2,  7, 10,  1, 10,  5,  7,  5, 10, -1 }},
		{{  0,  1,  7,  0,  7,  9,  1, 10,  7,  2,  7,  8, 10,  8,  7, -1 }},
		{{  9,  2,  7,  1, 10,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  7, 11,  4,  7,  1,  4,  2,  7,  4,  8,  2, -1, -1, -1, -1 }},
		{{  7, 11,  1,  7,  1,  2,  2,  1,  0, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4, 11,  1,  4,  7, 11,  4,  8,  7,  2,  7,  8,  0,  5,  9, -1 }},
		{{  7, 11,  1,  7,  1,  2,  5,  9,  1,  9,  2,  1, -1, -1, -1, -1 }},
		{{  8,  2,  7,  8,  7,  4,  4,  7,  5, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  2,  7,  5,  0,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  8,  2,  7,  8,  7,  4,  9,  0,  7,  0,  4,  7, -1, -1, -1, -1 }},
		{{  9,  2,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2, 10,  6,  2,  9, 10,  9, 11, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  8,  4,  2,  9,  6,  9, 10,  6,  9, 11, 10, -1, -1, -1, -1 }},
		{{  5, 11, 10,  5, 10,  2,  5,  2,  0,  6,  2, 10, -1, -1, -1, -1 }},
		{{  4,  5,  2,  4,  2,  8,  5, 11,  2,  6,  2, 10, 11, 10,  2, -1 }},
		{{  2, 10,  6,  9, 10,  2,  9,  1, 10,  9,  5,  1, -1, -1, -1, -1 }},
		{{  9,  6,  2,  9, 10,  6,  9,  5, 10,  1, 10,  5,  0,  8,  4, -1 }},
		{{ 10,  6,  2, 10,  2,  1,  1,  2,  0, -1, -1, -1, -1, -1, -1, -1 }},
		{{ 10,  6,  2, 10,  2,  1,  8,  4,  2,  4,  1,  2, -1, -1, -1, -1 }},
		{{  1,  9, 11,  1,  6,  9,  1,  4,  6,  6,  2,  9, -1, -1, -1, -1 }},
		{{  9, 11,  6,  9,  6,  2, 11,  1,  6,  8,  6,  0,  1,  0,  6, -1 }},
		{{  4,  6, 11,  4, 11,  1,  6,  2, 11,  5, 11,  0,  2,  0, 11, -1 }},
		{{  5, 11,  1,  8,  6,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  9,  5,  2,  5,  6,  6,  5,  4, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  9,  5,  2,  5,  6,  0,  8,  5,  8,  6,  5, -1, -1, -1, -1 }},
		{{  2,  0,  4,  6,  2,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  2,  8,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9, 11,  8, 11, 10,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  0,  9,  4,  9, 10, 10,  9, 11, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  5, 11,  0, 11,  8,  8, 11, 10, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  5, 11, 10,  4, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  1, 10,  5, 10,  9,  9, 10,  8, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  0,  9,  4,  9, 10,  5,  1,  9,  1, 10,  9, -1, -1, -1, -1 }},
		{{  0,  1, 10,  8,  0, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  4,  1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  4,  8,  1,  8, 11, 11,  8,  9, -1, -1, -1, -1, -1, -1, -1 }},
		{{  9, 11,  1,  0,  9,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  1,  4,  8,  1,  8, 11,  0,  5,  8,  5, 11,  8, -1, -1, -1, -1 }},
		{{  5, 11,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  5,  4,  8,  9,  5,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  9,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{  0,  4,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
		{{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }},
	} };

	//角点偏移，顺序与上面的角点编号一致
	constexpr std::array<glm::ivec3, 8> CORNER_OFFSET = { {
		{0,0,0},{1,0,0},{1,1,0},{0,1,0},
		{0,0,1},{1,0,1},{1,1,1},{0,1,1}
	} };

	//棱 -> 两端角点编号 + 该棱所在的轴
	//两端角点在棱方向上的坐标分别是0和1，所以第0个端点是"低"的那一端
	constexpr std::array<std::array<int, 3>, 12> EDGE_CORNERS = { {
		{{0,1,0}},{{3,2,0}},{{4,5,0}},{{7,6,0}},
		{{0,3,1}},{{1,2,1}},{{4,7,1}},{{5,6,1}},
		{{0,4,2}},{{1,5,2}},{{3,7,2}},{{2,6,2}}
	} };

	//请实现这个函数，使用Marching Cubes算法从有符号密度场sdf中构建网格
	//MC算法所需要的静态表可以写在命名空间中在函数上面
	//注意传入的密度场使用体素中心点采样法
	//输出MeshData结构体,其定义如下：
	//struct MeshData {
	//	std::vector<float> vertices;//三维顶点数据
	//	std::vector<float> uvws; //注意这是三维纹理数组坐标，第三个数据可以暂时设为0
	//	std::vector<float> aos;  //这个可以暂时全部填充为1.0f
	//	std::vector<float> normals;//顶点法线
	//  vertices/uvws/normals数组大小应该相等,aos大小为前三个数组的1/3
	//	std::vector<int> indices;
	//};
	//neighbours顺序为：左、右、上、下、前、后
	core::MeshData BuildMeshFromChunk(
		const voxel::chunk::Chunk<float>& chunk,
		const std::array<voxel::chunk::Chunk<float>*, 6> neighbours,
		int scale) {

		//实现思路：
		//把每个体素当作一个立方体单元的"最小角点"：单元(cx,cy,cz)的8个角点取体素(cx+dx,cy+dy,cz+dz)，
		//dx/dy/dz属于{0,1}。遍历cx/cy/cz在0..CHUNK_SIZE-1的单元，这样每个单元只由一个区块生成，
		//不会重复出面片；跨越区块边界的单元需要x/y/z等于CHUNK_SIZE那一层角点，
		//这层从对应的面邻居取，所以先铺一张(CHUNK_SIZE+1)^3的采样表。
		//密度约定：< 0为实心(内部)，>= 0为空气(外部)，也就是SDF的常规约定，
		//因此角点的符号位为1表示该角点在内部，查找表按这个约定使用。
		//邻居缺失时那一层角点的密度无从得知，按空气处理：于是实心体会在区块边界收口，
		//既不会漏出破洞，也不会凭空多出一层朝里的假面；
		//等邻居加载进来重新构建时，这层收口自然消失，和邻居的面拼上。

		constexpr int SIZE = static_cast<int>(nova_engine::core::config::CHUNK_SIZE);//32
		constexpr int SIDE = SIZE + 1;//33，采样网格每边的角点数
		constexpr int VOLUME = SIZE * SIZE * SIZE;//32768
		const std::vector<float>& voxels = chunk.GetVoxelSpan();
		if (voxels.size() < static_cast<size_t>(VOLUME)) return core::MeshData{};

		//采样表：下标 = x * SIDE * SIDE + y * SIDE + z，x/y/z在0..CHUNK_SIZE
		//known为0表示这个角点取不到(面邻居缺失或方向上有两个轴同时越界)
		struct Sample {
			float density = 1.0f;
			std::uint8_t known = 0;
		};
		std::array<Sample, SIDE * SIDE * SIDE> samples{};
		std::array<const float*, 6> neighbourData{};
		for (int dir = 0; dir < 6; dir++) {
			const voxel::chunk::Chunk<float>* neighbour = neighbours[dir];
			neighbourData[dir] = (neighbour && neighbour->GetVoxelSpan().size() >= static_cast<size_t>(VOLUME))
				? neighbour->GetVoxelSpan().data() : nullptr;
		}

		for (int x = 0; x <= SIZE; x++) {
			for (int y = 0; y <= SIZE; y++) {
				for (int z = 0; z <= SIZE; z++) {
					if (x < SIZE && y < SIZE && z < SIZE) {
						//区块自己的体素，索引与区块自身的布局一致：x * SIZE * SIZE + y * SIZE + z
						Sample& sample = samples[(x * SIDE + y) * SIDE + z];
						sample.density = voxels[(x * SIZE + y) * SIZE + z];
						sample.known = 1;
						continue;
					}

					//x/y/z等于CHUNK_SIZE的那一层要向邻居借：
					//把越界的轴折回边界，再从那个方向的邻居里取它对应的那一层体素
					int axis = -1;
					int dir = 0;
					int localX = x, localY = y, localZ = z;
					if (x >= SIZE) { axis = 0; dir = 1; localX = 0; }
					if (y >= SIZE) { if (axis >= 0) continue; axis = 1; dir = 3; localY = 0; }
					if (z >= SIZE) { if (axis >= 0) continue; axis = 2; dir = 5; localZ = 0; }

					//有两轴以上越界时(边界棱/角点)没有对应邻居，留作"取不到"
					const float* neighbour = neighbourData[dir];
					if (!neighbour) continue;

					Sample& sample = samples[(x * SIDE + y) * SIDE + z];
					sample.density = neighbour[(localX * SIZE + localY) * SIZE + localZ];
					sample.known = 1;
				}
			}
		}

		//角点法线：密度场的中心差分，指向密度增大的方向，也就是从实心指向空气的外法线
		//采样表外圈的角点缺少一侧的邻居，退化成单侧差分；两侧都取不到时保持零向量
		constexpr int STRIDE_XY = SIDE * SIDE;
		std::array<glm::vec3, SIDE * SIDE * SIDE> normals{};
		for (int x = 0; x <= SIZE; x++) {
			for (int y = 0; y <= SIZE; y++) {
				for (int z = 0; z <= SIZE; z++) {
					constexpr std::array<int, 3> step = { STRIDE_XY, SIDE, 1 };
					//各个轴上的采样点个数，用来判断某一侧是否还在采样表里
					const std::array<int, 3> coordinate = { x, y, z };
					const std::array<int, 3> limit = { SIZE, SIZE, SIZE };

					const int index = (x * SIDE + y) * SIDE + z;
					const Sample& centre = samples[index];

					glm::vec3 gradient(0.0f);
					for (int axis = 0; axis < 3; axis++) {
						const bool hasForward = coordinate[axis] + 1 <= limit[axis];
						const bool hasBackward = coordinate[axis] - 1 >= 0;

						const Sample* forward = hasForward ? &samples[index + step[axis]] : nullptr;
						const Sample* backward = hasBackward ? &samples[index - step[axis]] : nullptr;

						if (forward && backward && forward->known && backward->known)
							gradient[axis] = (forward->density - backward->density) * 0.5f;
						else if (forward && centre.known && forward->known)
							gradient[axis] = forward->density - centre.density;
						else if (backward && centre.known && backward->known)
							gradient[axis] = centre.density - backward->density;
					}

					const float length = glm::length(gradient);
					normals[index] = (length > 1e-6f) ? gradient / length : glm::vec3(0.0f);
				}
			}
		}

		core::MeshData mesh;
		const float voxelScale = static_cast<float>(scale);
		const float inverseExtent = 1.0f / (static_cast<float>(SIZE) * voxelScale);

		//每个单元里8个角点的位置/法线/密度，切换单元时复用同一份空间
		glm::vec3 cornerPosition[8];
		glm::vec3 cornerNormal[8];
		float cornerDensity[8];
		bool cornerUnknown[8];//角点的密度不是真实采样(邻居缺失/取不到)时置位
		int edgeVertex[12];

		//遍历区块拥有的所有单元
		for (int cx = 0; cx < SIZE; cx++) {
			for (int cy = 0; cy < SIZE; cy++) {
				for (int cz = 0; cz < SIZE; cz++) {

					//按8个角点的符号组成配置索引
					int cubeIndex = 0;
					for (int corner = 0; corner < 8; corner++) {
						const int sx = cx + CORNER_OFFSET[corner].x;
						const int sy = cy + CORNER_OFFSET[corner].y;
						const int sz = cz + CORNER_OFFSET[corner].z;
						const int index = (sx * SIDE + sy) * SIDE + sz;

						//取不到的角点按空气处理(正数)，实心体会在区块边界收口
						const Sample& sample = samples[index];
						cornerUnknown[corner] = (sample.known == 0);
						cornerDensity[corner] = sample.known ? sample.density : 1.0f;
						if (cornerDensity[corner] < 0.0f) cubeIndex |= 1 << corner;

						cornerPosition[corner] = glm::vec3(
							static_cast<float>(sx),
							static_cast<float>(sy),
							static_cast<float>(sz)) * voxelScale;

						cornerNormal[corner] = normals[index];
					}

					const std::uint16_t crossedEdges = edgeTable[cubeIndex];
					if (crossedEdges == 0) continue;//单元完全在内部或完全在外部

					//计算被切割的棱上的交点：位置和法线都用同一组角点插值
					for (int edge = 0; edge < 12; edge++) {
						if (((crossedEdges >> edge) & 1) == 0) { edgeVertex[edge] = -1; continue; }

						const int a = EDGE_CORNERS[edge][0];
						const int b = EDGE_CORNERS[edge][1];
						const int axis = EDGE_CORNERS[edge][2];

						//等值面取0，交点在 a->b 上的比例是 (0 - da) / (db - da)
						float t = cornerDensity[a] / (cornerDensity[a] - cornerDensity[b]);
						if (!std::isfinite(t)) t = 0.5f;
						t = glm::clamp(t, 0.0f, 1.0f);

						//邻居缺失时那一端的密度只是"按空气估计"的，用它插值会把交点拉进区块内部；
						//这里直接把交点钉在缺失角点所在的区块边界上，于是各个面的收口都正好落在边界平面上，
						//相邻区块的收口可以严丝合缝地接起来
						if (cornerUnknown[b]) t = 1.0f;
						if (cornerUnknown[a]) t = 0.0f;

						const glm::vec3 vertexPosition = glm::mix(cornerPosition[a], cornerPosition[b], t);
						glm::vec3 vertexNormal = glm::mix(cornerNormal[a], cornerNormal[b], t);
						const float length = glm::length(vertexNormal);
						if (length > 1e-6f) vertexNormal /= length;
						else {
							//退化时用棱方向兜底：从实心指向空气
							vertexNormal = glm::vec3(0.0f);
							vertexNormal[axis] = (cornerDensity[b] < cornerDensity[a]) ? 1.0f : -1.0f;
						}

						edgeVertex[edge] = static_cast<int>(mesh.vertices.size() / 3);

						mesh.vertices.push_back(vertexPosition.x);
						mesh.vertices.push_back(vertexPosition.y);
						mesh.vertices.push_back(vertexPosition.z);

						//三维纹理数组坐标：暂时用区块内的归一化坐标，第三维为0
						mesh.uvws.push_back(vertexPosition.x);
						mesh.uvws.push_back(vertexPosition.y);
						mesh.uvws.push_back(1.0f);

						//环境光遮蔽暂时全部填1
						mesh.aos.push_back(1.0f);

						mesh.normals.push_back(vertexNormal.x);
						mesh.normals.push_back(vertexNormal.y);
						mesh.normals.push_back(vertexNormal.z);
					}

					const std::array<std::int8_t, 16>& triangles = triTable[cubeIndex];
					//表格给出的环绕方向与这里的角点符号约定(负数为实心)相反，写出索引时反转一下，
					//让三角形的正面朝向区块外部，与渲染器的正面/背面剔除一致。
					//密度场的零点正好落在采样点上时(球心在体素中心、半径取整数就会出现)，
					//同一条棱的插值参数会退化成0或1，两条不同的棱可能算出同一个交点，
					//于是出现两个顶点重合的零面积三角形；这种三角形画不出任何东西，直接丢掉
					const auto distinctPositions = [&mesh](int a, int b) {
						const float dx = mesh.vertices[a * 3] - mesh.vertices[b * 3];
						const float dy = mesh.vertices[a * 3 + 1] - mesh.vertices[b * 3 + 1];
						const float dz = mesh.vertices[a * 3 + 2] - mesh.vertices[b * 3 + 2];
						return (dx * dx + dy * dy + dz * dz) > 1e-12f;
					};

					for (int i = 0; triangles[i] >= 0; i += 3) {
						const int first = edgeVertex[triangles[i]];
						const int second = edgeVertex[triangles[i + 1]];
						const int third = edgeVertex[triangles[i + 2]];
						if (!distinctPositions(first, second) ||
							!distinctPositions(second, third) ||
							!distinctPositions(first, third))
							continue;

						mesh.indices.push_back(third);
						mesh.indices.push_back(second);
						mesh.indices.push_back(first);
					}
				}
			}
		}

		return mesh;
	}

	core::MeshData BuildSNMeshFromChunk(
		const voxel::chunk::Chunk<float>& chunk,
		const std::array<voxel::chunk::Chunk<float>*, 7>& neighbours, int scale) {
		constexpr int CHUNK = (int)nova_engine::core::config::CHUNK_SIZE; // 32
		constexpr int SIZE = CHUNK + 1;  // 33 单元格数
		constexpr int SIDE = SIZE + 1;   // 34
		constexpr int SN = SIDE + 1;   // 35 采样点数 0..34

		static voxel::chunk::Chunk<float> emptyChunk(CHUNK);
		static std::once_flag flag;
		std::call_once(flag, []() {
			for (int x = 0; x < CHUNK; x++)
				for (int y = 0; y < CHUNK; y++)
					for (int z = 0; z < CHUNK; z++)
						emptyChunk.SetVoxel(x, y, z, nova_engine::utils::assist::PackAir());
			});

		std::array<const voxel::chunk::Chunk<float>*, 8> chunks{ &chunk };
		for (int i = 1; i < 8; i++)
			chunks[i] = neighbours[i - 1] ? neighbours[i - 1] : &emptyChunk;

		auto sampleData = [&](int sx, int sy, int sz) -> float {
			if (sx < 0) sx = 0;
			if (sy < 0) sy = 0;
			if (sz < 0) sz = 0;
			int idx = 0, lx = sx, ly = sy, lz = sz;
			if (lx >= CHUNK) { idx |= 1; lx -= CHUNK; }
			if (ly >= CHUNK) { idx |= 2; ly -= CHUNK; }
			if (lz >= CHUNK) { idx |= 4; lz -= CHUNK; }
			return chunks[idx]->GetVoxel(lx, ly, lz);
			};

		// 采样表 stride = SN = 35，覆盖 0..34
		auto sidx = [](int x, int y, int z) -> size_t {
			return (size_t(x) * SN + y) * SN + z;
			};

		// 修复 UB：resize 而非 reserve
		std::vector<float> density(SN * SN * SN);
		std::vector<float> tex(SN * SN * SN);

		bool inside0 = nova_engine::utils::assist::UnpackDensity(sampleData(0, 0, 0)) < 0.0f;
		bool hasNoMesh = true;
		for (int x = 0; x < SN; x++)
			for (int y = 0; y < SN; y++)
				for (int z = 0; z < SN; z++) {
					float raw = sampleData(x, y, z);
					float d = nova_engine::utils::assist::UnpackDensity(raw);
					density[sidx(x, y, z)] = d;
					tex[sidx(x, y, z)] = nova_engine::utils::assist::UnpackTexIdx(raw);
					if (inside0 != (d < 0.0f)) hasNoMesh = false;
				}
		if (hasNoMesh) return {};

		// 预计算前向差分法线表：与原点处 round(p) 的取值域一致，索引 0..33
		// 与原函数完全一致：g = d(x+1)-d(x)，不做中心差分
		constexpr int GRAD_N = SIZE; // 34 个点 0..33
		thread_local std::unique_ptr<glm::vec3[]> gradTable;
		if (!gradTable) gradTable = std::make_unique<glm::vec3[]>(GRAD_N * GRAD_N * GRAD_N);
		auto gidx = [](int x, int y, int z) -> size_t {
			return (size_t(x) * GRAD_N + y) * GRAD_N + z;
			};
		for (int x = 0; x < GRAD_N; x++)
			for (int y = 0; y < GRAD_N; y++)
				for (int z = 0; z < GRAD_N; z++) {
					glm::vec3 g(
						density[sidx(x + 1, y, z)] - density[sidx(x, y, z)],
						density[sidx(x, y + 1, z)] - density[sidx(x, y, z)],
						density[sidx(x, y, z + 1)] - density[sidx(x, y, z)]);
					float len = glm::length(g);
					gradTable[gidx(x, y, z)] = (len >= 1e-6f) ? g / len : glm::vec3(0.0f);
				}

		constexpr size_t EDGE_PER_DIR = size_t(SIZE) * SIDE * SIDE; // 33*34*34
		constexpr size_t EDGE_TOTAL = EDGE_PER_DIR * 3;
		constexpr size_t edgeDirX = 0;
		constexpr size_t edgeDirY = EDGE_PER_DIR;
		constexpr size_t edgeDirZ = 2 * EDGE_PER_DIR;

		thread_local std::unique_ptr<glm::vec3[]> edgeVer;
		thread_local std::unique_ptr<glm::vec3[]> edgeNor;
		thread_local std::unique_ptr<float[]>     edgeTex;
		thread_local std::unique_ptr<uint8_t[]>   edgeOri;
		if (!edgeVer) {
			edgeVer = std::make_unique<glm::vec3[]>(EDGE_TOTAL);
			edgeNor = std::make_unique<glm::vec3[]>(EDGE_TOTAL);
			edgeTex = std::make_unique<float[]>(EDGE_TOTAL);
			edgeOri = std::make_unique<uint8_t[]>(EDGE_TOTAL);
		}

		// 按原函数的三方向遍历顺序（X、Y、Z），保持缓存块布局一致
		auto fillEdges = [&](int axis) {
			size_t base = (axis == 0) ? edgeDirX : (axis == 1) ? edgeDirY : edgeDirZ;
			int lx = (axis == 0) ? SIZE : SIDE;
			int ly = (axis == 1) ? SIZE : SIDE;
			int lz = (axis == 2) ? SIZE : SIDE;
			for (int x = 0; x < lx; x++)
				for (int y = 0; y < ly; y++)
					for (int z = 0; z < lz; z++) {
						size_t i;
						if (axis == 0)      i = base + size_t(x) * SIDE * SIDE + size_t(y) * SIDE + z;
						else if (axis == 1) i = base + size_t(x) * SIZE * SIDE + size_t(y) * SIDE + z;
						else                i = base + size_t(x) * SIDE * SIZE + size_t(y) * SIZE + z;

						int nx = x, ny = y, nz = z;
						if (axis == 0) nx++; else if (axis == 1) ny++; else nz++;

						float d1 = density[sidx(x, y, z)];
						float d2 = density[sidx(nx, ny, nz)];
						if ((d1 < 0.0f) == (d2 < 0.0f)) {
							edgeVer[i] = glm::vec3(-1.0f);
							edgeNor[i] = glm::vec3(0.0f);
							edgeTex[i] = 0.0f;
							edgeOri[i] = 0;
							continue;
						}

						glm::vec3 v1(x, y, z), v2(nx, ny, nz);
						float t = d1 / (d1 - d2);
						glm::vec3 p = glm::mix(v1, v2, t);

						// 与原函数一致：取交点最近整数采样点的前向差分法线
						int px = (int)std::round(p.x);
						int py = (int)std::round(p.y);
						int pz = (int)std::round(p.z);
						px = std::clamp(px, 0, GRAD_N - 1);
						py = std::clamp(py, 0, GRAD_N - 1);
						pz = std::clamp(pz, 0, GRAD_N - 1);

						glm::vec3 n = gradTable[gidx(px, py, pz)];
						if (n == glm::vec3(0.0f)) {
							// 原函数兜底：从交点到空气端点，不 normalize
							n = ((d1 >= 0.0f && d2 < 0.0f) ? v1 : v2) - p;
						}

						edgeVer[i] = p;
						edgeNor[i] = n;
						// 始终使用内部实心方块的材质
						edgeTex[i] = (d1 < 0.0f) ? tex[sidx(x, y, z)] : tex[sidx(nx, ny, nz)];
						edgeOri[i] = (d1 < 0.0f) ? 1 : 2;
					}
			};
		fillEdges(0); // X
		fillEdges(1); // Y
		fillEdges(2); // Z

		thread_local std::unique_ptr<glm::vec3[]> voxelVer;
		thread_local std::unique_ptr<glm::vec3[]> voxelNor;
		if (!voxelVer) {
			voxelVer = std::make_unique<glm::vec3[]>(size_t(SIZE) * SIZE * SIZE);
			voxelNor = std::make_unique<glm::vec3[]>(size_t(SIZE) * SIZE * SIZE);
		}

		constexpr size_t eidxs[12] = {
			edgeDirZ, edgeDirZ + SIZE,
			edgeDirZ + SIDE * SIZE, edgeDirZ + SIDE * SIZE + SIZE,
			edgeDirY, edgeDirY + 1,
			edgeDirY + SIDE * SIZE, edgeDirY + SIDE * SIZE + 1,
			edgeDirX, edgeDirX + 1,
			edgeDirX + SIDE, edgeDirX + SIDE + 1
		};

		for (size_t i = 0; i < size_t(SIZE) * SIZE * SIZE; i++) {
			size_t x = i / (SIZE * SIZE);
			size_t y = (i % (SIZE * SIZE)) / SIZE;
			size_t z = i % SIZE;

			glm::vec3 vsum(0.0f), nsum(0.0f);
			int cnt = 0;
			for (int j = 0; j < 12; j++) {
				size_t eidx;
				if (j < 4)      eidx = eidxs[j] + x * SIDE * SIZE + y * SIZE + z;
				else if (j < 8) eidx = eidxs[j] + x * SIZE * SIDE + y * SIDE + z;
				else            eidx = eidxs[j] + x * SIDE * SIDE + y * SIDE + z;

				if (edgeVer[eidx].x == -1.0f) continue;
				vsum += edgeVer[eidx];
				nsum += edgeNor[eidx];
				cnt++;
			}
			if (cnt > 0) {
				voxelVer[i] = vsum / float(cnt);
				float nl = glm::length(nsum);
				voxelNor[i] = (nl > 1e-6f) ? nsum / nl : glm::vec3(0.0f);
			}
			else {
				voxelVer[i] = glm::vec3(-1.0f);
				voxelNor[i] = glm::vec3(0.0f);
			}
		}

		core::MeshData data;
		constexpr int subSize = SIZE - 1; // 32
		constexpr int meshBuildTable[3][4][3] = {
			{ { 0,0,0 },{ 1,0,0 },{ 1,1,0 },{ 0,1,0 } },
			{ { 0,0,0 },{ 1,0,0 },{ 1,0,1 },{ 0,0,1 } },
			{ { 0,0,0 },{ 0,1,0 },{ 0,1,1 },{ 0,0,1 } }
		};

		for (int i = 0; i < subSize * subSize * subSize; i++) {
			int x = i / (subSize * subSize);
			int y = (i % (subSize * subSize)) / subSize;
			int z = i % subSize;

			for (int dir = 0; dir < 3; dir++) {
				size_t eIdx;
				if (dir == 0)      eIdx = edgeDirZ + SIDE * SIZE + SIZE + x * SIDE * SIZE + y * SIZE + z;
				else if (dir == 1) eIdx = edgeDirY + SIDE * SIZE + 1 + x * SIZE * SIDE + y * SIDE + z;
				else               eIdx = edgeDirX + SIDE + 1 + x * SIDE * SIDE + y * SIDE + z;

				if (edgeVer[eIdx].x == -1.0f) continue;

				glm::vec3 qv[4], qn[4];
				bool ok = true;
				for (int off = 0; off < 4; off++) {
					int fx = x + meshBuildTable[dir][off][0];
					int fy = y + meshBuildTable[dir][off][1];
					int fz = z + meshBuildTable[dir][off][2];
					if (fx >= SIZE || fy >= SIZE || fz >= SIZE) { ok = false; break; }
					size_t vi = (size_t)fx * SIZE * SIZE + (size_t)fy * SIZE + fz;
					if (voxelVer[vi].x == -1.0f) { ok = false; break; }
					qv[off] = voxelVer[vi];
					qn[off] = voxelNor[vi];
				}
				if (!ok) continue;

				uint32_t base = (uint32_t)(data.vertices.size() / 3);
				for (int ii = 0; ii < 4; ii++) {
					data.vertices.push_back(qv[ii].x * scale);
					data.vertices.push_back(qv[ii].y * scale);
					data.vertices.push_back(qv[ii].z * scale);
					data.uvws.push_back(meshBuildTable[0][ii][0]);
					data.uvws.push_back(meshBuildTable[0][ii][1]);
					data.uvws.push_back(edgeTex[eIdx]);
					data.normals.push_back(qn[ii].x);
					data.normals.push_back(qn[ii].y);
					data.normals.push_back(qn[ii].z);
					data.aos.push_back(1.0f);
				}

				static constexpr int baseIndices[6] = { 0,1,2,0,2,3 };
				bool reverse = (dir == 0 && edgeOri[eIdx] == 2) ||
					(dir == 1 && edgeOri[eIdx] == 1) ||
					(dir == 2 && edgeOri[eIdx] == 2);
				if (reverse) for (int ii = 5; ii >= 0; ii--) data.indices.push_back(baseIndices[ii] + base);
				else         for (int ii = 0; ii < 6; ii++)  data.indices.push_back(baseIndices[ii] + base);
			}
		}
		return data;
	}

	core::MeshData BuildSNLodMesh(
		const voxel::lod::LodVoxelSDFData& lodData, int scale) {

		constexpr int N = (int)nova_engine::core::config::CHUNK_SIZE + 2; // 34
		constexpr int SIZE = N - 1; // 33

		const std::vector<float>& rawData = lodData.datas_;
		if (rawData.size() < (size_t)N * N * N) return {};

		auto idx3 = [](int x, int y, int z) -> size_t {
			return (size_t(x) * N + y) * N + z;
			};

		// 修复 UB：resize 而非 reserve
		std::vector<float> density((size_t)N * N * N);
		bool inside0 = nova_engine::utils::assist::UnpackDensity(rawData[idx3(0, 0, 0)]) < 0.0f;
		bool hasNoMesh = true;
		for (int x = 0; x < N; x++)
			for (int y = 0; y < N; y++)
				for (int z = 0; z < N; z++) {
					float d = nova_engine::utils::assist::UnpackDensity(rawData[idx3(x, y, z)]);
					density[idx3(x, y, z)] = d;
					if (inside0 != (d < 0.0f)) hasNoMesh = false;
				}
		if (hasNoMesh) return {};

		auto densitySafe = [&](int x, int y, int z) -> float {
			if (x < 0 || x >= N || y < 0 || y >= N || z < 0 || z >= N) return 1.0f;
			return density[idx3(x, y, z)];
			};

		// 法线：严格复刻原版：内部中心差分，边界单侧差分
		std::vector<glm::vec3> normals((size_t)N * N * N);
		for (int x = 0; x < N; x++)
			for (int y = 0; y < N; y++)
				for (int z = 0; z < N; z++) {
					glm::vec3 g(0.0f);
					if (x > 0 && x + 1 < N) g.x = (density[idx3(x + 1, y, z)] - density[idx3(x - 1, y, z)]) * 0.5f;
					else if (x + 1 < N)     g.x = density[idx3(x + 1, y, z)] - density[idx3(x, y, z)];
					else if (x > 0)         g.x = density[idx3(x, y, z)] - density[idx3(x - 1, y, z)];

					if (y > 0 && y + 1 < N) g.y = (density[idx3(x, y + 1, z)] - density[idx3(x, y - 1, z)]) * 0.5f;
					else if (y + 1 < N)     g.y = density[idx3(x, y + 1, z)] - density[idx3(x, y, z)];
					else if (y > 0)         g.y = density[idx3(x, y, z)] - density[idx3(x, y - 1, z)];

					if (z > 0 && z + 1 < N) g.z = (density[idx3(x, y, z + 1)] - density[idx3(x, y, z - 1)]) * 0.5f;
					else if (z + 1 < N)     g.z = density[idx3(x, y, z + 1)] - density[idx3(x, y, z)];
					else if (z > 0)         g.z = density[idx3(x, y, z)] - density[idx3(x, y, z - 1)];

					float len = glm::length(g);
					normals[idx3(x, y, z)] = (len > 1e-6f) ? g / len : glm::vec3(0.0f);
				}

		// 与原版 safeNormal 一致：越界返回 0
		auto normalAt = [&](int x, int y, int z) -> glm::vec3 {
			if (x < 0 || x >= N || y < 0 || y >= N || z < 0 || z >= N) return glm::vec3(0.0f);
			return normals[idx3(x, y, z)];
			};

		const size_t szX = (size_t)SIZE * N * N;
		const size_t szY = (size_t)N * SIZE * N;
		const size_t szZ = (size_t)N * N * SIZE;

		thread_local std::vector<glm::vec3> edgePosX, edgePosY, edgePosZ;
		thread_local std::vector<glm::vec3> edgeNormX, edgeNormY, edgeNormZ;
		thread_local std::vector<uint8_t>   edgeOriX, edgeOriY, edgeOriZ;

		edgePosX.assign(szX, glm::vec3(-1.0f)); edgePosY.assign(szY, glm::vec3(-1.0f)); edgePosZ.assign(szZ, glm::vec3(-1.0f));
		edgeNormX.assign(szX, glm::vec3(0.0f)); edgeNormY.assign(szY, glm::vec3(0.0f)); edgeNormZ.assign(szZ, glm::vec3(0.0f));
		edgeOriX.assign(szX, 0); edgeOriY.assign(szY, 0); edgeOriZ.assign(szZ, 0);

		auto eidxX = [](int ex, int ey, int ez) -> size_t { return (size_t(ex) * N + ey) * N + ez; };
		auto eidxY = [](int ex, int ey, int ez) -> size_t { return size_t(ex) * SIZE * N + size_t(ey) * N + ez; };
		auto eidxZ = [](int ex, int ey, int ez) -> size_t { return size_t(ex) * N * SIZE + size_t(ey) * SIZE + ez; };

		auto fillEdge = [&](int axis, int x, int y, int z) {
			int nx = x, ny = y, nz = z;
			if (axis == 0) nx++; else if (axis == 1) ny++; else nz++;
			float d1 = density[idx3(x, y, z)];
			float d2 = density[idx3(nx, ny, nz)];
			if ((d1 < 0.0f) == (d2 < 0.0f)) return;

			float t = d1 / (d1 - d2);
			glm::vec3 p = glm::mix(glm::vec3(x, y, z), glm::vec3(nx, ny, nz), t);
			glm::vec3 n = glm::mix(normalAt(x, y, z), normalAt(nx, ny, nz), t);
			float len = glm::length(n);
			n = (len > 1e-6f) ? n / len : glm::vec3(0.0f);
			uint8_t ori = (d1 < 0.0f) ? 1 : 2;

			if (axis == 0) { size_t i = eidxX(x, y, z); edgePosX[i] = p; edgeNormX[i] = n; edgeOriX[i] = ori; }
			else if (axis == 1) { size_t i = eidxY(x, y, z); edgePosY[i] = p; edgeNormY[i] = n; edgeOriY[i] = ori; }
			else { size_t i = eidxZ(x, y, z); edgePosZ[i] = p; edgeNormZ[i] = n; edgeOriZ[i] = ori; }
			};

		for (int x = 0; x < SIZE; x++) for (int y = 0; y < N; y++) for (int z = 0; z < N; z++) fillEdge(0, x, y, z);
		for (int x = 0; x < N; x++) for (int y = 0; y < SIZE; y++) for (int z = 0; z < N; z++) fillEdge(1, x, y, z);
		for (int x = 0; x < N; x++) for (int y = 0; y < N; y++) for (int z = 0; z < SIZE; z++) fillEdge(2, x, y, z);

		struct CellVert { glm::vec3 pos = glm::vec3(-1.0f); glm::vec3 normal = glm::vec3(0.0f); };
		std::vector<CellVert> cellVerts((size_t)N * N * N);

		constexpr int cornerOff[8][3] = {
			{ 0,0,0 },{ 1,0,0 },{ 1,1,0 },{ 0,1,0 },
			{ 0,0,1 },{ 1,0,1 },{ 1,1,1 },{ 0,1,1 }
		};
		// 原版是 [12][3]，第三个分量未使用；这里等价为 [12][2]
		constexpr int cellEdges[12][2] = {
			{ 0,4 },{ 1,5 },{ 2,6 },{ 3,7 }, // 沿 Z
			{ 0,3 },{ 1,2 },{ 4,7 },{ 5,6 }, // 沿 Y
			{ 0,1 },{ 2,3 },{ 4,5 },{ 6,7 }  // 沿 X
		};

		for (int x = 0; x < N; x++)
			for (int y = 0; y < N; y++)
				for (int z = 0; z < N; z++) {
					glm::vec3 sum(0.0f), sumN(0.0f);
					int count = 0;
					for (int e = 0; e < 12; e++) {
						int c1 = cellEdges[e][0], c2 = cellEdges[e][1];
						int x1 = x + cornerOff[c1][0], y1 = y + cornerOff[c1][1], z1 = z + cornerOff[c1][2];
						int x2 = x + cornerOff[c2][0], y2 = y + cornerOff[c2][1], z2 = z + cornerOff[c2][2];
						float d1 = densitySafe(x1, y1, z1);
						float d2 = densitySafe(x2, y2, z2);
						if ((d1 < 0.0f) == (d2 < 0.0f)) continue;

						float t = d1 / (d1 - d2);
						sum += glm::mix(glm::vec3(x1, y1, z1), glm::vec3(x2, y2, z2), t);
						// 与原版 safeNormal 一致：越界返回 0
						glm::vec3 n = glm::mix(normalAt(x1, y1, z1), normalAt(x2, y2, z2), t);
						float len = glm::length(n);
						if (len > 1e-6f) n /= len;
						sumN += n;
						count++;
					}
					if (count > 0) {
						size_t ci = idx3(x, y, z);
						cellVerts[ci].pos = sum / float(count);
						float len = glm::length(sumN);
						cellVerts[ci].normal = (len > 1e-6f) ? sumN / len : glm::vec3(0.0f);
					}
				}

		constexpr int meshBuildTable[3][4][3] = {
			{ { 0,0,0 },{ 1,0,0 },{ 1,1,0 },{ 0,1,0 } },
			{ { 0,0,0 },{ 1,0,0 },{ 1,0,1 },{ 0,0,1 } },
			{ { 0,0,0 },{ 0,1,0 },{ 0,1,1 },{ 0,0,1 } }
		};

		core::MeshData data;
		for (int x = 0; x < N; x++)
			for (int y = 0; y < N; y++)
				for (int z = 0; z < N; z++) {
					for (int dir = 0; dir < 3; dir++) {
						size_t eIdx; glm::vec3 ePos; uint8_t eOri;
						if (dir == 2) { // 沿 X，共享棱在 (x, y+1, z+1)
							// 原版只检查 x>=SIZE，这里补上 y/z 越界检查，避免越界读取
							if (x >= SIZE || y + 1 >= N || z + 1 >= N) continue;
							eIdx = eidxX(x, y + 1, z + 1); ePos = edgePosX[eIdx]; eOri = edgeOriX[eIdx];
						}
						else if (dir == 1) { // 沿 Y，共享棱在 (x+1, y, z+1)
							if (x + 1 >= N || y >= SIZE || z + 1 >= N) continue;
							eIdx = eidxY(x + 1, y, z + 1); ePos = edgePosY[eIdx]; eOri = edgeOriY[eIdx];
						}
						else { // 沿 Z，共享棱在 (x+1, y+1, z)
							if (x + 1 >= N || y + 1 >= N || z >= SIZE) continue;
							eIdx = eidxZ(x + 1, y + 1, z); ePos = edgePosZ[eIdx]; eOri = edgeOriZ[eIdx];
						}
						if (ePos.x == -1.0f) continue;

						glm::vec3 qp[4], qn[4];
						bool ok = true;
						for (int off = 0; off < 4; off++) {
							int fx = x + meshBuildTable[dir][off][0];
							int fy = y + meshBuildTable[dir][off][1];
							int fz = z + meshBuildTable[dir][off][2];
							if (fx < 0 || fx >= N || fy < 0 || fy >= N || fz < 0 || fz >= N) { ok = false; break; }
							size_t ci = idx3(fx, fy, fz);
							if (cellVerts[ci].pos.x == -1.0f) { ok = false; break; }
							qp[off] = cellVerts[ci].pos;
							qn[off] = cellVerts[ci].normal;
						}
						if (!ok) continue;

						uint32_t base = (uint32_t)(data.vertices.size() / 3);
						for (int ii = 0; ii < 4; ii++) {
							data.vertices.push_back((qp[ii].x - 1) * scale);
							data.vertices.push_back((qp[ii].y - 1) * scale);
							data.vertices.push_back((qp[ii].z - 1) * scale);
							data.uvws.push_back(meshBuildTable[0][ii][0]);
							data.uvws.push_back(meshBuildTable[0][ii][1]);
							data.uvws.push_back(3.0f);
							data.normals.push_back(qn[ii].x);
							data.normals.push_back(qn[ii].y);
							data.normals.push_back(qn[ii].z);
							data.aos.push_back(1.0f);
						}

						static constexpr int baseIndices[6] = { 0,1,2,0,2,3 };
						bool reverse = (dir == 0 && eOri == 2) ||
							(dir == 1 && eOri == 1) ||
							(dir == 2 && eOri == 2);
						if (reverse) for (int ii = 5; ii >= 0; ii--) data.indices.push_back(baseIndices[ii] + base);
						else         for (int ii = 0; ii < 6; ii++)  data.indices.push_back(baseIndices[ii] + base);
					}
				}
		return data;
	}
}
