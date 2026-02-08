// Standalone tracking test with all code in one file (no project headers).
// Includes bounding-box helpers (IoU, NMS) and a lightweight tracking pipeline.

#include <stdint.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Types and constants (merged from bbox_hw.h and track_hw.h)
// -----------------------------------------------------------------------------
typedef struct {
    float x_min;
    float y_min;
    float x_max;
    float y_max;
    float confidence;
    int class_id;
} BoundingBox;

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

#define MAX_TRACKS       16
#define MAX_MEASUREMENTS 48
#define IOU_THRESHOLD    0.3f
#define NMS_THRESHOLD    0.5f
#define CONF_THRESHOLD   0.4f
#define MAX_MISSES       3
#define ALPHA_GAIN       0.7f
#define BETA_GAIN        0.2f

// -----------------------------------------------------------------------------
// Bounding box helpers (from bbox_hw.c)
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

// -----------------------------------------------------------------------------
// Tracking state (DDR-resident arrays)
// -----------------------------------------------------------------------------
static TrackState   g_tracks[MAX_TRACKS] __attribute__((aligned(64)));
static BoundingBox  g_pred_boxes[MAX_TRACKS] __attribute__((aligned(64)));
static BoundingBox  g_meas_boxes[MAX_MEASUREMENTS] __attribute__((aligned(64)));
static float        g_iou[MAX_TRACKS][MAX_MEASUREMENTS] __attribute__((aligned(64)));
static int          g_track_count = 0;
static int          g_meas_count = 0;
static uint16_t     g_next_id = 1;

