#pragma once
#include <pebble.h>

//messaging keys
#define PersonID 0
#define PersonName 1
#define SGV 2
#define Battery 5
#define Direction 6
#define MinutesAgo 7
#define GraphHighPoint 10
#define GraphLowPoint 11
#define GraphXPoints 110
#define GraphYPoints 111
#define GraphStartStop 12
#define GetGraphs 13
#define GraphTimeScale 14
#define IntervalTime 50
#define EnableAlerts 51
#define RespectQuietTime 52
#define LowAlertValue 53
#define HighAlertValue 54
#define LowSnoozeTimeout 55
#define HighSnoozeTimeout 56
#define SendAlert 60

//declare reousrce IDs in this array for easy access when drawing bitmap arrows
static const uint32_t ICONS[] = {
	RESOURCE_ID_IMAGE_NONE, //0						
	RESOURCE_ID_IMAGE_UPUP,  //1
	RESOURCE_ID_IMAGE_UP,			 //2
	RESOURCE_ID_IMAGE_UP45,					 //3
	RESOURCE_ID_IMAGE_FLAT, //4
	RESOURCE_ID_IMAGE_DOWN45,   //5
	RESOURCE_ID_IMAGE_DOWN,	 //6
	RESOURCE_ID_IMAGE_DOWNDOWN,  //7
	RESOURCE_ID_IMAGE_NONE, //8					
};