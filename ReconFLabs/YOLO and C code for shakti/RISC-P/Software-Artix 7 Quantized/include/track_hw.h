#ifndef TRACK_HW_H
#define TRACK_HW_H

#include <stdint.h>
#include "bbox_hw.h"

#define MAX_TRACKS       16
#define MAX_MEASUREMENTS 48
#define IOU_THRESHOLD    0.3f
#define SIZE_RATIO_MIN   0.5f
#define SIZE_RATIO_MAX   2.0f
#define MAX_MISSES       3

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float w;
    float h;
    int missed;
    uint16_t id;
} TrackState;

// Accessors for current track buffer and count.
const TrackState* track_hw_get_tracks(int *count_out);

// Reset all tracks.
void track_hw_reset(void);

// Predict step for all tracks.
void track_hw_predict(float dt);

// Associate detections with tracks, update states, and create new tracks for unassigned detections.
void track_hw_associate_update(const BoundingBox *meas, int meas_count, float dt);

// Drop tracks that exceeded MAX_MISSES.
void track_hw_compact(void);

#endif
