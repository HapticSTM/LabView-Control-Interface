// HapticSPM.cpp : Defines the exported functions for the DLL.
#include "pch.h" // use stdafx.h in Visual Studio 2017 and earlier
#include <utility>
#include <limits.h>
#include <ctime>
#include "HapticSPM.h"

#ifdef  _WIN64
#pragma warning (disable:4996)
#endif
#include <cstdio>
#include <cassert>
#if defined(WIN32)
# include <conio.h>
#else
# include "conio.h"
#endif

#include <HD/hd.h>
#include <HDU/hduVector.h>
#include <HDU/hduError.h>


#include <string>
#include <iostream>
#include <Windows.h>
#include <vector>

/***** BEGIN GLOBAL VARIABLES *****/
/*** HAPTIC FRAME PROPERTIES ***/
//Frame origin, the bottom left corner of the haptic workspace (in units of mm), the area in which the pen is bounded.
const double frame_center_haptic[2] = { 0 , 20 };
//The frame width and frame height of the haptic workspace (in units of mm).
const double frame_size_haptic[2] = { 120 , 120 };

int mode = 0;

hduVector3Dd position;
hduVector3Dd velocity;
hduVector3Dd force_labview;

bool wall_toggle = false;
double wall_stiffness = 0.2;

bool drag_toggle[3] = {false, false, false};
double k_drag[3] = { 0.001 , 0.001 , 0.001 };

double force_max[3] = { 3, 3, 3 };
double force_min[3] = { -3, -3, -3 };

double signal = 0;
double gain = 1;

double k[4] = { 1, 1, 1, 1 };

int surface_force = 0;

bool safetip = false;

int num_clicks = 0;
bool planing_toggle = false;

struct {
    bool toggle = false;
    bool active = false;
} planing;

struct {
    hduVector3Dd n;
    double d = 0;
} planeProperties;

double lin(double xv) {
    if (std::pow(10, 8) * xv >= 1) {
        return 10 * std::log(std::pow(10, 8) * xv);
    }
    else {
        return 0;
    }
}



