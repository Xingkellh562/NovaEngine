#pragma once

#include <render/opengl/frame_buffer.h>
#include <memory>

namespace nova_engine::render::opengl {
	class FrameBufferLibrary {
	public:
		static FrameBufferLibrary& GetInstance() {
			static FrameBufferLibrary instance;
			return instance;
		}

		FrameBuffer* FindFrameBuffer(std::string) const;
		void AddFrameBuffer(std::string);

		void OnResize(int w, int h);
	private:
		std::unordered_map<std::string, std::unique_ptr<FrameBuffer>> buffers_;

		FrameBufferLibrary();
		~FrameBufferLibrary();
		void Cleanup();
	};
}