#include <stdint.h>
#include "uart.h"
#include "../include/mem_map.h"

// Test 1MB of memory (adjust based on your needs)
#define TEST_SIZE (1024 * 1024) 
#define TEST_STATUS_ADDR STATUS_DDR_ADDR

int main() {
    // Use 'volatile' to prevent the compiler from optimizing out memory accesses
    uint32_t *memory_ptr = (uint32_t *)DDR_BASE;
    uint32_t num_elements = TEST_SIZE / sizeof(uint32_t);
    uint32_t errors = 0;

    // Debug prints
    printf("DDR Test Starting...\n");
    printf("Test Size: %u bytes\n", TEST_SIZE);
    printf("Number of elements: %u\n", num_elements);

    // Step 1: Write Pattern
    printf("Step 1: Writing pattern to DDR...\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        // Pattern: Each address stores its own index
        memory_ptr[i] = i; 
    }

    // Step 2: Read and Verify
    printf("Step 2: Reading and verifying DDR...\n");
    for (uint32_t i = 0; i < num_elements; i++) {
        uint32_t read_val = memory_ptr[i];
        if (read_val != i) {
            errors++;
            if (errors <= 10) {
                printf("Error at index %u: expected %u, got %u\n", i, i, read_val);
            }
            if (errors > 10) break;
        }
    }

    printf("Verification complete. Total errors: %u\n", errors);

    if (errors == 0) {
        printf("DDR Test PASSED!\n");
        TEST_STATUS_ADDR[0] = 0x00000000;
    } else {
        printf("DDR Test FAILED with %u errors!\n", errors);
        TEST_STATUS_ADDR[0] = 0xBADDD000 | (errors & 0xFF);
    }

    return 0;
}