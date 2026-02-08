#ifndef PACKETIZE_H
#define PACKETIZE_H

#include <stdint.h>

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
                     int channels, int kernelSize, int stride, int padding);

/**
 * Calculate the output buffer size needed for packetization
 * 
 * @param height: Input image height
 * @param width: Input image width
 * @param channels: Number of channels
 * @param kernelSize: Convolution kernel size
 * @param stride: Stride for patch extraction
 * @param padding: Zero padding around the image
 * @return: Required output buffer size in elements
 */
int calculate_packetize_output_size(int height, int width, int channels, 
                                    int kernelSize, int stride, int padding);

/**
 * upsample_image - Upsample image using nearest-neighbor interpolation
 * 
 * @param input: Input image buffer
 * @param output: Output upsampled image buffer
 * @param height: Input image height
 * @param width: Input image width
 * @param channels: Number of channels
 * @param scale: Upsampling factor
 */
void upsample_image(uint8_t* input, uint8_t* output,
                    int height, int width, int channels,
                    int scale);

/**
 * maxpool_image - Apply max pooling to image
 * 
 * @param input: Input image buffer (CHW format)
 * @param output: Output pooled image buffer (CHW format)
 * @param height: Input image height
 * @param width: Input image width
 * @param channels: Number of channels
 * @param poolSize: Pooling window size
 * @param stride: Pooling stride
 */
void maxpool_image(uint8_t* input, uint8_t* output,
                   int height, int width, int channels,
                   int poolSize, int stride);

/**
 * upsample_float - Upsample float image using nearest-neighbor interpolation (CHW format)
 */
void upsample_float(const float* input, float* output,
                    int height, int width, int channels,
                    int scale);

/**
 * maxpool_float - Apply max pooling to float image (CHW format)
 */
void maxpool_float(const float* input, float* output,
                   int height, int width, int channels,
                   int poolSize, int stride, int padding);

#endif // PACKETIZE_H
