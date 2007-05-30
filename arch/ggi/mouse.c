/*
 * $Source: /cvs/cvsroot/d2x/arch/ggi/mouse.c,v $
 * $Revision: 1.1 $
 * $Author: bradleyb $
 * $Date: 2001/10/24 09:25:05 $
 *
 * GGI mouse support
 *
 * $Log: mouse.c,v $
 * Revision 1.1  2001/10/24 09:25:05  bradleyb
 * Moved input stuff to arch subdirs, as in d1x.
 *
 * Revision 1.4  2001/01/29 14:03:57  bradleyb
 * Fixed build, minor fixes
 *
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ggi/gii.h>
#include "fix.h"
#include "timer.h"
#include "event.h"
#include "mouse.h"

struct tMouseButton {
 ubyte pressed;
 fix time_wentDown;
 fix time_heldDown;
 uint numDowns;
 uint num_ups;
};

#define MOUSE_MAX_BUTTONS 3

static struct tMouseInfo {
 struct tMouseButton buttons[MOUSE_MAX_BUTTONS];
//added on 10/17/98 by Hans de Goede for mouse functionality
 int min_x, min_y;
 int max_x, max_y;
 int delta_x, delta_y;
 int x,y;
} Mouse;

void mouse_correct()
{
	if (Mouse.x < Mouse.min_x)
		Mouse.x = Mouse.min_x;
	else if (Mouse.x > Mouse.max_x)
		Mouse.x = Mouse.max_x;
	if (Mouse.y < Mouse.min_y)
		Mouse.y = Mouse.min_y;
	else if (Mouse.y > Mouse.max_y)
		Mouse.y = Mouse.max_y;
}

void mouse_handler_absolute(int x, int y)
{
	Mouse.delta_x += (x - Mouse.x);
	Mouse.delta_y += (y - Mouse.y);
	Mouse.x = x;
	Mouse.y = y;
//	mouse_correct();
}

void mouse_handler_relative(int x, int y)
{
	Mouse.delta_x += x;
	Mouse.delta_y += y;
	Mouse.x += x;
	Mouse.y += y;
//	mouse_correct();
}	

void mouse_handler_button(int button, ubyte state)
{
	if (!Mouse.buttons[button].pressed && state)
	{
		Mouse.buttons[button].time_wentDown = TimerGetFixedSeconds();
		Mouse.buttons[button].numDowns++;
	}
	else if (Mouse.buttons[button].pressed && !state)
	{
		Mouse.buttons[button].num_ups++;
	}
			
	Mouse.buttons[button].pressed = state;
}

void Mouse_close(void)
{
}

void Mouse_init(void)
{
	memset(&Mouse, 0, sizeof(Mouse);
}

void
mouse_set_limits( int x1, int y1, int x2, int y2 )
{
	Mouse.min_x = x1;
	Mouse.max_x = x2;
	Mouse.min_y = y1;
	Mouse.max_y = y2;
}

void mouse_flush()	// clears all mice events...
{
	Mouse.x = 0;
	Mouse.y = 0;
	Mouse.delta_x = 0;
	Mouse.delta_y = 0;
}

//========================================================================
void mouse_get_pos( int *x, int *y)
{
	event_poll();
	*x = Mouse.x;
	*y = Mouse.y;
}

void MouseGetDelta( int *dx, int *dy )
{
	event_poll();
	*dx = Mouse.delta_x;
	*dy = Mouse.delta_y;
	Mouse.delta_x = 0;
	Mouse.delta_y = 0;
}

int MouseGetButtons()
{
	ubyte buttons = 0;
	int i;
	event_poll();
	for (i = 0; i < MOUSE_MAX_BUTTONS; i++)
		buttons |= (Mouse.buttons[i].pressed << i);
	return buttons;
}

void mouse_set_pos( int x, int y)
{
	Mouse.x = x;
	Mouse.y = y;
}

void mouse_get_cyberman_pos( int *x, int *y )
{
}

// Returns how long this button has been down since last call.
fix MouseButtonDownTime(int button)
{
	if (Mouse.buttons[button].pressed)
		return (TimerGetFixedSeconds() - Mouse.buttons[button].time_wentDown);
	else
		return 0;
}

// Returns how many times this button has went down since last call
int MouseButtonDownCount(int button)
{
	int count = Mouse.buttons[button].numDowns;
	Mouse.buttons[button].numDowns = 0;
	return count;	
}

// Returns 1 if this button is currently down
int MouseButtonState(int button)
{
	return Mouse.buttons[button].pressed;
}
