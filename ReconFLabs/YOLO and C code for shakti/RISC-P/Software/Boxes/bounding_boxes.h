#ifndef BOUNDING_BOXES_H
#define BOUNDING_BOXES_H

#include <stdlib.h>
#include <math.h>

// Structure to represent a single bounding box
typedef struct {
    float x_min;
    float y_min;
    float x_max;
    float y_max;
    float confidence;
    int class_id;
} BoundingBox;

// Structure to hold an array of bounding boxes
typedef struct {
    BoundingBox* boxes;
    int box_count;
} BoundingBoxArray;

// Anchors in pixels (relative to 416 input)
extern int anchors[3][3][2];

// Strides for each detection head
extern int strides[3];

// Number of classes
extern int C;

// Utility function for sigmoid activation
float sigmoid(float x);

// Generate bounding boxes from model predictions
void generate_bounding_boxes(float* predictions, int grid_size, int num_anchors, 
                            float conf_threshold, int head_id, BoundingBoxArray* bbox_array);

// Free memory allocated for bounding boxes
void free_bounding_boxes(BoundingBoxArray* bbox_array);

// Print bounding boxes to console
void print_bounding_boxes(BoundingBoxArray* bbox_array);

// Calculate Intersection over Union (IoU) between two bounding boxes
float calculate_iou(BoundingBox box1, BoundingBox box2);

// Apply non-maximum suppression to filter overlapping boxes
void non_max_suppression(BoundingBoxArray* bbox_array, float iou_threshold);

#endif // BOUNDING_BOXES_H
