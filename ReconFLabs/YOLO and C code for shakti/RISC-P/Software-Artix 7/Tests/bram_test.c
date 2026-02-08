#include <stdint.h>
#include "uart.h"

// ============================================================================
// BRAM Memory Layout (track_hw.c - Software-Artix 7/Tracking/)
// ============================================================================
// CRITICAL: Do NOT test below offset 0x10000 (64KB) to avoid data corruption!
//
// Track Processing BRAM Data Structures (with 64-byte alignment):
//
// Structure Sizes (actual):
//   TrackState:   32 bytes (8 floats + 1 int + 1 uint16_t + padding)
//   BoundingBox:  28 bytes (6 floats + 1 int + padding)
//
// BRAM Allocation (64-byte aligned):
//   @ 0x0000 - 0x0400 (1KB):
//      TrackState g_tracks[16]                  [BRAM_SECTION BRAM_ALIGNED]
//      16 tracks × 64 bytes (aligned) = 1024 bytes
//
//   @ 0x0400 - 0x0800 (1KB):
//      BoundingBox g_pred_boxes[16]             [BRAM_SECTION BRAM_ALIGNED]
//      16 boxes × 64 bytes (aligned) = 1024 bytes
//
//   @ 0x0800 - 0x2000 (6KB):
//      BoundingBox g_meas_boxes[48]             [BRAM_SECTION BRAM_ALIGNED]
//      48 boxes × 64 bytes (aligned) = 3072 bytes
//
//   @ 0x2000 - 0x5000 (12KB):
//      float g_iou[16][48]                      [BRAM_SECTION BRAM_ALIGNED]
//      16 × 48 × 4 bytes = 3072 bytes (rounded to 12KB with alignment)
//
//   @ 0x5000 - 0x5010:
//      int g_track_count, g_meas_count          [.data section]
//      uint16_t g_next_id
//
// Total BRAM Used: ~20KB (0x5000)
// Assuming BRAM total: 128KB (0x20000)
// SAFE TEST OFFSET: 0x10000 (64KB) - well beyond all track data
// ============================================================================

#define BRAM_BASE 0x00000000u
#define BRAM_TOTAL_SIZE (128 * 1024)  // 128KB BRAM
#define BRAM_TRACK_DATA_SIZE 0x5000u   // ~20KB used by track structures
#define BRAM_TEST_OFFSET 0x10000u      // 64KB offset - SAFE for testing
#define BRAM_TEST_BASE (BRAM_BASE + BRAM_TEST_OFFSET)

#include "../include/mem_map.h"
// Status word written in DDR for host-side observation
#define TEST_STATUS_ADDR STATUS_BRAM_ADDR

// Test 32KB of BRAM (from 64KB-96KB range)
#define TEST_SIZE (32 * 1024)

