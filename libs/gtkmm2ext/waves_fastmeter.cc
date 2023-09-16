/*
    Copyright (C) 2003-2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#include <stdlib.h>

#include <glibmm.h>
#include <gdkmm.h>
#include <gdkmm/rectangle.h>
#include <gtkmm2ext/waves_fastmeter.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/rgb_macros.h>

using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o, int len,
		int clr0, int clr1, int clr2, int clr3,
		int clr4, int clr5, int clr6, int clr7,
		int clr8, int clr9,
		int bgc0, int bgc1,
		int bgh0, int bgh1,
		float stp0, float stp1,
		float stp2, float stp3,
		int styleflags
		)
	: _styleflags(1)
	, orientation(o)
	, _level_bar_size (0)
	, _peak_bar_position (0)
	, _peak_bar_size (2)
	, hold_cnt(hold)
	, hold_state(0)
	, bright_hold(false)
	, current_level(0)
	, current_peak(0)
	, highlight(false)
{
	_clr[0] = clr0;
	_clr[1] = clr1;
	_clr[2] = clr2;
	_clr[3] = clr3;
	_clr[4] = clr4;
	_clr[5] = clr5;
	_clr[6] = clr6;
	_clr[7] = clr7;
	_clr[8] = clr8;
	_clr[9] = clr9;

	_bgc[0] = bgc0;
	_bgc[1] = bgc1;

	_bgh[0] = bgh0;
	_bgh[1] = bgh1;

	_stp[0] = stp0;
	_stp[1] = stp1;
	_stp[2] = stp2;
	_stp[3] = stp3;

	set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	if (!len) {
		len = 250;
	}
	if (orientation == Vertical) {
		request_height = len;
		request_width = dimen;

	} else {
		request_height = dimen;
		request_width = len;
	}

	clear ();
}

FastMeter::~FastMeter ()
{
}

void
FastMeter::set_hold_count (long val)
{
	if (val < 1) {
		val = 1;
	}

	hold_cnt = val;
	hold_state = 0;
	current_peak = 0;

	queue_draw ();
}

void
FastMeter::on_size_request (GtkRequisition* req)
{
	if (orientation == Vertical) {
		vertical_size_request (req);
	} else {
		horizontal_size_request (req);
	}
}

void
FastMeter::vertical_size_request (GtkRequisition* req)
{
	req->height = request_height;
	req->height += 2;

	req->width  = request_width;
}

void
FastMeter::horizontal_size_request (GtkRequisition* req)
{
	req->width = request_width;
	req->width += 2;

	req->height  = request_height;
}

void
FastMeter::on_size_allocate (Gtk::Allocation &alloc)
{
	if (orientation == Vertical) {
		vertical_size_allocate (alloc);
	} else {
		horizontal_size_allocate (alloc);
	}
	queue_draw ();
}

void
FastMeter::vertical_size_allocate (Gtk::Allocation &alloc)
{
	if (alloc.get_width() != request_width) {
		alloc.set_width (request_width);
	}

	Gtk::Image::on_size_allocate (alloc);
}

void
FastMeter::horizontal_size_allocate (Gtk::Allocation &alloc)
{
	if (alloc.get_height() != request_height) {
		alloc.set_height (request_height);
	}

	Gtk::Image::on_size_allocate (alloc);
}

bool FastMeter::on_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation alc = get_allocation ();
	GdkRectangle area = { alc.get_x (), alc.get_y (), alc.get_width (), alc.get_height () };
	
	Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context ();
	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cr->clip ();
	
	if (orientation == Vertical) {
		vertical_expose (cr->cobj(), area);
	} else {
		horizontal_expose (cr->cobj(), area);
	}

	return true;
}

void
FastMeter::vertical_expose (cairo_t* cr, GdkRectangle& area)
{
	cairo_set_source_rgba (cr,
                           UINT_RGBA_R (_bgc[0]) / 255.0,
                           UINT_RGBA_G (_bgc[0]) / 255.0,
                           UINT_RGBA_B (_bgc[0]) / 255.0,
                           UINT_RGBA_A (_bgc[0]) / 255.0);
	cairo_rectangle (cr, area.x, area.y, area.width, area.height - _level_bar_size);
	cairo_fill (cr);

	cairo_set_source_rgba (cr, 0.69, 0.69, 0.69, 1);
	cairo_rectangle (cr, area.x, area.y + area.height - _level_bar_size, area.width, _level_bar_size);
	cairo_fill (cr);

	// draw peak bar

	if (hold_state) {
		float peak100 = current_peak * 100;

		int i = (sizeof (_stp)/sizeof (_stp [0])) - 1;
		for (; i > 0; i--) {
			if ((peak100 <= _stp [i]) && (peak100 >= _stp [i-1])) {
				break;
			}
		}

		int peak_color = _clr [(i + 1) * 2];
		cairo_set_source_rgba (cr,
							   UINT_RGBA_R (peak_color) / 255.0,
							   UINT_RGBA_G (peak_color) / 255.0,
							   UINT_RGBA_B (peak_color) / 255.0,
							   UINT_RGBA_A (peak_color) / 255.0);
		cairo_rectangle (cr, area.x, area.y + area.height - _peak_bar_position, area.width, _peak_bar_size);
		
		if (bright_hold) {
			cairo_fill_preserve (cr);
			cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.3);
		}
		cairo_fill (cr);

	} else {
		_peak_bar_position = 0;
	}
}

void
FastMeter::horizontal_expose (cairo_t* cr, GdkRectangle& area)
{
//	not implemented yet
}

void
FastMeter::set (float lvl, float peak)
{
	float old_level = current_level;
	float old_peak = current_peak;

	if ((request_width <= 0) || (request_height <= 0)) {
		return;
	}

	if (peak == -1) {
		if (lvl >= current_peak) {
			current_peak = lvl;
			hold_state = ((current_peak == 0) ? 0 : hold_cnt);
		}

		if (hold_state > 0) {
			if (--hold_state == 0) {
				current_peak = lvl;
				hold_state = ((current_peak == 0) ? 0 : hold_cnt);
			}
		}
		bright_hold = false;
	} else {
		current_peak = peak;
		hold_state = 1;
		bright_hold = true;
	}

	current_level = lvl;

	if (current_level == old_level && current_peak == old_peak && (hold_state == 0 || peak != -1)) {
		return;
	}

	if (orientation == Vertical) {
		queue_vertical_redraw ();
	} else {
		queue_horizontal_redraw ();
	}
}

void
FastMeter::queue_vertical_redraw ()
{
	
	Gtk::Allocation alc = get_allocation ();
	gint origin_x = alc.get_x ();
	gint origin_y = alc.get_y ();
	gint width = alc.get_width ();
	gint height = alc.get_height ();
	
	gint new_level_bar_size = (gint) floor (height * current_level);
	gint new_peak_bar_position = max(0, (gint) floor (height * current_peak));

	if (_level_bar_size != new_level_bar_size) {
		if (new_level_bar_size > _level_bar_size) {
			queue_draw_area (origin_x, origin_y + height - new_level_bar_size, width, new_level_bar_size - _level_bar_size);
		} else {
			queue_draw_area (origin_x, origin_y + height - _level_bar_size, width, _level_bar_size - new_level_bar_size);
		}
		_level_bar_size = new_level_bar_size;
	}


	/* redraw the last place where the last peak hold bar was;
	 the next expose will draw the new one whether its part of
	 expose region or not.
	 */
	if (_peak_bar_position != new_peak_bar_position) {
		queue_draw_area (origin_x, origin_y + height - _peak_bar_position, width, _peak_bar_size);
		if (hold_state && current_peak > 0) {
			queue_draw_area (origin_x, origin_y + height - new_peak_bar_position, width, _peak_bar_size);
		}
		_peak_bar_position = new_peak_bar_position;
	}
}

void
FastMeter::queue_horizontal_redraw ()
{
	// not implemented yet
}

void
FastMeter::set_highlight (bool onoff)
{
	if (highlight != onoff) {
		highlight = onoff;
		queue_draw ();
	}
}

void
FastMeter::clear ()
{
	current_level = 0;
	current_peak = 0;
	hold_state = 0;
	_peak_bar_position = _level_bar_size = 0;
	queue_draw ();
}
