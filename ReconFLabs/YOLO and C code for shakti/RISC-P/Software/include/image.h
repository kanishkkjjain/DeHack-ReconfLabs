#ifndef IMAGE_H
#define IMAGE_H

#include <stdlib.h>

typedef struct{
    int width;
    int height;
    int channels;
    unsigned int *data;
} Image;

#endif