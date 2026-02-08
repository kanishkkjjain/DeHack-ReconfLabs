#ifndef SCALE_H
#define SCALE_H

#include <stdint.h>
#include "../include/mem_map.h"

void nearest_neighbor(int* src, int* dst, int sw, int sh, int dw, int dh);
void bilinear(int* src, int* dst, int sw, int sh, int dw, int dh);
void bicubic(int* src, int* dst, int sw, int sh, int dw, int dh);
void box(int* src, int* dst, int sw, int sh, int dw, int dh);

#endif
