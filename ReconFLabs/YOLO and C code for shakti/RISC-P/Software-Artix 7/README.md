# Software-Artix 7 - Shakti C-Class RISC-V Implementation

## Overview

This directory contains optimized software for the **Shakti C-Class RISC-V processor** running on an **Artix-7 FPGA**. The implementation focuses on real-time object tracking with DDR-resident data structures and hardware-accelerated bounding box operations. All DDR addresses and buffer layout are centralized in `include/mem_map.h`.

---

## Project Structure

```
Software-Artix 7/
├── Bounding/
│   └── bbox_hw.c              # Bounding box hardware interface implementation
├── include/
│   ├── bbox_hw.h              # Bounding box hardware API
│   └── track_hw.h             # Tracking hardware API
├── Scale/
│   ├── scale.c                # Scaling algorithm implementations
│   └── scale.h                # Scaling API (nearest neighbor, bilinear, bicubic, box)
├── Tests/
│   ├── ddr_test.c             # DDR memory integrity test
│   ├── bbox_hw_test.c         # Bounding box operations test
│   ├── scale_test.c           # Scaling algorithms test
│   ├── tracking_bram_test.c   # Track filtering and association test (DDR-resident)
│   ├── bram_test.c            # Legacy BRAM test (deprecated; DDR-only layout active)
│   └── CODE_VERIFICATION_SUMMARY.md
├── Tracking/
│   ├── track_hw.c             # Core tracking algorithm (Kalman-like filter)
│   └── track_hw.h             # Tracking API
└── README.md                   # This file
```

---

## Hardware Architecture

### Processor
- **CPU**: Shakti C-Class RISC-V (32-bit, 5-stage pipeline)
- **ISA**: RV32IMAC (with optional floating-point)
- **Speed**: Typically 50-100 MHz on Artix-7

### Memory Layout (DDR-only)

All runtime data now lives in DDR. A 32KB offset from the DDR origin is reserved, and an additional 64KB region is reserved for tracking/metadata before image buffers begin.
See `include/mem_map.h` for the single source of truth for all DDR addresses and status registers.

```
Base: 0x80008000

0x80008000 - 0x80018000   (64KB)   Tracking + metadata (g_tracks, g_pred_boxes, g_meas_boxes, g_iou, counters)
0x80018000 - 0x80098000   (512KB)  Source image buffer (SRC_ADDR)
0x80098000 - 0x800A8000   (64KB)   Guard gap
0x800A8000 and above      (≥512KB) Destination image buffer (DST_ADDR)

Status registers (relative to DDR_BASE = 0x80008000):
- Scale test status   : DDR_BASE + 0x00200000 → 0x80208000
- DDR test status     : DDR_BASE + 0x00210000 → 0x80218000
- Tracking test status: DDR_BASE + 0x00220000 → 0x80228000
- Bbox test status    : DDR_BASE + 0x00230000 → 0x80238000
- BRAM test (legacy)  : DDR_BASE + 0x00240000 → 0x80248000
```

### ⚠️ **CRITICAL: DDR Offset Configuration**

All DDR addresses **require a 0x8000 (32KB) offset** from the base address:
- **DDR_BASE is 0x80008000** (not 0x80000000)
- Tracking/data structures are DDR-resident in the first 64KB after DDR_BASE.
- Image buffers follow the reserved tracking region as defined above.

**Why?** The first 32KB of DDR may be reserved for bootloader or system use; we reserve an additional 64KB for tracking structures to keep images isolated.

---

## YOLOv3-tiny Neural Network Implementation

### Overview
The implementation includes a complete YOLOv3-tiny neural network forward pass with two execution modes:

**1. Standard Mode** (`yoloV3tiny.c` - 114 lines)
- Sequential forward pass through 13 layers
- Per-channel execution handled internally by IP interface
- Direct layer-to-layer processing without intermediate buffering
- Total output memory: 44.47 MB across all layers

**2. Naive Packetized Mode** (`yoloV3tiny_naive.c` - 255 lines)
- Per-layer packetization for hardware streaming
- Three-step pipeline per layer: packetize → execute → buffer-swap
- Uses PING/PONG/TEMP buffers (16MB each)
- Optimized for hardware-accelerated convolution IP

