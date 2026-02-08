#include "../include/bbox_hw.h"

const int yolo_tiny_anchors[2][3][2] = {
    { {81, 82}, {135, 169}, {344, 319} },
    { {10, 14}, {23, 27}, {37, 58} }
};

const int yolo_tiny_strides[2] = {32, 16};

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

static float fast_sigmoid(float x) {
    float e = fast_exp(-x);
    return 1.0f / (1.0f + e);
}

int bbox_hw_decode_head(const float *pred, int grid, float conf_thresh, int head_id, int num_classes, BoundingBox *out, int max_out) {
    const int num_anchors = 3;
    const int stride = yolo_tiny_strides[head_id];
    const int values_per_anchor = 5 + num_classes;  // 4 box + 1 obj + num_classes
    int written = 0;

    for (int gy = 0; gy < grid; gy++) {
        for (int gx = 0; gx < grid; gx++) {
            for (int a = 0; a < num_anchors; a++) {
                int idx = ((gy * grid + gx) * num_anchors + a) * values_per_anchor;
                float obj = fast_sigmoid(pred[idx + 4]);
                
                // Find best class and combine with objectness
                float best_score = 0.0f;
                int best_class = 0;
                for (int c = 0; c < num_classes; c++) {
                    float cls_conf = fast_sigmoid(pred[idx + 5 + c]);
                    float score = obj * cls_conf;
                    if (score > best_score) {
                        best_score = score;
                        best_class = c;
                    }
                }
                
                if (best_score < conf_thresh) {
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
                box.confidence = best_score;
                box.class_id = best_class;

                if (written < max_out) {
                    out[written++] = box;
                }
            }
        }
    }
    return written;
}
