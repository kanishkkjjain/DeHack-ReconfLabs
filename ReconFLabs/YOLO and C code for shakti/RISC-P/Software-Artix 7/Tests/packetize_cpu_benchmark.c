#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// ==================== PACKETIZE FUNCTIONS ====================

int calculate_packetize_output_size(int height, int width, int channels, 
                                    int kernelSize, int stride, int padding)
{
    int outHeight = (height - kernelSize + 2 * padding) / stride + 1;
    int outWidth = (width - kernelSize + 2 * padding) / stride + 1;
    return channels * outHeight * outWidth * kernelSize * kernelSize;
}

void packetize_image(uint8_t* input, uint8_t *output,
                     int height, int width, int channels,
                     int kernelSize, int stride, int padding)
{
    int outHeight = (height - kernelSize + 2 * padding) / stride + 1;
    int outWidth  = (width  - kernelSize + 2 * padding) / stride + 1;

    int patchSize = kernelSize * kernelSize;
    int numPatches = outHeight * outWidth;

    // Zero output (padding)
    memset(output, 0, channels * patchSize * numPatches);

    for(int c = 0; c < channels; c++)
    {
        uint8_t* inBase  = input  + c * height * width;
        uint8_t* outBase = output + c * numPatches * patchSize;

        for(int oh = 0; oh < outHeight; oh++)
        {
            int inHBase = oh * stride - padding;

            for(int ow = 0; ow < outWidth; ow++)
            {
                int inWBase = ow * stride - padding;
                uint8_t* patchPtr = outBase + (oh * outWidth + ow) * patchSize;

                for(int kh = 0; kh < kernelSize; kh++)
                {
                    int inH = inHBase + kh;
                    if(inH < 0 || inH >= height) continue;

                    int inRowOffset = inH * width;

                    uint8_t* patchRow = patchPtr + kh * kernelSize;

                    for(int kw = 0; kw < kernelSize; kw++)
                    {
                        int inW = inWBase + kw;
                        if(inW < 0 || inW >= width) continue;

                        patchRow[kw] = inBase[inRowOffset + inW];
                    }
                }
            }
        }
    }
}


// ==================== MAIN TEST ====================

int main()
{
    printf("=== CPU Packetization Benchmark ===\n\n");
    
    // Parameters
    int height = 416;
    int width = 416;
    int channels = 3;
    int kernelSize = 3;
    int stride = 1;
    int padding = 1;
    
    // Allocate memory on CPU
    int input_size = height * width * channels;
    int output_size = calculate_packetize_output_size(height, width, channels, kernelSize, stride, padding);
    
    uint8_t* input = (uint8_t*)malloc(input_size);
    uint8_t* output = (uint8_t*)malloc(output_size);
    
    if(!input || !output)
    {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    printf("[SETUP] Input: %d bytes (%.2f MB)\n", input_size, input_size / (1024.0 * 1024.0));
    printf("[SETUP] Output: %d bytes (%.2f MB)\n", output_size, output_size / (1024.0 * 1024.0));
    
    // Initialize input with test pattern
    printf("[INIT] Initializing input image...\n");
    for(int i = 0; i < input_size; i++)
    {
        input[i] = (uint8_t)(i % 256);
    }
    
    // Time the packetization
    printf("[RUN] Packetizing 416x416x3 image...\n");
    
    clock_t start = clock();
    packetize_image(input, output, height, width, channels, kernelSize, stride, padding);
    clock_t end = clock();
    
    double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    double throughput_mb = output_size / (1024.0 * 1024.0) / ((double)(end - start) / CLOCKS_PER_SEC);
    
    printf("\n");
    printf("=== Results ===\n");
    printf("Time: %.2f ms\n", elapsed_ms);
    printf("Throughput: %.2f MB/s\n", throughput_mb);
    printf("Output grid: %dx%d patches\n", 
           (height - kernelSize + 2 * padding) / stride + 1,
           (width - kernelSize + 2 * padding) / stride + 1);
    
    // Verify output
    printf("\n=== Verification ===\n");
    printf("Patch[0,0] (padded): %d %d %d\n", output[0], output[1], output[2]);
    
    int out_h = (height - kernelSize + 2 * padding) / stride + 1;
    int out_w = (width - kernelSize + 2 * padding) / stride + 1;
    int center_idx = (out_h/2 * out_w + out_w/2) * kernelSize * kernelSize;
    printf("Patch[center]: %d %d %d\n", output[center_idx], output[center_idx+1], output[center_idx+2]);
    
    // Cleanup
    free(input);
    free(output);
    
    printf("\nTest complete\n");
    return 0;
}
