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

#include "location_list_item.h"
#include "location_list_dialog.h"

#include "ardour_ui.h"
#include "editor.h"
#include "public_editor.h"
#include "main_clock.h"
#include "marker.h"
#include "selection.h"
#include "waves_ui.h"

#include "ardour/location.h"
#include "ardour/rc_configuration.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>
#include <gtkmm/menu_elems.h>

#include "gtkmm2ext/keyboard.h"

#include "pbd/compose.h"
#include "pbd/whitespace.h"
#include "gui_thread.h"

#include "i18n.h"

PBD::Signal1<void,LocationListSkipMarker*> LocationListSkipMarker::CatchDeletion;

LocationListSkipMarker::LocationListSkipMarker (LocationListDialog* lld, RangeMarker* skip_marker)
    : LocationListItem (lld)
{
    _skip_marker = skip_marker;
    if (_skip_marker) {
        Marker::CatchDeletion.connect (*this, MISSING_INVALIDATOR, boost::bind (&LocationListSkipMarker::self_delete, this, _1), gui_context ());
        _skip_marker->SelectedChanged.connect (*this, invalidator (*this), boost::bind (&LocationListSkipMarker::marker_selected_changed, this), gui_context ());
    }
    set_location (_skip_marker->location ());
    _type_label.set_text ("Skip");
    _on_off_button.signal_clicked.connect (sigc::mem_fun (*this, &LocationListSkipMarker::on_off_button_clicked));
    _on_off_button.show ();
    
    ARDOUR::Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&LocationListSkipMarker::parameter_changed, this, _1), gui_context());
    
    build_display_menu ();
}

LocationListSkipMarker::~LocationListSkipMarker ()
{
    CatchDeletion (this);
}

void
LocationListSkipMarker::parameter_changed (std::string p)
{
    if (p == "skip-playback") {
        _on_off_button.set_active (ARDOUR::Config->get_skip_playback () && _location->is_skipping ());
    }
}

void
LocationListSkipMarker::popup_display_menu (guint32 when)
{
    build_display_menu ();
    _display_menu->popup (1, when);
}

void
LocationListSkipMarker::marker_selected_changed ()
{
    set_state (_skip_marker->selected () ? Gtk::STATE_SELECTED : Gtk::STATE_NORMAL);
}

void
LocationListSkipMarker::build_display_menu ()
{
	_display_menu = new Gtk::Menu;
	_display_menu->set_name ("ArdourContextMenu");
    
    Gtk::Menu_Helpers::MenuList& items = _display_menu->items ();
    if (!_location_list_dialog->multiple_selection_in_location_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Rename"), sigc::mem_fun (*this, &LocationListSkipMarker::begin_name_edit)));
    }
    
    if (_location_list_dialog->exists_selected_item_in_location_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Delete"), sigc::mem_fun (*this, &LocationListSkipMarker::delete_selected_ranges)));
    }
}

void
LocationListSkipMarker::set_selected (bool yn)
{
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();
    
    if (yn) {
        _skip_marker->set_selected (true);
        selection.add (_skip_marker);
    } else {
        _skip_marker->set_selected (false);
        selection.remove (_skip_marker);
    }
}

void
LocationListSkipMarker::delete_selected_ranges ()
{
    DeleteAllSelectedRanges (this);
}

void
LocationListSkipMarker::start_clock_value_changed ()
{
    _location->set_start (_start_clock.current_time ());
    _start_clock.set (_location->start (), true);
    _start_clock_button.set_text (_start_clock.get_text ());
}

void
LocationListSkipMarker::end_clock_value_changed ()
{
    _location->set_end (_end_clock.current_time ());
    _end_clock.set (_location->end (), true);
    _end_clock_button.set_text (_end_clock.get_text ());
}

void
LocationListSkipMarker::on_off_button_clicked (WavesButton*)
{
    _location->set_skipping (!_location->is_skipping ());
    if (!ARDOUR::Config->get_skip_playback ()) {
        ARDOUR::Config->set_skip_playback (true);
    }
}

void
LocationListSkipMarker::self_delete (Marker* marker)
{
    if (marker && (marker == _skip_marker)) {
        delete this;
    }
}

void
LocationListSkipMarker::display_item_data ()
{
    set_name (_location->name ());
    _start_clock.set (_location->start (), true);
    _start_clock_button.set_text (_start_clock.get_text ());
    _end_clock.set (_location->end (), true);
    _end_clock_button.set_text (_end_clock.get_text ());
    _on_off_button.set_active (ARDOUR::Config->get_skip_playback () && _location->is_skipping ());
}

PBD::ID
LocationListSkipMarker::get_id ()
{
    return _skip_marker->location ()->id ();
}

bool
LocationListSkipMarker::get_selected ()
{
    return _skip_marker->selected ();
}

void
LocationListSkipMarker::set_location (ARDOUR::Location* location)
{
    _location_connections.drop_connections ();
    
    _location = location;
    if (_location) {
        _location->NameChanged.connect (_location_connections, invalidator (*this), boost::bind (&LocationListSkipMarker::display_item_data, this), gui_context());
        _location->StartChanged.connect (_location_connections, invalidator (*this), boost::bind (&LocationListSkipMarker::display_item_data, this), gui_context());
        _location->EndChanged.connect (_location_connections, invalidator (*this), boost::bind (&LocationListSkipMarker::display_item_data, this), gui_context());
        _location->FlagsChanged.connect (_location_connections, invalidator (*this), boost::bind (&LocationListSkipMarker::display_item_data, this), gui_context());
        display_item_data ();
    }
}

void
LocationListSkipMarker::name_entry_changed ()
{
    if (!_location) {
        return;
    }
    
    std::string x = _name_entry.get_text ();
    
	if (x == _location->name() ) {
		return;
	}
    
    PBD::strip_whitespace_edges (x);
    
	if (x.length () == 0) {
		_name_entry.set_text (_location->name ());
		return;
	}
    
    _location->set_name (x);
}

std::string
LocationListSkipMarker::get_name ()
{
    return _skip_marker->location ()->name ();
}

void
LocationListSkipMarker::begin_name_edit ()
{
    if (!_location || session_is_recording ()) {
        return;
    }
    
    _name_entry.set_text (_location->name ());
    _name_button.hide ();
    _name_entry_eventbox.show ();
    _name_entry.show ();
    
    _name_entry.select_region (0, -1);
    _name_entry.set_state (Gtk::STATE_NORMAL);
    _name_entry.grab_focus ();
    _name_entry.start_editing (0);
}
