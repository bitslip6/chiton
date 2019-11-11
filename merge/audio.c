#include "demos.h"

// TODO: add audio filter configuration
void AudioFilter(AnimationFilterOut *state) {
    // beat calculations...
    // if frames > 0 then we are running the beat filter
    if (state->frames > 0) {
        if (state->mul < MAX_PULSE) {
            state->mul *= PULSE_INT;
        }
        if (state->mul > MAX_PULSE) { state->mul = MAX_PULSE; }
        else if (state->mul == MAX_PULSE) { state->mul = 1.0; }
        state->xoff = -1 + (float)rand()/RAND_MAX * 2.0;
        state->yoff = -0.5 + (float)rand()/RAND_MAX * 1.0;
        state->frames--;
    } else if (state->frames == 0 && state->mul > MIN_PULSE) {
        state->mul *= 0.9;
        if (state->mul < MIN_PULSE) { state->mul = MIN_PULSE; }
    }
}
