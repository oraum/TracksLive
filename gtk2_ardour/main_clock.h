/*
    Copyright (C) 2012 Paul Davis

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

#ifndef __main_clock_h__
#define __main_clock_h__

#include "audio_clock.h"

/** A simple subclass of AudioClock that adds the `display delta to edit cursor' option to its context menu */
class MainClock : public AudioClock
{
public:
	MainClock (const std::string &, bool, const std::string &, bool, bool, bool primary, bool duration = false, bool with_info = false);

private:
	
	void build_ops_menu ();
	void display_delta_to_edit_cursor ();
	bool _primary;
};

#endif /* __main_clock_h__ */