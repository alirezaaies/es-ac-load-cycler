/** @file moving_average.c */
#include "moving_average.h"

#include <stddef.h>

bool MovingAverage_Init(MovingAverage_Filter *filter,
                        float *storage,
                        uint16_t capacity)
{
    if ((filter == NULL) || (storage == NULL) || (capacity == 0U)) {
        return false;
    }
    filter->samples = storage;
    filter->capacity = capacity;
    MovingAverage_Reset(filter);
    return true;
}

void MovingAverage_Reset(MovingAverage_Filter *filter)
{
    if ((filter == NULL) || (filter->samples == NULL) ||
        (filter->capacity == 0U)) {
        return;
    }
    for (uint16_t index = 0U; index < filter->capacity; ++index) {
        filter->samples[index] = 0.0f;
    }
    filter->count = 0U;
    filter->next_index = 0U;
    filter->sum = 0.0f;
}

float MovingAverage_Update(MovingAverage_Filter *filter, float sample)
{
    if ((filter == NULL) || (filter->samples == NULL) ||
        (filter->capacity == 0U)) {
        return 0.0f;
    }

    if (filter->count == filter->capacity) {
        filter->sum -= filter->samples[filter->next_index];
    } else {
        ++filter->count;
    }
    filter->samples[filter->next_index] = sample;
    filter->sum += sample;
    filter->next_index =
        (uint16_t)((filter->next_index + 1U) % filter->capacity);
    return filter->sum / (float)filter->count;
}
