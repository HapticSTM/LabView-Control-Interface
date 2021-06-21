// Position.cpp : Defines the exported functions for the DLL.
#include "pch.h" // use stdafx.h in Visual Studio 2017 and earlier
#include <utility>
#include <limits.h>
#include "PositionDLL.h"

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

#include <HL/hl.h>

#include <string>
#include <iostream>
#include <Windows.h>
#include <vector>

//This is a dumb way to use fractions when declaring variables.
double frac(double xin, double yin) {
    return xin / yin;
}

//height of the plane
double height = 50;

//parameters for the initial 159x159 canvas
double imgsize = 159;
const double xzero = -60;
const double zzero = 80;
double scalex = frac(imgsize, 120);
double scalez = frac(imgsize, 120);

//parameters for the zoomed in canvas
double xpoint = 0;
double zpoint = 0;
double sizeofbox = 159;

//the force component in each direction
double forcex = 0;
double forcey = 0;
double forcez = 0;

//the positions mapped to the initial 159x159 canvas
double xmap0;
double zmap0;

int sequence = 0;

int bcurrent = 0;
int blast = 0;

hduVector3Dd point1, point2, point3;

typedef struct
{
    HDboolean m_buttonState;       /* Has the device button has been pressed. */
    hduVector3Dd m_devicePosition; /* Current device coordinates. */
    HDErrorInfo m_error;
} DeviceData;

static DeviceData gServoDeviceData;


