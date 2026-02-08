#ifndef IP_INTERFACE_H
#define IP_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

/* Hardware IP Register Definitions */

#define IP_CONV_BASE 0x80000000u

/* Control/Status Register Offsets */
#define IP_CONV_CONTROL_OFFSET      0x00u
#define IP_CONV_STATUS_OFFSET       0x04u
#define IP_CONV_INPUT_ADDR_OFFSET   0x10u
#define IP_CONV_OUTPUT_ADDR_OFFSET  0x14u
#define IP_CONV_KERNEL_ADDR_OFFSET  0x18u
#define IP_CONV_KERNEL_SIZE_OFFSET  0x1Cu

/* Register Pointers */
#define IP_CONV_CONTROL     ((volatile uint32_t*)(IP_CONV_BASE + IP_CONV_CONTROL_OFFSET))
#define IP_CONV_STATUS      ((volatile uint32_t*)(IP_CONV_BASE + IP_CONV_STATUS_OFFSET))
#define IP_CONV_INPUT_ADDR  ((volatile uint32_t*)(IP_CONV_BASE + IP_CONV_INPUT_ADDR_OFFSET))
#define IP_CONV_OUTPUT_ADDR ((volatile uint32_t*)(IP_CONV_BASE + IP_CONV_OUTPUT_ADDR_OFFSET))
#define IP_CONV_KERNEL_ADDR ((volatile uint32_t*)(IP_CONV_BASE + IP_CONV_KERNEL_ADDR_OFFSET))
#define IP_CONV_KERNEL_SIZE ((volatile uint32_t*)(IP_CONV_BASE + IP_CONV_KERNEL_SIZE_OFFSET))

#define IP_CONV_CTRL_START          (1u << 0)
#define IP_CONV_CTRL_ENABLE         (1u << 1)
#define IP_CONV_STAT_BUSY           (1u << 0)
#define IP_CONV_STAT_DONE           (1u << 1)
#define IP_CONV_STAT_ERROR          (1u << 2)

//TODO: QUANTIZED MODEL REQUIRED MULTIPLIER AND SHIFT VALUE

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
);

bool ip_pool_execute(
    const float *input_addr,
    uint32_t input_size,
    float *output_addr,
    int stride
);

bool ip_is_busy(void);

bool ip_wait_complete(void);

#endif // IP_INTERFACE_H
