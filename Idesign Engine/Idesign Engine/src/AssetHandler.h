#pragma once
#ifndef _ASSET_HANDLER_H_
#define _ASSET_HANDLER_H_

#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace Framework
{
    inline std::string GenerateGUID()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> dist16(0, 0xFFFF);

        std::stringstream ss;
        ss << std::hex << std::setfill('0')
            << std::setw(4) << dist16(gen)
            << std::setw(4) << dist16(gen) << "-"
            << std::setw(4) << dist16(gen) << "-"
            << std::setw(4) << ((dist16(gen) & 0x0FFF) | 0x4000) << "-"
            << std::setw(4) << ((dist16(gen) & 0x3FFF) | 0x8000) << "-"
            << std::setw(4) << dist16(gen)
            << std::setw(4) << dist16(gen)
            << std::setw(4) << dist16(gen);
        return ss.str();
    }

    struct AssetHandle 
    {
        std::string id;

        AssetHandle() = default;
        AssetHandle(const std::string& identifier) : id(identifier) {}

        bool operator==(const AssetHandle& other) const { return id == other.id; }
        bool operator!=(const AssetHandle& other) const { return id != other.id; }

        operator bool() const { return !id.empty(); }
    };
}

#endif // !_ASSET_HANDLER_H_
