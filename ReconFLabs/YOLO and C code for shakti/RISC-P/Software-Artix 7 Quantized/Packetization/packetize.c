#include <stdint.h>
#include <string.h>
#include "../include/mem_map.h"
#include "../include/packetize.h"

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

void upsample_image(uint8_t* input, uint8_t* output,
                    int height, int width, int channels,
                    int scale)
{
    int outHeight = height * scale;
    int outWidth  = width  * scale;

    for(int h = 0; h < height; h++)
    {
        for(int w = 0; w < width; w++)
        {
            for(int sh = 0; sh < scale; sh++)
            {
                int outH = h * scale + sh;
                for(int sw = 0; sw < scale; sw++)
                {
                    int outW = w * scale + sw;
                    
                    int inIdx = (h * width + w) * channels;
                    int outIdx = (outH * outWidth + outW) * channels;
                    
                    for(int c = 0; c < channels; c++)
                    {
                        output[outIdx + c] = input[inIdx + c];
                    }
                }
            }
        }
    }
}
