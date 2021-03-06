#ifndef _PORTAL_H
#define _PORTAL_H

#include <stdint.h>
#include "gstvideo/gstvideo.h"

#define playlist_duo_SIZE 10
#define playlist_solo_SIZE 10
struct this_gun_struct {
	uint8_t brightness = 0;
	int8_t state_duo = 0;  //state reported to other gun
	int8_t state_duo_previous = 0;
	int8_t state_solo = 0; //internal state for single player modes
	int8_t state_solo_previous = 0;
	bool initiator = false; //Did this gun start the connection request?
	uint32_t clock = 0;
	uint8_t ir_pwm = 255;
	bool connected = false; 
	
	int8_t playlist_solo[playlist_solo_SIZE]={GST_GOOM,GST_LIBVISUAL_JESS,GST_GOOM2K1,GST_MOVIE2,GST_LIBVISUAL_JAKDAW,GST_MOVIE3,GST_LIBVISUAL_OINKSIE,-1,-1,-1};
	int8_t playlist_solo_index = 1;
	int8_t effect_solo = GST_GOOM;

	int8_t playlist_duo[playlist_duo_SIZE]={GST_NORMAL,GST_REVTV,GST_GLCUBE,GST_GLHEAT,GST_EDGETV,-1,-1,-1,-1,-1};
	int8_t playlist_duo_index = 1;
	int8_t effect_duo = GST_NORMAL;
	
	float latency = 0.0;
	float coretemp = 0.0;
};  


struct other_gun_struct {
	int state = 0; //state read from other gun
	int state_previous = 0;
	
	uint32_t last_seen = 0;
	uint32_t clock = 0;
};  

#define GUN_EXPIRE 1000 //expire a gun in 1 second
#define LONG_PRESS_TIME  500
#define BUTTON_ACK_BLINK 100

#define BUTTON_BOTH_LONG_BLUE 0
#define BUTTON_BOTH_LONG_ORANGE 1
#define BUTTON_ORANGE_LONG 2
#define BUTTON_ORANGE_SHORT 3
#define BUTTON_BLUE_SHORT 4
#define BUTTON_BLUE_LONG 5
#define BUTTON_NONE 6

#define WEB_ORANGE_WIFI 100
#define WEB_BLUE_WIFI 104
#define WEB_BLUE_SELF 103
#define WEB_ORANGE_SELF 101
#define WEB_CLOSE 102

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#endif