#!/usr/bin/env python3
# Holds the numbers of our Vulkan driver's stage (a) (projects/brix/vulkan/
# driver.mlx) against the reference's own files at the tags the driver
# follows, so that none is from memory:
#
#   tools/check_vulkan_driver.py MESA LINUX
#
# MESA is a directory with these files of Mesa mesa-24.0.0 at their paths
# in its tree (https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.0.0/
# PATH), LINUX one with Linux v6.6's (https://raw.githubusercontent.com/
# torvalds/linux/v6.6/PATH); neither is kept in this repository:
#   src/amd/vulkan/radv_physical_device.c   the limits (radv_get_physical_
#                                           device_properties), the
#                                           descriptor counts' formula
#   src/amd/vulkan/radv_constants.h         the numbers the limits name
#   drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c the Polaris 10 device numbers
#
# Checked:
#   1. every limit driver.mlx's `limits` writes is the value RADV's limits
#      give it on this generation (GFX8, a Polaris 10: max_shared_size of
#      GFX7 and later, the descriptor counts' formula worked out from the
#      constants), and every limit RADV gives that driver.mlx leaves at 0 is
#      one the stage says it leaves (the timestamps: none at this stage);
#   2. `serves` takes exactly the device numbers amdgpu_drv.c lists as
#      CHIP_POLARIS10 (not a range: the list has gaps).
import os
import re
import sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
driver = os.path.join(root, "projects", "brix", "vulkan", "driver.mlx")
failures = 0


def fail(message):
    global failures
    failures += 1
    print("FAIL " + message)


def ok(message):
    print("ok   " + message)


def constants(text):
    """#define NAME VALUE of a C header, the value as text."""
    found = {}
    for m in re.finditer(r"^#define\s+([A-Z0-9_]+)\s+(.+?)\s*(?://.*|/\*.*)?$", text, re.M):
        found[m.group(1)] = m.group(2).strip()
    return found


def c_value(text, names, local):
    """A C expression of the limits block as a number (or a pair)."""
    text = text.strip().rstrip(",")
    if text in local:
        return local[text]
    if text.startswith("{") and text.endswith("}"):
        return tuple(c_value(part, names, local) for part in text[1:-1].split(","))
    expression = text
    # (A constant's value may name others: until none is left.)
    for _ in range(8):
        before = expression
        for name in sorted(names, key=len, reverse=True):
            expression = re.sub(r"\b%s\b" % name, "(%s)" % names[name], expression)
        if expression == before:
            break
    expression = expression.replace("UINT32_MAX", "0xFFFFFFFF").replace("INT16_MIN", "-32768").replace("INT16_MAX", "32767")
    expression = " ".join(expression.split())
    expression = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)u?ll\b", r"\1", expression)
    expression = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)u\b", r"\1", expression)
    expression = expression.replace("true", "1").replace("false", "0")
    # (C's division of whole numbers.)
    if "." not in expression:
        expression = expression.replace("/", "//")
    try:
        return eval(expression, {"__builtins__": {}}, {})
    except Exception:
        return None


def radv_limits(mesa):
    source = open(os.path.join(mesa, "src/amd/vulkan/radv_physical_device.c")).read()
    header = open(os.path.join(mesa, "src/amd/vulkan/radv_constants.h")).read()
    names = constants(header)
    # The formula of radv_max_descriptor_set_size, with its constants.
    body = re.search(r"radv_max_descriptor_set_size\(\)\s*\{(.*?)\n\}", source, re.S).group(1)
    formula = re.search(r"return (.*?);", body, re.S).group(1)
    formula = re.sub(r"/\*.*?\*/", "", formula, flags=re.S)
    names.setdefault("MAX_DYNAMIC_BUFFERS", "(%s + %s)" % (names["MAX_DYNAMIC_UNIFORM_BUFFERS"], names["MAX_DYNAMIC_STORAGE_BUFFERS"]))
    descriptors = c_value(formula, names, {})
    # The device's own facts: GFX8 (max_shared_size of GFX7 and later), a
    # Polaris 10; sample counts 0xf.
    shared = re.search(r"max_shared_size = device->rad_info.gfx_level >= GFX7 \? (\d+) : (\d+);", source)
    # radv_uniform_buffer_offset_alignment: the hardware's least, the
    # driconf override being 0 unless set.
    uniform = re.search(r"return MAX2\(uniform_offset_alignment, (\d+)\);", source)
    local = {"max_descriptor_set_size": descriptors, "sample_counts": 0xF, "pdevice->max_shared_size": int(shared.group(1)), "radv_uniform_buffer_offset_alignment(pdevice)": int(uniform.group(1))}
    block = re.search(r"pdevice->vk.properties = \(struct vk_properties\)\{(.*?)\n   \};", source, re.S).group(1)
    limits = {}
    for m in re.finditer(r"^\s*\.([A-Za-z0-9]+) = (.+?),?\s*(?:/\*.*\*/)?$", block, re.M):
        limits[m.group(1)] = c_value(m.group(2), names, local)
    return limits, descriptors


