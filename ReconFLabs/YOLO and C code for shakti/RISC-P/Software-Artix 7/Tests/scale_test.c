#include <stdint.h>
#include "../Scale/scale.h"
#include "../include/mem_map.h"
#include "uart.h"

// Status word written in DDR for host-side observation (no DMA, no UART prints).
#define TEST_STATUS_ADDR STATUS_SCALE_ADDR

static void fill_source_pattern(int sw, int sh) {
    uint8_t *src = SRC_ADDR;
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            src[y * sw + x] = (uint8_t)(y * sw + x);
        }
    }
}

static int verify_pixels(const uint8_t *buf, const uint8_t *expected, int count) {
    for (int i = 0; i < count; i++) {
        if (buf[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

static int verify_points(const uint8_t *buf, int width, const uint16_t points[][3], int count) {
    for (int i = 0; i < count; i++) {
        int x = points[i][0];
        int y = points[i][1];
        uint8_t exp = (uint8_t)points[i][2];
        if (buf[y * width + x] != exp) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    printf("Scale Test Starting...\n");
    
    const int sw_small = 4;
    const int sh_small = 4;
    const int dw_small = 2;
    const int dh_small = 2;

    fill_source_pattern(sw_small, sh_small);

    // Nearest-neighbor check against a known 4x4->2x2 downscale.
    printf("Test 1: Nearest-Neighbor 4x4->2x2\n");
    nearest_neighbor(sw_small, sh_small, dw_small, dh_small);
    static const uint8_t expected_nn[] = {0, 2, 8, 10};
    if (!verify_pixels(DST_ADDR, expected_nn, dw_small * dh_small)) {
        printf("Nearest-Neighbor Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD00001u;
        return 1;
    }
    printf("Nearest-Neighbor Test PASSED!\n");

    // Bilinear check on the same pattern; output overwrites the previous result.
    printf("Test 2: Bilinear 4x4->2x2\n");
    bilinear(sw_small, sh_small, dw_small, dh_small);
    static const uint8_t expected_bi[] = {0, 1, 6, 7};
    if (!verify_pixels(DST_ADDR, expected_bi, dw_small * dh_small)) {
        printf("Bilinear Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD00002u;
        return 1;
    }
    printf("Bilinear Test PASSED!\n");

    // Larger-case regression: 640x480 -> 416x416 using sentinel pixels.
    const int sw = 640;
    const int sh = 480;
    const int dw = 416;
    const int dh = 416;

    fill_source_pattern(sw, sh);

    // Nearest neighbor sentinel checks.
    printf("Test 3: Nearest-Neighbor 640x480->416x416\n");
    nearest_neighbor(sw, sh, dw, dh);
    static const uint16_t nn_points[][3] = {
        {0, 0, 0},            // top-left
        {415, 415, 126},      // bottom-right
        {208, 208, 64},       // center
        {100, 300, 153},      // mid-lower
        {200, 100, 179}       // mid-upper
    };
    if (!verify_points(DST_ADDR, dw, nn_points, (int)(sizeof(nn_points) / sizeof(nn_points[0])))) {
        printf("Nearest-Neighbor Large Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD00003u;
        return 1;
    }
    printf("Nearest-Neighbor Large Test PASSED!\n");

    // Bilinear sentinel checks.
    printf("Test 4: Bilinear 640x480->416x416\n");
    bilinear(sw, sh, dw, dh);
    static const uint16_t bi_points[][3] = {
        {0, 0, 0},            // top-left
        {415, 415, 145},      // bottom-right
        {208, 208, 127},      // center
        {100, 300, 81},       // mid-lower
        {200, 100, 160}       // mid-upper
    };
    if (!verify_points(DST_ADDR, dw, bi_points, (int)(sizeof(bi_points) / sizeof(bi_points[0])))) {
        printf("Bilinear Large Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD00004u;
        return 1;
    }
    printf("Bilinear Large Test PASSED!\n");

    printf("All Scale Tests PASSED!\n");
    TEST_STATUS_ADDR[0] = 0x00000000u;

    // Uncomment for UART/console debug if available.
    // printf("scale_test complete, status=0x%08X\n", TEST_STATUS_ADDR[0]);

    return 0;
}
