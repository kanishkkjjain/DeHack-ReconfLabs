// Standalone stress test: large-scale YOLO decode + measurement filtering
// Generates many predictions, decodes to bboxes, applies confidence + NMS filtering

#include <stdint.h>
#include <stdio.h>
#include <math.h>

// Types
typedef struct {
    float x_min;
    float y_min;
    float x_max;
    float y_max;
    float confidence;
    int class_id;
} BoundingBox;

#define MAX_BOXES 1024  // Large buffer for decoded boxes
#define CONF_THRESHOLD 0.4f
#define NMS_THRESHOLD 0.5f
#define MAX_PRED_ELEMENTS (26 * 26 * 3 * 6)  // Largest grid (26x26) with 3 anchors x 6 values

// YOLOv3-tiny anchors and strides
static const int yolo_tiny_anchors[2][3][2] = {
    { {81, 82}, {135, 169}, {344, 319} },   // 13x13 head
    { {10, 14}, {23, 27}, {37, 58} }        // 26x26 head
};

static const int yolo_tiny_strides[2] = {32, 16};

// Helpers
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

static float bbox_hw_iou(const BoundingBox *a, const BoundingBox *b) {
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

static int bbox_hw_nms(BoundingBox *boxes, int count, float iou_thresh) {
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

// YOLO decode: converts raw predictions to bounding boxes
static int bbox_hw_decode_head(const float *pred, int grid, float conf_thresh, 
                               int head_id, BoundingBox *out, int max_out) {
    const int num_anchors = 3;
    const int stride = yolo_tiny_strides[head_id];
    int written = 0;

    for (int gy = 0; gy < grid; gy++) {
        for (int gx = 0; gx < grid; gx++) {
            for (int a = 0; a < num_anchors; a++) {
                int idx = ((gy * grid + gx) * num_anchors + a) * 6;
                float obj = fast_sigmoid(pred[idx + 4]);
                if (obj < conf_thresh) continue;

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

// Filter measurements: confidence + NMS
static int filter_measurements(BoundingBox *boxes, int count, float conf_thresh, float nms_thresh) {
    // Step 1: Confidence filter
    int filtered = 0;
    for (int i = 0; i < count; i++) {
        if (boxes[i].confidence >= conf_thresh) {
            if (filtered != i) boxes[filtered] = boxes[i];
            filtered++;
        }
    }
    
    // Step 2: NMS on filtered boxes
    int final_count = bbox_hw_nms(boxes, filtered, nms_thresh);
    return final_count;
}

// Pseudo-random number generator (simple LCG)
static uint32_t rand_seed = 12345;
static float rand_float(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (float)(rand_seed & 0x7FFFFFFF) / 0x7FFFFFFF;
}

int main(void) {
    printf("=== Stress Test: Large-scale YOLO Decode + Filtering ===\n\n");

    // Static buffer for predictions
    static float pred_buffer[MAX_PRED_ELEMENTS];
    
    // Test parameters
    int grid_sizes[] = {13, 26};  // YOLOv3-tiny heads
    int num_heads = 2;
    
    BoundingBox all_boxes[MAX_BOXES];
    int total_boxes = 0;

    printf("Phase 1: Decode YOLO predictions for both heads\n");
    
    for (int h = 0; h < num_heads; h++) {
        int grid = grid_sizes[h];
        int grid_elements = grid * grid * 3 * 6;  // 3 anchors, 6 values each
        
        printf("  Head %d (grid %dx%d): ", h, grid, grid);
        
        if (grid_elements > MAX_PRED_ELEMENTS) {
            printf("ERROR: grid_elements %d exceeds buffer\n", grid_elements);
            return 1;
        }
        
        // Fill prediction buffer with random values
        for (int i = 0; i < grid_elements; i++) {
            pred_buffer[i] = (rand_float() - 0.5f) * 4.0f;
        }
        
        // Decode with moderate confidence threshold
        int boxes_decoded = bbox_hw_decode_head(pred_buffer, grid, CONF_THRESHOLD, h, 
                                                &all_boxes[total_boxes], 
                                                MAX_BOXES - total_boxes);
        total_boxes += boxes_decoded;
        printf("decoded %d boxes (running total: %d)\n", boxes_decoded, total_boxes);
    }

    printf("\nPhase 2: Confidence filtering + NMS on all %d boxes\n", total_boxes);
    
    // Record before state
    int before_nms = total_boxes;
    
    // Apply confidence filter (already done in decode, but re-verify)
    int confident_count = 0;
    for (int i = 0; i < total_boxes; i++) {
        if (all_boxes[i].confidence >= CONF_THRESHOLD) {
            confident_count++;
        }
    }
    printf("  After confidence filter: %d boxes\n", confident_count);
    
    // Apply NMS
    int after_nms = bbox_hw_nms(all_boxes, total_boxes, NMS_THRESHOLD);
    printf("  After NMS (threshold %.2f): %d boxes\n", NMS_THRESHOLD, after_nms);
    
    // Statistics
    float suppression_rate = (total_boxes > 0) ? 
        (100.0f * (total_boxes - after_nms) / total_boxes) : 0.0f;
    printf("\nPhase 3: Statistics\n");
    printf("  Initial detections: %d\n", total_boxes);
    printf("  Final detections: %d\n", after_nms);
    printf("  Suppression rate: %.1f%%\n", suppression_rate);
    printf("  Avg confidence: ");
    
    float avg_conf = 0.0f;
    for (int i = 0; i < after_nms; i++) {
        avg_conf += all_boxes[i].confidence;
    }
    avg_conf = (after_nms > 0) ? (avg_conf / after_nms) : 0.0f;
    printf("%.3f\n", avg_conf);

    // Sanity checks
    if (after_nms < 0 || after_nms > total_boxes) {
        printf("\nERROR: Invalid final count %d (expected 0-%d)\n", after_nms, total_boxes);
        return 1;
    }

    // Verify all final boxes pass filters
    for (int i = 0; i < after_nms; i++) {
        if (all_boxes[i].confidence < CONF_THRESHOLD) {
            printf("\nERROR: Box %d confidence %.3f below threshold %.3f\n", 
                   i, all_boxes[i].confidence, CONF_THRESHOLD);
            return 1;
        }
    }

    printf("\n=== Stress Test PASSED ===\n");
    return 0;
}
