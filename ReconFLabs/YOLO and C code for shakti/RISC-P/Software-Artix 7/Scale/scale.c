#include <stdint.h>
#include "scale.h"

static int dims_valid(int sw, int sh, int dw, int dh) {
    return (sw > 0) && (sh > 0) && (dw > 0) && (dh > 0);
}

static inline int clamp_idx(int v, int limit) {
    return (v < 0) ? 0 : (v >= limit ? limit - 1 : v);
}

void nearest_neighbor(int* src, int* dst, int sw, int sh, int dw, int dh) {
    if (!dims_valid(sw, sh, dw, dh)) {
        return;
    }

    float xr = (float)sw / (float)dw;
    float yr = (float)sh / (float)dh;

    for (int j = 0; j < dh; j++) {
        int nearest_y = (int)(j * yr);
        for (int i = 0; i < dw; i++) {
            int nearest_x = (int)(i * xr);
            dst[j * dw + i] = src[(nearest_y * sw) + nearest_x];
        }
    }
}

void bilinear(int* src, int* dst, int sw, int sh, int dw, int dh) {
    //TODO: Not working

    if (!dims_valid(sw, sh, dw, dh)) {
        return;
    }

    float xr = (float)(sw - 1) / dw;
    float yr = (float)(sh - 1) / dh;

    for (int j = 0; j < dh; j++) {
        for (int i = 0; i < dw; i++) {
            float x = i * xr;
            float y = j * yr;
            int xl = (int)x;
            int yl = (int)y;
            int xh = (xl + 1 < sw) ? xl + 1 : xl;
            int yh = (yl + 1 < sh) ? yl + 1 : yl;

            float xd = x - xl;
            float yd = y - yl;

            uint8_t p1 = src[yl * sw + xl];
            uint8_t p2 = src[yl * sw + xh];
            uint8_t p3 = src[yh * sw + xl];
            uint8_t p4 = src[yh * sw + xh];

            dst[j * dw + i] = (uint8_t)(
                p1 * (1 - xd) * (1 - yd) +
                p2 * xd * (1 - yd) +
                p3 * yd * (1 - xd) +
                p4 * xd * yd
            );
        }
    }
}

float c_kern(float t) {
    t = (t < 0) ? -t : t;
    if (t < 1.0f) return 1.0f - 2.0f * t * t + t * t * t;
    if (t < 2.0f) return -4.0f + 8.0f * t - 5.0f * t * t + t * t * t;
    return 0.0f;
}

void bicubic(int* src, int* dst, int sw, int sh, int dw, int dh) {
    if (!dims_valid(sw, sh, dw, dh)) {
        return;
    }

    float xr = (float)(sw - 1) / dw;
    float yr = (float)(sh - 1) / dh;

    for (int j = 0; j < dh; j++) {
        for (int i = 0; i < dw; i++) {
            float x = i * xr;
            float y = j * yr;
            int xi = (int)x;
            int yi = (int)y;
            float xf = x - xi;
            float yf = y - yi;

            float sum = 0;
            for (int m = -1; m <= 2; m++) {
                for (int n = -1; n <= 2; n++) {
                    int px = clamp_idx(xi + m, sw);
                    int py = clamp_idx(yi + n, sh);
                    sum += c_kern(m - xf) * c_kern(n - yf) * src[py * sw + px];
                }
            }
            dst[j * dw + i] = (uint8_t)sum;
        }
    }
}

void box(int* src, int* dst, int sw, int sh, int dw, int dh) {
    if (!dims_valid(sw, sh, dw, dh)) {
        return;
    }

    float xr = (float)sw / dw;
    float yr = (float)sh / dh;

    for (int j = 0; j < dh; j++) {
        for (int i = 0; i < dw; i++) {
            int xs = (int)(i * xr);
            int ys = (int)(j * yr);
            int xe = (int)((i + 1) * xr);
            int ye = (int)((j + 1) * yr);

            uint32_t s = 0, c = 0;
            for (int y = ys; y < ye && y < sh; y++) {
                for (int x = xs; x < xe && x < sw; x++) {
                    s += src[y * sw + x];
                    c++;
                }
            }
            dst[j * dw + i] = (uint8_t)(s / c);
        }
    }
}