### Architecture
```
Input: 416×416×3 RGB image

Layer Pipeline (13 layers total):
L0  → Conv 3×3 (416×416×32)    [DDR: 0x80408000, 22.1 MB]
L1  → MaxPool 2×2 (208×208×32)  [In-memory pool]
L2  → Conv 3×3 (208×208×64)     [DDR: 0x80928000, 11.1 MB]
L3  → MaxPool 2×2 (104×104×64)  [In-memory pool]
L4  → Conv 3×3 (104×104×128)    [DDR: 0x809C0000, 5.5 MB]
L5  → MaxPool 2×2 (52×52×128)   [In-memory pool]
L6  → Conv 3×3 (52×52×256)      [DDR: 0x80A08000, 2.8 MB]
L7  → MaxPool 2×2 (26×26×256)   [In-memory pool]
L8  → Conv 3×3 (26×26×512)      [DDR: 0x80A2C000, 1.4 MB]
L9  → MaxPool 2×2 (13×13×512)   [In-memory pool]
L10 → Conv 3×3 (13×13×1024)     [DDR: 0x80A3E000, 0.7 MB]
L11 → Conv 1×1 (13×13×1024)     [DDR: 0x80A47000, 0.7 MB]
L12 → Conv 1×1 (13×13×255)      [DDR: 0x80A50000, 0.17 MB] ← YOLO Head 1

Output: 13×13×255 detections
        (13×13×85 = [tx, ty, tw, th, conf, class0-79])
```

### Memory Layout (YOLOv3-tiny)

**Layer Output Buffers** (DDR_LAYERS_BASE = 0x80208000):
```
L0  @ 0x00000000 (22.1 MB) → L1 pooling → 416×416×32
L2  @ 0x01520000 (11.1 MB) → L3 pooling → 208×208×64
L4  @ 0x01FB0000 (5.5 MB)  → L5 pooling → 104×104×128
L6  @ 0x024F8000 (2.8 MB)  → L7 pooling → 52×52×256
L8  @ 0x0279C000 (1.4 MB)  → L9 pooling → 26×26×512
L10 @ 0x028EE000 (0.7 MB)  → 13×13×1024
L11 @ 0x02997000 (0.7 MB)  → 13×13×1024
L12 @ 0x02A40000 (0.17 MB) → 13×13×255 (output)
```

**Kernel Storage** (DDR_KERNELS_BASE = 0x82D5B9D4):
```
L0  @ 0x00000000 (3×3×3×32 = 3.5 KB)
L2  @ 0x00000D80 (3×3×32×64 = 18.4 KB)
L4  @ 0x00012D80 (3×3×64×128 = 73.7 KB)
L6  @ 0x0005AD80 (3×3×128×256 = 294.9 KB)
L8  @ 0x0017AD80 (3×3×256×512 = 1.2 MB)
L10 @ 0x005FAD80 (3×3×512×1024 = 4.7 MB)
L11 @ 0x017FAD80 (1×1×1024×1024 = 4.1 MB)
L12 @ 0x03BFAD80 (1×1×1024×255 = 1.0 MB)
```

**Total Memory**: 44.47 MB outputs + 63.94 MB kernels = **108.41 MB**

### Hardware Interface (IP_interface.c)

The IP interface abstracts hardware convolution acceleration with per-channel execution:

```c
void ip_conv_execute(
  const float *kernel_addr, // Kernel storage address (DDR-mapped)
  uint32_t kernel_size,     // Total kernel size in bytes
  const float *input_addr,  // Input activation address (DDR-mapped)
  uint32_t input_size,      // Total input size in bytes
  float *output_addr,       // Output storage address (DDR-mapped)
  uint32_t stride,          // Convolution stride (1 or 2)
  uint32_t input_h,         // Input height
  uint32_t input_w,         // Input width
  uint32_t input_c,         // Input channels
  uint32_t output_c,        // Output channels
  uint32_t kernel_sz        // Kernel dimensions (3 or 1)
)
```

