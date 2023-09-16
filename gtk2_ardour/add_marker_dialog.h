/*
 Copyright (C) 2016 Waves Audio Ltd.
 
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

#ifndef tracks_add_marker_dialog_h
#define tracks_add_marker_dialog_h

#include "waves_dialog.h"
#include <string.h>

class AddMarkerDialog : public WavesDialog {
public:
    
    AddMarkerDialog (std::string& marker_name);
    void on_show ();
    std::string get_marker_name () const { return _marker_name_entry.get_text(); }
    
private:
    WavesButton& _cancel_button;
    WavesButton& _ok_button;
    
    Gtk::Entry& _marker_name_entry;
    
    void on_cancel_button (WavesButton*);
    void on_ok_button (WavesButton*);
};

#endif
