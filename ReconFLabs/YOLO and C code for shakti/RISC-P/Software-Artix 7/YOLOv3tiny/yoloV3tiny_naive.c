#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../include/mem_map.h"
#include "../include/bbox_hw.h"
#include "../include/IP_interface.h"
#include "../include/scale.h"
#include "../include/track_hw.h"
#include "../include/packetize.h"


#define IMG_SIZE 416
#define NUM_CHANNELS 3
#define NUM_CLASSES 80
#define CONF_THRESHOLD 0.25f
#define NMS_THRESHOLD 0.4f

// YOLOv3-Tiny has these CONV LAYERS (13 total - pooling is separate):
// 0: 3 -> 16, 3x3/1
// 1: 16 -> 32, 3x3/1
// 2: 32 -> 64, 3x3/1
// 3: 64 -> 128, 3x3/1
// 4: 128 -> 256, 3x3/1
// 5: 256 -> 512, 3x3/1
// 6: 512 -> 1024, 3x3/1
// 7: 1024 -> 256, 1x1/1    (route from layer 4)
// 8: 256+256 -> 512, 3x3/1 (concatenate with layer 5 output)
// 9: 512 -> 1024, 3x3/1    (detection head 1 preperation)
// 10: 1024 -> 255, 1x1/1   (DETECTION 1)
// 11: 256 -> 512, 3x3/1    (route back, stride 2)
// 12: 512+1024 -> 1024, 3x3/1 (concatenate with layer 6)
// 13: 1024 -> 255, 1x1/1   (DETECTION 2)

static int configs[13][5] = {
        // {in_ch, out_ch, k_size, stride, padding}
        {3,    16,   3, 1, 1},   // 0: conv1
        {16,   32,   3, 1, 1},   // 1: conv2
        {32,   64,   3, 1, 1},   // 2: conv3
        {64,   128,  3, 1, 1},   // 3: conv4
        {128,  256,  3, 1, 1},   // 4: conv5 (route_1 saved)
        {256,  512,  3, 1, 1},   // 5: conv6
        {512,  1024, 3, 1, 1},   // 6: conv7
        {1024, 256,  1, 1, 0},   // 7: conv8 (route_2 saved)
        {256,  512,  3, 1, 1},   // 8: conv9
        {512,  255,  1, 1, 0},   // 9: conv10 (DETECTION HEAD 1)
        {256,  128,  1, 1, 0},   // 10: conv11 (from route_2)
        {384,  256,  3, 1, 1},   // 11: conv12 (384 = 128 upsampled + 256 route_1)
        {256,  255,  1, 1, 0}    // 12: conv13 (DETECTION HEAD 2)
};

#define NUM_CONV_LAYERS 13

typedef struct {
    int in_channels;
    int out_channels;
    int kernel_size;
    int stride;
    int padding;
    float *weights;     // [out_channels][in_channels][kernel_size][kernel_size]
    float *bias;        // [out_channels]
} ConvLayer;


static const char *COCO_NAMES[NUM_CLASSES] = {
    "person","bicycle","car","motorbike","aeroplane","bus","train","truck","boat","traffic light",
    "fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow",
    "elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee",
    "skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle",
    "wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange",
    "broccoli","carrot","hot dog","pizza","donut","cake","chair","sofa","pottedplant","bed",
    "diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone","microwave","oven",
    "toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"
};

//TODO
float* load_image(const char *path) {
    static float img_buffer[NUM_CHANNELS * IMG_SIZE * IMG_SIZE];
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open image: %s\n", path);
        return NULL;
    }
    
    int size = NUM_CHANNELS * IMG_SIZE * IMG_SIZE;
    int read = fread(img_buffer, sizeof(float), size, f);
    fclose(f);
    
    if (read != size) {
        fprintf(stderr, "ERROR: Read %d elements, expected %d\n", read, size);
        return NULL;
    }
    
    return img_buffer;
}

static int has_bn(int layer_index) {
    // All conv blocks except detection heads (conv10, conv13)
    // Index map: conv10 = 9, conv13 = 12
    return !(layer_index == 9 || layer_index == 12);
}

