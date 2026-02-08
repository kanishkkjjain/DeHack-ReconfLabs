#ifndef SCALE_H
#define SCALE_H

#include "../include/image.h"

// Scaling algorithms for Image preprocessing
// All functions return a new dynamically allocated Image
// Caller is responsible for freeing the returned image

// Nearest Neighbor: Fast, low quality, good for real-time systems
Image* nearest_neighbor_scale(Image* input, int new_width, int new_height);

// Bilinear: Medium quality, good for general YOLOv3 preprocessing
Image* bilinear_scale(Image* input, int new_width, int new_height);

// Bicubic: High quality, slower, good for offline processing
Image* bicubic_scale(Image* input, int new_width, int new_height);

// Box Filter: High quality for downscaling (averaging), good for 4K→416×416
Image* box_scale(Image* input, int new_width, int new_height);

#endif // SCALE_H
