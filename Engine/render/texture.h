#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace nova_engine::render {
    class TextureData {
    public:
        // 构造函数：显式传入通道数，并计算实际数据大小
        TextureData(int width, int height, int channels, const unsigned char* data)
            : width_(width), height_(height), channels_(channels),
            data_(data, data + width * height * channels) {
        }

        // 禁止隐式拷贝（数据量大），可提供移动语义
        TextureData(const TextureData&) = default;
        TextureData& operator=(const TextureData&) = default;
        TextureData(TextureData&&) = default;
        TextureData& operator=(TextureData&&) = default;
        ~TextureData() = default;

        // 访问器
        inline int width() const { return width_; }
        inline int height() const { return height_; }
        inline int channels() const { return channels_; }
        inline const unsigned char* data() const { return data_.data(); }
        inline std::size_t size() const { return data_.size(); }

        void Clear() {
            data_.clear();
        }

    private:
        int width_;
        int height_;
        int channels_;
        std::vector<unsigned char> data_;
    };

    TextureData LoadTexture(const char* path);

}