static void fuse_bn(ConvLayer *l, const float *gamma, const float *beta, const float *mean, const float *var) {
    const float eps = 1e-5f;
    int kernel_params = l->in_channels * l->kernel_size * l->kernel_size;

    for (int oc = 0; oc < l->out_channels; oc++) {
        float scale = gamma[oc] / sqrtf(var[oc] + eps);
        l->bias[oc] = beta[oc] - mean[oc] * scale;

        float *w = &l->weights[oc * kernel_params];
        for (int i = 0; i < kernel_params; i++) {
            w[i] *= scale;
        }
    }
}

//TODO
int load_weights(const char *path, ConvLayer *layers) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open weights: %s\n", path);
        return 0;
    }
    
    // Skip 5-int header
    int header[5];
    if (fread(header, sizeof(int), 5, f) != 5) {
        fprintf(stderr, "ERROR: Cannot read header\n");
        fclose(f);
        return 0;
    }
    
    printf("Weights file header: major=%d minor=%d revision=%d\n", header[0], header[1], header[2]);
    printf("\nLoading %d conv layers:\n", NUM_CONV_LAYERS);
    
    // Static buffers for BN parameters (reused across layers during loading)
    static float bn_beta[1024];   // Max out_channels = 1024
    static float bn_gamma[1024];
    static float bn_mean[1024];
    static float bn_var[1024];
    
    for (int i = 0; i < NUM_CONV_LAYERS; i++) {
        ConvLayer *l = &layers[i];

        if (has_bn(i)) {
            if (fread(bn_beta, sizeof(float), l->out_channels, f) != l->out_channels ||
                fread(bn_gamma, sizeof(float), l->out_channels, f) != l->out_channels ||
                fread(bn_mean, sizeof(float), l->out_channels, f) != l->out_channels ||
                fread(bn_var, sizeof(float), l->out_channels, f) != l->out_channels) {
                fprintf(stderr, "ERROR: Layer %d BN params read failed\n", i);
                fclose(f);
                return 0;
            }

            int weight_count = l->in_channels * l->kernel_size * l->kernel_size;
            for (int oc = 0; oc < l->out_channels; oc++) {
                int read = fread(&l->weights[oc * weight_count], sizeof(float), weight_count, f);
                if (read != weight_count) {
                    fprintf(stderr, "ERROR: Layer %d weights failed (%d/%d at oc=%d)\n", i, read, weight_count, oc);
                    fclose(f);
                    return 0;
                }
            }

            fuse_bn(l, bn_gamma, bn_beta, bn_mean, bn_var);
        } else {
            int bias_count = fread(l->bias, sizeof(float), l->out_channels, f);
            if (bias_count != l->out_channels) {
                fprintf(stderr, "ERROR: Layer %d bias read failed (%d/%d)\n", i, bias_count, l->out_channels);
                fclose(f);
                return 0;
            }

            int weight_count = l->in_channels * l->kernel_size * l->kernel_size;
            for (int oc = 0; oc < l->out_channels; oc++) {
                int read = fread(&l->weights[oc * weight_count], sizeof(float), weight_count, f);
                if (read != weight_count) {
                    fprintf(stderr, "ERROR: Layer %d weights failed (%d/%d at oc=%d)\n", i, read, weight_count, oc);
                    fclose(f);
                    return 0;
                }
            }
        }
        
        float params = (float)(l->out_channels * l->in_channels * l->kernel_size * l->kernel_size);
        printf("  [%2d] %4d->%4d k=%d: %.2fM params\n", i, l->in_channels, l->out_channels, 
               l->kernel_size, params/1e6);
    }
    
    fclose(f);
    printf("\n✓ All weights loaded successfully\n");
    return 1;
}

static void concat_channels(const float *a, const float *b, float *out, int C1, int C2, int H, int W) {
    int plane = H * W;
    memcpy(out, a, sizeof(float) * C1 * plane);
    memcpy(out + C1 * plane, b, sizeof(float) * C2 * plane);
}

