typedef struct Blob_t {
    float x1;
    float y1;
    float origin_x;
    float origin_y;
    float rad;
    u8 hue;
    u8 sat;
    u8 sat_o;
    u8 hue_o;
    u8 visible;
    u16 z_idx;

    float speed1;
    float speed2;
    float speed3;
    float speed4;
    float offx1;
    float offy1;
    float offx2;
    float offy2;
    float ctr;
    float ctr2;
    float ctr3;
    float ctr4;
} Blob_t;


void Int_Animate_Particles(DisplayBuffers *disp, AnimationOptions *opts);
void Animate_Particles(DisplayBuffers *disp, AnimationOptions *opts);
void Animate_Balloons(DisplayBuffers *disp, AnimationOptions *opts);
void Animate_Cube(DisplayBuffers *disp, AnimationOptions *opts);

IntParticle_t *IntMakeParticles(u8 number, u8 bunches, s16 xcenter, s16 ycenter);
void IntInitParticles(IntParticle_t *particles, u16 number, u8 bunch, s16 xcenter, s16 ycenter, u8 hue);