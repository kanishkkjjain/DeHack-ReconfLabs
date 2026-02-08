#ifndef TRACK_H
#define TRACK_H

#include <stdbool.h>
#include "../Boxes/bounding_boxes.h"

// Kalman filter parameters
#define ALPHA 0.6f  // Position smoothing (0-1, higher = trust measurement more)
#define BETA 0.4f   // Velocity correction
#define DT 1.0f     // Time step (seconds)

// Track management
#define MAX_MISSES 5

typedef struct {
    float x;
    float y;
    float vx;
    float vy;

    int missed_frames;

    int id;
} Track;

typedef struct {
    Track* tracks;
    int track_count;
    int max_tracks;
} TrackArray;

// Function declarations
void generate_associations(BoundingBoxArray* pred_boxes, BoundingBoxArray* meas_boxes, float iou_threshold, float** iou_matrix, bool size_check);
void cleanup_tracks(TrackArray* track_array);
void assign_detections_to_tracks(TrackArray* track_array, BoundingBoxArray* pred_boxes, BoundingBoxArray* meas_boxes, float** iou_matrix, bool* assigned_tracks, bool* assigned_detections);
void create_new_track(TrackArray* track_array, BoundingBox meas_box);
void update_track(Track* track, BoundingBox pred_box, BoundingBox meas_box, float dt);
void generate_new_tracks(TrackArray* track_array, BoundingBoxArray* meas_boxes, bool* assigned_detections);
void predict_tracks(TrackArray* track_array, BoundingBoxArray* pred_boxes, float dt);

#endif // TRACK_H
