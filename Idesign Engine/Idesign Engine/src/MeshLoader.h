// MeshLoader.h
#pragma once
#include <string>
#include "MeshProperties.h"

using namespace Framework;
class MeshLoader {
public:
    static void loadMesh(const std::string& path, MeshProperties& mesh);
};
