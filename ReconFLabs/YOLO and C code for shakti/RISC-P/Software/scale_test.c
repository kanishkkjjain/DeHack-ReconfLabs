#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "include/image.h"
#include "Scaling/scale.h"

// Helper function to create a test image with gradient pattern
Image* create_test_image(int width, int height, int channels) {
    Image* img = (Image*)malloc(sizeof(Image));
    img->width = width;
    img->height = height;
    img->channels = channels;
    img->data = (unsigned int*)malloc(width * height * channels * sizeof(unsigned int));

    // Fill with gradient pattern for testing
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            for (int c = 0; c < channels; c++) {
                unsigned int value = ((x * 255 / width) + (y * 255 / height)) / 2;
                img->data[(y * width + x) * channels + c] = value;
            }
        }
    }

    return img;
}

// Helper function to free image
void free_image(Image* img) {
    if (img != NULL) {
        if (img->data != NULL) {
            free(img->data);
        }
        free(img);
    }
}

// Helper function to verify scaled image dimensions
void verify_image(Image* img, int expected_width, int expected_height, const char* name) {
    if (img == NULL) {
        printf("  ✗ %s: NULL image returned\n", name);
        return;
    }

    if (img->width == expected_width && img->height == expected_height) {
        printf("  ✓ %s: %dx%d (correct)\n", name, img->width, img->height);
    } else {
        printf("  ✗ %s: Expected %dx%d, got %dx%d\n", name, expected_width, expected_height, img->width, img->height);
    }
}

// Helper function to check if pixel values are in valid range
void check_pixel_range(Image* img, const char* name) {
    int out_of_range = 0;
    unsigned int max_val = 0;

    for (int i = 0; i < img->width * img->height * img->channels; i++) {
        if (img->data[i] > 255) {
            out_of_range++;
        }
        if (img->data[i] > max_val) {
            max_val = img->data[i];
        }
    }

    if (out_of_range == 0) {
        printf(" %s: All pixel values in valid range [0, %u]\n", name, max_val);
    } else {
        printf(" %s: %d pixels out of range (max: %u)\n", name, out_of_range, max_val);
    }
}

int main() {
    printf("=== Image Scaling Algorithm Test ===\n\n");

    // Test parameters
    int src_width = 800, src_height = 600, channels = 3;
    int dst_width = 416, dst_height = 416;

    printf("Test Configuration:\n");
    printf("  Source: %dx%d (%d channels)\n", src_width, src_height, channels);
    printf("  Target: %dx%d\n\n", dst_width, dst_height);

    // Create test image
    printf("Creating test image...\n");
    Image* source = create_test_image(src_width, src_height, channels);
    printf(" Source image created\n\n");

    // ============ TEST 1: Nearest Neighbor ============
    printf("TEST 1: Nearest Neighbor Scaling\n");
    printf("  Quality: LOW | Speed: VERY FAST\n");
    printf("  Best for: Real-time systems with severe CPU limits\n");

    clock_t start = clock();
    Image* nn_scaled = nearest_neighbor_scale(source, dst_width, dst_height);
    clock_t end = clock();
    double nn_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;

    verify_image(nn_scaled, dst_width, dst_height, "Nearest Neighbor");
    check_pixel_range(nn_scaled, "Nearest Neighbor");
    printf("  Time: %.3f ms\n\n", nn_time);

    // ============ TEST 2: Bilinear ============
    printf("TEST 2: Bilinear Interpolation\n");
    printf("  Quality: MEDIUM-HIGH | Speed: MEDIUM\n");
    printf("  Best for: General purpose YOLOv3 pre-processing\n");

    start = clock();
    Image* bilinear_scaled = bilinear_scale(source, dst_width, dst_height);
    end = clock();
    double bilinear_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;

    verify_image(bilinear_scaled, dst_width, dst_height, "Bilinear");
    check_pixel_range(bilinear_scaled, "Bilinear");
    printf("  Time: %.3f ms\n\n", bilinear_time);

    // ============ TEST 3: Bicubic ============
    printf("TEST 3: Bicubic Interpolation\n");
    printf("  Quality: HIGH | Speed: SLOW\n");
    printf("  Best for: High-accuracy requirements where CPU isn't a bottleneck\n");

    start = clock();
    Image* bicubic_scaled = bicubic_scale(source, dst_width, dst_height);
    end = clock();
    double bicubic_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;

    verify_image(bicubic_scaled, dst_width, dst_height, "Bicubic");
    check_pixel_range(bicubic_scaled, "Bicubic");
    printf("  Time: %.3f ms\n\n", bicubic_time);

    // ============ TEST 4: Box Filter (Averaging) ============
    printf("TEST 4: Box Filter (Area Averaging)\n");
    printf("  Quality: HIGH (Downscale) | Speed: MEDIUM\n");
    printf("  Best for: Scaling 4K footage down to 416×416\n");

    start = clock();
    Image* box_scaled = box_scale(source, dst_width, dst_height);
    end = clock();
    double box_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;

    verify_image(box_scaled, dst_width, dst_height, "Box Filter");
    check_pixel_range(box_scaled, "Box Filter");
    printf("  Time: %.3f ms\n\n", box_time);

    // ============ PERFORMANCE COMPARISON ============
    printf("=== Performance Summary ===\n");
    printf("Algorithm          | Time (ms) | Relative Speed\n");
    printf("-------------------+-----------+---------------\n");

    double fastest = nn_time;
    printf("Nearest Neighbor   | %9.3f | 1.0x (fastest)\n", nn_time);
    printf("Bilinear           | %9.3f | %.2fx\n", bilinear_time, bilinear_time / fastest);
    printf("Bicubic            | %9.3f | %.2fx\n", bicubic_time, bicubic_time / fastest);
    printf("Box Filter         | %9.3f | %.2fx\n", box_time, box_time / fastest);

    // ============ MEMORY USAGE ============
    printf("\n=== Memory Analysis ===\n");
    int src_bytes = src_width * src_height * channels * sizeof(unsigned int);
    int dst_bytes = dst_width * dst_height * channels * sizeof(unsigned int);
    printf("Source image: %d bytes\n", src_bytes);
    printf("Scaled image: %d bytes (each)\n", dst_bytes);
    printf("Total memory used: %d bytes\n", src_bytes + (dst_bytes * 4));

    // ============ SAMPLE PIXEL VALUES ============
    printf("\n=== Sample Pixel Values (top-left corner, channel 0) ===\n");
    printf("Position (10,10):\n");
    printf("  Source:        %u\n", source->data[(10 * src_width + 10) * channels]);
    printf("  Nearest Neighbor: %u\n", nn_scaled->data[(10 * dst_width + 10) * channels]);
    printf("  Bilinear:      %u\n", bilinear_scaled->data[(10 * dst_width + 10) * channels]);
    printf("  Bicubic:       %u\n", bicubic_scaled->data[(10 * dst_width + 10) * channels]);
    printf("  Box Filter:    %u\n", box_scaled->data[(10 * dst_width + 10) * channels]);

    // ============ CLEANUP ============
    printf("\n=== Cleanup ===\n");
    free_image(source);
    free_image(nn_scaled);
    free_image(bilinear_scaled);
    free_image(bicubic_scaled);
    free_image(box_scaled);
    printf(" All images freed\n");

    printf("\n=== Test Complete ===\n");
    return 0;
}
