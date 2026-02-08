#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "bounding_boxes.h"

// Anchors in pixels (relative to 416 input)
int anchors[3][3][2] = {
  // 13x13 head (large objects)
  { {116,90}, {156,198}, {373,326} },

  // 26x26 head (medium objects)
  { {30,61}, {62,45}, {59,119} },

  // 52x52 head (small objects)
  { {10,13}, {16,30}, {33,23} }
};

int strides[3] = {32, 16, 8};

int C = 80; // Number of classes

float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

//TODO: Set max limit for bbox_array->boxes before using this function
//TODO: Allocate memory for bbox_array->boxes before using this function


void generate_bounding_boxes(float* predictions, int grid_size, int num_anchors, float conf_threshold, int head_id, BoundingBoxArray* bbox_array) {
    /*
    Input:
    - predictions: Pointer to the array of predictions from the model for a specific head.
    - grid_size: Size of the grid for the current head (e.g., 13, 26, or 52).
    - num_anchors: Number of anchors per grid cell (typically 3).
    - input_size: Size of the input image (e.g., 416).
    - conf_threshold: Confidence threshold to filter boxes.
    - head_id: Identifier for the current head (0, 1, or 2).
    - bbox_array: Pointer to a BoundingBoxArray structure to store the generated boxes.
    */

    int box_index = 0;
    for (int i = 0; i < grid_size; i++) 
    {
        for (int j = 0; j < grid_size; j++) 
        {
            for (int a = 0; a < num_anchors; a++) 
            {
                box_index = ((i * grid_size + j) * num_anchors + a) * (5 + C);
                float conf = sigmoid(predictions[box_index + 4]); //TODO: Check this index

                if (conf > conf_threshold) 
                {
                    float bx = (sigmoid(predictions[box_index + 0]) + j) * strides[head_id];
                    float by = (sigmoid(predictions[box_index + 1]) + i) * strides[head_id];
                    float bw = expf(predictions[box_index + 2]) * anchors[head_id][a][0];
                    float bh = expf(predictions[box_index + 3]) * anchors[head_id][a][1];
                    
                    // Calculate corner coordinates
                    float x_min = bx - bw / 2.0f;
                    float y_min = by - bh / 2.0f;
                    float x_max = bx + bw / 2.0f;
                    float y_max = by + bh / 2.0f;

                    // Find class with highest probability
                    int class_id = 0;
                    float max_class_prob = 0.0f;
                    for (int c = 0; c < C; c++) 
                    {
                        float class_prob = sigmoid(predictions[box_index + 5 + c]);
                        if (class_prob > max_class_prob) 
                        {
                            max_class_prob = class_prob;
                            class_id = c;
                        }
                    }

                    conf *= max_class_prob;

                    if(conf > conf_threshold) 
                    {
                        BoundingBox box;
                        box.x_min = x_min;
                        box.y_min = y_min;
                        box.x_max = x_max;
                        box.y_max = y_max;
                        box.confidence = conf;
                        box.class_id = class_id;

                        bbox_array->boxes[bbox_array->box_count] = box;
                        bbox_array->box_count++;
                    }
                }
            }
        }
    }
}

void free_bounding_boxes(BoundingBoxArray* bbox_array) {
    if (bbox_array->boxes != NULL) {
        free(bbox_array->boxes);
        bbox_array->boxes = NULL;
    }
    bbox_array->box_count = 0;
}

void print_bounding_boxes(BoundingBoxArray* bbox_array) {
    for (int i = 0; i < bbox_array->box_count; i++) {
        BoundingBox box = bbox_array->boxes[i];
        printf("Box %d: [x_min: %.2f, y_min: %.2f, x_max: %.2f, y_max: %.2f, confidence: %.2f, class_id: %d]\n",
               i, box.x_min, box.y_min, box.x_max, box.y_max, box.confidence, box.class_id);
    }
}

float calculate_iou(BoundingBox box_a, BoundingBox box_b) {
    float xA = fmaxf(box_a.x_min, box_b.x_min);
    float yA = fmaxf(box_a.y_min, box_b.y_min);
    float xB = fminf(box_a.x_max, box_b.x_max);
    float yB = fminf(box_a.y_max, box_b.y_max);

    float interArea = fmaxf(0, xB - xA) * fmaxf(0, yB - yA);
    float boxAArea = (box_a.x_max - box_a.x_min) * (box_a.y_max - box_a.y_min);
    float boxBArea = (box_b.x_max - box_b.x_min) * (box_b.y_max - box_b.y_min);

    float iou = interArea / (boxAArea + boxBArea - interArea);
    return iou;
}

void non_max_suppression(BoundingBoxArray* bbox_array, float iou_threshold) {
    /*
    Input:
    - bbox_array: Pointer to a BoundingBoxArray structure containing the bounding boxes.
    - iou_threshold: IoU threshold for suppression.
    */

    // Simple NMS implementation
    for (int i = 0; i < bbox_array->box_count; i++) {
        BoundingBox box_a = bbox_array->boxes[i];
        for (int j = i + 1; j < bbox_array->box_count; j++) {
            BoundingBox box_b = bbox_array->boxes[j];

            float iou = calculate_iou(box_a, box_b);

            if (iou > iou_threshold) {
                // Suppress box_b
                for (int k = j; k < bbox_array->box_count - 1; k++) {
                    bbox_array->boxes[k] = bbox_array->boxes[k + 1];
                }
                bbox_array->box_count--;
                j--; // Adjust index after removal
            }
        }
    }
}