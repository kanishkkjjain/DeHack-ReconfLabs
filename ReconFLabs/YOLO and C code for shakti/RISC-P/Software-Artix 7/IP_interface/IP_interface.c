/**
 * IP Interface Implementation for Hardware Acceleration
 */

#include "../include/IP_interface.h"

/* Execute one output filter (output channel) at a time.
 * Kernel: h×w×input_c (per-filter)
 * Input: All channels, all spatial positions
 * Output: One output channel (output_h×output_w)
 */
static bool ip_conv_execute_single_channel(
    const float *kernel_addr,
    uint32_t kernel_size_single,
    const float *input_addr,
    uint32_t input_size,
    float *output_addr,
    int stride
) {
    // Set kernel address and size
    *IP_CONV_KERNEL_ADDR = (uint32_t)(uintptr_t)kernel_addr;
    *IP_CONV_KERNEL_SIZE = kernel_size_single;
    
    // Set input/output addresses
    *IP_CONV_INPUT_ADDR = (uint32_t)(uintptr_t)input_addr;
    *IP_CONV_OUTPUT_ADDR = (uint32_t)(uintptr_t)output_addr;
    
    // Trigger execution
    *IP_CONV_CONTROL |= IP_CONV_CTRL_START;
    
    // Wait for completion
    if (!ip_wait_complete()) {
        return false;
    }
    
    return true;
}

bool ip_is_busy(void) {
    return (*IP_CONV_STATUS & IP_CONV_STAT_BUSY) != 0;
}

bool ip_wait_complete(void) {
    uint32_t timeout = 5000;
    while (ip_is_busy() && timeout-- > 0) {
        volatile int delay = 10;
        while (delay--);
    }
    if (timeout == 0) return false;
    return (*IP_CONV_STATUS & IP_CONV_STAT_ERROR) == 0;
}

bool ip_conv_execute(
    const float *kernel_addr,
    uint32_t kernel_size,
    const float *input_addr,
    uint32_t input_size,
    float *output_addr,
    int stride,
    int input_h,
    int input_w,
    int input_c,
    int output_c,
    int kernel_sz
) {
    // Calculate output spatial dimensions
    int output_h = input_h / stride;
    int output_w = input_w / stride;
    
    // Size of one output channel (one filter produces one output channel)
    // Spatial: output_h × output_w, depth: 1 channel per filter
    int output_channel_size = output_h * output_w * 4;  // 4 bytes per float32
    
    // Size of kernel weights for ONE filter (one output channel)
    // Kernel format: h × w × input_channels
    // Example: 3×3×64 kernel = 576 weights per filter
    int kernel_per_filter = kernel_sz * kernel_sz * input_c * 4;  // bytes
    
    // Process one filter at a time (one output channel per filter)
    for (int filter = 0; filter < output_c; filter++) {
        // Memory layout:
        // Kernels: [filter_0 | filter_1 | filter_2 | ... | filter_(N-1)]
        // Outputs: [channel_0 | channel_1 | channel_2 | ... | channel_(N-1)]
        uint32_t kernel_offset = filter * kernel_per_filter;     // Points to this filter's kernel
        uint32_t output_offset = filter * output_channel_size;    // Points to this output channel
        
        // Execute convolution for this one filter
        // Input:  Complete input activation (all input_c channels, all spatial positions)
        // Kernel: This filter's weights (h×w×input_c weights, float32)
        // Output: One output channel (output_h×output_w values, float32)
        if (!ip_conv_execute_single_channel(
            (const float *)((const uint8_t *)kernel_addr + kernel_offset),
            kernel_per_filter,            // Size in bytes: h×w×input_c×4
            input_addr,                   // Full input activation address
            input_size,                   // Full input size (all channels × spatial)
            (float *)((uint8_t *)output_addr + output_offset),
            stride
        )) {
            return false;
        }
    }
    
    return true;
}

bool ip_pool_execute(
    const float *input_addr,
    uint32_t input_size,
    float *output_addr,
    int stride
) {
    // Set input/output addresses
    *IP_CONV_INPUT_ADDR = (uint32_t)(uintptr_t)input_addr;
    *IP_CONV_OUTPUT_ADDR = (uint32_t)(uintptr_t)output_addr;
    
    // Trigger execution
    *IP_CONV_CONTROL |= IP_CONV_CTRL_START;
    
    // Wait for completion
    if (!ip_wait_complete()) {
        return false;
    }
    
    return true;
}