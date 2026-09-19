/** @file moving_average.h @brief O(1) moving-average filter without allocation. */
#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float *samples;
    uint16_t capacity;
    uint16_t count;
    uint16_t next_index;
    float sum;
} MovingAverage_Filter;

/** Initialize a filter using caller-owned storage of capacity float elements. */
bool MovingAverage_Init(MovingAverage_Filter *filter,
                        float *storage,
                        uint16_t capacity);

/** Add one sample and return the mean of all samples received so far. */
float MovingAverage_Update(MovingAverage_Filter *filter, float sample);

/** Clear samples and accumulated state while retaining the same storage. */
void MovingAverage_Reset(MovingAverage_Filter *filter);

#endif /* MOVING_AVERAGE_H */
