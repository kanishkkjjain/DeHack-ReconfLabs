#include "../include/track_hw.h"

#define ALPHA_GAIN 0.7f
#define BETA_GAIN  0.2f

// DDR-resident data structures for track processing
// g_tracks[16]      - Active track states (position, velocity, size, ID)
// g_pred_boxes[16]  - Predicted bounding boxes computed from tracks
// g_meas_boxes[48]  - Filtered measurements (after confidence/class/NMS filtering)
// g_iou[16][48]     - IoU matrix for track-to-measurement matching

static TrackState g_tracks[MAX_TRACKS] __attribute__((aligned(64)));
static BoundingBox g_pred_boxes[MAX_TRACKS] __attribute__((aligned(64)));
static BoundingBox g_meas_boxes[MAX_MEASUREMENTS] __attribute__((aligned(64)));  // Filtered measurements
static float g_iou[MAX_TRACKS][MAX_MEASUREMENTS] __attribute__((aligned(64)));

//TODO: Optimize memory usage by reusing measurement storage.
static int g_track_count = 0;
static int g_meas_count = 0;  // Number of filtered measurements
static uint16_t g_next_id = 1;

static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

static void build_pred_boxes(void) {
    // Build bounding boxes from current track states for IoU computation.
    for (int i = 0; i < g_track_count; i++) {
        const TrackState *t = &g_tracks[i];
        float half_w = 0.5f * (t->w > 0.0f ? t->w : 50.0f);
        float half_h = 0.5f * (t->h > 0.0f ? t->h : 50.0f);
        g_pred_boxes[i].x_min = t->x - half_w;
        g_pred_boxes[i].x_max = t->x + half_w;
        g_pred_boxes[i].y_min = t->y - half_h;
        g_pred_boxes[i].y_max = t->y + half_h;
        g_pred_boxes[i].confidence = 1.0f;
        g_pred_boxes[i].class_id = 0;
    }
}

static void filter_measurements(const BoundingBox *meas_in, int meas_count_in, 
                                 float conf_thresh, int target_class, float nms_thresh) {
    // Filter measurements by confidence and class, then apply NMS to remove overlaps.
    // This ensures each object has only one clean detection before track association.
    
    // Step 1: Filter by confidence and class (optional)
    g_meas_count = 0;
    for (int i = 0; i < meas_count_in && g_meas_count < MAX_MEASUREMENTS; i++) {
        // Check confidence threshold
        if (meas_in[i].confidence < conf_thresh) {
            continue;
        }
        
        // Check class (if target_class >= 0, only accept that class; -1 = accept all)
        if (target_class >= 0 && meas_in[i].class_id != target_class) {
            continue;
        }
        
        // Copy to filtered measurement array
        g_meas_boxes[g_meas_count++] = meas_in[i];
    }
    
    // Step 2: Apply NMS to remove overlapping detections within measurements
    g_meas_count = bbox_hw_nms(g_meas_boxes, g_meas_count, nms_thresh);
}

const TrackState* track_hw_get_tracks(int *count_out) {
    if (count_out) {
        *count_out = g_track_count;
    }
    return g_tracks;
}

void track_hw_reset(void) {
    g_track_count = 0;
    g_next_id = 1;
}

void track_hw_predict(float dt) {
    for (int i = 0; i < g_track_count; i++) {
        g_tracks[i].x += g_tracks[i].vx * dt;
        g_tracks[i].y += g_tracks[i].vy * dt;
    }
    build_pred_boxes();
}

static void compute_iou_matrix(void) {
    // Compute IoU between predicted track boxes and filtered measurement boxes
    for (int i = 0; i < g_track_count; i++) {
        for (int j = 0; j < g_meas_count; j++) {
            g_iou[i][j] = bbox_hw_iou(&g_pred_boxes[i], &g_meas_boxes[j]);
        }
    }
}

