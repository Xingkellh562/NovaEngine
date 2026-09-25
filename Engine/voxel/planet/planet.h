#pragma once

#include <string>

namespace nova_engine::voxel::planet {
	class Planet {
	public:
		Planet();
		~Planet();

		Planet(const Planet&) = delete;
		Planet& operator=(const Planet&) = delete;

	private:
		std::string name_;
	};
}