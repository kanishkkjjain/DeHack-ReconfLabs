/**
 * Memory Map for YOLOv3-tiny with Packetization Pipeline
 * DDR Memory Layout - NO DMA, Packetize-then-Convolve Flow
 * 
 * Base: 0x80008000 (32KB offset)
 * Total Available: Typically 512MB - 1GB in Artix-7 designs
 * 
 * FLOW: Input → Packetize → PacketBuffer → IP(PacketBuffer, Kernel, Output) → Wait
 */

#ifndef MEM_MAP_H
#define MEM_MAP_H

#include <stdint.h>

/* ============================================================================
 * DDR MEMORY BASE
 * ============================================================================ */

#define DDR_BASE 0x80008000u

/* ============================================================================
 * PIPELINE BUFFERS (Packetization + Convolution)
 * ============================================================================
 * 
 * PING-PONG Strategy:
 * - PACKET_BUFFER_A: Packetized input for current layer
 * - PACKET_BUFFER_B: Packetized input for next layer
 * - TEMP_BUFFER: Temporary storage for layer outputs
 * - OUTPUT_BUFFER_A: Output of current conv/pool
 * - OUTPUT_BUFFER_B: Output of next conv/pool
 * 
 * Buffers sized for largest packetized layer: 416×416×3 with 3×3 kernel
 * Max packet size: ~5-6 MB for first layer
 */

// Packet buffers (DDR addresses as uint32_t for IP interface)
#define PACKET_BUFFER_A_BASE  (DDR_BASE + 0x00000000u)
#define PACKET_BUFFER_A_SIZE  0x02000000u  // 32 MB for largest packetized layer
#define PACKET_BUFFER_A       ((float*)(PACKET_BUFFER_A_BASE))

#define PACKET_BUFFER_B_BASE  (DDR_BASE + 0x02000000u)
#define PACKET_BUFFER_B_SIZE  0x02000000u  // 32 MB for next packetized layer
#define PACKET_BUFFER_B       ((float*)(PACKET_BUFFER_B_BASE))

// Temporary buffer for layer outputs
#define TEMP_BUFFER_BASE      (DDR_BASE + 0x04000000u)
#define TEMP_BUFFER_SIZE      0x02000000u  // 32 MB for intermediate storage
#define TEMP_BUFFER           ((float*)(TEMP_BUFFER_BASE))

// Output buffers (ping-pong)
#define OUTPUT_BUFFER_A_BASE  (DDR_BASE + 0x06000000u)
#define OUTPUT_BUFFER_A_SIZE  0x02000000u  // 32 MB for conv/pool output
#define OUTPUT_BUFFER_A       ((float*)(OUTPUT_BUFFER_A_BASE))

#define OUTPUT_BUFFER_B_BASE  (DDR_BASE + 0x08000000u)
#define OUTPUT_BUFFER_B_SIZE  0x02000000u  // 32 MB for conv/pool output
#define OUTPUT_BUFFER_B       ((float*)(OUTPUT_BUFFER_B_BASE))

// Input image buffer (416×416×3 float = ~2MB)
#define INPUT_IMAGE_BASE      (DDR_BASE + 0x0A000000u)
#define INPUT_IMAGE_SIZE      0x00200000u  // 2 MB
#define INPUT_IMAGE           ((float*)(INPUT_IMAGE_BASE))

/* ============================================================================
 * YOLO-TINY LAYER OUTPUT MEMORY LAYOUT (Floating Point)
 * ============================================================================
 * 
 * Storage for outputs needed by routing/concatenation layers:
 * - Layer 4 output (route_1): used by layer 11
 * - Layer 7 output (route_2): used by layer 10
 * - Layer 10 output: used by layer 11 after upsample
 * - Detection heads: Layer 9 (13×13) and Layer 12 (26×26)
 */

#define DDR_LAYERS_BASE (DDR_BASE + 0x0A200000u)  // After input image

/* Layer outputs that need to be saved for routing - ADDRESS MACROS */
#define DDR_L4_OUTPUT_ADDR   (DDR_LAYERS_BASE + 0x00000000u)
#define DDR_L4_OUTPUT_SIZE   0x00548000u  // 26×26×256 float32 = 692,224 bytes

#define DDR_L7_OUTPUT_ADDR   (DDR_LAYERS_BASE + 0x000A9000u)
#define DDR_L7_OUTPUT_SIZE   0x0002A400u  // 13×13×256 float32 = 173,056 bytes

#define DDR_L10_OUTPUT_ADDR  (DDR_LAYERS_BASE + 0x000D3400u)
#define DDR_L10_OUTPUT_SIZE  0x00015200u  // 13×13×128 float32 = 86,528 bytes (after conv11)

/* Detection head outputs */
#define DDR_L9_OUTPUT_ADDR   (DDR_LAYERS_BASE + 0x000E8600u)
#define DDR_L9_OUTPUT_SIZE   0x0002A15Cu  // 13×13×255 float32 = 172,380 bytes

#define DDR_L12_OUTPUT_ADDR  (DDR_LAYERS_BASE + 0x0011275Cu)
#define DDR_L12_OUTPUT_SIZE  0x000A8370u  // 26×26×255 float32 = 689,520 bytes

