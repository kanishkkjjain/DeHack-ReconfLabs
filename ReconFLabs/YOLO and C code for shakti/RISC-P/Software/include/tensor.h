#ifndef TENSOR_H
#define TENSOR_H

#include <stdint.h>

typedef struct {
    int w, h, c;
    int8_t* data;
    float scale;
} Tensor;

#endif
