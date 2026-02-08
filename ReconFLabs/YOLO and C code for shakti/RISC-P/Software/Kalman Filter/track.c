#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../Boxes/bounding_boxes.h"
#include "track.h"

void generate_associations(BoundingBoxArray* pred_boxes, BoundingBoxArray* meas_boxes, float iou_threshold, float** iou_matrix, bool size_check)
{
    for (int i = 0; i < pred_boxes->box_count; i++) {
        for (int j = 0; j < meas_boxes->box_count; j++) {
            float iou = calculate_iou(pred_boxes->boxes[i], meas_boxes->boxes[j]);
            if (iou > iou_threshold)
            {
                iou_matrix[i][j] = iou;
            }
            else
            {
                iou_matrix[i][j] = 0.0f; // No association
            }            
        }
    }

    if (size_check)
    {
        // Additional size check can be implemented here
        for (int i = 0; i < pred_boxes->box_count; i++) {
            for (int j = 0; j < meas_boxes->box_count; j++) {
                BoundingBox pred_box = pred_boxes->boxes[i];
                BoundingBox meas_box = meas_boxes->boxes[j];

                float pred_width = pred_box.x_max - pred_box.x_min;
                float pred_height = pred_box.y_max - pred_box.y_min;
                float meas_width = meas_box.x_max - meas_box.x_min;
                float meas_height = meas_box.y_max - meas_box.y_min;

                float width_ratio = pred_width / meas_width;
                float height_ratio = pred_height / meas_height;

                // If size difference is too large, invalidate the association
                if (width_ratio < 0.5f || width_ratio > 2.0f || height_ratio < 0.5f || height_ratio > 2.0f) {
                    iou_matrix[i][j] = 0.0f; // Invalidate association
                }
            }
        }
    }
}
        
void cleanup_tracks(TrackArray* track_array)
{
    for(int i = 0; i < track_array->track_count; i++)
    {
        if(track_array->tracks[i].missed_frames > MAX_MISSES)
        {
            // Remove track by shifting remaining tracks
            for(int j = i; j < track_array->track_count - 1; j++)
            {
                track_array->tracks[j] = track_array->tracks[j + 1];
            }
            track_array->track_count--;
            i--; // Check the new track at this index
        }
    }
}

void assign_detections_to_tracks(TrackArray* track_array, BoundingBoxArray* pred_boxes, BoundingBoxArray* meas_boxes, float** iou_matrix, bool* assigned_tracks, bool* assigned_detections)
{
    for (int i = 0; i < track_array->track_count; i++)
    {
        float max_iou = 0.0f;
        int best_match = -1;

        for (int j = 0; j < meas_boxes->box_count; j++) {
            if (!assigned_detections[j] && iou_matrix[i][j] > max_iou) {
                max_iou = iou_matrix[i][j];
                best_match = j;
            }
        }

        if (best_match != -1) {
            // Assign detection to track
            assigned_tracks[i] = true;
            assigned_detections[best_match] = true;
            track_array->tracks[i].missed_frames = 0; // Reset missed frames

            // Update track with measurement
            update_track(&track_array->tracks[i], pred_boxes->boxes[i], meas_boxes->boxes[best_match], DT);
        } else {
            // No assignment, increment missed frames and update with prediction only
            track_array->tracks[i].missed_frames++;
            update_track(&track_array->tracks[i], pred_boxes->boxes[i], pred_boxes->boxes[i], DT);
        }
    }
}

void create_new_track(TrackArray* track_array, BoundingBox meas_box)
{
    if (track_array->track_count < track_array->max_tracks)
    {
        Track new_track;
        new_track.x = (meas_box.x_min + meas_box.x_max) / 2.0f;
        new_track.y = (meas_box.y_min + meas_box.y_max) / 2.0f;
        new_track.vx = 0.0f;
        new_track.vy = 0.0f;
        new_track.missed_frames = 0;
        new_track.id = track_array->track_count + 1; // Simple ID assignment

        track_array->tracks[track_array->track_count] = new_track;
        track_array->track_count++;
    }
}

void update_track(Track* track, BoundingBox pred_box, BoundingBox meas_box, float dt)
{
    
    // Predicted position (already updated by predict_tracks)
    float pred_x = (pred_box.x_min + pred_box.x_max) / 2.0f;
    float pred_y = (pred_box.y_min + pred_box.y_max) / 2.0f;
    
    // Measurement position
    float meas_x = (meas_box.x_min + meas_box.x_max) / 2.0f;
    float meas_y = (meas_box.y_min + meas_box.y_max) / 2.0f;
    
    // Innovation (measurement - prediction)
    float innov_x = meas_x - pred_x;
    float innov_y = meas_y - pred_y;
    
    // Update position and velocity
    track->x = pred_x + ALPHA * innov_x;
    track->y = pred_y + ALPHA * innov_y;
    track->vx = track->vx + (BETA / dt) * innov_x;
    track->vy = track->vy + (BETA / dt) * innov_y;
}

void generate_new_tracks(TrackArray* track_array, BoundingBoxArray* meas_boxes, bool* assigned_detections)
{
    for(int j = 0; j < meas_boxes->box_count; j++)
    {
        if (track_array->track_count < track_array->max_tracks && !assigned_detections[j])
        {
            create_new_track(track_array, meas_boxes->boxes[j]);
        }
    }
}

void predict_tracks(TrackArray* track_array, BoundingBoxArray* pred_boxes, float dt)
{
    for(int i = 0; i < track_array->track_count; i++)
    {
        Track* track = &track_array->tracks[i];

        // Simple constant velocity model - predict next position
        float pred_x = track->x + track->vx * dt;
        float pred_y = track->y + track->vy * dt;

        // Use previous box size if available, otherwise use default
        float box_width = 50.0f;  // Default width
        float box_height = 50.0f; // Default height
        
        if (i < pred_boxes->box_count) {
            float prev_width = pred_boxes->boxes[i].x_max - pred_boxes->boxes[i].x_min;
            float prev_height = pred_boxes->boxes[i].y_max - pred_boxes->boxes[i].y_min;
            if (prev_width > 0) box_width = prev_width;
            if (prev_height > 0) box_height = prev_height;
        }

        // Create predicted bounding box
        pred_boxes->boxes[i].x_min = pred_x - box_width / 2.0f;
        pred_boxes->boxes[i].x_max = pred_x + box_width / 2.0f;
        pred_boxes->boxes[i].y_min = pred_y - box_height / 2.0f;
        pred_boxes->boxes[i].y_max = pred_y + box_height / 2.0f;
    }
    
    // Update box count after populating
    pred_boxes->box_count = track_array->track_count;
}