/* Layer outputs as float pointers for convenience */
#define DDR_L4_OUTPUT        ((float*)(DDR_L4_OUTPUT_ADDR))
#define DDR_L7_OUTPUT        ((float*)(DDR_L7_OUTPUT_ADDR))
#define DDR_L9_OUTPUT        ((float*)(DDR_L9_OUTPUT_ADDR))
#define DDR_L10_OUTPUT       ((float*)(DDR_L10_OUTPUT_ADDR))
#define DDR_L12_OUTPUT       ((float*)(DDR_L12_OUTPUT_ADDR))

/* ============================================================================
 * KERNEL STORAGE (Model Weights)
 * ============================================================================
 * 
 * All kernels stored continuously from KERNEL_BASE
 * Each layer's kernels stored sequentially
 * 
 * Layout: For N_out output channels, store N_out kernel sets consecutively
 * Format: [out_ch_0][out_ch_1]...[out_ch_N] where each out_ch is [in_channels×k×k]
 */

#define DDR_KERNELS_BASE (DDR_LAYERS_BASE + 0x001BAACCu)  // After layer outputs

/* Layer kernels (float32) - sizes calculated as: out_ch × in_ch × k × k × 4 bytes */
#define DDR_L0_KERNELS   (DDR_KERNELS_BASE + 0x00000000u)  // 16 × 3 × 3 × 3 × 4 = 1,728 bytes
#define DDR_L2_KERNELS   (DDR_KERNELS_BASE + 0x00000700u)  // 32 × 16 × 3 × 3 × 4 = 18,432 bytes
#define DDR_L4_KERNELS   (DDR_KERNELS_BASE + 0x00005100u)  // 64 × 32 × 3 × 3 × 4 = 73,728 bytes
#define DDR_L6_KERNELS   (DDR_KERNELS_BASE + 0x00017100u)  // 128 × 64 × 3 × 3 × 4 = 294,912 bytes
#define DDR_L8_KERNELS   (DDR_KERNELS_BASE + 0x00059100u)  // 256 × 128 × 3 × 3 × 4 = 1,179,648 bytes
#define DDR_L10_KERNELS  (DDR_KERNELS_BASE + 0x00179100u)  // 512 × 256 × 3 × 3 × 4 = 4,718,592 bytes
#define DDR_L12_KERNELS  (DDR_KERNELS_BASE + 0x005D9100u)  // 1024 × 512 × 3 × 3 × 4 = 18,874,368 bytes
#define DDR_L14_KERNELS  (DDR_KERNELS_BASE + 0x017D9100u)  // 256 × 1024 × 1 × 1 × 4 = 1,048,576 bytes
#define DDR_L16_KERNELS  (DDR_KERNELS_BASE + 0x018D9100u)  // 512 × 256 × 3 × 3 × 4 = 4,718,592 bytes
#define DDR_L18_KERNELS  (DDR_KERNELS_BASE + 0x01D59100u)  // 255 × 512 × 1 × 1 × 4 = 522,240 bytes
#define DDR_L20_KERNELS  (DDR_KERNELS_BASE + 0x01DD8B00u)  // 128 × 256 × 1 × 1 × 4 = 131,072 bytes
#define DDR_L22_KERNELS  (DDR_KERNELS_BASE + 0x01DF8B00u)  // 256 × 384 × 3 × 3 × 4 = 3,538,944 bytes
#define DDR_L24_KERNELS  (DDR_KERNELS_BASE + 0x02158B00u)  // 255 × 256 × 1 × 1 × 4 = 261,120 bytes

/* ============================================================================
 * TRACKING AND STATUS REGIONS
 * ============================================================================ */

#define DDR_TRACKING_BASE    (DDR_KERNELS_BASE + 0x02198B00u)
#define DDR_TRACKING_SIZE    0x00010000u  // 64KB for tracking state

#define DDR_DETECTION_BASE   (DDR_TRACKING_BASE + DDR_TRACKING_SIZE)
#define DDR_DETECTION_SIZE   0x00020000u  // 128KB for detection results (up to 8192 detections)
#define DDR_DETECTION        ((BoundingBox*)(DDR_DETECTION_BASE))

/* ============================================================================
 * TEST STATUS REGISTERS (host-visible)
 * ============================================================================ */

#define STATUS_BASE          (DDR_DETECTION_BASE + DDR_DETECTION_SIZE)
#define STATUS_SCALE_ADDR    ((volatile uint32_t*)(STATUS_BASE + 0x00000000u))
#define STATUS_DDR_ADDR      ((volatile uint32_t*)(STATUS_BASE + 0x00010000u))
#define STATUS_TRACK_ADDR    ((volatile uint32_t*)(STATUS_BASE + 0x00020000u))
#define STATUS_BBOX_ADDR     ((volatile uint32_t*)(STATUS_BASE + 0x00030000u))
#define STATUS_BRAM_ADDR     ((volatile uint32_t*)(STATUS_BASE + 0x00040000u))

/* ============================================================================
 * HELPER MACROS
 * ============================================================================ */

// Calculate packet buffer size for a layer
#define CALC_PACKET_SIZE(h, w, c, k, s, p) \
    ((c) * (((h) + 2 * (p) - (k)) / (s) + 1) * (((w) + 2 * (p) - (k)) / (s) + 1) * (k) * (k) * 4)

// Calculate output size for a layer
#define CALC_OUTPUT_SIZE(h, w, c, s) \
    ((c) * ((h) / (s)) * ((w) / (s)) * 4)

#endif // MEM_MAP_H
