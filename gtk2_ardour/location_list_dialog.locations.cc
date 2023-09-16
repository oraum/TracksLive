/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "ardour_ui.h"
#include "editor.h"
#include "location_list_item.h"
#include "location_list_dialog.h"
#include "marker_list_item.h"
#include "marker.h"
#include "gui_thread.h"
#include "gui_object.h"
#include "enums.h"
#include "selection.h"
#include "time_axis_view.h"
#include "time_selection.h"

#include "ardour/session.h"
#include "ardour/location.h"

#include "pbd/unwind.h"

#include "i18n.h"

const std::string LocationListDialog::xml_node_name ("LocationListSelections");
const std::string LocationListDialog::xml_node_locations_order_list ("LocationsOrder"); ///< contains in xml_node_name

void
LocationListDialog::update_order ()
{
    if (_items_deletion_in_progress) {
        return;
    }
    
    std::size_t order_number = 1;
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
        (*i)->set_order_number (order_number++);
    }
}

void
LocationListDialog::add_new_id_to_locatoins_order_list (PBD::ID id)
{
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name), xml_node_locations_order_list);
    GUIObjectState::get_or_add_node (node_order_list, id.to_s ());
}

void
LocationListDialog::remove_id_from_locatoins_order_list (PBD::ID id)
{
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name), xml_node_locations_order_list);
    node_order_list->remove_nodes_and_delete ("id", id.to_s ());
}

void
LocationListDialog::remove_location_item (LocationListItem* item)
{
    if (_session && _session->deletion_in_progress ()) {
		return;
	}

    remove_id_from_locatoins_order_list (item->get_id ());
    std::list <LocationListItem*>::iterator i = std::find (_location_items.begin (), _location_items.end (), item);
    
    if (i != _location_items.end ()) {
        _location_list_home.remove (*(*i));
        _location_items.erase (i);
    }
    
    update_order ();
}

void
LocationListDialog::add_skip_marker (RangeMarker* skip_marker)
{
    // Now create the LocationListItem for newly added marker
    LocationListSkipMarker* location_list_skip_marker = new LocationListSkipMarker (this, skip_marker);
    _location_items.push_back (location_list_skip_marker);
    _location_list_home.pack_start (*location_list_skip_marker, false, false);
    
    location_list_skip_marker->set_order_number (_location_items.size ());
    location_list_skip_marker->show ();
    
    add_new_id_to_locatoins_order_list (skip_marker->location ()->id ());
}

void
LocationListDialog::add_selection (Selection& selection)
{
    LocationListSelection* location_list_selection = create_selection (selection.time.start (), selection.time.end_frame (), next_available_selection_name ());
    
    XMLNode* current_location_root = GUIObjectState::get_or_add_node (_root, location_list_selection->get_id ().to_s ());
    
    char buf[64];
    snprintf (buf, sizeof (buf), "%" PRId64, location_list_selection->start ());
	current_location_root->add_property ("start", buf);
    snprintf (buf, sizeof (buf), "%" PRId64, location_list_selection->end ());
	current_location_root->add_property ("end", buf);
    current_location_root->add_property ("name", location_list_selection->get_name ());
    
    add_new_id_to_locatoins_order_list (location_list_selection->get_id ());
    selection.time.set_location_list_selection (location_list_selection);
    
    for (TrackViewList::iterator i  = selection.time.tracks_in_range.begin (); i != selection.time.tracks_in_range.end (); ++i) {
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
        
		if (rtav) {
            GUIObjectState::get_or_add_node (current_location_root, rtav->route_state_id ());
		}
	}
    
    _session->set_dirty ();
}

void
LocationListDialog::on_selection_name_changed (LocationListSelection* location_list_selection)
{
    XMLNode* current_location_root = GUIObjectState::get_or_add_node (_root, location_list_selection->get_id ().to_s ());
    current_location_root->add_property ("name", location_list_selection->get_name ());
    
    _session->set_dirty ();
}

