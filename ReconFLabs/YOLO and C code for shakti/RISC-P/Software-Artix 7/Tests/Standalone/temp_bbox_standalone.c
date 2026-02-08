// Standalone bounding box test with all code in one file (no .h includes).
// Contains bbox implementations (IoU, NMS, decode) and a minimal test main.

#include <stdint.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Minimal types and constants (was bbox_hw.h)
// -----------------------------------------------------------------------------
typedef struct {
    float x_min;
    float y_min;
    float x_max;
    float y_max;
    float confidence;
    int class_id;
} BoundingBox;

// YOLOv3-tiny anchors and strides (was bbox_hw.c global data)
static const int yolo_tiny_anchors[2][3][2] = {
    { {81, 82}, {135, 169}, {344, 319} },   // 13x13 head
    { {10, 14}, {23, 27}, {37, 58} }        // 26x26 head
};

static const int yolo_tiny_strides[2] = {32, 16};

// -----------------------------------------------------------------------------
// Helpers (from bbox_hw.c)
// -----------------------------------------------------------------------------
static float fast_exp(float x) {
    const float ln2 = 0.69314718f;
    float y = x / ln2;
    int yi = (int)y;
    float yf = y - yi;
    float p = 1.0f + yf * (0.6930f + yf * (0.2402f + 0.0555f * yf));
    union { uint32_t u; float f; } conv;
    conv.u = (uint32_t)((yi + 127) << 23);
    return conv.f * p;
}

static float fast_sigmoid(float x) {
    float e = fast_exp(-x);
    return 1.0f / (1.0f + e);
}

// -----------------------------------------------------------------------------
// bbox API implementations (from bbox_hw.c)
// -----------------------------------------------------------------------------
float bbox_hw_iou(const BoundingBox *a, const BoundingBox *b) {
    float xA = (a->x_min > b->x_min) ? a->x_min : b->x_min;
    float yA = (a->y_min > b->y_min) ? a->y_min : b->y_min;
    float xB = (a->x_max < b->x_max) ? a->x_max : b->x_max;
    float yB = (a->y_max < b->y_max) ? a->y_max : b->y_max;

    float inter_w = xB - xA;
    float inter_h = yB - yA;
    if (inter_w <= 0.0f || inter_h <= 0.0f) {
        return 0.0f;
    }

    float inter = inter_w * inter_h;
    float area_a = (a->x_max - a->x_min) * (a->y_max - a->y_min);
    float area_b = (b->x_max - b->x_min) * (b->y_max - b->y_min);
    float denom = area_a + area_b - inter;
    return (denom > 0.0f) ? (inter / denom) : 0.0f;
}

int bbox_hw_nms(BoundingBox *boxes, int count, float iou_thresh) {
    // Greedy NMS; boxes expected pre-sorted by confidence externally.
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (bbox_hw_iou(&boxes[i], &boxes[j]) > iou_thresh) {
                for (int k = j; k < count - 1; k++) {
                    boxes[k] = boxes[k + 1];
                }
                count--;
                j--;
            }
        }
    }
    return count;
}

int bbox_hw_decode_head(const float *pred, int grid, float conf_thresh, int head_id, BoundingBox *out, int max_out) {
    const int num_anchors = 3;
    const int stride = yolo_tiny_strides[head_id];
    int written = 0;

    for (int gy = 0; gy < grid; gy++) {
        for (int gx = 0; gx < grid; gx++) {
            for (int a = 0; a < num_anchors; a++) {
                int idx = ((gy * grid + gx) * num_anchors + a) * 6; // 5 + 1 class
                float obj = fast_sigmoid(pred[idx + 4]);
                if (obj < conf_thresh) {
                    continue;
                }

                float bx = (fast_sigmoid(pred[idx + 0]) + gx) * stride;
                float by = (fast_sigmoid(pred[idx + 1]) + gy) * stride;
                float bw = fast_exp(pred[idx + 2]) * (float)yolo_tiny_anchors[head_id][a][0];
                float bh = fast_exp(pred[idx + 3]) * (float)yolo_tiny_anchors[head_id][a][1];

                BoundingBox box;
                box.x_min = bx - bw * 0.5f;
                box.y_min = by - bh * 0.5f;
                box.x_max = bx + bw * 0.5f;
                box.y_max = by + bh * 0.5f;
                box.confidence = obj;
                box.class_id = 0;

                if (written < max_out) {
                    out[written++] = box;
                }
            }
        }
    }
    return written;
}

// -----------------------------------------------------------------------------
// Local test helpers
// -----------------------------------------------------------------------------
static int approx_equal(float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
}

// -----------------------------------------------------------------------------
// Main test
// -----------------------------------------------------------------------------
int main(void) {
    printf("Standalone BBox Test Starting...\n");

    // Test 1: IoU
    BoundingBox a = {0.0f, 0.0f, 2.0f, 2.0f, 1.0f, 0};
    BoundingBox b = {1.0f, 1.0f, 3.0f, 3.0f, 1.0f, 0};
    float iou = bbox_hw_iou(&a, &b);
    printf("IoU: %.4f (expected ~0.1429)\n", iou);
    if (!approx_equal(iou, 1.0f / 7.0f, 0.01f)) {
        printf("IoU Test FAILED\n");
        return 1;
    }

    // Test 2: NMS
    BoundingBox boxes[3] = {
        {0.0f, 0.0f, 2.0f, 2.0f, 0.90f, 0},
        {0.2f, 0.2f, 2.2f, 2.2f, 0.80f, 0},
        {5.0f, 5.0f, 6.0f, 6.0f, 0.70f, 0}
    };
    int remain = bbox_hw_nms(boxes, 3, 0.3f);
    printf("NMS remaining: %d (expected 2)\n", remain);
    if (remain != 2) {
        printf("NMS Test FAILED\n");
        return 1;
    }

    // Test 3: Decode head smoke test (grid=1, head 0)
    float pred[18];
    for (int i = 0; i < 18; i++) pred[i] = 0.0f;
    int written = bbox_hw_decode_head(pred, 1, 0.4f, 0, boxes, 3);
    printf("Decode head written: %d (expected 3)\n", written);
    if (written != 3) {
        printf("Decode Test FAILED\n");
        return 1;
    }

    printf("Standalone BBox Test PASSED\n");
    return 0;
}
