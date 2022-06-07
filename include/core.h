

namespace core {

    float value_map(float x, float in_min, float in_max, float out_min, float out_max);
    
    /**
     * compute rate of change from n1 to n2 of steps
     */
    float step(unsigned int steps, int n1, int n2);

    /**
     * compute absolute difference of 2 numbers
     */
    int absdiff(int n1, int n2);
    
    float absdiff_f(float n1, float n2);
    
    /**
     * calculate distance between x1/y1 and x2/y2
     */
    int calc_distance(int x1, int y1, int x2, int y2);

    /**
     * calculate distance between x1/y1 and x2/y2
     */
    float calc_distance_float(float x1, float y1, float x2, int y2);
    
    /**
     * mod with support for 0, 0 = y
     */
    int mod_with_zero(int x, int y);

    void fft(float *data, unsigned long nn);
}