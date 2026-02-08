#ifndef LAYER_H
#define LAYER_H

#include <stdint.h>
#include "tensor.h"


typedef enum {
    LAYER_CONV,
    LAYER_POOL,
    LAYER_UPSAMPLE,
    LAYER_YOLO
} LayerType;

typedef struct {
    LayerType type;
    void (*forward)(Tensor*, Tensor*, void* params);
    void* params;
} Layer;

#endif
