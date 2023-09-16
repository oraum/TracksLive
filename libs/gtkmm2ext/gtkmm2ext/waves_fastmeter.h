/*
    Copyright (C) 2003 Paul Davis

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

*/

#ifndef __gtkmm2ext_fastmeter_h__
#define __gtkmm2ext_fastmeter_h__

#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <cairomm/pattern.h>

#include "gtkmm/image.h"
#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

	class LIBGTKMM2EXT_API FastMeter : public Gtk::Image {
  public:
	enum Orientation {
		Horizontal,
		Vertical
	};

	FastMeter (long hold_cnt, unsigned long width, Orientation, int len=0,
			int clr0=0x008800ff, int clr1=0x008800ff,
			int clr2=0x00ff00ff, int clr3=0x00ff00ff,
			int clr4=0xffaa00ff, int clr5=0xffaa00ff,
			int clr6=0xffff00ff, int clr7=0xffff00ff,
			int clr8=0xff0000ff, int clr9=0xff0000ff,
			int bgc0=0x333333ff, int bgc1=0x444444ff,
			int bgh0=0x991122ff, int bgh1=0x551111ff,
			float stp0 = 55.0, // log_meter(-18);
			float stp1 = 77.5, // log_meter(-9);
			float stp2 = 92.5, // log_meter(-3); // 95.0, // log_meter(-2);
			float stp3 = 100.0,
			int styleflags = 3
			);
	virtual ~FastMeter ();

	void set (float level, float peak = -1);
	void clear ();

	float get_level() { return current_level; }
	float get_user_level() { return current_user_level; }
	float get_peak() { return current_peak; }

	long hold_count() { return hold_cnt; }
	void set_hold_count (long);
	void set_highlight (bool);
	bool get_highlight () { return highlight; }

protected:
	bool on_expose_event (GdkEventExpose *);
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation&);

private:

	float _stp[4];
	int _clr[10];
	int _bgc[2];
	int _bgh[2];
	int _styleflags;

	Orientation orientation;
	gint _level_bar_size;
	gint _peak_bar_position;
	gint _peak_bar_size;
	gint request_width;
	gint request_height;
	unsigned long hold_cnt;
	unsigned long hold_state;
	bool bright_hold;
	float current_level;
	float current_peak;
	float current_user_level;
	bool highlight;

	void vertical_expose (cairo_t*, GdkRectangle&);
	void vertical_size_request (GtkRequisition*);
	void vertical_size_allocate (Gtk::Allocation&);
	void queue_vertical_redraw ();

	void horizontal_expose (cairo_t*, GdkRectangle&);
	void horizontal_size_request (GtkRequisition*);
	void horizontal_size_allocate (Gtk::Allocation&);
	void queue_horizontal_redraw ();
};


} /* namespace */

 #endif /* __gtkmm2ext_fastmeter_h__ */
