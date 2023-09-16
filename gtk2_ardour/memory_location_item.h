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

#ifndef __memory_location_item__
#define __memory_location_item__

#include <cstddef>
#include <gtkmm/eventbox.h>

namespace PBD
{
    class ID;
}

class MemoryLocationItem : public Gtk::EventBox
{
public:
    MemoryLocationItem ();
    virtual ~MemoryLocationItem ();
    virtual void set_order_number (std::size_t num) = 0;
    std::size_t get_order_number () const;

    virtual void set_selected (bool yn) = 0;
    virtual bool get_selected () = 0;
    virtual void end_all_editing () = 0;
    virtual PBD::ID get_id () = 0;
    
    virtual void begin_name_edit () = 0;
protected:
    bool session_is_recording ();
    void set_session_dirty ();

    std::size_t _order_number;
private:

};

#endif /* __memory_location_item__ */