void
LocationListDialog::timeaxisview_deleted (TimeAxisView *tv)
{
    if (_session && _session->deletion_in_progress ()) {
        return;
	}
    
	ENSURE_GUI_THREAD (*this, &LocationListDialog::timeaxisview_deleted, tv);
    
	RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (tv);
    
    XMLNodeList const & children = _root->children ();
    bool some_item_was_removed = false;
    
    XMLNodeList::const_iterator selections_iterator = children.begin ();
    
    {
        PBD::Unwinder<bool> prevent_sync_order_keys (_items_deletion_in_progress, true);
        
        while (selections_iterator != children.end ()) {
            
            if (children.empty ()) {
                break;
            }

            XMLNodeList::const_iterator temp_iterator = selections_iterator;
            temp_iterator++;
            (*selections_iterator)->remove_nodes_and_delete ("id", rtav->route_state_id ());
            
            if ( (*selections_iterator)->children ().empty ()) {
                
                XMLProperty* p = (*selections_iterator)->property ("id");
                std::string str_id = p->value ().c_str ();
                remove_id_from_locatoins_order_list (str_id);
                _root->remove_nodes_and_delete ("id", p->value ().c_str ());
                
                // after REMOVE from list selections_iterator becomes invalid
                some_item_was_removed = true;
                
                for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
                    LocationListSelection* lls = dynamic_cast<LocationListSelection*>(*i);
                    if (lls && (lls->get_id () == PBD::ID (str_id))) {
                        delete *i; /* ~LocationListSelection invokes signal CatchDeletion which call funcation
                                      LocationListDialog::remove_location_item () which erase element from _location_items. 
                                      So do not erase elements from _location_items here. */
                        break;
                    }
                }
                
                selections_iterator = temp_iterator;
            } else {
                ++selections_iterator;
            }
        }
    }
    
    if (some_item_was_removed) {
        update_order ();
    }
}

void
LocationListDialog::load_selection_list ()
{
    _root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name);
    XMLNodeList const & children = _root->children ();
    
    for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
        XMLProperty* p = (*i)->property ("start");
        ARDOUR::framepos_t start;
        if (p) {
            sscanf (p->value().c_str(), "%" PRId64, &start);
        } else {
            continue;
        }
        
        p = (*i)->property ("end");
        ARDOUR::framepos_t end;
        if (p) {
            sscanf (p->value().c_str(), "%" PRId64, &end);
        } else {
            continue;
        }
        
        std::string id;
        p = (*i)->property("id");
        if (p) {
            id = p->value ().c_str ();
        } else {
            continue;
        }
        
        std::string name;
        p = (*i)->property("name");
        if (p) {
            name = p->value ().c_str ();
        } else {
            continue;
        }
        
        LocationListSelection* lls = create_selection (start, end, name);
        lls->set_id (id); // set id
    }
    
    sort_item_according_position ();
}

/*
 Sort items according their position in session before it was closed.
 */
void
LocationListDialog::sort_item_according_position ()
{
    Gtk::Box_Helpers::BoxList* childList = &_location_list_home.children ();
    childList->erase (childList->begin (), childList->end ());

    std::list <LocationListItem*> temp_location_items;
    
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (_root, xml_node_locations_order_list);
    XMLNodeList const & children = node_order_list->children ();
    std::size_t order_num = 1;
    
    for (XMLNodeList::const_iterator id_iterator = children.begin (); id_iterator != children.end (); ++id_iterator) {
        
        XMLProperty* p = (*id_iterator)->property ("id");
        std::string str_id = p->value ().c_str ();
        PBD::ID id = PBD::ID (str_id);
        
        for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
            if ((*i)->get_id () == id) {
                _location_list_home.pack_start (*(*i), false, false);
                (*i)->set_order_number (order_num++);
                temp_location_items.push_back ((*i));
                break;
            }
        }
    }
    
    std::swap (temp_location_items, _location_items);
    update_order ();
}

LocationListSelection*
LocationListDialog::create_selection (ARDOUR::framepos_t start, ARDOUR::framepos_t end, std::string name)
{
    // Now create the LocationListItem for selection
    LocationListSelection* location_list_selection = new LocationListSelection (this, start, end, name);
    _location_items.push_back (location_list_selection);
    
    _location_list_home.pack_start (*location_list_selection, false, false);
    location_list_selection->set_order_number (_location_items.size ());
    location_list_selection->show ();
    
    return location_list_selection;
}