/*** HAPTIC CALLBACK ***/
//This is where the arm code is run and the forces are written.
HDCallbackCode HDCALLBACK FrictionlessPlaneCallback(void* data)
{
    hdEnable(HD_MAX_FORCE_CLAMPING);
    const double time_interval = 2;
    time_t time_initial = time(NULL);
    const double force_trigger = 2;
    hduVector3Dd plane_points[3];

    hduVector3Dd force;

    hdBeginFrame(hdGetCurrentDevice());

    hdGetDoublev(HD_CURRENT_POSITION, position);
    hdGetDoublev(HD_CURRENT_VELOCITY, velocity);

    //Calculate z force
    switch (mode) {
    case 0: //Custom Mode
        force = force_labview;
        break;
    case 1: //Read Mode
        if (position[1] <= gain * signal) {
            double penetrationDistance;
            if (planing.toggle && planing.active) {
                double h;
                h = (planeProperties.n[0]*position[0] + planeProperties.n[1] * position[1] + planeProperties.n[2] * position[2] + planeProperties.d) / sqrt(pow(planeProperties.n[0],2)+ pow(planeProperties.n[1], 2)+ pow(planeProperties.n[2], 2));
                penetrationDistance = fabs(position[1] - h - gain * signal);
            }
            else {
                penetrationDistance = fabs(position[1] - gain * signal);
            }
            
            switch (surface_force) {
            case 0: //Linear Force
                force[1] = k[0] * penetrationDistance;
                break;
            case 1: //Coulomb
                force[1] = k[0] * 0.7 / pow(penetrationDistance / -15 + 2.17, 2) - 0.12;
                break;
            case 2: //Lennard-Jones Force
                force[1] = k[0] * -4.0 * 0.5 * (6 / pow(penetrationDistance / -30 + 2.05, 7) - 12 / pow(penetrationDistance / -30 + 2.05, 13));
                break;
            case 3: //Van der Waals Force
                force[1] = k[0] * 1 / pow(penetrationDistance / -15 + 1.84, 7);
                break;
            case 4: //Exponential
                force[1] = k[0] * pow(2.71828, 0.1 * penetrationDistance - 2) - 0.1;
                break;
            default: //Linear
                force[1] = k[0] * penetrationDistance;
                break;
            }
        }
        else {
            force[1] = 0;
        }
        break;
    case 2: //Write Mode
        switch (surface_force) {
        case 0: //Log Force
            if (gain * signal) {
            
            }
                
                //force[1] = k[0] * 2.48218 * std::log(k[1] * (1 / k[2]) * gain * signal + 1);
            break;
        case 1: //Linear Force
            force[1] = k[0] * gain * signal;
        case 2: //Coulomb Force
            if (gain * signal >= 300) {
                force[1] = force_max[1];
            }
            else {
                force[1] = k[0] * 100 / std::pow(lin(gain * signal) - 240, 2);
            }
            break;
        case 3: //Lennard-Jones Potential w/ Exponential position scaling.
            if (gain * signal >= 200) {
                force[1] = force_max[1];
            }
            else {
                force[1] = k[1] * -1.0 * 4.0 * 3.0 * ((6 * std::pow(50, 6)) / std::pow(fabs(10 * lin(gain * signal) - 240), 7) - (12 * std::pow(50, 12)) / std::pow(fabs(10 * lin(gain * signal) - 240), 13));
            }
            break;
        case 4: //Van der Waals
            if (gain * signal >= 300) {
                force[1] = force_max[1];
            }
            else {
                force[1] = k[0] * 1000 / std::pow(lin(gain * signal) - 240, 7);
            }
            break;
        default: //Log
            force[1] = k[0] * 2.48218 * std::log(k[1] * (1 / k[2]) * gain * signal + 1);
            break;
        }
        break;
    }

    //Calculate xy forces & drag forces
    if (wall_toggle) {
        if (position[0] < frame_center_haptic[0] - frame_size_haptic[0] / 2) {
            force[0] = wall_stiffness * (frame_center_haptic[0] - frame_size_haptic[0] / 2 - position[0]) + force[0];
        }
        else if (position[0] > frame_center_haptic[0] + frame_size_haptic[0] / 2) {
            force[0] = (frame_center_haptic[0] + frame_size_haptic[0] / 2 - position[0]) + force[0];
        }
        else if (drag_toggle[0]){
            force[0] = -1 * k_drag[0] * velocity[0] + force[0];
        }

        if (position[2] < frame_center_haptic[1] - frame_size_haptic[1] / 2) {
            force[2] = wall_stiffness * (frame_center_haptic[1] - frame_size_haptic[1] / 2 - position[2]) + force[2];
        }
        else if (position[2] > frame_center_haptic[1] + frame_size_haptic[1] / 2) {
            force[2] = wall_stiffness * (frame_center_haptic[1] + frame_size_haptic[1] / 2 - position[2]) + force[2];
        }
        else if (drag_toggle[2]) {
            force[2] = -1 * k_drag[2] * velocity[2] + force[2];
        }

        if (drag_toggle[1]) {
            force[1] = -1 * k_drag[1] * velocity[1] + force[1];
        }
    } 
    else {
        for (int i = 0; i < 3; i++) {
            if (drag_toggle[i]) {
                force[i] = -1 * k_drag[i] * velocity[i] + force[i];
            }
        }
    }

    //Checks if the calculated forces are within the range of acceptable forces, and, if not, coerces them to within the range.
    for (int i = 0; i < 3; i++) {
        if (force[i] > force_max[i]) {
            force[i] = force_max[i];
        }
        else if (force[i] < force_min[i]) {
            force[i] = force_min[i];
        }
    }

    //Writes the final forces
    if (safetip == false) {
        hdSetDoublev(HD_CURRENT_FORCE, force);
    }
    else {
        hduVector3Dd zeroforce = { 0, 0, 0 };
        hdSetDoublev(HD_CURRENT_FORCE, zeroforce);
    }

    //Handles the sequence of choosing points to plane
    if (num_clicks <= 2 && planing_toggle) {
        time_t time_current = time(NULL);
        if (time_current - time_initial >= 2 && force[1] >= force_trigger) {
            time_initial = time_current;
            plane_points[num_clicks] = position;
            if (num_clicks == 2) {
                hduVector3Dd a = plane_points[1] - plane_points[0];
                hduVector3Dd b = plane_points[2] - plane_points[1];
                hduVector3Dd normal = -1 * a.crossProduct(b);
                if (normal[1] < 0) {
                    normal = -1 * normal;
                }
                planeProperties.n = normal;
                planeProperties.d = -1 * (normal[0] * plane_points[0][0] + normal[1] * plane_points[0][1] + normal[2] * plane_points[0][2]);
            }
            num_clicks++;
        }
    }
    else if (num_clicks == 3 && position[1] >= 80 && planing.active == false) {
        planing.active = true;
    }

    hdEndFrame(hdGetCurrentDevice());

    // In case of error, terminate the callback.
    HDErrorInfo error;
    if (HD_DEVICE_ERROR(error = hdGetError()))
    {
        hduPrintError(stderr, &error, "Error detected during main scheduler callback\n");

        if (hduIsSchedulerError(&error))
        {
            return HD_CALLBACK_DONE;
        }
    }

    return HD_CALLBACK_CONTINUE;
}