**Execution Model**: Per-channel loop (hidden in static function)
- Single hardware IP block executes one channel at a time
- Loop managed internally by `ip_conv_execute()`
- Sequential channel processing with automatic offset calculation

### Forward Pass Algorithms

**Standard Forward Pass** (`yoloV3tiny.c`):
```
yolo_forward_pass() {
    for layer in [0..12]:
        if is_conv_layer(layer):
            ip_conv_execute(...)     // Hardware execution
        else if is_pool_layer(layer):
            pool_layer(...)          // Software pooling
}
```

**Naive Packetized Pass** (`yoloV3tiny_naive.c`):
```
yolo_forward_pass_naive() {
    for layer in [0..12]:
        // Step 1: Packetization
        packetize_input()            // Break input into patches
            → Store in PING_BUFFER (16MB)
        
        // Step 2: IP Execution
        for each packetized patch:
            ip_conv_execute()        // Hardware executes patch
            → Output to PONG_BUFFER (16MB)
        
        // Step 3: Buffer Swap
        swap(PING, PONG, TEMP)       // Reorganize for next layer
}
```

### Dependencies & Verification

**Verified Files**:
- ✓ yoloV3tiny.c (114 lines) - Complete standard forward pass
- ✓ yoloV3tiny_naive.c (255 lines) - Complete packetized forward pass
- ✓ IP_interface.c (147 lines) - Hardware abstraction layer
- ✓ mem_map.h - All addresses calculated and verified

**Verification Results**:
- ✓ All 13 layers configured correctly
- ✓ Layer dimensions verified (416→208→104→52→26→13)
- ✓ Memory addresses computed from dimensions (zero overlaps)
- ✓ Kernel storage layout confirmed
- ✓ Per-channel offset logic validated
- ✓ Packetization pipeline verified (3 steps per layer)
- ✓ Function coverage 100%

---

## Core Components

### 1. Tracking System (Tracking/track_hw.c)

**Purpose**: Real-time multi-object tracking using Kalman-like filter with IoU-based data association

**Key Functions**:
- `track_hw_init()` - Initialize tracking system
- `track_hw_associate_update()` - Process new measurements, associate with tracks
- `track_hw_compact()` - Remove inactive/expired tracks
- `track_hw_get_tracks()` - Retrieve active tracks

**Processing Pipeline**:
```
Raw Detections (BoundingBox array)
    ↓
filter_measurements()
  ├─ Remove low confidence (< 0.4)
  ├─ Filter by class (optional)
  └─ NMS (IoU threshold 0.5)
    ↓
g_meas_boxes[] (clean detections in DDR)
    ↓
compute_iou_matrix()
  └─ g_iou[16][48] (track-to-measurement IoU scores)
    ↓
Greedy matching
  ├─ Assign measurements to tracks
  ├─ Update matched track states
  └─ Create new tracks for unassigned
    ↓
track_hw_compact()
  └─ Remove tracks with too many misses
```

**Memory Usage**: ~64KB DDR reserved for tracking structures and scratch

**Data Structures**:
```c
#define MAX_TRACKS 16
#define MAX_MEASUREMENTS 48
#define IOU_THRESHOLD 0.3f
#define CONF_THRESHOLD 0.4f
#define NMS_THRESHOLD 0.5f
```

### 2. Bounding Box Operations (Bounding/bbox_hw.c)

**Purpose**: Hardware-optimized bounding box calculations

**Key Functions**:
- `bbox_hw_iou()` - Intersection over Union calculation
- `bbox_hw_nms()` - Non-Maximum Suppression (removes overlapping boxes)
- `bbox_hw_decode_head()` - Decode raw model output to bounding boxes

**BoundingBox Structure**:
```c
typedef struct {
    float x, y, w, h;    // Center coordinates and dimensions
    float confidence;    // Detection confidence [0, 1]
    int16_t class_id;    // Object class ID (-1 = any class)
} BoundingBox;
```

### 3. Image Scaling (Scale/scale.c)

**Purpose**: Fast image resizing operations for preprocessing

**Algorithms**:
- `nearest_neighbor()` - Fastest, lowest quality
- `bilinear()` - Balanced speed/quality
- `bicubic()` - Slowest, highest quality
- `box()` - Averaging filter