std::string
LocationListDialog::next_available_selection_name ()
{
    std::string next_name;
    
    for (std::size_t n = 1; ; ++n) {
        
        next_name = string_compose ("Selection %1", n);
        
        std::list <LocationListItem*>::iterator i;
        for (i = _location_items.begin (); i != _location_items.end (); ++i) {
            
            if ( (*i)->get_name () == next_name) {
                break;
            }
        }
        
        if (i == _location_items.end ()) {
            break;
        }
    }
    
    return next_name;
}

GUIObjectState&
LocationListDialog::gui_object_state ()
{
	return *ARDOUR_UI::instance ()->gui_object_state;
}

void
LocationListDialog::clear_selection_for_ranges (LocationListItem*)
{
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
        (*i)->set_selected (false);
    }
}

void
LocationListDialog::delete_selected_ranges (LocationListItem*)
{
    std::list <LocationListItem*>::iterator i = _location_items.begin ();
    
    {
        PBD::Unwinder<bool> prevent_sync_order_keys (_items_deletion_in_progress, true);
    
        while (i != _location_items.end ()) {
            if ((*i)->get_selected ()) {
                std::list <LocationListItem*>::iterator j = i++;
                /* ~LocationListSelection and LocationListSkipMarker invokes signal CatchDeletion which call funcation
                    LocationListDialog::remove_location_item () which erase element from _location_items.
                    So do not erase elements from _location_items here. */
                LocationListSkipMarker* skip = dynamic_cast<LocationListSkipMarker*> (*j);
                if (skip) {
                    ARDOUR_UI::instance ()->the_editor ().externally_remove_marker (skip->get_skip_marker ()->location ());
                } else {
                    delete *j;
                }
                
                continue;
            }
            ++i;
        }
    }
    
    update_order ();
}

bool
LocationListDialog::multiple_selection_in_location_list ()
{
    std::size_t number_of_selected = 0;
    
    for (std::list <LocationListItem*>::const_iterator i = _location_items.begin (); i != _location_items.end () && number_of_selected < 2; ++i) {
        if ((*i)->get_selected ()) {
            number_of_selected++;
            if (number_of_selected >= 2) {
                break;
            }
        }
    }
    
    return number_of_selected >= 2;
}

void
LocationListDialog::do_extend_selection_for_ranges (std::size_t start_num, std::size_t end_num, bool select_first_skip)
{
    for (std::list <LocationListItem*>::iterator i = _location_items.end (); i != _location_items.begin (); --i) {
        
        std::size_t num = (*i)->get_order_number ();
        
        if ((start_num <= num) && (num <= end_num)) {
            (*i)->set_selected (true);
        }
    }
    
    LocationListItem* lls = _location_items.front ();
    if ((start_num <= lls->get_order_number ()) && (lls->get_order_number () <= end_num)) {
        lls->set_selected (true);
    }
}

void
LocationListDialog::extend_selection_for_ranges (LocationListItem* lli)
{
    std::size_t curr_selected = lli->get_order_number ();
    std::size_t prev_selected = 0;
    std::size_t next_selected = _location_items.size () + 1;
    
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
		
        if ((*i)->get_selected ()) {
            std::size_t num = (*i)->get_order_number ();
            
            // Find previous selected
            if (num < curr_selected && num > prev_selected) {
                prev_selected = num;
                continue;
            }
            
            // Find next selected
            if (num > curr_selected && num < next_selected) {
                next_selected = num;
            }
        }
    }
    
    if ((prev_selected == 0) && (next_selected == _location_items.size () + 1)) {
        lli->set_selected (true);
    } else if (prev_selected > 0) {
        do_extend_selection_for_ranges (prev_selected, curr_selected, false);
    } else {
        do_extend_selection_for_ranges (curr_selected, next_selected, true);
    }
}

bool
LocationListDialog::exists_selected_item_in_location_list ()
{
    for (std::list<LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
        if ((*i)->get_selected ()) {
            return true;
        }
    }
    
    return false;
}

