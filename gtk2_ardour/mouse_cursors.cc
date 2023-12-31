/*
    Copyright (C) 2000-2010 Paul Davis

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

#include <gdkmm/cursor.h>

#include "gtkmm2ext/cursors.h"

#include "utils.h"
#include "mouse_cursors.h"
#include "editor_xpms"

using namespace ARDOUR_UI_UTILS;

char MouseCursors::__dummy_for_invalid_cursor;

MouseCursors::MouseCursors ()
	: cross_hair (0)
	, scissors (0)
	, trimmer (0)
	, right_side_trim (0)
	, anchored_right_side_trim (0)
	, left_side_trim (0)
	, anchored_left_side_trim (0)
	, right_side_trim_left_only (0)
	, left_side_trim_right_only (0)
	, fade_in (0)
	, fade_out (0)
	, selector (0)
	, grabber (0)
	, grabber_edit_point (0)
	, zoom_in (0)
	, zoom_out (0)
	, time_fx (0)
	, fader (0)
	, speaker (0)
	, midi_pencil (0)
	, midi_select (0)
	, midi_resize (0)
	, midi_erase (0)
	, up_down (0)
	, wait (0)
	, timebar (0)
	, transparent (0)
	, resize_top (0)
	, move (0)
{
}

void
MouseCursors::drop_all ()
{
	delete cross_hair; cross_hair = 0;
	delete scissors; scissors = 0;
	delete trimmer; trimmer = 0;
	delete right_side_trim; right_side_trim = 0;
	delete anchored_right_side_trim; anchored_right_side_trim = 0;
	delete left_side_trim; left_side_trim = 0;
	delete anchored_left_side_trim; anchored_left_side_trim = 0;
	delete right_side_trim_left_only; right_side_trim_left_only = 0;
	delete left_side_trim_right_only; left_side_trim_right_only = 0;
	delete fade_in; fade_in = 0;
	delete fade_out; fade_out = 0;
	delete selector; selector = 0;
	delete grabber; grabber = 0;
	delete grabber_edit_point; grabber_edit_point = 0;
	delete zoom_in; zoom_in = 0;
	delete zoom_out; zoom_out = 0;
	delete time_fx; time_fx = 0;
	delete fader; fader = 0;
	delete speaker; speaker = 0;
	delete midi_pencil; midi_pencil = 0;
	delete midi_select; midi_select = 0;
	delete midi_resize; midi_resize = 0;
	delete midi_erase; midi_erase = 0;
	delete up_down; up_down = 0;
	delete wait; wait = 0;
	delete timebar; timebar = 0;
	delete transparent; transparent = 0;
	delete resize_top; resize_top = 0;
	delete move; move = 0;

        /* no need to drop _invalid */
}

Gdk::Cursor*
MouseCursors::make_cursor (const char* name, int hotspot_x, int hotspot_y)
{
	Gtkmm2ext::CursorInfo* info = Gtkmm2ext::CursorInfo::lookup_cursor_info (name);

	if (info) {
		hotspot_x = info->x;
		hotspot_y = info->y;
	}

	Glib::RefPtr<Gdk::Pixbuf> p (::get_icon (name, _cursor_set));
	return new Gdk::Cursor (Gdk::Display::get_default(), p, hotspot_x, hotspot_y);
}

void
MouseCursors::set_cursor_set (const std::string& name)
{
	using namespace Glib;
	using namespace Gdk;

	drop_all ();
	_cursor_set = name;

	/* these will throw exceptions if their images cannot be found.
	   
	   the default hotspot coordinates will be overridden by any 
	   data found by Gtkmm2ext::Cursors::load_cursor_info(). the values
	   here from the set of cursors used by Ardour; new cursor/icon
	   sets should come with a hotspot info file.
	*/

        /* using a null GdkCursor* in a call to gdk_window_set_cursor()
           causes the GdkWindow to use the cursor of the parent window.

           Since we never set the cursor for the top level window(s),
           this will be default cursor on a given platform. 
           
           Obviously, if this was used with deeply nested windows
           some of which have non-default cursors and some don't,
           this might fail, but in reality we use it only in cases
           of single nesting or non-non-default cursors.
        */

#define DEFAULT_PLATFORM_CURSOR (Gdk::Cursor*) 0

	zoom_in = make_cursor ("zoom_in_cursor", 10, 5);
	zoom_out = make_cursor ("zoom_out_cursor", 5, 5);
	scissors = make_cursor ("scissors", 5, 0);
        all_direction_move = new Cursor (FLEUR);
	grabber = DEFAULT_PLATFORM_CURSOR;
	grabber_edit_point = DEFAULT_PLATFORM_CURSOR;
	left_side_trim = make_cursor ("trim_left_cursor", 5, 11);
	anchored_left_side_trim = make_cursor ("anchored_trim_left_cursor", 5, 11);
	right_side_trim = make_cursor ("trim_right_cursor", 23, 11);
	anchored_right_side_trim = make_cursor ("anchored_trim_right_cursor", 23, 11);
	left_side_trim_right_only = make_cursor ("trim_left_cursor_right_only", 5, 11);
	right_side_trim_left_only = make_cursor ("trim_right_cursor_left_only", 23, 11);
	fade_in = new Cursor(LEFT_PTR);
	fade_out = new Cursor(LEFT_PTR);
	move = make_cursor ("move_cursor", 11, 11);
        selector = new Cursor (XTERM); /* odd but this is actually an I-Beam */

	Gdk::Color fbg ("#ffffff");
	Gdk::Color ffg ("#000000");

	{
		RefPtr<Bitmap> source = Bitmap::create ((char const *) fader_cursor_bits, fader_cursor_width, fader_cursor_height);
		RefPtr<Bitmap> mask = Bitmap::create ((char const *) fader_cursor_mask_bits, fader_cursor_width, fader_cursor_height);
		fader = new Cursor (source, mask, ffg, fbg, fader_cursor_x_hot, fader_cursor_y_hot);
	}

	{
		RefPtr<Bitmap> source = Bitmap::create ((char const *) speaker_cursor_bits, speaker_cursor_width, speaker_cursor_height);
		RefPtr<Bitmap> mask = Bitmap::create ((char const *) speaker_cursor_mask_bits, speaker_cursor_width, speaker_cursor_height);
		speaker = new Cursor (source, mask, ffg, fbg, speaker_cursor_width >> 1, speaker_cursor_height >> 1);
	}

	{
		char pix[4] = { 0, 0, 0, 0 };
		RefPtr<Bitmap> bits = Bitmap::create (pix, 2, 2);
		Color c;
		transparent = new Cursor (bits, bits, c, c, 0, 0);
	}

	cross_hair = new Cursor (CROSSHAIR);
	trimmer =  new Cursor (SB_H_DOUBLE_ARROW);
	time_fx = new Cursor (SIZING);
	wait = new Cursor (WATCH);
	timebar = DEFAULT_PLATFORM_CURSOR;
	midi_pencil = new Cursor (PENCIL);
	midi_select = new Cursor (CENTER_PTR);
	midi_resize = new Cursor (SIZING);
	midi_erase = new Cursor (DRAPED_BOX);
	up_down = new Cursor (SB_V_DOUBLE_ARROW);
}
