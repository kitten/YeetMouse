// Maximum number of packets allowed to be sent from the mouse at once. Linux's default value is 8, which at
// least causes EOVERFLOW for my mouse (SteelSeries Rival 600). Increase this, if 'dmesg -w' tells you to!
#define BUFFER_SIZE 8

#define FP64_ONE 4294967296ll
#define FP64_SHIFT 32

/*
 * Values here are not floats, or ints. They are a representation of the Q32.32 Fixed-Point values.
 * Do not change them manually, unless you know what you're doing.
 * After su successfully running the GUI at least once, the values in the "/sys/module/leetmouse/parameters/"
 * will be updated with their floating point representations, from that point on you can either change them
 * manually (editing the files with usual floating point values), or (the recommended way) through the GUI.
 */

// Changes behaviour of the scroll-wheel. Default is 3.0f
#define SCROLLS_PER_TICK (3ll << FP64_SHIFT)

// There values are just here to allow you to comfortably start the GUI and change them to your preferences.
#define SENSITIVITY FP64_ONE //0.85f
#define ACCELERATION 644245120ll // 10737418240ll - 2.5f  // 644245120ll - 0.15f
#define OUTPUT_CAP 0
#define OFFSET 0
//#define POST_SCALE_X 0.4f
//#define POST_SCALE_Y 0.4f
#define INPUT_CAP 0
#define MIDPOINT (6ll << FP64_SHIFT)
#define PRESCALE FP64_ONE
#define USE_SMOOTHING 1

// Rotation (in radians)
#define ROTATION_ANGLE 0

// LUT settings
#define LUT_SIZE 0
#define LUT_STRIDE FP64_ONE
#define LUT_DATA 0


#define ACCELERATION_MODE 1

// For exponential curves.
#define EXPONENT 1717986944ll //26214