int main(void) {
    printf("BRAM Test Starting...\n");
    printf("Testing BRAM from SAFE offset to avoid track data corruption\n");
    printf("Track data structures occupy 0x0000-0x5000 (~20KB)\n");
    
    uint32_t *bram_ptr = (uint32_t *)BRAM_TEST_BASE;
    uint32_t num_elements = TEST_SIZE / sizeof(uint32_t);
    uint32_t errors = 0;

    printf("BRAM Total Size: %u KB\n", BRAM_TOTAL_SIZE / 1024);
    printf("BRAM Track Data Size: 0x%X (%u KB)\n", BRAM_TRACK_DATA_SIZE, BRAM_TRACK_DATA_SIZE / 1024);
    printf("BRAM Base Address: 0x%08X\n", BRAM_BASE);
    printf("BRAM Test Address: 0x%08X (offset 0x%X = %u KB)\n", 
           BRAM_TEST_BASE, BRAM_TEST_OFFSET, BRAM_TEST_OFFSET / 1024);
    printf("Test Size: %u bytes (%u KB, %u elements)\n", 
           TEST_SIZE, TEST_SIZE / 1024, num_elements);

    // Test 1: Write sequential pattern
    printf("\nTest 1: Write Sequential Pattern\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        bram_ptr[i] = 0xDEADBEEF + i;
    }
    printf("Sequential pattern written to BRAM\n");

    // Test 2: Read and verify sequential pattern
    printf("Test 2: Read and Verify Sequential Pattern\n");
    errors = 0;
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t expected = 0xDEADBEEF + i;
        uint32_t read_val = bram_ptr[i];
        if (read_val != expected) {
            errors++;
            if (errors <= 5) {
                printf("Error at offset 0x%X: expected 0x%08X, got 0x%08X\n", 
                       i * sizeof(uint32_t), expected, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("Sequential Pattern Test PASSED!\n");
    } else {
        printf("Sequential Pattern Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0001u | (errors & 0xFF);
        return 1;
    }

    // Test 3: Write walking ones/zeros pattern
    printf("\nTest 3: Write Walking Ones Pattern\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        bram_ptr[i] = 1u << (i % 32);
    }
    printf("Walking ones pattern written to BRAM\n");

    // Test 4: Verify walking ones pattern
    printf("Test 4: Verify Walking Ones Pattern\n");
    errors = 0;
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t expected = 1u << (i % 32);
        uint32_t read_val = bram_ptr[i];
        if (read_val != expected) {
            errors++;
            if (errors <= 5) {
                printf("Error at index %u: expected 0x%08X, got 0x%08X\n", 
                       i, expected, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("Walking Ones Test PASSED!\n");
    } else {
        printf("Walking Ones Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0002u | (errors & 0xFF);
        return 1;
    }

    // Test 5: Checkerboard pattern
    printf("\nTest 5: Write Checkerboard Pattern\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        bram_ptr[i] = (i % 2) ? 0xAAAAAAAAu : 0x55555555u;
    }
    printf("Checkerboard pattern written to BRAM\n");

    // Test 6: Verify checkerboard pattern
    printf("Test 6: Verify Checkerboard Pattern\n");
    errors = 0;
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t expected = (i % 2) ? 0xAAAAAAAAu : 0x55555555u;
        uint32_t read_val = bram_ptr[i];
        if (read_val != expected) {
            errors++;
            if (errors <= 5) {
                printf("Error at index %u: expected 0x%08X, got 0x%08X\n", 
                       i, expected, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("Checkerboard Test PASSED!\n");
    } else {
        printf("Checkerboard Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0003u | (errors & 0xFF);
        return 1;
    }

    // Test 7: Inverse checkerboard pattern
    printf("\nTest 7: Write Inverse Checkerboard Pattern\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        bram_ptr[i] = (i % 2) ? 0x55555555u : 0xAAAAAAAAu;
    }
    printf("Inverse checkerboard pattern written to BRAM\n");

    // Test 8: Verify inverse checkerboard
    printf("Test 8: Verify Inverse Checkerboard Pattern\n");
    errors = 0;
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t expected = (i % 2) ? 0x55555555u : 0xAAAAAAAAu;
        uint32_t read_val = bram_ptr[i];
        if (read_val != expected) {
            errors++;
            if (errors <= 5) {
                printf("Error at index %u: expected 0x%08X, got 0x%08X\n", 
                       i, expected, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("Inverse Checkerboard Test PASSED!\n");
    } else {
        printf("Inverse Checkerboard Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0004u | (errors & 0xFF);
        return 1;
    }

    // Test 9: Random-like pattern (using multiplicative congruential generator)
    printf("\nTest 9: Write Pseudo-Random Pattern\n");
    uint32_t seed = 0x12345678u;
    for (uint32_t i = 0; i < num_elements; i++) {
        seed = (seed * 1103515245u + 12345u) & 0x7FFFFFFFu;
        bram_ptr[i] = seed;
    }
    printf("Pseudo-random pattern written to BRAM\n");

    // Test 10: Verify random pattern
    printf("Test 10: Verify Pseudo-Random Pattern\n");
    errors = 0;
    seed = 0x12345678u;
    for (uint32_t i = 0; i < num_elements; i++) {
        seed = (seed * 1103515245u + 12345u) & 0x7FFFFFFFu;
        uint32_t expected = seed;
        uint32_t read_val = bram_ptr[i];
        if (read_val != expected) {
            errors++;
            if (errors <= 5) {
                printf("Error at index %u: expected 0x%08X, got 0x%08X\n", 
                       i, expected, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("Pseudo-Random Test PASSED!\n");
    } else {
        printf("Pseudo-Random Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0005u | (errors & 0xFF);
        return 1;
    }

    // Test 11: All zeros
    printf("\nTest 11: Write All Zeros\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        bram_ptr[i] = 0x00000000u;
    }
    printf("All zeros written to BRAM\n");

    // Test 12: Verify all zeros
    printf("Test 12: Verify All Zeros\n");
    errors = 0;
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t read_val = bram_ptr[i];
        if (read_val != 0x00000000u) {
            errors++;
            if (errors <= 5) {
                printf("Error at index %u: expected 0x00000000, got 0x%08X\n", 
                       i, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("All Zeros Test PASSED!\n");
    } else {
        printf("All Zeros Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0006u | (errors & 0xFF);
        return 1;
    }

    // Test 13: All ones
    printf("\nTest 13: Write All Ones\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        bram_ptr[i] = 0xFFFFFFFFu;
    }
    printf("All ones written to BRAM\n");

    // Test 14: Verify all ones
    printf("Test 14: Verify All Ones\n");
    errors = 0;
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t read_val = bram_ptr[i];
        if (read_val != 0xFFFFFFFFu) {
            errors++;
            if (errors <= 5) {
                printf("Error at index %u: expected 0xFFFFFFFF, got 0x%08X\n", 
                       i, read_val);
            }
            if (errors > 5) break;
        }
    }
    if (errors == 0) {
        printf("All Ones Test PASSED!\n");
    } else {
        printf("All Ones Test FAILED! Errors: %u\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADB0007u | (errors & 0xFF);
        return 1;
    }

    printf("\nAll BRAM Tests PASSED!\n");
    printf("Total BRAM tested: %u KB from offset 0x%X\n", TEST_SIZE / 1024, BRAM_TEST_OFFSET);
    TEST_STATUS_ADDR[0] = 0x00000000u;
    return 0;
}
