// MeshLoader.h
#pragma once
#include <string>
#include "MeshComponent.h"

class MeshLoader {
public:
    static void loadMesh(const std::string& path, MeshComponent& mesh);
};