// -----------------------------------------------------------------------------
// Tracking helpers
// -----------------------------------------------------------------------------
static void build_pred_boxes(void) {
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

static void filter_measurements(const BoundingBox *meas_in, int meas_count_in, float conf_thresh, int target_class, float nms_thresh) {
    g_meas_count = 0;
    for (int i = 0; i < meas_count_in && g_meas_count < MAX_MEASUREMENTS; i++) {
        if (meas_in[i].confidence < conf_thresh) continue;
        if (target_class >= 0 && meas_in[i].class_id != target_class) continue;
        g_meas_boxes[g_meas_count++] = meas_in[i];
    }
    g_meas_count = bbox_hw_nms(g_meas_boxes, g_meas_count, nms_thresh);
}

static void compute_iou_matrix(void) {
    for (int i = 0; i < g_track_count; i++) {
        for (int j = 0; j < g_meas_count; j++) {
            g_iou[i][j] = bbox_hw_iou(&g_pred_boxes[i], &g_meas_boxes[j]);
        }
    }
}

static void update_track_state(TrackState *t, const BoundingBox *m, float dt) {
    // Simple alpha-beta style update
    float mx = 0.5f * (m->x_min + m->x_max);
    float my = 0.5f * (m->y_min + m->y_max);
    float mw = m->x_max - m->x_min;
    float mh = m->y_max - m->y_min;

    float rx = mx - t->x;
    float ry = my - t->y;

    t->x += ALPHA_GAIN * rx;
    t->y += ALPHA_GAIN * ry;
    t->vx += (BETA_GAIN * rx) / dt;
    t->vy += (BETA_GAIN * ry) / dt;
    t->w = mw;
    t->h = mh;
    t->missed = 0;
}

static void create_track(const BoundingBox *m) {
    if (g_track_count >= MAX_TRACKS) return;
    float mx = 0.5f * (m->x_min + m->x_max);
    float my = 0.5f * (m->y_min + m->y_max);
    float mw = m->x_max - m->x_min;
    float mh = m->y_max - m->y_min;
    TrackState *t = &g_tracks[g_track_count++];
    t->x = mx;
    t->y = my;
    t->vx = 0.0f;
    t->vy = 0.0f;
    t->w = mw;
    t->h = mh;
    t->missed = 0;
    t->id = g_next_id++;
}

// -----------------------------------------------------------------------------
// Public-ish tracking API (standalone)
// -----------------------------------------------------------------------------
static void track_hw_reset(void) {
    g_track_count = 0;
    g_meas_count = 0;
    g_next_id = 1;
}

static void track_hw_predict(float dt) {
    for (int i = 0; i < g_track_count; i++) {
        g_tracks[i].x += g_tracks[i].vx * dt;
        g_tracks[i].y += g_tracks[i].vy * dt;
    }
    build_pred_boxes();
}

static void track_hw_compact(void) {
    int w = 0;
    for (int i = 0; i < g_track_count; i++) {
        if (g_tracks[i].missed <= MAX_MISSES) {
            if (w != i) g_tracks[w] = g_tracks[i];
            w++;
        }
    }
    g_track_count = w;
}

static void track_hw_associate_update(const BoundingBox *meas_in, int meas_count_in, float dt) {
    filter_measurements(meas_in, meas_count_in, CONF_THRESHOLD, -1, NMS_THRESHOLD);
    build_pred_boxes();
    compute_iou_matrix();

    int meas_assigned[MAX_MEASUREMENTS] = {0};

    // Greedy: for each track pick best measurement above IOU threshold
    for (int i = 0; i < g_track_count; i++) {
        float best_iou = IOU_THRESHOLD;
        int best_j = -1;
        for (int j = 0; j < g_meas_count; j++) {
            if (meas_assigned[j]) continue;
            float iou = g_iou[i][j];
            if (iou > best_iou) {
                best_iou = iou;
                best_j = j;
            }
        }
        if (best_j >= 0) {
            update_track_state(&g_tracks[i], &g_meas_boxes[best_j], dt);
            meas_assigned[best_j] = 1;
        } else {
            g_tracks[i].missed += 1;
        }
    }

    // Create tracks for unassigned measurements
    for (int j = 0; j < g_meas_count; j++) {
        if (!meas_assigned[j]) {
            create_track(&g_meas_boxes[j]);
        }
    }
}

static const TrackState* track_hw_get_tracks(int *count_out) {
    if (count_out) *count_out = g_track_count;
    return g_tracks;
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------
static int run_tracking_tests(void) {
    printf("Standalone Tracking Test Starting...\n");
    track_hw_reset();
    const float dt = 1.0f;

    // Frame 1 detections -> expect 2 tracks
    BoundingBox meas1[2] = {
        {-1.0f, -1.0f, 1.0f, 1.0f, 0.90f, 0},
        {10.0f, 10.0f, 12.0f, 12.0f, 0.95f, 0}
    };
    track_hw_associate_update(meas1, 2, dt);
    int count = 0;
    track_hw_get_tracks(&count);
    if (count != 2) {
        printf("FAIL: expected 2 tracks after frame1, got %d\n", count);
        return 1;
    }

    // Frame 2: one low-confidence meas dropped, still 2 tracks
    BoundingBox meas2[3] = {
        {-0.5f, -0.5f, 1.5f, 1.5f, 0.85f, 0},
        {10.5f, 10.5f, 12.5f, 12.5f, 0.92f, 0},
        {30.0f, 30.0f, 32.0f, 32.0f, 0.30f, 0}
    };
    track_hw_predict(dt);
    track_hw_associate_update(meas2, 3, dt);
    track_hw_compact();
    track_hw_get_tracks(&count);
    if (count != 2) {
        printf("FAIL: expected 2 tracks after frame2, got %d\n", count);
        return 1;
    }

    // Frame 3: no detections -> misses accumulate
    track_hw_predict(dt);
    track_hw_associate_update(NULL, 0, dt);
    track_hw_compact();
    track_hw_get_tracks(&count);
    if (count != 2) {
        printf("FAIL: expected 2 tracks after frame3, got %d\n", count);
        return 1;
    }

    // Frame 4: still no detections -> tracks should be dropped after MAX_MISSES
    for (int i = 0; i < MAX_MISSES + 1; i++) {
        track_hw_predict(dt);
        track_hw_associate_update(NULL, 0, dt);
        track_hw_compact();
    }
    track_hw_get_tracks(&count);
    if (count != 0) {
        printf("FAIL: expected tracks removed after misses, got %d\n", count);
        return 1;
    }

    printf("Standalone Tracking Test PASSED\n");
    return 0;
}

int main(void) {
    int rc = run_tracking_tests();
    return rc;
}
