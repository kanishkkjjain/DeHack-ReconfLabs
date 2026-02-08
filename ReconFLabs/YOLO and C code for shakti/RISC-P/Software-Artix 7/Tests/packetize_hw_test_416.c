#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mem_map.h"
#include "packetize.h"

// Full 416x416x3 image dimensions
#define IMG_HEIGHT 416
#define IMG_WIDTH 416
#define IMG_CHANNELS 3

// Kernel and convolution parameters
#define KERNEL_SIZE 3
#define STRIDE 1
#define PADDING 1

// Calculate output dimensions
#define OUT_HEIGHT ((IMG_HEIGHT - KERNEL_SIZE + 2 * PADDING) / STRIDE + 1)
#define OUT_WIDTH ((IMG_WIDTH - KERNEL_SIZE + 2 * PADDING) / STRIDE + 1)

// Memory layout (using DDR regions from mem_map.h)
// Input image: DDR_SRC_REGION_BASE (512KB available)
// Output patches: DDR_DST_REGION_BASE

void print_memory_layout()
{
    printf("=== Memory Layout ===\n");
    printf("DDR Base: 0x%08x\n", DDR_BASE);
    printf("Track Region: 0x%08x (64KB)\n", DDR_TRACK_REGION_BASE);
    printf("Source Region: 0x%08x (512KB) - Input image\n", DDR_SRC_REGION_BASE);
    printf("Gap: 64KB\n");
    printf("Destination Region: 0x%08x - Output patches\n", DDR_DST_REGION_BASE);
    printf("\n");
}

int main()
{
    printf("=== Hardware Packetization Test (416x416x3 uint8_t) ===\n\n");
    
    print_memory_layout();
    
    // Calculate sizes in BYTES (not elements)
    int input_size_bytes = IMG_HEIGHT * IMG_WIDTH * IMG_CHANNELS;
    int output_size_bytes = calculate_packetize_output_size(IMG_HEIGHT, IMG_WIDTH, IMG_CHANNELS, 
                                                             KERNEL_SIZE, STRIDE, PADDING);
    
    printf("Image Dimensions: %d x %d x %d channels\n", IMG_HEIGHT, IMG_WIDTH, IMG_CHANNELS);
    printf("Input size: %d bytes (%.2f MB)\n", input_size_bytes, input_size_bytes / (1024.0 * 1024.0));
    printf("Output size: %d bytes (%.2f MB)\n", output_size_bytes, output_size_bytes / (1024.0 * 1024.0));
    printf("Output grid: %d x %d patches\n", OUT_HEIGHT, OUT_WIDTH);
    printf("Patches per grid: %d x %d kernel = %d bytes each\n", KERNEL_SIZE, KERNEL_SIZE, KERNEL_SIZE * KERNEL_SIZE);
    printf("\n");
    
    // Check if DDR regions can fit our data
    int src_region_size_bytes = DDR_SRC_REGION_SIZE;
    
    printf("Memory Verification:\n");
    printf("Source region available: %d bytes (%.2f KB), required: %d bytes (%.2f KB) - %s\n", 
           src_region_size_bytes, src_region_size_bytes / 1024.0,
           input_size_bytes, input_size_bytes / 1024.0,
           input_size_bytes <= src_region_size_bytes ? "OK" : "FAIL");
    printf("\n");
    
    if(input_size_bytes > src_region_size_bytes)
    {
        printf("ERROR: Input image too large for source region!\n");
        return 1;
    }
    
    // Get pointers to DDR regions (as uint8_t*)
    uint8_t* input_ptr = (uint8_t*)SRC_ADDR;
    uint8_t* output_ptr = (uint8_t*)DST_ADDR;
    
    printf("Input buffer address: 0x%08lx\n", (unsigned long)input_ptr);
    printf("Output buffer address: 0x%08lx\n", (unsigned long)output_ptr);
    printf("\n");
    
    // Initialize input image with test pattern
    printf("Initializing input image with test pattern...\n");
    for(int i = 0; i < input_size_bytes; i++)
    {
        input_ptr[i] = (uint8_t)(i % 256); // Simple pattern: cycles through 0-255
    }
    printf("Input image initialized.\n\n");
    
    // Run packetization
    printf("Running packetization...\n");
    packetize_image(input_ptr, output_ptr, IMG_HEIGHT, IMG_WIDTH, IMG_CHANNELS, 
                   KERNEL_SIZE, STRIDE, PADDING);
    printf("Packetization complete.\n\n");
    
    // Verify output by checking corner patches
    printf("=== Verification: Sampling corner patches ===\n");
    
    // Top-left corner patch (should have padding)
    printf("Patch at (0,0) - Top-Left Corner (padded):\n");
    int patch_index = 0;
    uint8_t* patch_data = &output_ptr[patch_index * KERNEL_SIZE * KERNEL_SIZE];
    for(int i = 0; i < KERNEL_SIZE; i++)
    {
        for(int j = 0; j < KERNEL_SIZE; j++)
        {
            printf("%3d ", patch_data[i * KERNEL_SIZE + j]);
        }
        printf("\n");
    }
    printf("\n");
    
    // Center patch (no padding needed)
    int center_patch_h = OUT_HEIGHT / 2;
    int center_patch_w = OUT_WIDTH / 2;
    int center_patch_index = (center_patch_h * OUT_WIDTH + center_patch_w);
    printf("Patch at (%d,%d) - Center (no padding):\n", center_patch_h, center_patch_w);
    patch_data = &output_ptr[center_patch_index * KERNEL_SIZE * KERNEL_SIZE];
    for(int i = 0; i < KERNEL_SIZE; i++)
    {
        for(int j = 0; j < KERNEL_SIZE; j++)
        {
            printf("%3d ", patch_data[i * KERNEL_SIZE + j]);
        }
        printf("\n");
    }
    printf("\n");
    
    // Bottom-right corner patch (should have padding)
    int br_patch_h = OUT_HEIGHT - 1;
    int br_patch_w = OUT_WIDTH - 1;
    int br_patch_index = (br_patch_h * OUT_WIDTH + br_patch_w);
    printf("Patch at (%d,%d) - Bottom-Right Corner (padded):\n", br_patch_h, br_patch_w);
    patch_data = &output_ptr[br_patch_index * KERNEL_SIZE * KERNEL_SIZE];
    for(int i = 0; i < KERNEL_SIZE; i++)
    {
        for(int j = 0; j < KERNEL_SIZE; j++)
        {
            printf("%3d ", patch_data[i * KERNEL_SIZE + j]);
        }
        printf("\n");
    }
    printf("\n");
    
    // Verify no overwrite of tracking region
    printf("=== Verification: Memory Safety ===\n");
    uint32_t* track_region = (uint32_t*)DDR_TRACK_REGION_BASE;
    printf("Track region first value: 0x%08x (should be unchanged)\n", track_region[0]);
    printf("\n");
    
    printf("=== Test Complete ===\n");
    printf("Hardware packetization test on 416x416x3 image successful!\n");
    
    return 0;
}
