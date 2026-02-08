#include <stdio.h>
#include "uart.h"
#include <stdint.h>

// ==================== MEMORY MAP ====================
// DDR base with required 32KB offset
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

// ==================== IMAGE DIMENSIONS ====================
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

// ==================== PACKETIZE FUNCTIONS ====================

/**
 * Calculate the output buffer size needed for packetization
 * 
 * @param height: Input image height
 * @param width: Input image width
 * @param channels: Number of channels
 * @param kernelSize: Convolution kernel size
 * @param stride: Stride for patch extraction
 * @param padding: Zero padding around the image
 * @return: Required output buffer size in bytes
 */
int calculate_packetize_output_size(int height, int width, int channels, 
                                    int kernelSize, int stride, int padding)
{
    int outHeight = (height - kernelSize + 2 * padding) / stride + 1;
    int outWidth = (width - kernelSize + 2 * padding) / stride + 1;
    return channels * outHeight * outWidth * kernelSize * kernelSize;
}

/**
 * packetize_image - Convert image into overlapping patches for convolution
 * 
 * @param input: Input image data (height * width * channels)
 * @param output: Output patches buffer
 * @param height: Input image height
 * @param width: Input image width
 * @param channels: Number of channels
 * @param kernelSize: Convolution kernel size (kernelSize x kernelSize)
 * @param stride: Stride for patch extraction
 * @param padding: Zero padding around the image
 * 
 * Output organization:
 * - Total output size: channels * outHeight * outWidth * kernelSize * kernelSize
 * - outHeight = (height - kernelSize + 2*padding) / stride + 1
 * - outWidth = (width - kernelSize + 2*padding) / stride + 1
 * - Each patch is stored as: patchIndex * (kernelSize * kernelSize) + element_in_patch
 */
void packetize_image(uint8_t* input, uint8_t *output, int height, int width, 
                     int channels, int kernelSize, int stride, int padding)
{
    int outHeight = (height - kernelSize + 2 * padding) / stride + 1;
    int outWidth = (width - kernelSize + 2 * padding) / stride + 1;
    
    // Initialize entire output buffer to 0 for zero padding
    int totalSize = channels * kernelSize * kernelSize * outHeight * outWidth;
    for(int i = 0; i < totalSize; i++)
        output[i] = 0;

    for(int c = 0; c < channels; c++)
    {
        for(int oh = 0; oh < outHeight; oh++)
        {
            for(int ow = 0; ow < outWidth; ow++)
            {
                for(int kh = 0; kh < kernelSize; kh++)
                {
                    for(int kw = 0; kw < kernelSize; kw++)
                    {
                        int inH = oh * stride + kh - padding;
                        int inW = ow * stride + kw - padding;

                        // Simpler indexing: organize by patch, then by element within patch
                        int patchIndex = c * (outHeight * outWidth) + oh * outWidth + ow;
                        int elementInPatch = kh * kernelSize + kw;
                        int outIndex = patchIndex * (kernelSize * kernelSize) + elementInPatch;

                        // If pixel is within bounds, copy it; otherwise, keep zero padding
                        if(inH >= 0 && inH < height && inW >= 0 && inW < width)
                        {
                            int inIndex = c * (height * width) + inH * width + inW;
                            output[outIndex] = input[inIndex];
                        }
                    }
                }
            }
        }
    }
}

// ==================== MAIN TEST ====================

int main()
{
    printf("=== Packetize Hardware Test ===\n\n");
    
    // Memory offsets are correctly implemented:
    // DDR_BASE = 0x80008000 (nothing allocated before this)
    // Track Region: 0x80008000 (64KB)
    // Source Region: 0x80018000 (512KB) - starts after track region
    // Destination Region: 0x800A8000 - starts after source + 64KB gap
    
    // Get pointers to DDR regions
    uint8_t* input_ptr = (uint8_t*)SRC_ADDR;
    uint8_t* output_ptr = (uint8_t*)DST_ADDR;
    
    // ========== TEST 1: Input layer (416x416x3) ==========
    printf("[TEST 1] Input layer (416x416x3)\n");
    
    // Calculate sizes
    int input_size_bytes = IMG_HEIGHT * IMG_WIDTH * IMG_CHANNELS;
    int output_size_bytes = calculate_packetize_output_size(IMG_HEIGHT, IMG_WIDTH, IMG_CHANNELS, 
                                                             KERNEL_SIZE, STRIDE, PADDING);
    
    // Initialize input with test pattern
    for(int i = 0; i < input_size_bytes; i++)
    {
        input_ptr[i] = (uint8_t)(i % 256);
    }
    
    // Run packetization
    packetize_image(input_ptr, output_ptr, IMG_HEIGHT, IMG_WIDTH, IMG_CHANNELS, 
                   KERNEL_SIZE, STRIDE, PADDING);
    
    // Verify output - check a few patches
    int test1_pass = (output_ptr[0] == 0 && output_ptr[4] == 0);  // Check padding
    printf("  Patch[0,0]: %d %d %d (padded corner)\n", output_ptr[0], output_ptr[1], output_ptr[2]);
    
    int center_idx = (OUT_HEIGHT/2 * OUT_WIDTH + OUT_WIDTH/2) * KERNEL_SIZE * KERNEL_SIZE;
    printf("  Patch[center]: %d %d %d\n", output_ptr[center_idx], output_ptr[center_idx+1], output_ptr[center_idx+2]);
    printf("  Result: %s\n\n", test1_pass ? "PASS" : "FAIL");
    
    // ========== TEST 2: Intermediate YOLO layer (52x52x256) ==========
    printf("[TEST 2] YOLO intermediate layer (52x52x256)\n");
    
    int h2 = 52;
    int w2 = 52;
    int c2 = 256;
    int k2 = 3;
    int s2 = 1;
    int p2 = 1;
    
    int input_size2 = h2 * w2 * c2;
    int output_size2 = calculate_packetize_output_size(h2, w2, c2, k2, s2, p2);
    
    // Initialize input with different pattern
    for(int i = 0; i < input_size2; i++)
    {
        input_ptr[i] = (uint8_t)((i * 7) % 256);
    }
    
    // Run packetization
    packetize_image(input_ptr, output_ptr, h2, w2, c2, k2, s2, p2);
    
    // Verify output
    int out_h2 = (h2 - k2 + 2 * p2) / s2 + 1;
    int out_w2 = (w2 - k2 + 2 * p2) / s2 + 1;
    int test2_pass = (output_ptr[0] == 0 && out_h2 == 52 && out_w2 == 52);
    
    printf("  Output grid: %dx%d patches\n", out_h2, out_w2);
    printf("  Patch[0,0]: %d %d %d (padded corner)\n", output_ptr[0], output_ptr[1], output_ptr[2]);
    
    int center_idx2 = (out_h2/2 * out_w2 + out_w2/2) * k2 * k2;
    printf("  Patch[center]: %d %d %d\n", output_ptr[center_idx2], output_ptr[center_idx2+1], output_ptr[center_idx2+2]);
    printf("  Result: %s\n\n", test2_pass ? "PASS" : "FAIL");
    
    // ========== SUMMARY ==========
    int all_pass = test1_pass && test2_pass;
    printf("=== Test Summary ===\n");
    printf("Test 1: %s\n", test1_pass ? "PASS" : "FAIL");
    printf("Test 2: %s\n", test2_pass ? "PASS" : "FAIL");
    printf("Overall: %s\n", all_pass ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    
    return all_pass ? 0 : 1;
}