def driver_limits():
    text = open(driver).read()
    body = re.search(r"fn limits\(into: \*vk.PhysicalDeviceLimits\) -> void \{(.*?)\n\}", text, re.S).group(1)
    local = {"all": 0xFFFFFFFF, "samples": 0xF, "vk.FALSE": 0, "vk.TRUE": 1}
    for m in re.finditer(r"const (\w+): u32 = (\w+)", body):
        local[m.group(1)] = int(m.group(2), 0)
    found = {}
    for m in re.finditer(r"into\.\*\.(\w+)(?:\[(\d)\])? = (\S+)", body):
        value = m.group(3)
        number = local[value] if value in local else float(value) if "." in value else int(value, 0)
        if m.group(2) is None:
            found[m.group(1)] = number
        else:
            found.setdefault(m.group(1), {})[int(m.group(2))] = number
    for key, value in list(found.items()):
        if isinstance(value, dict):
            found[key] = tuple(value[i] for i in sorted(value))
    return found


# The limits the stage gives otherwise, and why.
OURS = {
    # No timestamps at this stage: the queue family's valid bits are 0.
    "timestampComputeAndGraphics": 0,
    "timestampPeriod": 1.0,
}


def check_limits(mesa):
    radv, descriptors = radv_limits(mesa)
    ours = driver_limits()
    if descriptors != 8388606:
        fail("radv_max_descriptor_set_size works out at %r, the driver writes 8388606" % descriptors)
    checked = 0
    for name, value in ours.items():
        if name in OURS:
            if value != OURS[name]:
                fail("%s is %r, the stage's own is %r" % (name, value, OURS[name]))
            continue
        if name not in radv:
            fail("%s: no such limit in RADV's" % name)
            continue
        want = radv[name]
        if want is None or (isinstance(want, tuple) and None in want):
            fail("%s: RADV's value could not be worked out" % name)
            continue
        if isinstance(want, tuple) or isinstance(value, tuple):
            if tuple(float(x) for x in (want if isinstance(want, tuple) else (want,))) != tuple(float(x) for x in (value if isinstance(value, tuple) else (value,))):
                fail("%s is %r, RADV's %r" % (name, value, want))
        elif float(value) != float(want):
            fail("%s is %r, RADV's %r" % (name, value, want))
        checked += 1
    # Every limit RADV has: written by the driver (the sparse residency
    # fields are VkPhysicalDeviceSparseProperties', not limits, and stay
    # false: sparse binding is not offered).
    properties = {"apiVersion", "driverVersion", "vendorID", "deviceID", "deviceType", "sparseResidencyNonResidentStrict", "sparseResidencyStandard2DBlockShape", "sparseResidencyStandard3DBlockShape"}
    missing = sorted(set(radv) - set(ours) - properties)
    if missing:
        fail("limits RADV gives that the driver does not write: %s" % ", ".join(missing))
    ok("%d limits as RADV gives them on GFX8, the timestamps' two the stage's own" % checked)


def check_serves(linux):
    text = open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c")).read()
    listed = sorted(int(m.group(1), 16) for m in re.finditer(r"\{0x1002, (0x[0-9A-Fa-f]+), PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10\}", text))
    source = open(driver).read()
    m = re.search(r"const POLARIS10 = \[_\]u32\{([^}]*)\}", source)
    if not m:
        fail("serves: its list is not where it was")
        return
    ours = sorted(int(x.strip(), 16) for x in m.group(1).split(","))
    if ours != listed:
        fail("serves takes %s, amdgpu_drv.c's Polaris 10 are %s" % ([hex(x) for x in ours], [hex(x) for x in listed]))
    else:
        ok("serves takes the %d device numbers amdgpu_drv.c lists as CHIP_POLARIS10" % len(listed))


def main():
    if len(sys.argv) != 3:
        print("usage: tools/check_vulkan_driver.py MESA LINUX")
        return 2
    check_limits(sys.argv[1])
    check_serves(sys.argv[2])
    if failures:
        print("%d checks failed" % failures)
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
