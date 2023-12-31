/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_time_selection_h__
#define __ardour_gtk_time_selection_h__

#include <list>
#include "ardour/types.h"
#include "track_view_list.h"
#include "location_list_item.h"

namespace ARDOUR {
	class RouteGroup;
}

class TimeSelection : public std::list<ARDOUR::AudioRange>
{
public:
    TimeSelection ();
	ARDOUR::AudioRange& operator[](uint32_t);

    ARDOUR::framepos_t start();
    ARDOUR::framepos_t end_frame();
    ARDOUR::framepos_t length();
    
	bool consolidate ();

    TrackViewList tracks_in_range;
    void set_location_list_selection (LocationListSelection* lls);
    LocationListSelection* get_location_list_selection ();
    bool update_current_selection_is_allowed ();
    void set_ignore_lls_change (bool yn);
private:
    LocationListSelection* _location_list_selection;
    bool _ignore_lls_change; ///< ignore _location_list_selection change
};


#endif /* __ardour_gtk_time_selection_h__ */