void print_tensor_stats(float *t, int size, const char *name) {
    float min_v = t[0], max_v = t[0], sum = 0;
    for (int i = 0; i < size; i++) {
        if (t[i] < min_v) min_v = t[i];
        if (t[i] > max_v) max_v = t[i];
        sum += t[i];
    }
    printf("%s: min=%.3f max=%.3f mean=%.3f\n", name, min_v, max_v, sum/size);
}


void yolov3_tiny_forward_pass(ConvLayer *layers) {
    /*
     * This function executes the YOLOv3-Tiny forward pass on pre-loaded weights and image.
     * It assumes input image is already copied into INPUT_IMAGE (DDR).
     * All intermediate buffers reside in DDR, managed with ping-pong buffers.
     * ROUTE layers saved explicitly for concatenation and detection heads.
     */

    float *packet_a = PACKET_BUFFER_A;
    float *packet_b = PACKET_BUFFER_B;
    float *output_a = OUTPUT_BUFFER_A;
    float *output_b = OUTPUT_BUFFER_B;
    float *current = NULL;  // Tracks active output buffer

    int packet_size;

    // Layer 0: Conv 3x3, 3→16, 416x416
    printf("[L0] Conv 3x3, 3->16, 416x416\n");
    packetize_image_float(INPUT_IMAGE, packet_a, 416, 416, 3, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(416, 416, 3, 3, 1, 1);
    ip_conv_execute(layers[0].weights, packet_size, packet_a, packet_size, output_a, 1, 416, 416, 3, 16, 3);
    maxpool_float(output_a, output_b, 416, 416, 16, 2, 2, 0);
    current = output_b;  // OUTPUT_BUFFER_B active
    printf("  ✓ Layer 0 complete\n\n");

    // Layer 1: Conv 3x3, 16→32, 208x208
    printf("[L1] Conv 3x3, 16->32, 208x208\n");
    packetize_image_float(current, packet_b, 208, 208, 16, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(208, 208, 16, 3, 1, 1);
    ip_conv_execute(layers[1].weights, packet_size, packet_b, packet_size, output_a, 1, 208, 208, 16, 32, 3);
    maxpool_float(output_a, output_b, 208, 208, 32, 2, 2, 0);
    current = output_b;  // OUTPUT_BUFFER_B active
    printf("  ✓ Layer 1 complete\n\n");

    // Layer 2: Conv 3x3, 32→64, 104x104
    printf("[L2] Conv 3x3, 32->64, 104x104\n");
    packetize_image_float(current, packet_a, 104, 104, 32, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(104, 104, 32, 3, 1, 1);
    ip_conv_execute(layers[2].weights, packet_size, packet_a, packet_size, output_b, 1, 104, 104, 32, 64, 3);
    maxpool_float(output_b, output_a, 104, 104, 64, 2, 2, 0);
    current = output_a;  // OUTPUT_BUFFER_A active
    printf("  ✓ Layer 2 complete\n\n");

    // Layer 3: Conv 3x3, 64→128, 52x52
    printf("[L3] Conv 3x3, 64->128, 52x52\n");
    packetize_image_float(current, packet_b, 52, 52, 64, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(52, 52, 64, 3, 1, 1);
    ip_conv_execute(layers[3].weights, packet_size, packet_b, packet_size, output_b, 1, 52, 52, 64, 128, 3);
    maxpool_float(output_b, output_a, 52, 52, 128, 2, 2, 0);
    current = output_a;  // OUTPUT_BUFFER_A active
    printf("  ✓ Layer 3 complete\n\n");

    // Layer 4: Conv 3x3, 128→256, 26x26 (ROUTE_1 saved)
    printf("[L4] Conv 3x3, 128->256, 26x26 [ROUTE_1]\n");
    packetize_image_float(current, packet_a, 26, 26, 128, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(26, 26, 128, 3, 1, 1);
    ip_conv_execute(layers[4].weights, packet_size, packet_a, packet_size, output_b, 1, 26, 26, 128, 256, 3);
    memcpy(DDR_L4_OUTPUT, output_b, 256 * 26 * 26 * sizeof(float));  // Save ROUTE_1 for later
    maxpool_float(output_b, output_a, 26, 26, 256, 2, 2, 0);
    current = output_a;  // OUTPUT_BUFFER_A active
    printf("  ✓ ROUTE_1 saved at 0x%08X\n\n", DDR_L4_OUTPUT_ADDR);

    // Layer 5: Conv 3x3, 256→512, 13x13
    printf("[L5] Conv 3x3, 256->512, 13x13\n");
    packetize_image_float(current, packet_b, 13, 13, 256, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(13, 13, 256, 3, 1, 1);
    ip_conv_execute(layers[5].weights, packet_size, packet_b, packet_size, output_b, 1, 13, 13, 256, 512, 3);
    maxpool_float(output_b, output_a, 13, 13, 512, 2, 1, 1);
    current = output_a;  // OUTPUT_BUFFER_A active
    printf("  ✓ Layer 5 complete\n\n");

    // Layer 6: Conv 3x3, 512→1024, 13x13
    printf("[L6] Conv 3x3, 512->1024, 13x13\n");
    packetize_image_float(current, packet_a, 13, 13, 512, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(13, 13, 512, 3, 1, 1);
    ip_conv_execute(layers[6].weights, packet_size, packet_a, packet_size, output_b, 1, 13, 13, 512, 1024, 3);
    current = output_b;  // OUTPUT_BUFFER_B active
    printf("  ✓ Layer 6 complete\n\n");

    // Layer 7: Conv 1x1, 1024→256, 13x13 (ROUTE_2 saved)
    printf("[L7] Conv 1x1, 1024->256, 13x13 [ROUTE_2]\n");
    packetize_image_float(current, packet_b, 13, 13, 1024, 1, 1, 0);
    packet_size = CALC_PACKET_SIZE(13, 13, 1024, 1, 1, 0);
    ip_conv_execute(layers[7].weights, packet_size, packet_b, packet_size, output_a, 1, 13, 13, 1024, 256, 1);
    memcpy(DDR_L7_OUTPUT, output_a, 256 * 13 * 13 * sizeof(float));  // Save ROUTE_2
    printf("  ✓ ROUTE_2 saved at 0x%08X\n\n", DDR_L7_OUTPUT_ADDR);

    // Layer 8: Conv 3x3, 256→512, 13x13
    printf("[L8] Conv 3x3, 256->512, 13x13\n");
    packetize_image_float(output_a, packet_a, 13, 13, 256, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(13, 13, 256, 3, 1, 1);
    ip_conv_execute(layers[8].weights, packet_size, packet_a, packet_size, output_b, 1, 13, 13, 256, 512, 3);
    current = output_b;  // OUTPUT_BUFFER_B active
    printf("  ✓ Layer 8 complete\n\n");

    // Layer 9: Conv 1x1, 512→255, 13x13 (DETECTION HEAD 1)
    printf("[L9] Conv 1x1, 512->255, 13x13 [HEAD 1]\n");
    packetize_image_float(current, packet_b, 13, 13, 512, 1, 1, 0);
    packet_size = CALC_PACKET_SIZE(13, 13, 512, 1, 1, 0);
    ip_conv_execute(layers[9].weights, packet_size, packet_b, packet_size, output_a, 1, 13, 13, 512, 255, 1);
    memcpy(DDR_L9_OUTPUT, output_a, 255 * 13 * 13 * sizeof(float));  // Save HEAD 1
    printf("  ✓ HEAD 1 saved at 0x%08X\n\n", DDR_L9_OUTPUT_ADDR);

    // Layer 10: Conv 1x1, 256→128, 13x13 (from ROUTE_2)
    printf("[L10] Conv 1x1, 256->128, 13x13 [from ROUTE_2]\n");
    packetize_image_float(DDR_L7_OUTPUT, packet_a, 13, 13, 256, 1, 1, 0);
    packet_size = CALC_PACKET_SIZE(13, 13, 256, 1, 1, 0);
    ip_conv_execute(layers[10].weights, packet_size, packet_a, packet_size, output_b, 1, 13, 13, 256, 128, 1);
    upsample_float(output_b, DDR_L10_OUTPUT, 13, 13, 128, 2);  // Upsample x2 to 26x26
    printf("  ✓ Layer 10 complete\n\n");

    // CONCAT: Upsampled(128) + ROUTE_1(256) → 384 channels at 26x26
    printf("[CONCAT] 128 + 256 -> 384 channels at 26x26\n");
    concat_channels(DDR_L10_OUTPUT, DDR_L4_OUTPUT, output_a, 128, 256, 26, 26);
    // output_a buffer is now concatenated input for layer 11
    printf("  ✓ In OUTPUT_BUFFER_A\n\n");

    // Layer 11: Conv 3x3, 384→256, 26x26
    printf("[L11] Conv 3x3, 384->256, 26x26\n");
    packetize_image_float(output_a, packet_b, 26, 26, 384, 3, 1, 1);
    packet_size = CALC_PACKET_SIZE(26, 26, 384, 3, 1, 1);
    ip_conv_execute(layers[11].weights, packet_size, packet_b, packet_size, output_b, 1, 26, 26, 384, 256, 3);
    current = output_b;  // OUTPUT_BUFFER_B active
    printf("  ✓ Layer 11 complete\n\n");

    // Layer 12: Conv 1x1, 256→255, 26x26 (DETECTION HEAD 2)
    printf("[L12] Conv 1x1, 256->255, 26x26 [HEAD 2]\n");
    packetize_image_float(current, packet_a, 26, 26, 256, 1, 1, 0);
    packet_size = CALC_PACKET_SIZE(26, 26, 256, 1, 1, 0);
    ip_conv_execute(layers[12].weights, packet_size, packet_a, packet_size, output_a, 1, 26, 26, 256, 255, 1);
    memcpy(DDR_L12_OUTPUT, output_a, 255 * 26 * 26 * sizeof(float));  // Save HEAD 2
    printf("  ✓ HEAD 2 saved at 0x%08X\n\n", DDR_L12_OUTPUT_ADDR);
}


int decode_detections(ConvLayer *layers,
                      float conf_thresh,
                      float nms_thresh,
                      BoundingBox *detections,
                      int max_detections,
                      const char *output_txt_path) {

    int det_count = 0;
    int det_count_head2 = 0;

    // Head 1 (small scale, 13x13)
    det_count = bbox_hw_decode_head(DDR_L9_OUTPUT, 13, conf_thresh, 0, detections, max_detections);

    if (det_count < max_detections) {
        int remaining = max_detections - det_count;
        // Head 2 (large scale, 26x26)
        det_count_head2 = bbox_hw_decode_head(DDR_L12_OUTPUT, 26, conf_thresh, 1, detections + det_count, remaining);
        det_count += det_count_head2;
    }

    printf("Raw detections above confidence threshold: %d\n", det_count);

    // Non-Maximum Suppression
    int final_count = bbox_hw_nms(detections, det_count, nms_thresh);
    printf("Detections after NMS: %d\n", final_count);

    // Save to file (optional)
    if (output_txt_path) {
        FILE *outf = fopen(output_txt_path, "w");
        if (!outf) {
            fprintf(stderr, "WARNING: Could not write to %s\n", output_txt_path);
        } else {
            int show = final_count < 20 ? final_count : 20;
            for (int i = 0; i < show; i++) {
                BoundingBox *d = &detections[i];
                fprintf(outf, "%d %.6f %.2f %.2f %.2f %.2f\n", d->class_id, d->confidence, d->x_min, d->y_min, d->x_max, d->y_max);
            }
            fclose(outf);
            printf("Detections saved to %s\n", output_txt_path);
        }
    }

    // Print top detections to console
    int show = final_count < 20 ? final_count : 20;
    for (int i = 0; i < show; i++) {
        BoundingBox *d = &detections[i];
        const char *name = (d->class_id >= 0 && d->class_id < NUM_CLASSES) ? COCO_NAMES[d->class_id] : "unknown";
        printf("[%2d] %s (id=%d) conf=%.3f\n", i, name, d->class_id, d->confidence);
        printf("     box=(x1=%.1f,y1=%.1f,x2=%.1f,y2=%.1f)\n", d->x_min, d->y_min, d->x_max, d->y_max);
    }

    return final_count;
}



int main() {
    /* =====================================================================
     * POINTER STRATEGY FOR CLARITY:
     * =====================================================================
     * 
     * Memory Addresses (DDR in Artix-7):
     * - Defined as "uint32_t" macros ending in "_BASE" or "_ADDR"
     *   Example: PACKET_BUFFER_A_BASE = 0x80008000u
     * 
     * Float Pointers (CPU operations):
     * - Derived from addresses using (float*) cast
     * - Macros without "_BASE" suffix: PACKET_BUFFER_A, OUTPUT_BUFFER_A, etc.
     *   Example: #define PACKET_BUFFER_A ((float*)(PACKET_BUFFER_A_BASE))
     * - Used by CPU functions: packetize_image_float(), maxpool_float(), memcpy()
     * 
    * IP Function Calls (Hardware):
    * - Take float* pointers (DDR-mapped) and cast internally
    * - Use pointer macros: PACKET_BUFFER_A, OUTPUT_BUFFER_A, etc.
    *   Example: ip_conv_execute(..., PACKET_BUFFER_A, ...)
    * 
    * RULE: Use (float*) pointers for both CPU ops and hardware calls
     * ===================================================================== */
    
    printf("===============================================\n");
    printf("YOLOv3-Tiny Packetization Pipeline\n");
    printf("NO DMA - Packetize-then-Convolve Flow\n");
    printf("NO MALLOC - All data in DDR\n");
    printf("===============================================\n\n");
    
    // Stack-allocated layer configuration (no malloc)
    ConvLayer layers[NUM_CONV_LAYERS];
    
    for (int i = 0; i < NUM_CONV_LAYERS; i++) {
        layers[i].in_channels = configs[i][0];
        layers[i].out_channels = configs[i][1];
        layers[i].kernel_size = configs[i][2];
        layers[i].stride = configs[i][3];
        layers[i].padding = configs[i][4];
        layers[i].weights = NULL;  // Loaded from disk to DDR during load_weights
        layers[i].bias = NULL;      // Loaded from disk to DDR during load_weights
    }
    printf("✓ Layer config initialized (stack)\n\n");
    
    // Load input image
    printf("Loading test image...\n");
    float *image = load_image("test_image.bin");
    if (!image) return 1;
    
    // Copy to INPUT_IMAGE buffer in DDR
    memcpy(INPUT_IMAGE, image, NUM_CHANNELS * IMG_SIZE * IMG_SIZE * sizeof(float));
    printf("✓ Image loaded to DDR at 0x%08X\n\n", (uint32_t)INPUT_IMAGE_BASE);
    
    // Load weights
    printf("Loading weights from yolov3-tiny.weights...\n");
    if (!load_weights("yolov3-tiny.weights", layers)) {
        fprintf(stderr, "FATAL: Failed to load weights\n");
        return 1;
    }
    
    // Execute forward pass
    printf("\n=== Forward Pass with Packetization Pipeline ===\n");
    printf("All intermediate data stored in DDR ping-pong buffers\n");
    printf("Zero-copy pointer swapping (no memcpy overhead)\n\n");

    yolov3_tiny_forward_pass(layers);

    
    printf("\n=== YOLO Decode (DDR storage) ===\n");
    int max_dets = 13 * 13 * 3 + 26 * 26 * 3;
    BoundingBox *dets = DDR_DETECTION;

    int final_count = decode_detections(layers, CONF_THRESHOLD, NMS_THRESHOLD, dets, max_dets, "detections.txt");



    printf("\n===============================================\n");
    printf("Forward pass complete\n");
    printf("Inference data (detections) stored at: 0x%08X\n", DDR_DETECTION_BASE);
    printf("===============================================\n");
    
    return 0;
}
