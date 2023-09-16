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
#include "track_view_list.h"

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

PBD::Signal1<void,LocationListSelection*> LocationListSelection::CatchDeletion;
PBD::Signal1<void,LocationListSelection*> LocationListSelection::NameChanged;

LocationListSelection::LocationListSelection (LocationListDialog* lld, ARDOUR::framepos_t start, ARDOUR::framepos_t end, std::string name)
    : LocationListItem (lld)
{
    _name = name;
    _start = start;
    _end = end;
    init ();
}

LocationListSelection::~LocationListSelection ()
{
    XMLNode* root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name);
    root->remove_nodes_and_delete ("id", string_compose ("%1", _id));
    CatchDeletion (this);
}

void
LocationListSelection::init ()
{
    _type_label.set_text ("Selection");
    
    LocationListItem::set_name (_name);
    _start_clock.set (_start, true);
    _start_clock_button.set_text (_start_clock.get_text ());
    _end_clock.set (_end, true);
    _end_clock_button.set_text (_end_clock.get_text ());
}

void
LocationListSelection::popup_display_menu (guint32 when)
{
    build_display_menu ();
    _display_menu->popup (1, when);
}

/*
 Allow to show update "Current selection menu item" in context selection menu on canvas
 if start, end or incoming routes of this selection were changed
 */
bool
LocationListSelection::allow_to_show_update_current_selection_menu_on_canvas ()
{
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();
    
    // compare start and end position
    if ((selection.time.start () == _start) && (selection.time.end_frame () == _end)) {

        std::list<std::string> selection_ids;
        for (TrackViewList::iterator i = selection.time.tracks_in_range.begin (); i != selection.time.tracks_in_range.end (); ++i) {
            RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
            
            if (rtav) {
                selection_ids.push_back (rtav->route_state_id ());
            }
        }

        std::list<std::string> item_ids = get_routes_id ();

        // compare route lists
        if (selection_ids.size () != item_ids.size ()) {
            return true;
        }
        
        selection_ids.sort ();
        item_ids.sort ();
        
        std::list<std::string>::iterator s = selection_ids.begin ();
        std::list<std::string>::iterator i = item_ids.begin ();
        
        while ((s != selection_ids.end ()) && ((*s++) == (*i++))) {
        }
        
        if (s == selection_ids.end ()) { ///< if this expression equals TRUE then (i == items_end ()) also TRUE;
            return false; // do not allow to show "Update current selection" on canvas
        }
    }
    
    return true;
}

bool
LocationListSelection::update_current_selection_is_allowed ()
{
    return allow_to_show_update_current_selection_menu_on_canvas ();
}

void
LocationListSelection::build_display_menu ()
{
    delete _display_menu;
	_display_menu = new Gtk::Menu;
	_display_menu->set_name ("ArdourContextMenu");
    
    Gtk::Menu_Helpers::MenuList& items = _display_menu->items ();
    
    if (!_location_list_dialog->multiple_selection_in_location_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Rename"), sigc::mem_fun (*this, &LocationListSelection::begin_name_edit)));

		if (update_current_selection_is_allowed ()) {
			items.push_back (Gtk::Menu_Helpers::MenuElem (("Update"), sigc::mem_fun (*this, &LocationListSelection::update_current_selection_time_and_route_ids)));
		}
    }

    if (_location_list_dialog->exists_selected_item_in_location_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Delete"), sigc::mem_fun (*this, &LocationListSelection::delete_selected_ranges)));
    }
}

void
LocationListSelection::delete_selected_ranges ()
{
    DeleteAllSelectedRanges (this);
}

void
LocationListSelection::update_time_in_gui_object ()
{
    XMLNode* root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name);
    XMLNode* current_location_root = gui_object_state ().get_or_add_node (root, get_id ().to_s ());
    char buf[64];
    snprintf (buf, sizeof (buf), "%" PRId64, _start);
	current_location_root->add_property ("start", buf);
    snprintf (buf, sizeof (buf), "%" PRId64, _end);
	current_location_root->add_property ("end", buf);
    
    set_session_dirty ();
}

void
LocationListSelection::update_current_selection_time_and_route_ids ()
{
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();

    _start = selection.time.start ();
    _start_clock.set (_start, true);
    _start_clock_button.set_text (_start_clock.get_text ());

    _end = selection.time.end_frame ();
    _end_clock.set (_end, true);
    _end_clock_button.set_text (_end_clock.get_text ());
    
    update_time_in_gui_object ();
    
    XMLNode* root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name);
    XMLNode* current_location_root = GUIObjectState::get_or_add_node (root, get_id ().to_s ());
    
    for (TrackViewList::iterator i = selection.time.tracks_in_range.begin (); i != selection.time.tracks_in_range.end (); ++i) {
        RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
        
        if (rtav) {
            GUIObjectState::get_or_add_node (current_location_root, rtav->route_state_id ());
        }
    }
}

void
LocationListSelection::start_clock_value_changed ()
{
    if (_start_clock.current_time () < _end) {
        _start = _start_clock.current_time ();
    }
    
    _start_clock.set (_start, true);
    _start_clock_button.set_text (_start_clock.get_text ());
    
    if (get_selected ()) {
        ARDOUR_UI::instance ()->the_editor ().restore_selection (this);
    }
    update_time_in_gui_object ();
}

void
LocationListSelection::end_clock_value_changed ()
{
    if (_end_clock.current_time () > _start) {
        _end = _end_clock.current_time ();
    }
    
    _end_clock.set (_end, true);
    _end_clock_button.set_text (_end_clock.get_text ());
    
    if (get_selected ()) {
        ARDOUR_UI::instance ()->the_editor ().restore_selection (this);
    }
    update_time_in_gui_object ();
}

bool
LocationListSelection::get_selected ()
{
    return get_state () == Gtk::STATE_SELECTED;
}

void
LocationListSelection::set_selected (bool yn)
{
    if (yn) {
        set_state (Gtk::STATE_SELECTED);
        ARDOUR_UI::instance ()->the_editor ().restore_selection (this);
    } else {
        set_state (Gtk::STATE_NORMAL);
    }
}

GUIObjectState&
LocationListSelection::gui_object_state ()
{
	return *ARDOUR_UI::instance ()->gui_object_state;
}

std::list<std::string>
LocationListSelection::get_routes_id ()
{
    XMLNode* root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name);
    XMLNode* location_root = GUIObjectState::get_or_add_node (root, _id.to_s ());
        
    std::list<std::string> id_s;
    
    XMLNodeList const & children = location_root->children ();
    for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
        XMLProperty* p = (*i)->property ("id");
        if (p) {
            id_s.push_back (p->value ());
        }
    }
    
    return id_s;
}

void
LocationListSelection::name_entry_changed ()
{
    std::string x = _name_entry.get_text ();
    
	if (x == _name) {
		return;
	}
    
    PBD::strip_whitespace_edges (x);
    
	if (x.length () == 0) {
		_name_entry.set_text (_name);
		return;
	}
    
    _name = x;
}

void
LocationListSelection::begin_name_edit ()
{
    if (session_is_recording ()) {
        return;
    }
    
    _name_entry.set_text (_name);
    _name_button.hide ();
    _name_entry_eventbox.show ();
    _name_entry.show ();
    
    _name_entry.select_region (0, -1);
    _name_entry.set_state (Gtk::STATE_NORMAL);
    _name_entry.grab_focus ();
    _name_entry.start_editing (0);
}

void
LocationListSelection::set_name (std::string name)
{
    LocationListItem::set_name (name);
    NameChanged (this);
}
