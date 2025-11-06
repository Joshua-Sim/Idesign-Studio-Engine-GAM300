
#pragma once
#ifndef _MATERIAL_LOADER_H_
#define _MATERIAL_LOADER_H_
#include "JsonSerialize.h"
#include "MaterialProperties.h"
namespace Framework
{
	std::unique_ptr<MaterialProperties> LoadMaterialAsset(const std::string& path, const AssetHandle& guid);
}
#endif // !_MATERIAL_LOADER_H_
