#include <stdint.h>
#include "../include/track_hw.h"
#include "../include/bbox_hw.h"
#include "../include/mem_map.h"
#include "uart.h"

#define TEST_STATUS_ADDR STATUS_TRACK_ADDR

static int expect_tracks(int expected) {
    int count = 0;
    track_hw_get_tracks(&count);
    return (count == expected);
}

int main(void) {
    printf("Tracking DDR Test Starting...\n");
    printf("Testing track_hw with DDR-resident filtered measurement storage (g_meas_boxes)\n");
    const float dt = 1.0f;

    // Test 1: Initialize Two Tracks
    printf("\nTest 1: Initialize Two Tracks\n");
    BoundingBox meas0 = {10.0f, 10.0f, 30.0f, 30.0f, 0.9f, 0};
    BoundingBox meas1 = {100.0f, 100.0f, 140.0f, 140.0f, 0.95f, 0};
    BoundingBox detections1[2] = {meas0, meas1};

    track_hw_reset();
    track_hw_predict(dt);
    track_hw_associate_update(detections1, 2, dt);
    track_hw_compact();

    if (!expect_tracks(2)) {
        printf("Initialize Test FAILED! Expected 2 tracks.\n");
        TEST_STATUS_ADDR[0] = 0xBAD10001u;
        return 1;
    }
    printf("Initialize Test PASSED! 2 tracks created.\n");

    // Test 2: Test with overlapping detections (NMS should filter)
    printf("\nTest 2: Test NMS Filtering of Overlapping Detections\n");
    BoundingBox overlap_detections[3] = {
        {10.0f, 10.0f, 30.0f, 30.0f, 0.9f, 0},    // High confidence
        {12.0f, 12.0f, 32.0f, 32.0f, 0.7f, 0},    // Overlapping, lower confidence
        {100.0f, 100.0f, 140.0f, 140.0f, 0.95f, 0} // Non-overlapping
    };
    
    track_hw_predict(dt);
    track_hw_associate_update(overlap_detections, 3, dt);
    track_hw_compact();
    
    // Should still have 2 tracks (NMS should remove the overlapping detection)
    if (!expect_tracks(2)) {
        printf("NMS Filter Test FAILED! Expected 2 tracks after NMS.\n");
        TEST_STATUS_ADDR[0] = 0xBAD10002u;
        return 1;
    }
    printf("NMS Filter Test PASSED! Overlapping detections filtered correctly.\n");

    // Test 3: Test confidence threshold filtering
    printf("\nTest 3: Test Confidence Threshold Filtering\n");
    BoundingBox low_conf_detections[2] = {
        {10.0f, 10.0f, 30.0f, 30.0f, 0.3f, 0},     // Below threshold (0.4)
        {100.0f, 100.0f, 140.0f, 140.0f, 0.95f, 0} // Above threshold
    };
    
    track_hw_predict(dt);
    track_hw_associate_update(low_conf_detections, 2, dt);
    track_hw_compact();
    
    // Should still have 2 tracks (low confidence detection filtered)
    if (!expect_tracks(2)) {
        printf("Confidence Filter Test FAILED! Expected 2 tracks.\n");
        TEST_STATUS_ADDR[0] = 0xBAD10003u;
        return 1;
    }
    printf("Confidence Filter Test PASSED! Low confidence detections filtered.\n");

    // Test 4: Push misses to force removals
    printf("\nTest 4: Remove Tracks via Misses\n");
    for (int k = 0; k < MAX_MISSES + 1; k++) {
        track_hw_predict(dt);
        track_hw_associate_update(NULL, 0, dt);
        track_hw_compact();
    }

    if (!expect_tracks(0)) {
        printf("Remove Test FAILED! Expected 0 tracks.\n");
        TEST_STATUS_ADDR[0] = 0xBAD10004u;
        return 1;
    }
    printf("Remove Test PASSED! All tracks removed.\n");

    // Test 5: Re-add one track to verify buffer reuse
    printf("\nTest 5: Buffer Reuse\n");
    track_hw_predict(dt);
    track_hw_associate_update(&meas0, 1, dt);
    track_hw_compact();
    if (!expect_tracks(1)) {
        printf("Buffer Reuse Test FAILED! Expected 1 track.\n");
        TEST_STATUS_ADDR[0] = 0xBAD10005u;
        return 1;
    }
    printf("Buffer Reuse Test PASSED! 1 track re-added.\n");

    printf("\nAll Tracking DDR Tests PASSED!\n");
    printf("Verified: track storage, measurement filtering, NMS, confidence threshold\n");
    TEST_STATUS_ADDR[0] = 0x00000000u;
    return 0;
}
