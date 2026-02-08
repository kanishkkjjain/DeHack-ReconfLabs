/**
 * Memory Map for YOLOv3-tiny Layer Outputs (Layers 0-13)
 * DDR Memory Layout with Tight Packing
 * 
 * Base: 0x80008000 (32KB offset)
 * Total Available: Typically 512MB - 1GB in Artix-7 designs
 */

#ifndef MEM_MAP_H
#define MEM_MAP_H

#include <stdint.h>

/* ============================================================================
 * DDR MEMORY BASE AND RESERVED REGIONS
 * ============================================================================ */

#define DDR_BASE 0x80008000u

// Reserved regions in DDR
#define DDR_TRACK_REGION_SIZE 0x00010000u  // 64KB reserved for tracking/metadata
#define DDR_SRC_REGION_SIZE   0x00080000u  // 512KB source image buffer
#define DDR_GAP_AFTER_SRC     0x00010000u  // 64KB guard gap before destination

// Logical regions
#define DDR_TRACK_REGION_BASE (DDR_BASE)
#define DDR_SRC_REGION_BASE   (DDR_BASE + DDR_TRACK_REGION_SIZE)
#define DDR_DST_REGION_BASE   (DDR_BASE + DDR_TRACK_REGION_SIZE + DDR_SRC_REGION_SIZE + DDR_GAP_AFTER_SRC)

// Image buffer pointers
#define SRC_ADDR ((uint8_t*)(DDR_SRC_REGION_BASE))
#define DST_ADDR ((uint8_t*)(DDR_DST_REGION_BASE))

/* ============================================================================
 * YOLO-TINY LAYER OUTPUT MEMORY LAYOUT (Quantized int8)
 * ============================================================================
 * 
 * Memory allocated starting at DDR_LAYERS_BASE with tight packing.
 * Each layer output buffer allocated sequentially.
 * 
 * Calculation basis: outputs stored as int8 (1 byte per value)
 * Tight packing: no alignment padding, sequential allocation
 */

#define DDR_LAYERS_BASE (DDR_BASE + 0x00200000u)  // Start after status registers

/* Layer 0: Conv/Pool | 416×416×32 int8 */
#define DDR_L0_ADDR      0x80208000u
#define DDR_L0_SIZE      0x00548000u  // 5,537,792 bytes (416×416×32×1)

/* Layer 2: Conv/Pool | 208×208×64 int8 */
#define DDR_L2_ADDR      0x80750000u
#define DDR_L2_SIZE      0x002A4000u  // 2,768,896 bytes (208×208×64×1)

/* Layer 4: Conv/Pool | 104×104×128 int8 */
#define DDR_L4_ADDR      0x809F4000u
#define DDR_L4_SIZE      0x00152000u  // 1,384,448 bytes (104×104×128×1)

/* Layer 6: Conv/Pool | 52×52×256 int8 */
#define DDR_L6_ADDR      0x80B46000u
#define DDR_L6_SIZE      0x000A9000u  // 692,224 bytes (52×52×256×1)

/* Layer 8: Conv/Pool | 26×26×512 int8 */
#define DDR_L8_ADDR      0x80BEF000u
#define DDR_L8_SIZE      0x00054800u  // 346,112 bytes (26×26×512×1)

/* Layer 10: Conv/Pool | 13×13×1024 int8 */
#define DDR_L10_ADDR      0x80C43800u
#define DDR_L10_SIZE      0x0002A400u  // 173,056 bytes (13×13×1024×1)

/* Layer 11: Conv/Pool | 13×13×1024 int8 */
#define DDR_L11_ADDR      0x80C6DC00u
#define DDR_L11_SIZE      0x0002A400u  // 173,056 bytes (13×13×1024×1)

/* Layer 12: Conv/Pool | 13×13×255 int8 */
#define DDR_L12_ADDR      0x80C98000u
#define DDR_L12_SIZE      0x0000A857u  // 43,095 bytes (13×13×255×1)

/* Layer 13: Conv 1×1 (YOLO Head 2) | 26×26×255 int8 */
#define DDR_L13_ADDR     0x80CA2857u
#define DDR_L13_SIZE     0x0002A15Cu  // 172,380 bytes

/* ============================================================================
 * LAYER INPUT POINTERS (for forward pass)
 * ============================================================================
 * 
 * Sequential layers:
 * - Layer N output → Layer N+1 input
 * - Exception: Layer 13 (YOLO head 2) uses Layer 8 output
 */

#define DDR_L0_INPUT     (DDR_SRC_REGION_BASE)  // Original image (416×416×3)
#define DDR_L2_INPUT     (DDR_L0_ADDR)          // L1 (maxpool) output = L0 output reduced
#define DDR_L4_INPUT     (DDR_L2_ADDR)          // L3 (maxpool) output = L2 output reduced
#define DDR_L6_INPUT     (DDR_L4_ADDR)          // L5 (maxpool) output = L4 output reduced
#define DDR_L8_INPUT     (DDR_L6_ADDR)          // L7 (maxpool) output = L6 output reduced
#define DDR_L10_INPUT    (DDR_L8_ADDR)          // L9 (maxpool) output = L8 output reduced
#define DDR_L11_INPUT    (DDR_L10_ADDR)         // L11 input = L10 output
#define DDR_L12_INPUT    (DDR_L11_ADDR)         // L12 input = L11 output
#define DDR_L13_INPUT    (DDR_L8_ADDR)          // L13 input = L8 output (separate head)