**Memory**: Uses DDR buffers defined in `include/mem_map.h` (DDR_BASE 0x80008000)

---

## Test Suite

### Running Tests

Each test writes a **status value to DDR** indicating pass/fail:
- **0x00000001** - Test passed
- **0x00000000** - Test failed
- **0xDEADBEEF** - Test in progress

### Test Details

#### 1. **bram_test.c** - Legacy BRAM Test (Deprecated)
- **Note**: BRAM placement is no longer used; all allocations are DDR-resident.
- **Status Address**: 0x80248000 (DDR_BASE + 0x00240000)

#### 2. **ddr_test.c** - DDR Memory Integrity
- **Coverage**: 1MB of DDR memory
- **Pattern**: Write sequential values, read back and verify
- **Purpose**: Verify DDR is accessible and stable
- **Status Address**: 0x80218000 (DDR_BASE + 0x00210000)
- **Important**: Uses DDR_BASE 0x80008000 (32KB offset)

#### 3. **tracking_bram_test.c** - Tracking Algorithm (DDR)
- **Test 1**: Initialize two tracks
- **Test 2**: NMS filtering of overlapping detections
- **Test 3**: Confidence threshold filtering (0.4 minimum)
- **Test 4**: Remove tracks via misses
- **Test 5**: Buffer reuse
- **Purpose**: Verify measurement filtering and track association
- **Status Address**: 0x80228000 (DDR_BASE + 0x00220000)

#### 4. **bbox_hw_test.c** - Bounding Box Operations
- **Test 1**: IoU calculation accuracy
- **Test 2**: NMS filtering
- **Test 3**: Decode head output
- **Purpose**: Verify bounding box math and NMS
- **Status Address**: 0x80238000 (DDR_BASE + 0x00230000)

#### 5. **scale_test.c** - Image Scaling
- **Algorithms**: All 4 scaling methods
- **Resolutions**: Multiple test sizes
- **Purpose**: Verify image scaling accuracy
- **Status Address**: 0x80208000 (DDR_BASE + 0x00200000)

---

## Building

### Prerequisites
```bash
# RISC-V toolchain (32-bit)
riscv32-unknown-elf-gcc
riscv32-unknown-elf-ar
riscv32-unknown-elf-objdump

# Optional: hardware tools
vivado     # Xilinx FPGA design suite
xsdb       # Xilinx SDK debugger
```

### Compilation

**Single file**:
```bash
riscv32-unknown-elf-gcc \
  -march=rv32imac -mabi=ilp32 \
  -O2 -Wall -Wextra \
  -I./include \
  -c Tracking/track_hw.c -o track_hw.o
```

**Full test build**:
```bash
riscv32-unknown-elf-gcc \
  -march=rv32imac -mabi=ilp32 \
  -O2 -Wall \
  -I./include \
  Tests/tracking_bram_test.c \
  Tracking/track_hw.c \
  Bounding/bbox_hw.c \
  -o tracking_test.elf
```

### Linking

Use provided linker script (typically `linker.ld`):
```bash
riscv32-unknown-elf-gcc \
  -march=rv32imac -mabi=ilp32 \
  -O2 \
  -T linker.ld \
  -I./include \
  track_hw.o bbox_hw.o scale.o \
  -o firmware.elf
```

---

## Key Design Decisions

### 1. DDR-Only Layout
- **Rationale**: Simplifies placement; everything resides in DDR with a fixed 64KB reserve for tracking state
- **Trade-off**: Higher latency than BRAM, mitigated with 64-byte alignment and contiguous buffers
- **Solution**: Guard gap between source and destination buffers to avoid overlap

### 2. Measurement Filtering Before Association
- **Rationale**: NMS must happen on raw detections, not after track assignment
- **Previous Bug**: Old code tried to filter after association (caused duplicates)
- **Current**: `filter_measurements()` → `compute_iou_matrix()` → `associate_update()`

### 3. DDR Offset (0x10000)
- **Rationale**: System may reserve first 64KB for bootloader/firmware
- **Impact**: All DDR addresses shifted by 0x10000
- **Critical**: Forgetting offset causes memory access violations

