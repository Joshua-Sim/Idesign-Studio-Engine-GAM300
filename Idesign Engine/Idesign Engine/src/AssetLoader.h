
#pragma once
#ifndef _ASSET_LOADER_H_
#define _ASSET_LOADER_H_

#include <AssetCompiler.h>

namespace Framework
{
    template<typename T>
    struct AssetLoader 
    {
        static std::unique_ptr<T> Load(const std::string& binaryPath, const prop_list& props) 
        {
            std::ifstream in(binaryPath, std::ios::binary);
            if (!in.is_open()) throw std::runtime_error("Failed to open binary file: " + binaryPath);

            auto obj = std::make_unique<T>();
            BinaryToObject(in, *obj, props);
            return obj;
        }

        static std::vector<std::unique_ptr<T>> LoadArray(const std::string& binaryPath, const prop_list& props) 
        {
            std::ifstream in(binaryPath, std::ios::binary);
            if (!in.is_open()) throw std::runtime_error("Failed to open binary file: " + binaryPath);

            uint32_t count = 0;
            in.read(reinterpret_cast<char*>(&count), sizeof(count));
            if (!in || count == 0) throw std::runtime_error("Invalid or empty binary array: " + binaryPath);

            std::vector<std::unique_ptr<T>> result;
            result.reserve(count);

            for (uint32_t i = 0; i < count; ++i) 
            {
                auto obj = std::make_unique<T>();
                BinaryToObject(in, *obj, props);
                result.push_back(std::move(obj));
            }
            return result;
        }
    };
}
#endif // !_ASSET_LOADER_H_