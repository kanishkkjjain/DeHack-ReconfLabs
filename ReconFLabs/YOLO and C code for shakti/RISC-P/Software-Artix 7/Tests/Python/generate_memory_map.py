#!/usr/bin/env python3
"""
Generate accurate memory map for YOLOv3-tiny based on layer dimensions
"""

# Layer output configurations: layer_id -> (h, w, c)
LAYERS = {
    0:  (416, 416, 32),
    2:  (208, 208, 64),
    4:  (104, 104, 128),
    6:  (52, 52, 256),
    8:  (26, 26, 512),
    10: (13, 13, 1024),
    11: (13, 13, 1024),
    12: (13, 13, 255),
}

# Kernel configurations: layer_id -> (kernel_size, input_channels, output_channels)
KERNELS = {
    0:  (3, 3, 32),
    2:  (3, 32, 64),
    4:  (3, 64, 128),
    6:  (3, 128, 256),
    8:  (3, 256, 512),
    10: (3, 512, 1024),
    11: (3, 1024, 1024),
    12: (1, 1024, 255),
}

def calculate_layer_addresses():
    """Calculate output layer addresses"""
    print("\n" + "="*80)
    print("LAYER OUTPUT ADDRESSES AND SIZES")
    print("="*80)
    
    layer_map = []
    current_offset = 0x00000000
    
    for layer_id in sorted(LAYERS.keys()):
        h, w, c = LAYERS[layer_id]
        size = h * w * c * 4  # float32 = 4 bytes
        
        layer_map.append({
            'id': layer_id,
            'h': h,
            'w': w,
            'c': c,
            'size': size,
            'offset': current_offset,
        })
        
        print(f"\nL{layer_id:2d}: {h:3d}×{w:3d}×{c:4d} = {size:>10,} bytes (0x{size:08X})")
        print(f"     Offset: 0x{current_offset:08X}")
        print(f"     #define DDR_L{layer_id}_ADDR      (DDR_LAYERS_BASE + 0x{current_offset:08X}u)")
        print(f"     #define DDR_L{layer_id}_SIZE      0x{size:08X}u  // {size:,} bytes ({h}×{w}×{c}×4)")
        
        current_offset += size
    
    total_output = current_offset
    print(f"\n{'='*80}")
    print(f"Total output memory: {total_output:,} bytes (0x{total_output:08X})")
    print(f"That's {total_output/(1024*1024):.2f}MB")
    print(f"Kernel base offset from layer base: 0x{total_output:08X}")
    
    return layer_map, total_output


def calculate_kernel_addresses(layer_base_offset):
    """Calculate kernel addresses"""
    print("\n" + "="*80)
    print("KERNEL ADDRESSES AND SIZES")
    print("="*80)
    
    kernel_map = []
    current_offset = 0x00000000
    
    for layer_id in sorted(KERNELS.keys()):
        ksize, in_c, out_c = KERNELS[layer_id]
        size = ksize * ksize * in_c * out_c * 4  # float32 = 4 bytes
        
        kernel_map.append({
            'id': layer_id,
            'ksize': ksize,
            'in_c': in_c,
            'out_c': out_c,
            'size': size,
            'offset': current_offset,
        })
        
        print(f"\nL{layer_id:2d}: {ksize}×{ksize}×{in_c}×{out_c} = {size:>10,} bytes (0x{size:08X})")
        print(f"     Offset: 0x{current_offset:08X}")
        print(f"     #define DDR_L{layer_id}_KERNELS  (DDR_KERNELS_BASE + 0x{current_offset:08X}u)")
        print(f"     // {size:,} bytes")
        
        current_offset += size
    
    total_kernel = current_offset
    print(f"\n{'='*80}")
    print(f"Total kernel memory: {total_kernel:,} bytes (0x{total_kernel:08X})")
    print(f"That's {total_kernel/(1024*1024):.2f}MB")
    
    return kernel_map, total_kernel


def generate_c_code(layer_map, total_output, kernel_map):
    """Generate C header file content"""
    
    code = ""
    
    # Layer definitions
    code += "/* ============================================================================\n"
    code += " * YOLO-TINY LAYER OUTPUT MEMORY LAYOUT (Floating Point)\n"
    code += " * ============================================================================\n"
    code += " * \n"
    code += " * Memory allocated starting at DDR_LAYERS_BASE with tight packing.\n"
    code += " * Each layer output buffer allocated sequentially.\n"
    code += " * \n"
    code += " * Calculation basis: outputs stored as float32 (4 bytes per value)\n"
    code += " * Tight packing: no alignment padding, sequential allocation\n"
    code += " */\n\n"
    code += "#define DDR_LAYERS_BASE (DDR_BASE + 0x00200000u)  // Start after status registers\n\n"
    
    for layer in layer_map:
        lid = layer['id']
        h = layer['h']
        w = layer['w']
        c = layer['c']
        offset = layer['offset']
        size = layer['size']
        
        code += f"/* Layer {lid}: Conv/Pool | {h}×{w}×{c} float32 */\n"
        code += f"#define DDR_L{lid}_ADDR      (DDR_LAYERS_BASE + 0x{offset:08X}u)\n"
        code += f"#define DDR_L{lid}_SIZE      0x{size:08X}u  // {size:,} bytes ({h}×{w}×{c}×4)\n\n"
    
    # Kernel definitions
    code += "/* ============================================================================\n"
    code += " * KERNEL STORAGE (Model Weights)\n"
    code += " * ============================================================================\n"
    code += " * \n"
    code += " * All kernels stored continuously from KERNEL_BASE\n"
    code += " * Each layer's kernels stored sequentially: [Ch0] [Ch1] ... [ChN]\n"
    code += " * \n"
    code += " * Layout: For N_out output channels, store N_out kernel sets consecutively\n"
    code += " */\n\n"
    code += f"#define DDR_KERNELS_BASE (DDR_LAYERS_BASE + 0x{total_output:08X}u)  // After layer outputs\n\n"
    code += "/* Layer kernels (float32) */\n"
    
    for kernel in kernel_map:
        lid = kernel['id']
        ksize = kernel['ksize']
        in_c = kernel['in_c']
        out_c = kernel['out_c']
        offset = kernel['offset']
        size = kernel['size']
        
        code += f"#define DDR_L{lid}_KERNELS  (DDR_KERNELS_BASE + 0x{offset:08X}u) // {size:,} bytes\n"
    
    return code


def main():
    print("\n" + "="*80)
    print("YOLOv3-TINY MEMORY MAP CALCULATOR")
    print("="*80)
    
    # Calculate layer addresses
    layer_map, total_output = calculate_layer_addresses()
    
    # Calculate kernel addresses
    kernel_map, total_kernel = calculate_kernel_addresses(total_output)
    
    # Generate C code
    print("\n" + "="*80)
    print("GENERATED C CODE")
    print("="*80 + "\n")
    
    c_code = generate_c_code(layer_map, total_output, kernel_map)
    print(c_code)
    
    # Summary
    print("\n" + "="*80)
    print("SUMMARY")
    print("="*80)
    print(f"Total output buffers: {total_output:,} bytes ({total_output/(1024*1024):.2f}MB)")
    print(f"Total kernel storage: {total_kernel:,} bytes ({total_kernel/(1024*1024):.2f}MB)")
    print(f"Combined: {total_output + total_kernel:,} bytes ({(total_output + total_kernel)/(1024*1024):.2f}MB)")
    
    return c_code


if __name__ == "__main__":
    c_code = main()
