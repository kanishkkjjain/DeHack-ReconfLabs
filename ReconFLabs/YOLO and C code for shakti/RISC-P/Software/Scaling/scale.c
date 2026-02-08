#include <stdio.h>
#include <stdlib.h>
#include "../include/image.h"

/*
Algorithm,Complexity,Quality,Best Use Case
Nearest Neighbor,Very Low,Low,Real-time systems with severe CPU limits.
Bilinear,Medium,Medium-High,General purpose YOLOv3 pre-processing.
Bicubic,High,High,High-accuracy requirements where CPU isn't a bottleneck.
Box,Medium,High (Downscale),Scaling 4K footage down to 416×416.
*/

Image* nearest_neighbor_scale(Image* input, int new_width, int new_height) {
    Image* output = (Image*)malloc(sizeof(Image));
    output->width = new_width;
    output->height = new_height;
    output->channels = input->channels;
    output->data = (unsigned int*)malloc(new_width * new_height * input->channels * sizeof(unsigned int));

    float x_ratio = (float)input->width / new_width;
    float y_ratio = (float)input->height / new_height;

    for (int j = 0; j < new_height; j++) {
        for (int i = 0; i < new_width; i++) {
            int nearest_x = (int)(i * x_ratio);
            int nearest_y = (int)(j * y_ratio);

            for (int c = 0; c < input->channels; c++) {
                output->data[(j * new_width + i) * input->channels + c] =
                    input->data[(nearest_y * input->width + nearest_x) * input->channels + c];
            }
        }
    }

    return output;
}

Image* bilinear_scale(Image* input, int new_width, int new_height) {
    Image* output = (Image*)malloc(sizeof(Image));
    output->width = new_width;
    output->height = new_height;
    output->channels = input->channels;
    output->data = (unsigned int*)malloc(new_width * new_height * input->channels * sizeof(unsigned int));

    float x_ratio = (float)(input->width - 1) / new_width;
    float y_ratio = (float)(input->height - 1) / new_height;

    for (int j = 0; j < new_height; j++) {
        for (int i = 0; i < new_width; i++) {
            float x = i * x_ratio;
            float y = j * y_ratio;
            int x_low = (int)x;
            int y_low = (int)y;
            int x_high = (x_low + 1 < input->width) ? x_low + 1 : x_low;
            int y_high = (y_low + 1 < input->height) ? y_low + 1 : y_low;

            float x_diff = x - x_low;
            float y_diff = y - y_low;

            for (int c = 0; c < input->channels; c++) {
                unsigned int a = input->data[(y_low * input->width + x_low) * input->channels + c];
                unsigned int b = input->data[(y_low * input->width + x_high) * input->channels + c];
                unsigned int c_val = input->data[(y_high * input->width + x_low) * input->channels + c];
                unsigned int d = input->data[(y_high * input->width + x_high) * input->channels + c];

                // Bilinear interpolation
                unsigned int pixel_value =
                    (unsigned int)(
                        a * (1 - x_diff) * (1 - y_diff) +
                        b * x_diff * (1 - y_diff) +
                        c_val * y_diff * (1 - x_diff) +
                        d * x_diff * y_diff
                    );

                output->data[(j * new_width + i) * input->channels + c] = pixel_value;
            }
        }
    }

    return output;
}

// Cubic interpolation kernel
float cubic_kernel(float t) {
    t = (t < 0) ? -t : t;
    if (t < 1.0f) {
        return 1.0f - 2.0f * t * t + t * t * t;
    } else if (t < 2.0f) {
        return -4.0f + 8.0f * t - 5.0f * t * t + t * t * t;
    }
    return 0.0f;
}

Image* bicubic_scale(Image* input, int new_width, int new_height) {
    Image* output = (Image*)malloc(sizeof(Image));
    output->width = new_width;
    output->height = new_height;
    output->channels = input->channels;
    output->data = (unsigned int*)malloc(new_width * new_height * input->channels * sizeof(unsigned int));

    float x_ratio = (float)(input->width - 1) / new_width;
    float y_ratio = (float)(input->height - 1) / new_height;


    for (int j = 0; j < new_height; j++) {
        for (int i = 0; i < new_width; i++) {
            float x = i * x_ratio;
            float y = j * y_ratio;
            int x_int = (int)x;
            int y_int = (int)y;
            float x_frac = x - x_int;
            float y_frac = y - y_int;

            for (int c = 0; c < input->channels; c++) {
                float pixel_value = 0.0f;
                for (int n = -1; n <= 2; n++) {
                    for (int m = -1; m <= 2; m++) {
                        int px = x_int + m;
                        int py = y_int + n;
                        px = (px < 0) ? 0 : (px >= input->width) ? input->width - 1 : px;
                        py = (py < 0) ? 0 : (py >= input->height) ? input->height - 1 : py;
                        
                        float weight = cubic_kernel(m - x_frac) * cubic_kernel(n - y_frac);
                        pixel_value += weight * input->data[(py * input->width + px) * input->channels + c];
                    }
                }
                output->data[(j * new_width + i) * input->channels + c] = (unsigned int)pixel_value;
            }
        }
    }

    return output;
}

Image* box_scale(Image* input, int new_width, int new_height) {
    Image* output = (Image*)malloc(sizeof(Image));
    output->width = new_width;
    output->height = new_height;
    output->channels = input->channels;
    output->data = (unsigned int*)malloc(new_width * new_height * input->channels * sizeof(unsigned int));

    float x_ratio = (float)input->width / new_width;
    float y_ratio = (float)input->height / new_height;

    for (int j = 0; j < new_height; j++) {
        for (int i = 0; i < new_width; i++) {
            int x_start = (int)(i * x_ratio);
            int y_start = (int)(j * y_ratio);
            int x_end = (int)((i + 1) * x_ratio);
            int y_end = (int)((j + 1) * y_ratio);

            for (int c = 0; c < input->channels; c++) {
                unsigned long sum = 0;
                int count = 0;

                for (int y_src = y_start; y_src < y_end && y_src < input->height; y_src++) {
                    for (int x_src = x_start; x_src < x_end && x_src < input->width; x_src++) {
                        sum += input->data[(y_src * input->width + x_src) * input->channels + c];
                        count++;
                    }
                }

                output->data[(j * new_width + i) * input->channels + c] = (unsigned int)(sum / count);
            }
        }
    }

    return output;
}