/*** BEGIN LABVIEW FUNCTIONS ***/
/*MAIN*/
//Start & Stop Functions
__declspec(dllexport) int start_haptic_loop(int input)
{
    // Mode switch
        //0: Read mode
        //1: STM Write mode
        //2: AFM Write mode 
    mode = input;
    
    // Main start function
    HDErrorInfo error;
    num_clicks = 0;

    // Initialize the default haptic device.
    HHD hHD = hdInitDevice(HD_DEFAULT_DEVICE);
    if (HD_DEVICE_ERROR(error = hdGetError()))
    {
        // Failed to initialize haptic device
        return -2;
    }

    // Start the servo scheduler and enable forces.
    hdEnable(HD_FORCE_OUTPUT);
    hdStartScheduler();
    if (HD_DEVICE_ERROR(error = hdGetError()))
    {
        //Error
        return -1;
    }

    // Schedule the frictionless plane callback, which will then run at 
    // servoloop rates and command forces if the user penetrates the plane.
    HDCallbackCode hPlaneCallback = hdScheduleAsynchronous(
        FrictionlessPlaneCallback, 0, HD_DEFAULT_SCHEDULER_PRIORITY);

    while (safetip == false)
    {
        return 3;
        if (!hdWaitForCompletion(hPlaneCallback, HD_WAIT_CHECK_STATUS))
        {
            break;
        }
    }

    // Cleanup and shutdown the haptic device, cleanup all callbacks.
    hdStopScheduler();
    hdUnschedule(hPlaneCallback);
    hdDisableDevice(hHD);
    return 0;
}
__declspec(dllexport) void stop_haptic_loop() {
    num_clicks = 0;
    HHD hHD = hdInitDevice(HD_DEFAULT_DEVICE);
    HDCallbackCode hPlaneCallback = hdScheduleAsynchronous(
        FrictionlessPlaneCallback, 0, HD_DEFAULT_SCHEDULER_PRIORITY);
    hdStopScheduler();
    hdUnschedule(hPlaneCallback);
    hdDisableDevice(hHD);
}

/*** PEN PROPERTIES ***/
//Position
__declspec(dllexport) void position_get(double *output) {
    output[0] = position[0];
    output[1] = position[2];
    output[2] = position[1];
}
__declspec(dllexport) void position_reframed_get(double* frame_properties, bool zscaling, double surface_location, double pos_gain, double* output) {   
    double frame_center_nano[2] = { frame_properties[0], frame_properties[1] };
    double frame_size_nano[2] = { frame_properties[2], frame_properties[3] };

    double positionReframed[3] = {position[0], position[2], position[1]};

    //planing
    if (planing.toggle && num_clicks == 2 && planing.active) {
        if (mode == 2) {
            positionReframed[2] = positionReframed[2] + -1 * (planeProperties.d + planeProperties.n[0] * positionReframed[0] + planeProperties.n[1]*positionReframed[1]) / planeProperties.n[2];
        }
        else {
            positionReframed[2] = 79 * pow(10, -9);
        }
    }

    if (wall_toggle) {
        for (int i = 0; i < 2; i++) {
            if (positionReframed[i] < frame_center_haptic[i] - frame_size_haptic[i] / 2) {
                positionReframed[i] = frame_center_haptic[i] - frame_size_haptic[i] / 2;
            }
            else if (positionReframed[i] > frame_center_haptic[i] + frame_size_haptic[i] / 2) {
                positionReframed[i] = frame_center_haptic[i] + frame_size_haptic[i] / 2;
            }
        }
    }
    positionReframed[0] = frame_size_nano[0] / frame_size_haptic[0] * (positionReframed[0] - frame_center_haptic[0]) + frame_center_nano[0];
    positionReframed[1] = -1 * frame_size_nano[1] / frame_size_haptic[1] * (positionReframed[1] - frame_center_haptic[1]) + frame_center_nano[1];

    //zscaling
    if (zscaling) {
        //puts floor at -50 mm down (remove if unnecessary)
        if (positionReframed[2] < -50) {
            positionReframed[2] = -50;
        }
        positionReframed[2] = 1 / pos_gain * std::pow(10, -12) * positionReframed[2] + surface_location;
    }

    output[0] = positionReframed[0];
    output[1] = positionReframed[1];
    output[2] = positionReframed[2];
}