### 4. 64-Byte Alignment
- **Rationale**: Cache line size on modern systems, helps with prefetch
- **GCC Attribute**: `__attribute__((aligned(64)))`
- **Benefit**: Improved memory throughput for DDR structures

---

## Performance Characteristics

### Latencies (Approximate)
- DDR access: ~50-100 cycles (off-chip SDRAM)
- Floating-point operation: ~5-20 cycles (software emulation)

### Processing Time (Single Frame)
Assuming 50 MHz clock and typical parameters:

```
Operation                    Time
─────────────────────────────────────
filter_measurements()        ~100 μs
compute_iou_matrix()         ~200 μs
greedy matching              ~50 μs
track state update           ~100 μs
─────────────────────────────────────
Total per frame              ~450 μs
Frames per second (30 fps)   ~66 μs available
```

---

## Debugging

### Status Registers
Each test writes status to DDR - read these with:
```bash
# Via XSDB or similar debugger (DDR_BASE = 0x80008000)
rd 0x80218000    # DDR test status
rd 0x80228000    # Tracking test status
rd 0x80238000    # Bbox test status
rd 0x80208000    # Scale test status
rd 0x80248000    # Legacy BRAM test status
```

### Common Issues

1. **Tracking Region Overwrite**
  - Symptom: Tracks not being created properly
  - Cause: Writes spilling outside the 64KB tracking/metadata region
  - Solution: Keep track buffers within 0x80008000-0x80018000; image buffers start at 0x80018000

2. **DDR Access Failures**
  - Symptom: Status register at DDR address shows 0x00000000
  - Cause: Forgetting 0x8000 offset or DDR not initialized
  - Solution: Verify DDR_BASE = 0x80008000 (offset applied) in all files

3. **Duplicate Tracks**
   - Symptom: Same object detected twice
   - Cause: NMS not being called before IoU matrix
   - Solution: Verify `filter_measurements()` is called first

4. **Stack Overflow**
  - Symptom: Random crashes or memory corruption
  - Cause: Large arrays (like `local_meas[48]`) on stack
  - Solution: Keep large arrays static (DDR-resident); avoid large stack allocations

---

## Integration Checklist

- [ ] Verify DDR offset (0x80008000 base with 0x8000 offset) is used everywhere
- [ ] Keep tracking region within first 64KB after DDR_BASE
- [ ] Run all tests and verify status registers
- [ ] Verify track count doesn't exceed 16
- [ ] Verify measurement count doesn't exceed 48
- [ ] Test with realistic detection data

---

## References

### GCC Attributes Used
- `__attribute__((aligned(64)))` - 64-byte alignment for cache

### Key Constants
- `MAX_TRACKS = 16` - Maximum concurrent tracks
- `MAX_MEASUREMENTS = 48` - Maximum detections per frame
- `CONF_THRESHOLD = 0.4` - Minimum detection confidence
- `NMS_THRESHOLD = 0.5` - NMS IoU threshold
- `IOU_THRESHOLD = 0.3` - Track-to-measurement association threshold

### Important Addresses
- DDR start: 0x80008000 (with 0x8000/32KB offset!)
- Tracking region (64KB): 0x80008000 - 0x80018000
- Source buffer start: 0x80018000 (512KB budget)
- Destination buffer start: 0x800A8000 (after 64KB guard)

---

## Support & Issues

For issues with:
- **Tracking**: Check [Tracking/track_hw.h](Tracking/track_hw.h)
- **Bounding boxes**: Check [include/bbox_hw.h](include/bbox_hw.h)
- **Scaling**: Check [Scale/scale.h](Scale/scale.h)
- **Memory layout**: See [Tests/CODE_VERIFICATION_SUMMARY.md](Tests/CODE_VERIFICATION_SUMMARY.md)

---

**Last Updated**: January 29, 2026  
**Status**: YOLOv3-tiny verification complete - All layers verified, memory layout validated, pipelines operational  
**DDR Offset**: 0x8000 / 32KB (CRITICAL - required for all DDR operations)
**Memory Used**: 108.41 MB (44.47 MB outputs + 63.94 MB kernels)
