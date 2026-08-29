#include "texture.h"
#include <stb_image/stb_image.h>
#include <log/logger.h>

namespace nova_engine::render {
    TextureData LoadTexture(const char* path) {
        int width, height, channels;
        unsigned char* data = stbi_load(path, &width, &height, &channels, 0); // 0 表示保持原通道
        if (!data) {
            // 抛出异常或返回空对象
            /*throw std::runtime_error("Failed to load texture: " + std::string(path));*/

            NE_CORE_ERROR("RenderSystem","Failed to load texture from {}." ,path);
        }
        else {
            NE_CORE_INFO("RenderSystem","Load texture successfully from path:{},size:{} byte", path , width*height* channels);
        }

        // 构造 TextureData 时拷贝数据
        TextureData tex(width, height, channels, data);

        // 释放 stb_image 原始内存
        stbi_image_free(data);

        return tex;
    }
}