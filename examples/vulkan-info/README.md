# vulkan-info

Lists the Vulkan drivers installed on the system and the physical devices
each exposes — through `std.vulkan.loader`, which reads the ICD manifests
and loads the drivers itself, with no C Vulkan loader.

```sh
mlx4 examples/vulkan-info/main.mlx -o vulkan-info
./vulkan-info             # every driver on the search path
./vulkan-info --system    # through the system loader (libvulkan.so.1)
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json ./vulkan-info
```

With Mesa's lavapipe (package `mesa-vulkan-drivers`):

```text
driver libvulkan_lvp.so (/usr/share/vulkan/icd.d/lvp_icd.json)
    interface version 5, Vulkan 1.4.318
    device: llvmpipe (LLVM 20.1.2, 256 bits) (CPU, Vulkan 1.4.318)
      queue family 0: 1 queue(s), graphics compute transfer
      memory heap 0: 16095 MiB, device local
```

See `docs/reference/vulkan.md`.
