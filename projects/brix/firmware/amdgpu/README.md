# AMD GPU microcode (Polaris 10 / RX 580)

These `polaris10_*.bin` blobs are the signed microcode the RX 580's engines
run: the graphics command processor (`pfp`, `me`, `ce`), the compute command
processor (`mec`, `mec2`, and their `_2` revisions), the RLC, the SDMA
engines, the SMU power/clock controller (`smc`, and the `k_`/`k2_` board
variants) and the memory controller (`mc`). The amdgpu driver
(`projects/brix/kernel/gpu.mlx` and the stages that follow it) loads them
from `/system/firmware/amdgpu/` at bring-up; the GPU will not run its command
rings or compute without them.

Origin: the `linux-firmware` project (decompressed from its `.bin.zst`).
They are redistributable under the AMD microcode licence that ships with
linux-firmware (`LICENSE.amdgpu`): binary redistribution for use with the
hardware is permitted. They are data the GPU executes, not part of brixOS,
and carry their own licence.

The video blobs (`uvd`, `vce`) are deliberately left out: brixOS decodes
video on the CPU/Vulkan path, so they are not needed for compute.
