pub fn main() void {
    asm volatile ("" : : : "memory", "rcx");
}