//Velocity
__declspec(dllexport) void velocity_get(double* output) {
    output[0] = velocity[0];
    output[1] = velocity[2];
    output[2] = velocity[1];
}
__declspec(dllexport) void velocity_reframed_get(double* frame_properties, double* output) {
    double frame_origin_nano[2] = { frame_properties[0], frame_properties[1] };
    double frame_size_nano[2] = { frame_properties[2], frame_properties[3] };

    output[0] = velocity[0] * frame_size_nano[0] / frame_size_haptic[0];
    output[1] = -velocity[2] * frame_size_nano[1] / frame_size_haptic[1];
    output[2] = velocity[1];
}

//Forces
__declspec(dllexport) void forces_get(double* output) {
    hduVector3Dd force;
    hdGetDoublev(HD_CURRENT_FORCE, force);
    output[0] = force[0];
    output[1] = force[2];
    output[2] = force[1];
}
__declspec(dllexport) void forces_set(double* input) {
    force_labview[0] = input[0];
    force_labview[1] = input[2];
    force_labview[2] = input[1];
}
__declspec(dllexport) void drag_force_set(int* toggle, double* coeff) {
    drag_toggle[0] = toggle[0];
    drag_toggle[1] = toggle[2];
    drag_toggle[2] = toggle[1];

    k_drag[0] = coeff[0];
    k_drag[1] = coeff[2];
    k_drag[2] = coeff[1];
}
__declspec(dllexport) void force_extrema_set(double* maxforce, double* minforce) {
    force_max[0] = maxforce[0];
    force_max[1] = maxforce[2];
    force_max[2] = maxforce[1];

    force_min[0] = minforce[0];
    force_min[1] = minforce[2];
    force_min[2] = minforce[1];
}

//Angles
__declspec(dllexport) void angles_get(double *output) {
    hduVector3Dd angles;
    hdGetDoublev(HD_CURRENT_GIMBAL_ANGLES, angles);
    output[0] = angles[0];
    output[1] = angles[1];
    output[2] = angles[2];
}

//Button State
__declspec(dllexport) int8_t buttonstate_get() {
    HDint nCurrentButtons;
    hdGetIntegerv(HD_CURRENT_BUTTONS, &nCurrentButtons);
    return nCurrentButtons;
}

/*OTHER*/
__declspec(dllexport) void safety_trigger(bool input, bool *output) {
    if (input) {
        safetip = input;
        stop_haptic_loop();
        *output = true;
    }
    else {
        *output = false;
    }
}
__declspec(dllexport) void frame_wall_set(bool input, double k_wall) {
    wall_toggle = input;
    wall_stiffness = k_wall;
}
__declspec(dllexport) void signal_input(double input_signal, double input_gain) {
    signal = input_signal;
    gain = input_gain;
}
__declspec(dllexport) void surface_force_set(double* input, double* output, int setting) {
    surface_force = setting;
    for (int i = 0; i < 4; i++) {
        k[i] = output[i];
    }
}
__declspec(dllexport) int8_t planing_set(bool reset, bool toggle) {
    if (reset) {
        num_clicks = 0;
        planing.active = false;
    }
    planing.toggle = toggle;
    int8_t x = num_clicks;
    return x;
}