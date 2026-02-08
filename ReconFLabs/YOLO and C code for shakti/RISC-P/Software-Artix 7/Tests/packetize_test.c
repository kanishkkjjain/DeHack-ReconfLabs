#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packetize.h"

// Helper function to print a patch
void print_patch(uint8_t* patch, int kernelSize)
{
    for(int i = 0; i < kernelSize; i++)
    {
        for(int j = 0; j < kernelSize; j++)
        {
            printf("%3d ", patch[i * kernelSize + j]);
        }
        printf("\n");
    }
}

// Test 1: Basic 4x4 image with 3x3 kernel, stride 1, padding 1
void test_basic_3x3_kernel()
{
    printf("=== Test 1: 4x4 image, 3x3 kernel, stride 1, padding 1 ===\n");
    
    int height = 4, width = 4, channels = 1;
    int kernelSize = 3, stride = 1, padding = 1;
    
    uint8_t input[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };
    
    int outputSize = calculate_packetize_output_size(height, width, channels, kernelSize, stride, padding);
    printf("Output size: %d elements\n", outputSize);
    
    uint8_t* output = (uint8_t*)malloc(outputSize);
    packetize_image(input, output, height, width, channels, kernelSize, stride, padding);
    
    printf("Patches (4x4 grid, each 3x3):\n");
    int numPatches = (height - kernelSize + 2 * padding) / stride + 1;
    for(int i = 0; i < numPatches * numPatches; i++)
    {
        printf("Patch %d:\n", i);
        print_patch(&output[i * kernelSize * kernelSize], kernelSize);
        printf("\n");
    }
    
    free(output);
    printf("\n");
}

// Test 2: 2x2 kernel with stride 1, no padding
void test_2x2_no_padding()
{
    printf("=== Test 2: 4x4 image, 2x2 kernel, stride 1, padding 0 ===\n");
    
    int height = 4, width = 4, channels = 1;
    int kernelSize = 2, stride = 1, padding = 0;
    
    uint8_t input[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };
    
    int outputSize = calculate_packetize_output_size(height, width, channels, kernelSize, stride, padding);
    printf("Output size: %d elements\n", outputSize);
    
    uint8_t* output = (uint8_t*)malloc(outputSize);
    packetize_image(input, output, height, width, channels, kernelSize, stride, padding);
    
    printf("Patches (3x3 grid, each 2x2):\n");
    int numPatches = (height - kernelSize + 2 * padding) / stride + 1;
    for(int i = 0; i < numPatches * numPatches; i++)
    {
        printf("Patch %d:\n", i);
        print_patch(&output[i * kernelSize * kernelSize], kernelSize);
        printf("\n");
    }
    
    free(output);
    printf("\n");
}

// Test 3: Stride 2
void test_stride_2()
{
    printf("=== Test 3: 4x4 image, 2x2 kernel, stride 2, padding 0 ===\n");
    
    int height = 4, width = 4, channels = 1;
    int kernelSize = 2, stride = 2, padding = 0;
    
    uint8_t input[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };
    
    int outputSize = calculate_packetize_output_size(height, width, channels, kernelSize, stride, padding);
    printf("Output size: %d elements\n", outputSize);
    
    uint8_t* output = (uint8_t*)malloc(outputSize);
    packetize_image(input, output, height, width, channels, kernelSize, stride, padding);
    
    printf("Patches (2x2 grid, each 2x2):\n");
    int numPatches = (height - kernelSize + 2 * padding) / stride + 1;
    for(int i = 0; i < numPatches * numPatches; i++)
    {
        printf("Patch %d:\n", i);
        print_patch(&output[i * kernelSize * kernelSize], kernelSize);
        printf("\n");
    }
    
    free(output);
    printf("\n");
}

// Test 4: Multi-channel
void test_multichannel()
{
    printf("=== Test 4: 2x2 image, 2 channels, 2x2 kernel, stride 1, padding 0 ===\n");
    
    int height = 2, width = 2, channels = 2;
    int kernelSize = 2, stride = 1, padding = 0;
    
    // Input: 2x2 image with 2 channels
    // Format: [C0_H0W0, C1_H0W0, C0_H0W1, C1_H0W1, C0_H1W0, C1_H1W0, C0_H1W1, C1_H1W1]
    uint8_t input[8] = {
        1, 2,  // pixel (0,0): channel 0=1, channel 1=2
        3, 4,  // pixel (0,1): channel 0=3, channel 1=4
        5, 6,  // pixel (1,0): channel 0=5, channel 1=6
        7, 8   // pixel (1,1): channel 0=7, channel 1=8
    };
    
    int outputSize = calculate_packetize_output_size(height, width, channels, kernelSize, stride, padding);
    printf("Output size: %d elements\n", outputSize);
    
    uint8_t* output = (uint8_t*)malloc(outputSize);
    packetize_image(input, output, height, width, channels, kernelSize, stride, padding);
    
    printf("Single patch per channel (2 channels, each 2x2):\n");
    for(int c = 0; c < channels; c++)
    {
        printf("Channel %d:\n", c);
        print_patch(&output[c * kernelSize * kernelSize], kernelSize);
        printf("\n");
    }
    
    free(output);
    printf("\n");
}

int main()
{
    printf("Packetize Image Tests (uint8_t)\n");
    printf("===============================\n\n");
    
    test_basic_3x3_kernel();
    test_2x2_no_padding();
    test_stride_2();
    test_multichannel();
    
    printf("All tests completed successfully!\n");
    
    return 0;
}
