#ifndef BBOX_HW_H
#define BBOX_HW_H

#include <stdint.h>

// YOLOv3-tiny anchor definitions (pixels for 416x416 input) and strides.
extern const int yolo_tiny_anchors[2][3][2];
extern const int yolo_tiny_strides[2];

typedef struct {
    float x_min;
    float y_min;
    float x_max;
    float y_max;
    float confidence;
    int class_id;
} BoundingBox;

// Compute IoU between two boxes.
float bbox_hw_iou(const BoundingBox *a, const BoundingBox *b);

// In-place NMS. Returns remaining count after suppression.
int bbox_hw_nms(BoundingBox *boxes, int count, float iou_thresh);

// Decode one YOLO-tiny head into bounding boxes; returns boxes written.
// num_classes: number of classes (1 for single-class, 80 for COCO)
int bbox_hw_decode_head(const float *pred, int grid, float conf_thresh, int head_id, int num_classes, BoundingBox *out, int max_out);

#endif
