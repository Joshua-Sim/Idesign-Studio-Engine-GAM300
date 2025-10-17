local ok = true
ok = ok and Cook.WindowConfig("src/Data/WindowAsset.json",  "src/Data/WindowAsset.cfg")
ok = ok and Cook.Audio       ("src/Data/AudioAsset.json",   "src/Data/AudioAsset.bin",  "AudioProperties")
ok = ok and Cook.Textures    ("src/Data/TextureAsset.json", "src/Data/TextureAsset.bin","textures")
ok = ok and Cook.Meshes      ("src/Data/MeshAsset.json",    "src/Data/MeshAsset.bin",   "meshes")
print(ok and "[Cook] OK" or "[Cook] FAILED")
return ok