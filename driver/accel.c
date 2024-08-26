// SPDX-License-Identifier: GPL-2.0-or-later

#include "accel.h"
#include "util.h"
#include "config.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/string.h>   //strlen
#include "FixedMath/Fixed64.h"

MODULE_AUTHOR("Christopher Williams <chilliams (at) gmail (dot) com>"); //Original idea of this module
MODULE_AUTHOR("Klaus Zipfel <klaus (at) zipfel (dot) family>");         //Current maintainer
MODULE_AUTHOR("Maciej Grzęda <gmaciejg525 (at) gmail (dot) com>");      // Current maintainer
// Sorry if you have issues with compilation because of this silly character in my family name lol <3

//Converts a preprocessor define's value in "config.h" to a string - Suspect this to change in future version without a "config.h"
#define _s(x) #x
#define s(x) _s(x)

//Convenient helper for float based parameters, which are passed via a string to this module (must be individually parsed via atof() - available in util.c)
#define PARAM_F(param, default, desc)                           \
    FP_LONG g_##param = (long long)default;                                  \
    static char* g_param_##param = s(default);                  \
    module_param_named(param, g_param_##param, charp, 0644);    \
    MODULE_PARM_DESC(param, desc);

#define PARAM(param, default, desc)                             \
    static char g_##param = default;                            \
    module_param_named(param, g_##param, byte, 0644);           \
    MODULE_PARM_DESC(param, desc);

// ########## Kernel module parameters

// Simple module parameters (instant update)
PARAM(no_bind,          0,                  "This will disable binding to this driver via 'leetmouse_bind' by udev.");
PARAM(update,           0,                  "Triggers an update of the acceleration parameters below");
PARAM(AccelerationMode, ACCELERATION_MODE,  "Sets the algorithm to be used for acceleration");

// Acceleration parameters (type pchar. Converted to float via "update_params" triggered by /sys/module/leetmouse/parameters/update)
PARAM_F(InputCap,       INPUT_CAP,          "Limit the maximum pointer speed before applying acceleration.");
PARAM_F(Sensitivity,    SENSITIVITY,        "Mouse base sensitivity.");
PARAM_F(Acceleration,   ACCELERATION,       "Mouse acceleration sensitivity.");
PARAM_F(OutputCap,      OUTPUT_CAP,         "Cap maximum sensitivity.");
PARAM_F(Offset,         OFFSET,             "Mouse base sensitivity.");
PARAM_F(Exponent,       EXPONENT,           "Exponent for algorithms that use it");
PARAM_F(Midpoint,       MIDPOINT,           "Midpoint for sigmoid function");
PARAM  (UseSmoothing,   USE_SMOOTHING,      "Whether to smooth out functions (doesn't apply to all)")
PARAM_F(ScrollsPerTick, SCROLLS_PER_TICK,   "Amount of lines to scroll per scroll-wheel tick.");


// Updates the acceleration parameters. This is purposely done with a delay!
// First, to not hammer too much the logic in "accelerate()", which is called VERY OFTEN!
// Second, to fight possible cheating. However, this can be OFC changed, since we are OSS...
#define PARAM_UPDATE(param) (FP64_FromString(g_param_##param, &g_##param))

// Aggregate values that don't change with speed to save on calculations done every irq
struct ModesConstants {
    bool is_init;
    // Jump
    FP_LONG C0; // the "integral" evaluated at 0
    FP_LONG r; // basically a smoothness factor

    FP_LONG accel_sub_1;
    FP_LONG exp_sub_1;
} modesConst = { .is_init = false, .C0 = 0, .r = 0, .accel_sub_1 = 0, .exp_sub_1 = 0 };

// Recalculate new modes constants
static void update_constants(void) {
    modesConst.accel_sub_1 = FP64_Sub(g_Acceleration, FP64_ONE);
    modesConst.exp_sub_1 = FP64_Sub(g_Exponent, FP64_ONE);

    modesConst.r = FP64_DivPrecise(FP64_Mul(Two, Pi), FP64_Mul(g_Exponent, g_Midpoint));
    modesConst.C0 = FP64_DivPrecise(FP64_Mul(modesConst.accel_sub_1, FP64_Log(FP64_Add(1, FP64_Exp(FP64_Mul(modesConst.r, g_Midpoint))))), modesConst.r);
}

static ktime_t g_next_update = 0;
INLINE void update_params(ktime_t now)
{
    if(!g_update) return;
    if(now < g_next_update) return;
    g_update = 0;
    g_next_update = now + 1000000000ll;    //Next update is allowed after 1s of delay

    PARAM_UPDATE(InputCap);
    PARAM_UPDATE(Sensitivity);
    PARAM_UPDATE(Acceleration);
    PARAM_UPDATE(OutputCap);
    PARAM_UPDATE(Offset);
    PARAM_UPDATE(ScrollsPerTick);
    PARAM_UPDATE(Exponent);
    PARAM_UPDATE(Midpoint);

    update_constants();
}

const FP_LONG fp64_0_1     = 429496736;
const FP_LONG fp64_1    = 1ll << FP64_Shift;
const FP_LONG fp64_10      = 10ll << FP64_Shift;
const FP_LONG fp64_100     = 100ll << FP64_Shift;
const FP_LONG fp64_1000    = 1000ll << FP64_Shift;
const FP_LONG fp64_10000    = 10000ll << FP64_Shift;

// Acceleration happens here
int accelerate(int *x, int *y, int *wheel)
{
    FP_LONG delta_x, delta_y, delta_whl, ms, speed;
    //static long buffer_x = 0;
    //static long buffer_y = 0;
    static long buffer_whl = 0;
    //Static float assignment should happen at compile-time and thus should be safe here. However, avoid non-static assignment of floats outside kernel_fpu_begin()/kernel_fpu_end()
    static FP_LONG carry_x = 0;
    static FP_LONG carry_y = 0;
    static FP_LONG carry_whl = 0;
    static FP_LONG last_ms = One;
    //static long long iter = 0;
    static ktime_t last;//, elapsed_time;
    ktime_t now;
    int status = 0;

    if(!modesConst.is_init)
        update_constants();

    delta_x = FP64_FromInt(*x);
    delta_y = FP64_FromInt(*y);
    //delta_whl = FP64_FromInt(*wheel);

    //Add buffer values, if present, and reset buffer
    //delta_x = FP64_Add(delta_x, FP64_FromInt((int) buffer_x)); buffer_x = 0;
    //delta_y = FP64_Add(delta_y, FP64_FromInt((int) buffer_y)); buffer_y = 0;

    //Calculate frametime
    now = ktime_get();
    long long dt = (now - last);
    //int frac = dt % 10000;
    // We can't just store milliseconds as this would lose a lot of precision (nano -> mili, that's 10^-6 difference).
    // But we have only Q16.16 bits of precision, meaning 16 bits for the fractional part of the number (it's constant!).
    // So it would be wasteful to store a millisecond in a fixed point format, because the integral part would be at max like 100
    // and we would lose all the precision on the fractional part, so we move everything storing millis * 100.
    // Now we have at max 10000 to store in the integral part (technically 0xFFFF) and a bit less information in the fractional part
    // that would be lost either way.
    /// THE ABOVE NO LONGER HOLDS, AS I'VE MOVED (AGAIN), THIS TIME TO 64bit FIXED POINT MATH
    //ms = FP64_FromInt(dt / 10000ll) + FP64_Div(FP64_FromInt(frac), fp64_10000); // NOT MILLISECONDS, its ms * 100
    ms = FP64_DivPrecise(FP64_FromInt(dt), FP64_FromInt(1000000));
    last = now;
    //if(ms < 1) ms = last_ms;    //Sometimes, urbs appear bunched -> Beyond µs resolution so the timing reading is plain wrong. Fallback to last known valid frametime
    // Editor node: I have no idea, what this line above really does, but commenting it out solves all my problems
    // with incorrect data. Its seems to that it tries to fix a problem that doesnt exist, or doesnt exist on my
    // specific setup (PC / System / Mice)
    if(dt >= fp64_100) ms = fp64_100;

    //if(ms > 100) ms = 100;      //Original InterAccel has 200 here. RawAccel rounds to 100. So do we.
    last_ms = ms;

    //Update acceleration parameters periodically
    update_params(now);

    //Calculate velocity (one step before rate, which divides rate by the last frametime)
    speed = FP64_Sqrt(FP64_Add(FP64_Mul(delta_x, delta_x), FP64_Mul(delta_y, delta_y)));

    //Apply speedcap
    if(g_InputCap > 0){
        //if(speed >= g_InputCap) {
        if(FP64_Sub(speed, g_InputCap) > 0) {
            speed = g_InputCap;
        }
    }

    //Calculate rate from travelled overall distance and add possible rate offsets
    speed = FP64_DivPrecise(speed, ms);
    speed = FP64_Sub(speed, g_Offset);

    // Apply acceleration if movement is over offset
    if(speed > 0)
    {
        // Linear acceleration
        if(g_AccelerationMode == 1) {
            // Speed * Acceleration
            //speed *= g_Acceleration;
            //speed += 1;

            // FIXED-POINT:
            speed = FP64_Mul(speed, g_Acceleration);
            speed = FP64_Add(fp64_1, speed);
        }

        // Power acceleration
        else if(g_AccelerationMode == 2) {
            // (Speed * Acceleration)^Exponent

            speed = FP64_Mul(speed, g_Acceleration);
            speed = FP64_PowFast(speed, g_Exponent);
        }

        // Classic acceleration
        else if(g_AccelerationMode == 3) {
            // (Speed * Acceleration) ^ (Exponent - 1) + 1
            // Same as above just without adding the one
            //speed *= g_Acceleration;
            //speed += 1;
            //B_pow(&speed, &g_Exponent);

            // FIXED-POINT:
            speed = FP64_Mul(speed, g_Acceleration);
            speed = FP64_PowFast(speed, modesConst.exp_sub_1);
            speed = FP64_Add(speed, fp64_1);
        }

        // Motivity (Sigmoid function)
        else if(g_AccelerationMode == 4) {
            // Acceleration / ( 1 + e ^ (midpoint - x))
            //product = g_Midpoint-speed;
            //motivity = e;
            //B_pow(&motivity, &product);
            //motivity = g_Acceleration / (1 + motivity);
            //speed = motivity;

            // FIXED-POINT:
            FP_LONG exp = FP64_ExpFast(FP64_Sub(g_Midpoint, speed));
            speed = FP64_Add(fp64_1, FP64_DivPrecise(modesConst.accel_sub_1, FP64_Add(fp64_1, exp)));
        }

        // Jump / Smooth Jump
        else if(g_AccelerationMode == 5) {
            // r = 2pi/(k*midpoint), where k is the smoothness factor (stored inside g_Exponent)
            // Jump: Acceleration / (1 + exp(r(midpoint - x))) + 1
            // Smooth: Integral of the above divided by x pretty much

            FP_LONG D = FP64_ExpFast(FP64_Mul(modesConst.r, FP64_Sub(g_Midpoint, speed)));

            if(g_UseSmoothing) { // smooth
                FP_LONG integral = FP64_Mul(modesConst.accel_sub_1, FP64_Add(speed, FP64_DivPrecise(FP64_LogFast(FP64_Add(fp64_1, D)), modesConst.r)));
                // Not really an integral
                speed = FP64_Add(FP64_DivPrecise(FP64_Sub(integral, modesConst.C0), speed), fp64_1);
            }
            else {
                speed = FP64_Add(FP64_DivPrecise(modesConst.accel_sub_1, FP64_Add(fp64_1, D)), fp64_1);
            }
        }
    }
    else
        speed = fp64_1; // Set speed to 1 if "below offset"

    // Actually apply accelerated sensitivity, allow post-scaling and apply carry from previous round
    // Like RawAccel, sensitivity will be a final multiplier:
    if(g_Sensitivity != fp64_1)
        speed = FP64_Mul(speed, g_Sensitivity);

    // Apply Output Limit
    if(g_OutputCap > 0)
        speed = FP64_Min(g_OutputCap, speed);

    // Apply acceleration
    delta_x = FP64_Mul(delta_x, speed);
    delta_y = FP64_Mul(delta_y, speed);
    //delta_x *= speed;
    //delta_y *= speed;

    delta_x = FP64_Add(delta_x, carry_x);
    delta_y = FP64_Add(delta_y, carry_y);
    //delta_x += carry_x;
    //delta_y += carry_y;

    // I don't do wheel, sorry
    //delta_whl *= g_ScrollsPerTick/3.0f;

    //Cast back to int
    *x = FP64_RoundToInt(delta_x);
    *y = FP64_RoundToInt(delta_y);
    //*x = Leet_round(&delta_x);
    //*y = Leet_round(&delta_y);
    //*wheel = Leet_round(&delta_whl);

    //Save carry for next round
    carry_x = FP64_Sub(delta_x, FP64_FromInt(*x));
    carry_y = FP64_Sub(delta_y, FP64_FromInt(*y));
    //carry_whl = delta_whl - *wheel;

    // Used to very roughly estimate the performance
    //elapsed_time += ktime_sub(ktime_get(), now);
    //if(iter++ == 1000) {
    //    iter = 0;
    //    pr_info("Leetmouse: Averaged at %lldns during 1000 iters\n", ktime_to_ns(elapsed_time));
    //    elapsed_time = 0;
    //}

    return status;
}

/*
 * Some other changes worth noting are:
 * Added "ATTRS{bInterfaceNumber}=="00"" in 99-leetmouse.rules (To stop binding keyboards)
 * Early return in usbmouse.c (usb_mouse_probe()) when keyboard detected
 * "leetmouse_manage" fixed to use bash
 */