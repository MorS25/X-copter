/* Copyright (C) 2015  Matteo Hausner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "XPLMNavigation.h"
#include "XPLMProcessing.h"
#include "XPStandardWidgets.h"
#include "XPWidgets.h"
#include "XPLMUtilities.h" //REMOVE

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// define name
#define NAME "X-copter"
#define NAME_LOWERCASE "x_copter"

// define version
#define VERSION "0.1"

// define maximum distance of the pick up location from the present position in meters
#define PICK_UP_LOCATION_MAX_DISTANCE 50000

// define minimum distance of the drop off location from the present position in meters
#define DROP_OFF_LOCATION_MAX_DISTANCE 50000

// global dataref variables
static XPLMDataRef ongroundAnyDataRef = NULL, latitudeDataRef = NULL, longitudeDegDataRef = NULL, earthRadiusMDataRef = NULL;

// global widget variables
static XPWidgetID briefingWidget = NULL, briefingCaption = NULL, startMissionButton = NULL;

// global internal variables
static int missionUnderway = 0;
static float missionStartTime = 0.0f;
static XPLMNavRef pickUpLocation = XPLM_NAV_NOT_FOUND, dropOffLocation = XPLM_NAV_NOT_FOUND;

// converts from degrees to radians
inline double DegreesToRadians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

// converts from degrees to radians
inline double RadiansToDegress(double radians)
{
    return radians * (180.0 / M_PI);
}

// calculates the destination point given distance and bearing from a starting point
static void GetDestinationPoint(float *destinationLatitude, float *destinationLongitude, float startLatitude, float startLongitude, float distance, float bearing)
{
    double d = (double) (distance / XPLMGetDataf(earthRadiusMDataRef));

    *destinationLatitude = (float )RadiansToDegress(asin(sin(DegreesToRadians((double) startLatitude)) * cos(d) + cos(DegreesToRadians((double) startLatitude)) * sin(d) * cos(DegreesToRadians((double) bearing))));
    *destinationLongitude = (float) RadiansToDegress(DegreesToRadians((double) startLongitude) + atan2(sin(DegreesToRadians((double) bearing)) * sin(d) * cos(DegreesToRadians((double) startLatitude)), cos(d) - sin(DegreesToRadians((double) startLatitude)) * sin(DegreesToRadians((double) *destinationLatitude))));
}

// hides the briefing widget
static void HideBriefingWidget(void)
{
    if (XPIsWidgetVisible(briefingWidget))
        XPHideWidget(briefingWidget);
}

// handles the briefing widget
static int BriefingWidgetHandler(XPWidgetMessage inMessage, XPWidgetID inWidget, long inParam1, long inParam2)
{
    if (inMessage == xpMessage_CloseButtonPushed)
        HideBriefingWidget();
    else if (inMessage == xpMsg_MouseUp && inParam1 == (long) startMissionButton)
    {
        missionStartTime = XPLMGetElapsedTime();
        missionUnderway = 1;

        HideBriefingWidget();
    }

    return 0;
}

static void UpdateBriefingWidget(void)
{
    char briefingString[2048];
    if (pickUpLocation != XPLM_NAV_NOT_FOUND && dropOffLocation != XPLM_NAV_NOT_FOUND && dropOffLocation != pickUpLocation)
    {
        // provide user with mission information
        char pickUpLocationId[32];
        char pickUpLocationName[256];
        XPLMGetNavAidInfo(pickUpLocation, NULL, NULL, NULL, NULL, NULL, NULL, pickUpLocationId, pickUpLocationName, NULL);
        char dropOffLocationId[32];
        char dropOffLocationName[256];
        XPLMGetNavAidInfo(dropOffLocation, NULL, NULL, NULL, NULL, NULL, NULL, dropOffLocationId, dropOffLocationName, NULL);
        sprintf(briefingString, "Hello X-Copter Alfa One, your mission is to pick up a VIP at [%s] \"%s\" and take them to [%s] \"%s\". Time is crucial, so get cracking!", pickUpLocationId, pickUpLocationName, dropOffLocationId, dropOffLocationName);
    }
    else
       // notify user that the current location is unsuitable for a mission
       sprintf(briefingString, "Unfortunately there are no missions avaiable at your current location, please try looking for a mission in an area with some helipads.");

    XPSetWidgetDescriptor(briefingCaption, briefingString);
}

// handles the menu-entries
static void MenuHandlerCallback(void *inMenuRef, void *inItemRef)
{
    // new mission menu entry
    if ((long) inItemRef == 0)
    {
        missionUnderway = 0;
        pickUpLocation = XPLM_NAV_NOT_FOUND;
        dropOffLocation = XPLM_NAV_NOT_FOUND;

        float latitude = (float) XPLMGetDataf(latitudeDataRef);
        float longitude = (float) XPLMGetDataf(longitudeDegDataRef);

        float destinationLatitude = 0.0f, destinationLongitude = 0.0f;
        float distance = (float) (rand() % PICK_UP_LOCATION_MAX_DISTANCE);
        float bearing = (float) (rand() % 360);

        GetDestinationPoint(&destinationLatitude, &destinationLongitude, latitude, longitude, distance, bearing);
        pickUpLocation = XPLMFindNavAid("[H] ", NULL, &destinationLatitude, &destinationLongitude, NULL, xplm_Nav_Airport);

        char out[2048];
        sprintf(out, "Lat: %f  Lon: %f Dist: %f Bear: %f Ret: %d\n", destinationLatitude, destinationLongitude, distance, bearing, pickUpLocation);
        XPLMDebugString(out);

        if (pickUpLocation != XPLM_NAV_NOT_FOUND)
        {
            destinationLatitude = 0.0f, destinationLongitude = 0.0f;
            distance = (float) (rand() % DROP_OFF_LOCATION_MAX_DISTANCE);
            bearing = (float) (rand() % 360);

            GetDestinationPoint(&destinationLatitude, &destinationLongitude, latitude, longitude, distance, bearing);
            dropOffLocation = XPLMFindNavAid("[H] ", NULL, &destinationLatitude, &destinationLongitude, NULL, xplm_Nav_Airport);

            char out[2048];
            sprintf(out, "Lat: %f  Lon: %f Dist: %f Bear: %f Ret: %d\n", destinationLatitude, destinationLongitude, distance, bearing, dropOffLocation);
            XPLMDebugString(out);

            while (dropOffLocation == pickUpLocation)
                dropOffLocation = XPLMGetNextNavAid(dropOffLocation);
        }

        if (briefingWidget == NULL)
        {
            // create briefing widget
            int x = 10, y = 0, w = 800, h = 400;
            XPLMGetScreenSize(NULL, &y);
            y -= 100;

            int x2 = x + w;
            int y2 = y - h;

            // widget window
            briefingWidget = XPCreateWidget(x, y, x2, y2, 1, NAME" Mission Briefing", 1, 0, xpWidgetClass_MainWindow);

            // add close box
            XPSetWidgetProperty(briefingWidget, xpProperty_MainWindowHasCloseBoxes, 1);

            // add mission briefing caption
            briefingCaption = XPCreateWidget(x + 10, y - 30, x2 - 20, y - 135, 1, NULL, 0, briefingWidget, xpWidgetClass_Caption);

            // add start mission button
            startMissionButton = XPCreateWidget(x + 10, y - 155, x + 80, y - 170, 1, "Start Mission", 0, briefingWidget, xpWidgetClass_Button);

            // register widget handler
            XPAddWidgetCallback(briefingWidget, (XPWidgetFunc_t) BriefingWidgetHandler);
        }
        else
        {
            // brifing widget already created
            if (!XPIsWidgetVisible(briefingWidget))
                XPShowWidget(briefingWidget);
        }

        UpdateBriefingWidget();
    }
}

// flightloop-callback that handles the mission
static float FlightLoopCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon)
{
    return -1.0f;
}

// draw-callback that performs the actual drawing of the ?
static int DrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void *inRefcon)
{
    return 1;
}

PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc)
{
    // set plugin info
    strcpy(outName, NAME);
    strcpy(outSig, "de.bwravencl." NAME_LOWERCASE);
    strcpy(outDesc, NAME " let's you fly dynamic helicopter missions!");

    // initialize random seed
    srand(time(NULL));

    // obtain datarefs
    ongroundAnyDataRef = XPLMFindDataRef("sim/flightmodel/failures/onground_any");
    latitudeDataRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
    longitudeDegDataRef = XPLMFindDataRef("sim/flightmodel/position/longitude");
    earthRadiusMDataRef = XPLMFindDataRef("sim/physics/earth_radius_m");

    // create menu-entries
    int subMenuItem = XPLMAppendMenuItem(XPLMFindPluginsMenu(), NAME, 0, 1);
    XPLMMenuID menu = XPLMCreateMenu(NAME, XPLMFindPluginsMenu(), subMenuItem, MenuHandlerCallback, 0);
    XPLMAppendMenuItem(menu, "New Mission", (void*) 0, 1);

    // register flight loop callback
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1, NULL);

    // register draw callback
    XPLMRegisterDrawCallback(DrawCallback, xplm_Phase_LastCockpit, 0, NULL);

    return 1;
}

PLUGIN_API void	XPluginStop(void)
{
}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API int XPluginEnable(void)
{
    return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, long inMessage, void *inParam)
{
}
