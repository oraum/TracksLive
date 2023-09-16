/*
    Copyright (C) 2015 Waves Audio Ltd.

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

#include "memory_location_item.h"
#include "ardour_ui.h"
#include "ardour/session.h"

MemoryLocationItem::MemoryLocationItem ()
    : Gtk::EventBox ()
{
}

MemoryLocationItem::~MemoryLocationItem ()
{    
}

std::size_t
MemoryLocationItem::get_order_number () const
{
    return _order_number;
}

bool
MemoryLocationItem::session_is_recording ()
{
    return ARDOUR_UI::instance ()->the_session ()->actively_recording ();
}

void
MemoryLocationItem::set_session_dirty ()
{
    ARDOUR_UI::instance ()->set_session_dirty ();
}
