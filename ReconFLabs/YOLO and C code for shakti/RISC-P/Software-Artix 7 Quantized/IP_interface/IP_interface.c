/**
 * IP Interface Implementation for Hardware Acceleration
 */

#include "../include/IP_interface.h"

#define KERNEL_HEADER_BYTES (33u)

/* Execute one output filter (output channel) at a time.
 * Kernel format: [multiplier: 4B] [shift: 4B] [weights: k×k×in_c×1B]
 * Input: int8 activations (all channels, all spatial positions)
 * Output: int8 activations (one output channel: output_h×output_w)
 */
static bool ip_conv_execute_single_channel(
    uint32_t kernel_addr,
    uint32_t kernel_size_single,
    uint32_t input_addr,
    uint32_t input_size,
    uint32_t output_addr,
    int stride,
    int32_t multiplier,
    int32_t shift
) {
    // Set quantization parameters
    *IP_CONV_MULTIPLIER = multiplier;
    *IP_CONV_SHIFT = shift;
    
    // Set kernel address and size (points to int8 weights, after multiplier/shift)
    *IP_CONV_KERNEL_ADDR = kernel_addr + 8;  // Skip 8 bytes (multiplier + shift)
    *IP_CONV_KERNEL_SIZE = kernel_size_single - 8;  // Size of int8 weights only
    
    // Set input/output addresses
    *IP_CONV_INPUT_ADDR = input_addr;
    *IP_CONV_OUTPUT_ADDR = output_addr;
    
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
    uint32_t kernel_addr,
    uint32_t kernel_size,
    uint32_t input_addr,
    uint32_t input_size,
    uint32_t output_addr,
    int stride,
    int input_h,
    int input_w,
    int input_c,
    int output_c,
    int kernel_sz
) {
    (void)kernel_size;

    // Calculate output spatial dimensions
    int output_h = input_h / stride;
    int output_w = input_w / stride;
    
    // Size of one output channel (one filter produces one output channel)
    // Spatial: output_h × output_w, depth: 1 channel per filter
    int output_channel_size = output_h * output_w * 1;  // 1 byte per int8
    
    // Size per filter in quantized format: [multiplier: 4B] [shift: 4B] [weights: k×k×in_c×1B]
    // Example: 3×3×64 kernel = 8 + 576 = 584 bytes per filter
    int kernel_per_filter = 4 + 4 + (kernel_sz * kernel_sz * input_c * 1);  // bytes
    
    // Process one filter at a time (one output channel per filter)
    for (int filter = 0; filter < output_c; filter++) {
        // Memory layout:
        // Kernels: [filter_0 | filter_1 | filter_2 | ... | filter_(N-1)]
        // Each filter: [multiplier: 4B] [shift: 4B] [weights: k×k×in_c×1B]
        // Outputs: [channel_0 | channel_1 | channel_2 | ... | channel_(N-1)]
        uint32_t kernel_offset = KERNEL_HEADER_BYTES + (filter * kernel_per_filter);     // Points to this filter's data
        uint32_t output_offset = filter * output_channel_size;    // Points to this output channel
        
        // Read quantization parameters for this filter
        int32_t* mult_ptr = (int32_t*)(kernel_addr + kernel_offset);
        int32_t* shift_ptr = (int32_t*)(kernel_addr + kernel_offset + 4);
        int32_t multiplier = *mult_ptr;
        int32_t shift = *shift_ptr;
        
        // Execute convolution for this one filter
        // Input:  Complete input activation (all input_c channels, int8)
        // Kernel: This filter's weights (h×w×in_c, int8)
        // Output: One output channel (output_h×output_w, int8)
        if (!ip_conv_execute_single_channel(
            kernel_addr + kernel_offset,  // Address of this filter (includes mult/shift/weights)
            kernel_per_filter,            // Total size: 8 + k×k×in_c×1
            input_addr,                   // Full input activation address (int8)
            input_size,                   // Full input size (all channels × spatial)
            output_addr + output_offset,  // Output location for this channel (int8)
            stride,
            multiplier,                   // Quantization multiplier
            shift                         // Quantization shift
        )) {
            return false;
        }
    }
    
    return true;
}

bool ip_pool_execute(
    uint32_t input_addr,
    uint32_t input_size,
    uint32_t output_addr,
    int stride
) {
    // Set input/output addresses
    *IP_CONV_INPUT_ADDR = input_addr;
    *IP_CONV_OUTPUT_ADDR = output_addr;
    
    // Trigger execution
    *IP_CONV_CONTROL |= IP_CONV_CTRL_START;
    
    // Wait for completion
    if (!ip_wait_complete()) {
        return false;
    }
    
    return true;
}