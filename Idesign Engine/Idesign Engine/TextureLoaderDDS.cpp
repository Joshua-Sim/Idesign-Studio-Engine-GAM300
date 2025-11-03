#include "pch.h"
#include <glew.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>
#include <cstring>

// -------------------------------------------------------------
// Minimal DDS structures
// -------------------------------------------------------------
struct DDS_PIXELFORMAT
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rMask;
    uint32_t gMask;
    uint32_t bMask;
    uint32_t aMask;
};

struct DDS_HEADER
{
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

// DX10 extension header (for BC6/BC7)
struct DDS_HEADER_DXT10
{
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

// -------------------------------------------------------------
// DXGI format IDs we care about
// -------------------------------------------------------------
enum DXGI_FORMAT : uint32_t
{
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
    DXGI_FORMAT_BC4_UNORM = 80,
    DXGI_FORMAT_BC5_UNORM = 83,
    DXGI_FORMAT_BC7_UNORM = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB = 99
};

// -------------------------------------------------------------
// Utility: map DXGI to GL internal format
// -------------------------------------------------------------
static GLenum DXGIToGL(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:      return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case DXGI_FORMAT_BC1_UNORM_SRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case DXGI_FORMAT_BC2_UNORM:      return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case DXGI_FORMAT_BC2_UNORM_SRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case DXGI_FORMAT_BC3_UNORM:      return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case DXGI_FORMAT_BC3_UNORM_SRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case DXGI_FORMAT_BC4_UNORM:      return GL_COMPRESSED_RED_RGTC1;
    case DXGI_FORMAT_BC5_UNORM:      return GL_COMPRESSED_RG_RGTC2;
    case DXGI_FORMAT_BC7_UNORM:      return GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
    case DXGI_FORMAT_BC7_UNORM_SRGB: return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
    default: return 0;
    }
}

// -------------------------------------------------------------
// Main loader
// -------------------------------------------------------------
GLuint LoadDDS(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        std::cerr << "[DDS] Failed to open " << path << "\n";
        return 0;
    }

    DDS_HEADER header{};
    f.read(reinterpret_cast<char*>(&header), sizeof(DDS_HEADER));
    if (header.magic != 0x20534444) // DDS
    {
        std::cerr << "[DDS] Invalid magic\n";
        return 0;
    }

    DXGI_FORMAT dxgiFmt = DXGI_FORMAT_BC1_UNORM;
    size_t dataOffset = sizeof(DDS_HEADER);

    // If DX10 header present, read it
    if (header.ddspf.fourCC == 0x30315844) // DX10
    {
        DDS_HEADER_DXT10 dx10{};
        f.read(reinterpret_cast<char*>(&dx10), sizeof(dx10));
        dxgiFmt = static_cast<DXGI_FORMAT>(dx10.dxgiFormat);
        dataOffset += sizeof(DDS_HEADER_DXT10);
    }
    else
    {
        dxgiFmt = static_cast<DXGI_FORMAT>(header.ddspf.fourCC);
    }

    GLenum internalFmt = DXGIToGL(dxgiFmt);
    if (!internalFmt)
    {
        std::cerr << "[DDS] Unsupported DXGI format: " << dxgiFmt << "\n";
        return 0;
    }

    // Read remaining file
    f.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(f.tellg());
    size_t dataSize = fileSize - dataOffset;
    f.seekg(dataOffset, std::ios::beg);
    std::vector<uint8_t> buffer(dataSize);
    f.read(reinterpret_cast<char*>(buffer.data()), dataSize);
    f.close();

    const uint32_t width = header.width;
    const uint32_t height = header.height;
    const uint32_t mips = (header.mipMapCount > 0) ? header.mipMapCount : 1;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // upload each mip level
    size_t offset = 0;
    uint32_t w = width, h = height;
    for (uint32_t i = 0; i < mips; ++i)
    {
        uint32_t blockSize = (internalFmt == GL_COMPRESSED_RG_RGTC2 ||
            internalFmt == GL_COMPRESSED_RED_RGTC1) ? 8 : 16;
        uint32_t size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;

        glCompressedTexImage2D(GL_TEXTURE_2D, i, internalFmt, w, h, 0,
            size, buffer.data() + offset); //MIPMAP

        offset += size;
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        (mips > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    std::cout << "[DDS] Loaded " << path << " ("
        << width << "x" << height << ", mips " << mips << ")\n";
    return tex;
}