static int size_ratio_ok(const BoundingBox *pred, const BoundingBox *meas) {
    float pw = pred->x_max - pred->x_min;
    float ph = pred->y_max - pred->y_min;
    float mw = meas->x_max - meas->x_min;
    float mh = meas->y_max - meas->y_min;
    if (mw <= 0.0f || mh <= 0.0f || pw <= 0.0f || ph <= 0.0f) {
        return 1;
    }
    float rw = pw / mw;
    float rh = ph / mh;
    return (rw >= SIZE_RATIO_MIN && rw <= SIZE_RATIO_MAX && rh >= SIZE_RATIO_MIN && rh <= SIZE_RATIO_MAX);
}

static void update_track_state(TrackState *t, const BoundingBox *pred_box, const BoundingBox *meas_box, float dt) {

    // Kalman filter update step.

    float pred_x = 0.5f * (pred_box->x_min + pred_box->x_max);
    float pred_y = 0.5f * (pred_box->y_min + pred_box->y_max);
    float meas_x = 0.5f * (meas_box->x_min + meas_box->x_max);
    float meas_y = 0.5f * (meas_box->y_min + meas_box->y_max);

    float innov_x = meas_x - pred_x;
    float innov_y = meas_y - pred_y;

    t->x = pred_x + ALPHA_GAIN * innov_x;
    t->y = pred_y + ALPHA_GAIN * innov_y;
    t->vx += (BETA_GAIN / dt) * innov_x;
    t->vy += (BETA_GAIN / dt) * innov_y;
    t->w = meas_box->x_max - meas_box->x_min;
    t->h = meas_box->y_max - meas_box->y_min;
    t->missed = 0;
}

static void create_track(const BoundingBox *meas_box) {

    // Create a new track from the measurement.
    // Track stored at the end of the current track list.

    if (g_track_count >= MAX_TRACKS) {
        return;
    }
    TrackState *t = &g_tracks[g_track_count++]; // Allocate new track, then increment count.
    t->x = 0.5f * (meas_box->x_min + meas_box->x_max);
    t->y = 0.5f * (meas_box->y_min + meas_box->y_max);
    t->vx = 0.0f;
    t->vy = 0.0f;
    t->w = meas_box->x_max - meas_box->x_min;
    t->h = meas_box->y_max - meas_box->y_min;
    t->missed = 0;
    t->id = g_next_id++;
}

void track_hw_associate_update(const BoundingBox *meas_in, int meas_count_in, float dt) {
    // Filter measurements: confidence threshold, class filtering, and NMS
    filter_measurements(meas_in, meas_count_in, 0.4f, -1, 0.5f);
    // Params: conf_thresh=0.4, target_class=-1 (accept all), nms_thresh=0.5
    
    // Compute IoU matrix between predicted tracks and filtered measurements
    compute_iou_matrix();

    int assigned_meas[MAX_MEASUREMENTS];
    for (int j = 0; j < g_meas_count; j++) {
        assigned_meas[j] = 0;
    }

    for (int i = 0; i < g_track_count; i++) {
        float best_iou = 0.0f;
        int best_j = -1;
        for (int j = 0; j < g_meas_count; j++) {
            if (assigned_meas[j]) {
                continue;
            }
            float iou = g_iou[i][j];
            if (iou > best_iou && iou >= IOU_THRESHOLD && size_ratio_ok(&g_pred_boxes[i], &g_meas_boxes[j])) {
                best_iou = iou;
                best_j = j;
            }
        }

        if (best_j >= 0) {
            update_track_state(&g_tracks[i], &g_pred_boxes[i], &g_meas_boxes[best_j], dt);
            assigned_meas[best_j] = 1;
        } else {
            g_tracks[i].missed++;
        }
    }

    // Create new tracks for unassigned detections.
    for (int j = 0; j < g_meas_count; j++) {
        if (!assigned_meas[j]) {
            create_track(&g_meas_boxes[j]);
        }
    }

    build_pred_boxes();
}

void track_hw_compact(void) {
    for (int i = 0; i < g_track_count; i++) {
        if (g_tracks[i].missed > MAX_MISSES) {
            // Remove track by shifting others down.
            for (int j = i; j < g_track_count - 1; j++) {
                g_tracks[j] = g_tracks[j + 1];
                g_pred_boxes[j] = g_pred_boxes[j + 1];
            }
            g_track_count--; // Decrement track count.
            i--;
        }
    }
}