/* ============================================================================
 * KERNEL STORAGE (Quantized Model Weights)
 * ============================================================================
 * 
 * All kernels stored continuously from KERNEL_BASE
 * Format per filter: [multiplier: 4B] [shift: 4B] [weights: k×k×in_c×1B]
 * 
 * Layout: For N_out output channels, store N_out filter sets consecutively
 */

#define DDR_KERNELS_FILE_BASE 0x80CCC9B3u  // File header starts here (4 bytes)
#define DDR_KERNELS_BASE      0x80CCC9B7u  // First kernel header starts after file header

/* Layer kernels (quantized: header + mult + shift + int8 weights) */
#define DDR_L0_KERNELS  0x80CCC9B7u  // 1,153 bytes (header 33 + 32 filters × 35 bytes)
#define DDR_L2_KERNELS  0x80CCCE38u  // 18,977 bytes (header 33 + 64 filters × 296 bytes)
#define DDR_L4_KERNELS  0x80CD1859u  // 74,785 bytes (header 33 + 128 filters × 584 bytes)
#define DDR_L6_KERNELS  0x80CE3C7Au  // 296,993 bytes (header 33 + 256 filters × 1160 bytes)
#define DDR_L8_KERNELS  0x80D2C49Bu  // 1,183,777 bytes (header 33 + 512 filters × 2312 bytes)
#define DDR_L10_KERNELS 0x80E4C4BCu  // 4,726,817 bytes (header 33 + 1024 filters × 4616 bytes)
#define DDR_L11_KERNELS 0x812CE4DDu  // 9,445,409 bytes (header 33 + 1024 filters × 9224 bytes)
#define DDR_L12_KERNELS 0x81BD04FEu  // 263,193 bytes (header 33 + 255 filters × 1032 bytes)
#define DDR_L13_KERNELS 0x81C10917u  // 132,633 bytes (header 33 + 255 filters × 520 bytes)

/* ============================================================================
 * TEST STATUS REGISTERS (host-visible)
 * ============================================================================ */

// Test status registers (host-visible)
#define STATUS_SCALE_ADDR  ((volatile uint32_t*)(DDR_BASE + 0x00200000u))
#define STATUS_DDR_ADDR    ((volatile uint32_t*)(DDR_BASE + 0x00210000u))
#define STATUS_TRACK_ADDR  ((volatile uint32_t*)(DDR_BASE + 0x00220000u))
#define STATUS_BBOX_ADDR   ((volatile uint32_t*)(DDR_BASE + 0x00230000u))
#define STATUS_BRAM_ADDR   ((volatile uint32_t*)(DDR_BASE + 0x00240000u))

/* ============================================================================
 * HELPER MACROS FOR LAYER ACCESS
 * ============================================================================ */

#define GET_LAYER_OUTPUT_ADDR(layer) \
    ((layer) == 0 ? DDR_L0_ADDR : \
     (layer) == 2 ? DDR_L2_ADDR : \
     (layer) == 4 ? DDR_L4_ADDR : \
     (layer) == 6 ? DDR_L6_ADDR : \
     (layer) == 8 ? DDR_L8_ADDR : \
     (layer) == 10 ? DDR_L10_ADDR : \
     (layer) == 11 ? DDR_L11_ADDR : \
     (layer) == 12 ? DDR_L12_ADDR : \
     (layer) == 13 ? DDR_L13_ADDR : 0)

#define GET_LAYER_INPUT_ADDR(layer) \
    ((layer) == 0 ? DDR_L0_INPUT : \
     (layer) == 2 ? DDR_L2_INPUT : \
     (layer) == 4 ? DDR_L4_INPUT : \
     (layer) == 6 ? DDR_L6_INPUT : \
     (layer) == 8 ? DDR_L8_INPUT : \
     (layer) == 10 ? DDR_L10_INPUT : \
     (layer) == 11 ? DDR_L11_INPUT : \
     (layer) == 12 ? DDR_L12_INPUT : \
     (layer) == 13 ? DDR_L13_INPUT : 0)

#define GET_LAYER_KERNELS(layer) \
    ((layer) == 0 ? DDR_L0_KERNELS : \
     (layer) == 2 ? DDR_L2_KERNELS : \
     (layer) == 4 ? DDR_L4_KERNELS : \
     (layer) == 6 ? DDR_L6_KERNELS : \
     (layer) == 8 ? DDR_L8_KERNELS : \
     (layer) == 10 ? DDR_L10_KERNELS : \
     (layer) == 11 ? DDR_L11_KERNELS : \
     (layer) == 12 ? DDR_L12_KERNELS : \
     (layer) == 13 ? DDR_L13_KERNELS : 0)

#endif // MEM_MAP_H