/*******************************************************************************
 Haptic plane callback.  The plane is oriented along Y=0 and provides a
 repelling force if the device attempts to penetrates through it.
*******************************************************************************/
HDCallbackCode HDCALLBACK FrictionlessPlaneCallback(void* data)
{
    // Stiffnes, i.e. k value, of the plane.  Higher stiffness results
    // in a harder surface.
    const double planeStiffness = 0.25;
    // Amount of force the user needs to apply in order to pop through
    // the plane.
    const double popthroughForceThreshold = 3.0;
    // Plane direction changes whenever the user applies sufficient
    // force to popthrough it.
    // 1 means the plane is facing +Y.
    // -1 means the plane is facing -Y.
    static int directionFlag = 1;
    const double wallStiffness = 0.25;

    sequence = 0;

    hdBeginFrame(hdGetCurrentDevice());
    
    // Get the position of the device.
    hduVector3Dd position;
    hdGetDoublev(HD_CURRENT_POSITION, position);

    // Get the velocity of the device.
    hduVector3Dd velocity;
    hdGetDoublev(HD_CURRENT_VELOCITY, velocity);

    //Get the button states
    HDint nCurrentButtons, nLastButtons;

    hdGetIntegerv(HD_CURRENT_BUTTONS, &nCurrentButtons);
    hdGetIntegerv(HD_LAST_BUTTONS, &nLastButtons);

    bcurrent = nCurrentButtons;
    blast = nLastButtons;

    if (bcurrent + blast == 2) {
        if (sequence == 0)
        {
            point1 = position[1]
        }
    }

    //These map the raw position of the arm to a 159x159 canvas.
        //Both are bounded by [0, 160].
    if (scalex * (position[0] - xzero) >= imgsize) {
        xmap0 = imgsize;
    }
    else if (scalex * (position[0] - xzero) <= 0) {
        xmap0 = 0;
    }
    else {
        xmap0 = scalex * (position[0] - xzero);
    }

    if (scalez * (zzero - position[2]) >= imgsize) {
        zmap0 = imgsize;
    }
    else if (scalez * (zzero - position[2]) <= 0) {
        zmap0 = 0;
    }
    else {
        zmap0 = scalez * (zzero - position[2]);
    }



    //Sets drag coefficients
    double coeffx = 0.001;
    double coeffz = 0.002;

    // If the user has penetrated the plane, set the device force to 
    // repel the user in the direction of the surface normal of the plane.
    // Penetration occurs if the plane is facing in +Y and the user's Y position
    // is negative, or vice versa.
    if (xmap0 == 0)
    {
        double penetrationDistancex = frac(159, imgsize) * fabs(scalex * (position[0] - xzero));
        double kx = wallStiffness;
        double xx = penetrationDistancex;
        forcex = kx * xx;
    }
    else if (xmap0 == imgsize)
    {
        double penetrationDistancex = frac(159, imgsize) * fabs(imgsize - scalex * (position[0] - xzero));
        double kx = wallStiffness;
        double xx = penetrationDistancex;
        forcex = -1 * kx * xx;
    }
    else {
        forcex = -1 * coeffx * velocity[0];
    }

    if (zmap0 == 0)
    {
        double penetrationDistancex = frac(159, imgsize) * fabs(scalez * (zzero - position[2]));
        double kz = wallStiffness;
        double xz = penetrationDistancex;
        forcez = -1 * kz * xz;
    }
    else if (zmap0 == imgsize)
    {
        double penetrationDistancex = frac(159, imgsize) * fabs(imgsize - scalez * (zzero - position[2]));
        double kz = wallStiffness;
        double xz = penetrationDistancex;
        forcez = kz * xz;
    }
    else {
        forcez = -1 * coeffz * velocity[2];
    }
 
    if

        double h = height;

    if ((position[1] <= h && directionFlag > 0) ||
        (position[1] > h) && (directionFlag < 0))
    {
        double penetrationDistance = fabs(position[1] - h);
        double forceDirection = directionFlag;

        // Hooke's law explicitly:
        double k = planeStiffness;
        double x = penetrationDistance * forceDirection;
        forcey = k * x;
    }
    
    
    hduVector3Dd finalforce(forcex, forcey, forcez);
    hdSetDoublev(HD_CURRENT_FORCE, finalforce);
        
    
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

//this function is run only once, and is responsible for starting the device/frames
__declspec(dllexport) int start()
{
    HDErrorInfo error;

    sequence = 0;

    // Initialize the default haptic device.
    HHD hHD = hdInitDevice(HD_DEFAULT_DEVICE);
    if (HD_DEVICE_ERROR(error = hdGetError()))
    {
        hduPrintError(stderr, &error, "Failed to initialize haptic device");
        fprintf(stderr, "\nPress any key to quit.\n");
        getch();
        return -1;
    }

    // Start the servo scheduler and enable forces.
    hdEnable(HD_FORCE_OUTPUT);
    hdStartScheduler();
    if (HD_DEVICE_ERROR(error = hdGetError()))
    {
        hduPrintError(stderr, &error, "Failed to start the scheduler");
        fprintf(stderr, "\nPress any key to quit.\n");
        getch();
        return -1;
    }

    // Schedule the frictionless plane callback, which will then run at 
    // servoloop rates and command forces if the user penetrates the plane.
    HDCallbackCode hPlaneCallback = hdScheduleAsynchronous(
        FrictionlessPlaneCallback, 0, HD_DEFAULT_SCHEDULER_PRIORITY);

    printf("Plane example.\n");
    printf("Move the device up and down to feel a plane along Y=0.\n");
    printf("Push hard against the plane to popthrough to the other side.\n");
    printf("Press any key to quit.\n\n");

    while (!_kbhit())
    {
        return 3;
        if (!hdWaitForCompletion(hPlaneCallback, HD_WAIT_CHECK_STATUS))
        {
            fprintf(stderr, "\nThe main scheduler callback has exited\n");
            fprintf(stderr, "\nPress any key to quit.\n");
            getch();
            break;
        }
    }

    // Cleanup and shutdown the haptic device, cleanup all callbacks.
    hdStopScheduler();
    hdUnschedule(hPlaneCallback);
    hdDisableDevice(hHD);
    return 0;
}

//Positions: These map the position on the canvas to desired scope of the data
__declspec(dllexport) int getposx() {
    return std::floor(frac(sizeofbox, imgsize) * xmap0 + xpoint);
}
__declspec(dllexport) double getposy() {
    hduVector3Dd position;
    hdGetDoublev(HD_CURRENT_POSITION, position);
    return position[1];
}
__declspec(dllexport) int getposz() {
    return std::floor(frac(sizeofbox, imgsize) * zmap0 + zpoint);
}

//Forces
__declspec(dllexport) double getforcex() {
    hduVector3Dd force;
    hdGetDoublev(HD_CURRENT_FORCE, force);
    return force[0];
}
__declspec(dllexport) double getforcey() {
    hduVector3Dd force;
    hdGetDoublev(HD_CURRENT_FORCE, force);
    return force[1];
}
__declspec(dllexport) double getforcez() {
    hduVector3Dd force;
    hdGetDoublev(HD_CURRENT_FORCE, force);
    return force[2];
}

//Angles
__declspec(dllexport) double gettheta() {
    hduVector3Dd angles;
    hdGetDoublev(HD_CURRENT_GIMBAL_ANGLES, angles);
    return angles[0];
}
__declspec(dllexport) double getphi() {
    hduVector3Dd angles;
    hdGetDoublev(HD_CURRENT_GIMBAL_ANGLES, angles);
    return angles[1];
}
__declspec(dllexport) double getpsi() {
    hduVector3Dd angles;
    hdGetDoublev(HD_CURRENT_GIMBAL_ANGLES, angles);
    return angles[2];
}

//Parameters
__declspec(dllexport) void config(double ypos, int x0, int z0, int scope, int sizeofimg) {
    height = ypos;
    xpoint = x0;
    zpoint = z0;
    imgsize = sizeofimg;
    sizeofbox = scope;
    scalex = frac(imgsize, 120);
    scalez = frac(imgsize, 120);
}


__declspec(dllexport) void shutdown() {
        HHD hHD = hdInitDevice(HD_DEFAULT_DEVICE);
        HDCallbackCode hPlaneCallback = hdScheduleAsynchronous(
            FrictionlessPlaneCallback, 0, HD_DEFAULT_SCHEDULER_PRIORITY);
        hdStopScheduler();
        hdUnschedule(hPlaneCallback);
        hdDisableDevice(hHD); 
}

__declspec(dllexport) int clicks() {
    return bcurrent + blast;
}