local ok = true
ok = ok and Cook.WindowConfig("Assets/JSON/WindowAsset.json",  "Resource/WindowAsset.cfg",  "Descriptor/WindowAsset.desc")
ok = ok and Cook.Audio       ("Assets/JSON/AudioAsset.json",   "Resource/AudioAsset.bin",	"Descriptor/AudioAsset.desc", "AudioProperties")
ok = ok and Cook.Textures    ("Assets/JSON/TextureAsset.json", "Resource/TextureAsset.bin", "Descriptor/TextureAsset.desc", "textures")
ok = ok and Cook.Meshes      ("Assets/JSON/MeshAsset.json",    "Resource/MeshAsset.bin",	"Descriptor/MeshAsset.desc",  "mesh")
print(ok and "[Cook] OK" or "[Cook] FAILED")
return ok