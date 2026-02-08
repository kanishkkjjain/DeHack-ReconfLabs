#include <stdint.h>
#include "../include/bbox_hw.h"
#include "../include/mem_map.h"
#include "uart.h"

#define TEST_STATUS_ADDR STATUS_BBOX_ADDR

static int approx_equal(float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
}

int main(void) {
    printf("Bounding Box HW Test Starting...\n");
    
    // IoU check.
    printf("Test 1: IoU Calculation\n");
    BoundingBox a = {0.0f, 0.0f, 2.0f, 2.0f, 1.0f, 0};
    BoundingBox b = {1.0f, 1.0f, 3.0f, 3.0f, 1.0f, 0};
    float iou = bbox_hw_iou(&a, &b);
    printf("IoU Result: %.3f\n", iou);
    if (!approx_equal(iou, 0.142f, 0.01f)) {
        printf("IoU Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD20001u;
        return 1;
    }
    printf("IoU Test PASSED!\n");

    // NMS check (expect suppression to leave 2 boxes).
    printf("Test 2: Non-Maximum Suppression\n");
    BoundingBox nms_boxes[3] = {
        {0.0f, 0.0f, 2.0f, 2.0f, 0.9f, 0},
        {0.2f, 0.2f, 2.2f, 2.2f, 0.8f, 0},
        {5.0f, 5.0f, 6.0f, 6.0f, 0.7f, 0}
    };
    int remain = bbox_hw_nms(nms_boxes, 3, 0.3f);
    printf("NMS remaining boxes: %d (expected 2)\n", remain);
    if (remain != 2) {
        printf("NMS Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD20002u;
        return 1;
    }
    printf("NMS Test PASSED!\n");

    // Decode head smoke test (grid=1, head 0) with neutral logits -> expect 3 boxes written.
    printf("Test 3: Decode Head\n");
    float pred[18];
    for (int i = 0; i < 18; i++) {
        pred[i] = 0.0f;
    }
    int written = bbox_hw_decode_head(pred, 1, 0.4f, 0, nms_boxes, 3);
    printf("Decode head boxes written: %d (expected 3)\n", written);
    if (written != 3) {
        printf("Decode Head Test FAILED!\n");
        TEST_STATUS_ADDR[0] = 0xBAD20003u;
        return 1;
    }
    printf("Decode Head Test PASSED!\n");

    printf("All Bounding Box HW Tests PASSED!\n");
    TEST_STATUS_ADDR[0] = 0x00000000u;
    return 0;
}
