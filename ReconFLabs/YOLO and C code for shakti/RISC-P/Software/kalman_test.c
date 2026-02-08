#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "Kalman Filter/track.h"
#include "Boxes/bounding_boxes.h"

// Helper function to create a test bounding box
BoundingBox create_bbox(float x_min, float y_min, float x_max, float y_max, float confidence, int class_id) {
    BoundingBox bbox;
    bbox.x_min = x_min;
    bbox.y_min = y_min;
    bbox.x_max = x_max;
    bbox.y_max = y_max;
    bbox.confidence = confidence;
    bbox.class_id = class_id;
    return bbox;
}

// Helper function to print track information
void print_track(Track* track) {
    printf("    Track %d: pos=(%.2f, %.2f), vel=(%.2f, %.2f), missed=%d\n",
           track->id, track->x, track->y, track->vx, track->vy, track->missed_frames);
}

int main() {
    printf("=== Kalman Filter Tracking Test ===\n\n");
    
    // Initialize track array
    TrackArray track_array = {
        .tracks = (Track*)malloc(100 * sizeof(Track)),
        .track_count = 0,
        .max_tracks = 100
    };
    
    if (track_array.tracks == NULL) {
        printf("Error: Failed to allocate memory for tracks\n");
        return 1;
    }
    
    // ============ FRAME 1: Initial Detections ============
    printf("FRAME 1: Initial detections\n");
    
    BoundingBox frame1_detections[] = {
        create_bbox(10.0f, 20.0f, 50.0f, 60.0f, 0.95f, 0),
        create_bbox(100.0f, 50.0f, 140.0f, 90.0f, 0.92f, 1)
    };
    
    BoundingBoxArray meas_boxes_frame1 = {
        .boxes = frame1_detections,
        .box_count = 2
    };
    
    // Allocation tracking arrays
    bool* assigned_detections1 = (bool*)calloc(2, sizeof(bool));
    
    // No predicted boxes on frame 1, so just create new tracks
    generate_new_tracks(&track_array, &meas_boxes_frame1, assigned_detections1);
    
    printf("  Created %d new tracks:\n", track_array.track_count);
    for (int i = 0; i < track_array.track_count; i++) {
        print_track(&track_array.tracks[i]);
    }
    
    free(assigned_detections1);
    
    // ============ FRAME 2: Predict and Update ============
    printf("\nFRAME 2: Predict and update with new detections\n");
    
    // Allocate predicted boxes array
    BoundingBox* pred_boxes_frame2 = (BoundingBox*)malloc(track_array.track_count * sizeof(BoundingBox));
    
    BoundingBoxArray pred_boxes = {
        .boxes = pred_boxes_frame2,
        .box_count = 0
    };
    
    // Predict next positions
    predict_tracks(&track_array, &pred_boxes, DT);
    printf("  Predicted %d track positions\n", pred_boxes.box_count);
    
    // New measurement for frame 2
    BoundingBox frame2_detections[] = {
        create_bbox(12.0f, 22.0f, 52.0f, 62.0f, 0.94f, 0),
        create_bbox(105.0f, 55.0f, 145.0f, 95.0f, 0.90f, 1)
    };
    
    BoundingBoxArray meas_boxes_frame2 = {
        .boxes = frame2_detections,
        .box_count = 2
    };
    
    // Allocate IoU matrix (num_tracks x num_detections)
    float** iou_matrix2 = (float**)malloc(track_array.track_count * sizeof(float*));
    for (int i = 0; i < track_array.track_count; i++) {
        iou_matrix2[i] = (float*)calloc(meas_boxes_frame2.box_count, sizeof(float));
    }
    
    // Allocate assignment arrays
    bool* assigned_tracks2 = (bool*)calloc(track_array.track_count, sizeof(bool));
    bool* assigned_detections2 = (bool*)calloc(meas_boxes_frame2.box_count, sizeof(bool));
    
    // Generate associations (IoU calculations)
    generate_associations(&pred_boxes, &meas_boxes_frame2, 0.1f, iou_matrix2, false);
    
    // Assign detections to tracks
    assign_detections_to_tracks(&track_array, &pred_boxes, &meas_boxes_frame2, iou_matrix2, assigned_tracks2, assigned_detections2);
    
    // Create new tracks for unassigned detections
    generate_new_tracks(&track_array, &meas_boxes_frame2, assigned_detections2);
    
    printf("  Updated %d existing tracks:\n", track_array.track_count);
    for (int i = 0; i < track_array.track_count; i++) {
        print_track(&track_array.tracks[i]);
    }
    
    // Free Frame 2 memory
    for (int i = 0; i < track_array.track_count; i++) {
        free(iou_matrix2[i]);
    }
    free(iou_matrix2);
    free(assigned_tracks2);
    free(assigned_detections2);
    free(pred_boxes_frame2);
    
    // ============ FRAME 3: Another Cycle ============
    printf("\nFRAME 3: Another predict-update cycle\n");
    
    // Allocate predicted boxes for frame 3
    BoundingBox* pred_boxes_frame3 = (BoundingBox*)malloc(track_array.track_count * sizeof(BoundingBox));
    
    BoundingBoxArray pred_boxes3 = {
        .boxes = pred_boxes_frame3,
        .box_count = 0
    };
    
    // Predict
    predict_tracks(&track_array, &pred_boxes3, DT);
    printf("  Predicted %d track positions\n", pred_boxes3.box_count);
    
    // Single detection for frame 3 (first object only)
    BoundingBox frame3_detections[] = {
        create_bbox(14.0f, 24.0f, 54.0f, 64.0f, 0.93f, 0)
    };
    
    BoundingBoxArray meas_boxes_frame3 = {
        .boxes = frame3_detections,
        .box_count = 1
    };
    
    // Allocate IoU matrix
    float** iou_matrix3 = (float**)malloc(track_array.track_count * sizeof(float*));
    for (int i = 0; i < track_array.track_count; i++) {
        iou_matrix3[i] = (float*)calloc(1, sizeof(float));
    }
    
    // Allocate assignment arrays
    bool* assigned_tracks3 = (bool*)calloc(track_array.track_count, sizeof(bool));
    bool* assigned_detections3 = (bool*)calloc(1, sizeof(bool));
    
    // Generate associations
    generate_associations(&pred_boxes3, &meas_boxes_frame3, 0.1f, iou_matrix3, false);
    
    // Assign detections
    assign_detections_to_tracks(&track_array, &pred_boxes3, &meas_boxes_frame3, iou_matrix3, assigned_tracks3, assigned_detections3);
    
    // Create new tracks for unassigned
    generate_new_tracks(&track_array, &meas_boxes_frame3, assigned_detections3);
    
    printf("  Updated %d tracks:\n", track_array.track_count);
    for (int i = 0; i < track_array.track_count; i++) {
        print_track(&track_array.tracks[i]);
    }
    
    // Free Frame 3 memory
    for (int i = 0; i < track_array.track_count; i++) {
        free(iou_matrix3[i]);
    }
    free(iou_matrix3);
    free(assigned_tracks3);
    free(assigned_detections3);
    free(pred_boxes_frame3);
    
    // ============ FRAME 4: NMS Test with Overlapping Boxes ============
    printf("\nFRAME 4: NMS filtering overlapping detections\n");
    
    // Allocate predicted boxes
    BoundingBox* pred_boxes_frame4 = (BoundingBox*)malloc(track_array.track_count * sizeof(BoundingBox));
    BoundingBoxArray pred_boxes4 = {
        .boxes = pred_boxes_frame4,
        .box_count = 0
    };
    
    // Predict
    predict_tracks(&track_array, &pred_boxes4, DT);
    
    // Create detections with overlapping boxes (5 detections, but NMS will reduce)
    BoundingBox frame4_detections[] = {
        create_bbox(15.0f, 25.0f, 55.0f, 65.0f, 0.92f, 0),    // Overlaps with Track 1
        create_bbox(16.0f, 26.0f, 56.0f, 66.0f, 0.89f, 0),    // Overlaps with first detection (will be suppressed)
        create_bbox(127.0f, 77.0f, 167.0f, 117.0f, 0.88f, 1), // Overlaps with Track 2
        create_bbox(128.0f, 78.0f, 168.0f, 118.0f, 0.85f, 1), // Overlaps with third detection (will be suppressed)
        create_bbox(200.0f, 150.0f, 240.0f, 190.0f, 0.91f, 2) // New object
    };
    
    BoundingBoxArray meas_boxes_frame4_original = {
        .boxes = frame4_detections,
        .box_count = 5
    };
    
    // Copy for NMS
    BoundingBox* meas_copy_frame4 = (BoundingBox*)malloc(5 * sizeof(BoundingBox));
    for (int i = 0; i < 5; i++) {
        meas_copy_frame4[i] = frame4_detections[i];
    }
    BoundingBoxArray meas_boxes_frame4 = {
        .boxes = meas_copy_frame4,
        .box_count = 5
    };
    
    printf("  Before NMS: %d detections\n", meas_boxes_frame4.box_count);
    
    // Apply NMS
    non_max_suppression(&meas_boxes_frame4, 0.3f);  // IoU threshold 0.3
    printf("  After NMS: %d detections (overlapping boxes suppressed)\n", meas_boxes_frame4.box_count);
    
    // IoU matrix and assignment
    float** iou_matrix4 = (float**)malloc(track_array.track_count * sizeof(float*));
    for (int i = 0; i < track_array.track_count; i++) {
        iou_matrix4[i] = (float*)calloc(meas_boxes_frame4.box_count, sizeof(float));
    }
    
    bool* assigned_tracks4 = (bool*)calloc(track_array.track_count, sizeof(bool));
    bool* assigned_detections4 = (bool*)calloc(meas_boxes_frame4.box_count, sizeof(bool));
    
    // Associate and update
    generate_associations(&pred_boxes4, &meas_boxes_frame4, 0.1f, iou_matrix4, false);
    assign_detections_to_tracks(&track_array, &pred_boxes4, &meas_boxes_frame4, iou_matrix4, assigned_tracks4, assigned_detections4);
    
    printf("  Updated %d tracks:\n", track_array.track_count);
    for (int i = 0; i < track_array.track_count; i++) {
        print_track(&track_array.tracks[i]);
    }
    
    // Create new track for unassigned detection
    generate_new_tracks(&track_array, &meas_boxes_frame4, assigned_detections4);
    printf("  After creating new tracks: %d total tracks\n", track_array.track_count);
    
    // Free Frame 4 memory
    for (int i = 0; i < 2; i++) {  // Free only the 2 rows we allocated
        free(iou_matrix4[i]);
    }
    free(iou_matrix4);
    free(assigned_tracks4);
    free(assigned_detections4);
    free(pred_boxes_frame4);
    free(meas_copy_frame4);
    
    // ============ FRAMES 5-10: Simulate Missed Detections ============
    printf("\nFRAMES 5-10: Simulating missed detections (no measurements)\n");
    
    for (int frame = 5; frame <= 10; frame++) {
        // Simply increment missed frames for all tracks
        for (int i = 0; i < track_array.track_count; i++) {
            track_array.tracks[i].missed_frames++;
        }
        
        printf("  Frame %d - Tracks after missed detection:\n", frame);
        for (int i = 0; i < track_array.track_count; i++) {
            print_track(&track_array.tracks[i]);
        }
    }
    
    // ============ CLEANUP: Delete Tracks with Excessive Misses ============
    printf("\nCLEANUP: Removing tracks with > %d missed frames\n", MAX_MISSES);
    printf("Before cleanup: %d tracks\n", track_array.track_count);
    for (int i = 0; i < track_array.track_count; i++) {
        print_track(&track_array.tracks[i]);
    }
    
    cleanup_tracks(&track_array);
    printf("After cleanup: %d tracks remaining (old tracks deleted)\n", track_array.track_count);
    for (int i = 0; i < track_array.track_count; i++) {
        print_track(&track_array.tracks[i]);
    }
    
    free(track_array.tracks);
    printf("\n=== Test Complete ===\n");
    
    return 0;
}