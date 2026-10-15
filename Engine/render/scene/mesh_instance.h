#pragma once

#include <render/core/handles.h>
#include <render/device/render_device.h>
#include <glm/vec3.hpp>

namespace nova_engine::render::scene {
	struct MeshInstance {
		//非拥有指针，只在析构时归还
		device::IRenderDevice* owner = nullptr;
		core::MeshHandle mesh{};

		glm::vec3 position{ 0.0f };
		glm::vec3 rotation{ 0.0f,0.0f,1.0f };
		bool awake = true;

		MeshInstance() = default;
		explicit MeshInstance(device::IRenderDevice& dev) : owner(&dev) {}

		~MeshInstance() {
			if (owner && mesh.Valid()) {
				owner->DestroyMesh(mesh);
				mesh = {};
			}
		}

		MeshInstance(const MeshInstance&) = delete;
		MeshInstance& operator=(const MeshInstance&) = delete;

		MeshInstance(MeshInstance&& other) noexcept : 
			owner(other.owner),
			mesh(other.mesh),
			position(other.position),
			rotation(other.rotation),
			awake(other.awake)
		{
			other.owner = nullptr;
			other.mesh = {};
		}

		MeshInstance& operator=(MeshInstance&& other) noexcept {
			if (this != &other) {
				if (owner && mesh.Valid()) owner->DestroyMesh(mesh);

				owner = other.owner;
				mesh = other.mesh;
				position = other.position;
				rotation = other.rotation;
				awake = other.awake;

				other.owner = nullptr;
				other.mesh = {};
			}
			return *this;
		}

		void Sleep() noexcept { awake = false; }
		void Wake() noexcept { awake = true; }
	};
}