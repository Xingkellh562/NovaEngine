#include <glad/glad.h>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

namespace nova_engine::render::opengl {
	class IRenderer {
	public:
		virtual ~IRenderer() = default;
		virtual void Render();
		virtual void OnResize(int w, int h) {}
	};

	class RendererTable {
	public:
		using CreatorFunc = std::function<std::unique_ptr<IRenderer>()>;
		static RendererTable& Get() { static RendererTable instance;return instance; }

		void RegisterRenderer(const std::string& name, CreatorFunc creator) {
			creators_[name] = std::move(creator);
		}

		std::unique_ptr<IRenderer> Create(const std::string& name) {
			auto it = creators_.find(name);
			return (it != creators_.end()) ? it->second() : nullptr;
		}

		const auto& GetCreators() const { return creators_; }

	private:
		std::unordered_map<std::string, CreatorFunc> creators_{};
	};

	template<typename T>
	class RendererRegister {
	public:
		explicit RendererRegister(const std::string& name) {
			RendererTable::Get().RegisterRenderer(name, []() -> std::unique_ptr<IRenderer> {
				return std::make_unique<T>();
				})
		}
	};
}