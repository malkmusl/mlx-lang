# Unfinished: GPU tile painting and zero-copy presentation

gpu-painting-zero-copy.patch applies on 22703cc: four mlx-cc/mlxlibcpp fixes with tests (epoch 29), ANGLE's share-context lock, GPU painting by default, and port/angle/vulkan_present.cpp (untested). Open: paint 18/19 (scheme.html) from offsetof(skvx::Vec<4,float>, hi) being 0 instead of 8 in mlx-cc; bump the epoch again when committing (intermediate compilers filled the cache with epochs 27-29); measure; MLX_COMPOSITING=cpu checks; PORT.md.
