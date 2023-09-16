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
#include "marker_template_item.h"
#include "marker.h"
#include "memory_location_item.h"
#include "gui_thread.h"
#include "selection.h"

#include "ardour/location.h"
#include "ardour/midi_scene_change.h"
#include "ardour/session.h"

#include "pbd/unwind.h"

#include "i18n.h"

const std::string LocationListDialog::xml_node_midi_marker_templates ("MidiMarkersTemplates");
const std::string LocationListDialog::xml_node_markers_order_list ("MarkersOrder"); ///< contains in xml_node_name

struct SignalOrderItemsSorter {
	bool operator () (MemoryLocationItem* a, MemoryLocationItem* b) {
		return a->get_order_number () < b->get_order_number ();
	}
};

void
LocationListDialog::add_new_id_to_markers_order_list (PBD::ID id)
{
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name), xml_node_markers_order_list);
    GUIObjectState::get_or_add_node (node_order_list, id.to_s ());
}

void
LocationListDialog::remove_id_from_markers_order_list (PBD::ID id)
{
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name), xml_node_markers_order_list);
    node_order_list->remove_nodes_and_delete ("id", id.to_s ());
}

void
LocationListDialog::replac_id_in_markers_order_list (PBD::ID old_id, PBD::ID new_id)
{
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (gui_object_state ().get_or_add_node (LocationListDialog::xml_node_name), xml_node_markers_order_list);
    XMLNode* node = GUIObjectState::get_or_add_node (node_order_list, old_id.to_s ());
    node->add_property ("id", new_id.to_s ());
}

void
LocationListDialog::add_midi_marker (Marker* marker)
{
    // Now create the MarkerListItem for newly added marker
    MarkerListItem* marker_list_item = new MarkerListItem (marker, this);
    _marker_locations.push_back (marker_list_item);
    
    if (!_marker_templates.size ()) {
        marker_list_item->set_order_number (_marker_locations.size () + _marker_templates.size () + 1);
    } else {
        apply_marker_template (marker_list_item);
    }
    marker_list_item->show ();
    add_new_id_to_markers_order_list (marker->location ()->id ());
    
    sync_order_keys ();
}

void
LocationListDialog::apply_marker_template (MarkerListItem* marker_list_item)
{
    Marker* marker = marker_list_item->get_marker ();
    boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (marker->location ()->scene_change ());
    
	if (msc) {
        for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin (); i != _marker_templates.end (); ++i) {
            if ( ((*i)->get_bank () == msc->bank ()) &&
                 ((*i)->get_program () == msc->program ()) &&
                 ((*i)->get_channel () == msc->channel ())) {
                
                marker->location ()->set_name ((*i)->get_name ());
                marker->set_color (Marker::XMLColor [(*i)->get_color_item ()]);
                marker_list_item->set_order_number ((*i)->get_order_number ());
                replac_id_in_markers_order_list ((*i)->get_id (), marker_list_item->location ()->id ());
                delete (*i);
                break;
            }
        }
	} else {
        if (_marker_templates.size ()) {
            std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin ();
            marker->location ()->set_scene_change (boost::shared_ptr<ARDOUR::MIDISceneChange> (new ARDOUR::MIDISceneChange ((*i)->get_channel (), (*i)->get_bank (), (*i)->get_program ())));
            marker->location ()->set_name ((*i)->get_name ());
            marker->set_color (Marker::XMLColor [(*i)->get_color_item ()]);
            marker_list_item->set_order_number ((*i)->get_order_number ());
            replac_id_in_markers_order_list ((*i)->get_id (), marker_list_item->location ()->id ());
            delete (*i);
        }
    }
}

void
LocationListDialog::sync_order_keys ()
{
    if ( (_session && _session->deletion_in_progress ()) || (_session == NULL) || _items_deletion_in_progress) {
		return;
	}
    
    // First detach all the prviously added markers from the ui tree.
    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
		_midi_marker_list_home.remove (*(*i)); // we suppose _midi_marker_list_home is the parent
	}
    
    for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin(); i != _marker_templates.end (); ++i) {
        _midi_marker_list_home.remove (*(*i));
    }
    
    std::list<MemoryLocationItem*> memory_location_items;
    memory_location_items.assign (_marker_locations.begin (), _marker_locations.end ());
    memory_location_items.insert (memory_location_items.end (), _marker_templates.begin (), _marker_templates.end ());
    
    SignalOrderItemsSorter sorter;
    memory_location_items.sort (sorter);
    
    std::size_t order_num = 0;
    for (std::list<MemoryLocationItem*>::iterator i = memory_location_items.begin (); i != memory_location_items.end (); ++i) {
        (*i)->set_order_number (++order_num);
        _midi_marker_list_home.pack_start (*(*i), false, false);
    }
}

void
LocationListDialog::remove_item (MarkerListItem* item)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}
    
    std::list<MarkerListItem*>::iterator i = std::find (_marker_locations.begin (), _marker_locations.end (), item);
    
    if (i != _marker_locations.end ()) {
        remove_id_from_markers_order_list ((*i)->location ()->id ());
        _midi_marker_list_home.remove (*(*i));
        _marker_locations.erase (i);
    }
    
    sync_order_keys ();
}

void
LocationListDialog::remove_marker_template_item (MarkerTemplateItem* item)
{
    if (_session && _session->deletion_in_progress()) {
		return;
	}
    
    std::list<MarkerTemplateItem*>::iterator i = std::find (_marker_templates.begin (), _marker_templates.end (), item);
    
    if (i != _marker_templates.end ()) {
        remove_id_from_markers_order_list ((*i)->get_id ());
        _midi_marker_list_home.remove (*(*i));
        _marker_templates.erase (i);
    }
    
    sync_order_keys ();
    ARDOUR_UI::instance()->set_session_dirty ();
}

void
LocationListDialog::on_add_midi_marker_template_button_clicked (WavesButton*)
{
    if (session_is_recording ()) {
        return;
    }
    
    end_all_editing ();
    
    // Now create the MarkerListItem for newly added marker
    MarkerTemplateItem* marker_template_item = create_marker_template ();
    add_new_id_to_markers_order_list (marker_template_item->get_id ());
    marker_template_item->set_order_number (_marker_locations.size () + _marker_templates.size ());
    marker_template_item->set_program (_marker_locations.size () + _marker_templates.size ());
    update_midi_marker_template_in_xml (marker_template_item);
    marker_template_item->begin_name_edit ();
}

void
LocationListDialog::update_midi_marker_template_in_xml (MarkerTemplateItem* marker_template_item)
{
    if (_marker_template_item_load_in_progress) {
        return;
    }
    
    XMLNode* root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_midi_marker_templates);
    XMLNode* current_location_root = GUIObjectState::get_or_add_node (root, marker_template_item->get_id ().to_s ());
    current_location_root->add_property ("name", marker_template_item->get_name ());
    current_location_root->add_property ("color", marker_template_item->get_color ());
    current_location_root->add_property ("bank", marker_template_item->get_bank ());
    current_location_root->add_property ("program", marker_template_item->get_program ());
    current_location_root->add_property ("channel", marker_template_item->get_channel ());
    
    ARDOUR_UI::instance()->set_session_dirty ();
}

MarkerTemplateItem*
LocationListDialog::create_marker_template ()
{
    MarkerTemplateItem* marker_template_item = new MarkerTemplateItem (this);
    _marker_templates.push_back (marker_template_item);
    marker_template_item->show ();
    _midi_marker_list_home.pack_start (*marker_template_item, false, false);
    
    return marker_template_item;
}

void
LocationListDialog::load_marker_template_list ()
{
    XMLNode* root = gui_object_state ().get_or_add_node (LocationListDialog::xml_node_midi_marker_templates);
    XMLNodeList const & children = root->children ();
    
    for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
        
        XMLProperty* p_id = (*i)->property("id");
        if (!p_id) {
            continue;
        }
        
        XMLProperty* p_name = (*i)->property ("name");
        if (!p_name) {
            continue;
        }
        
        XMLProperty* p_color = (*i)->property ("color");
        if (!p_color) {
            continue;
        }
        
        XMLProperty* p_program = (*i)->property ("program");
        if (!p_program) {
            continue;
        }
        
        XMLProperty* p_bank = (*i)->property ("bank");
        if (!p_bank) {
            continue;
        }
        
        XMLProperty* p_channel = (*i)->property ("channel");
        if (!p_channel) {
            continue;
        }
        
        {
            PBD::Unwinder<bool> prevent_update_midi_marker_template_in_xml (_marker_template_item_load_in_progress, true);
            MarkerTemplateItem* marker_template_item = create_marker_template ();
            
            marker_template_item->set_id (p_id->value ());
            marker_template_item->set_name_externally (p_name->value ().c_str ());
            marker_template_item->set_color (p_color->value ().c_str ());
            
            char * pEnd;
            marker_template_item->set_program (strtol (p_program->value ().c_str (), &pEnd, 10));
            marker_template_item->set_bank (strtol (p_bank->value ().c_str (), &pEnd, 10));
            marker_template_item->set_channel (strtol (p_channel->value ().c_str (), &pEnd, 10));
            
            marker_template_item->set_order_number (_marker_locations.size () + _marker_templates.size ());
        }
    }
    
    sort_marker_items_according_position ();
}

void
LocationListDialog::sort_marker_items_according_position ()
{
    Gtk::Box_Helpers::BoxList* childList = &_midi_marker_list_home.children ();
    childList->erase (childList->begin (), childList->end ());
    
    std::list<MemoryLocationItem*> memory_location_items;
    memory_location_items.assign (_marker_locations.begin (), _marker_locations.end ());
    memory_location_items.insert (memory_location_items.end (), _marker_templates.begin (), _marker_templates.end ());
    
    XMLNode* node_order_list = GUIObjectState::get_or_add_node (_root, xml_node_markers_order_list);
    XMLNodeList const & children = node_order_list->children ();
    std::size_t order_num = 1;
    
    for (XMLNodeList::const_iterator id_iterator = children.begin (); id_iterator != children.end (); ++id_iterator) {

        XMLProperty* p = (*id_iterator)->property ("id");
        std::string str_id = p->value ().c_str ();
        PBD::ID id = PBD::ID (str_id);

        for (std::list <MemoryLocationItem*>::iterator i = memory_location_items.begin (); i != memory_location_items.end (); ++i) {
            if ((*i)->get_id () == id) {
                _midi_marker_list_home.pack_start (*(*i), false, false);
                (*i)->set_order_number (order_num++);
                break;
            }
        }
    }
}

void
LocationListDialog::do_extend_midi_selection (std::size_t start_num, std::size_t end_num)
{
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();

    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin(); i != _marker_locations.end(); ++i) {
        
        std::size_t num = (*i)->get_order_number ();
        
        if ((start_num <= num) && (num <= end_num)) {
            Marker* marker = (*i)->get_marker ();
            selection.add (marker);
        }
    }
    
    for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin(); i != _marker_templates.end (); ++i) {
        
        std::size_t num = (*i)->get_order_number ();
        
        if ((start_num <= num) && (num <= end_num)) {
            (*i)->set_selected (true);
        }
    }
}

void
LocationListDialog::find_extend_range_and_extend (MemoryLocationItem* mli)
{
    std::size_t curr_selected = mli->get_order_number ();
    std::size_t prev_selected = 0;
    std::size_t next_selected = _marker_locations.size () + _marker_templates.size () + 1;
    
    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin(); i != _marker_locations.end(); ++i) {
		
        if ((*i)->get_marker ()->selected ()) {
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
    
    for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin(); i != _marker_templates.end (); ++i) {
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

    if (prev_selected != 0) {
        do_extend_midi_selection (prev_selected, curr_selected);
    } else if (next_selected != _marker_locations.size () + _marker_templates.size () + 1) {
        do_extend_midi_selection (curr_selected, next_selected);
    } else {
        do_extend_midi_selection (curr_selected, curr_selected);
    }
}

void
LocationListDialog::delete_all_selected_marker_items ()
{
    {
        Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();
        std::list<Marker*>::iterator i = selection.markers.begin ();
    
    
        PBD::Unwinder<bool> prevent_sync_order_keys (_items_deletion_in_progress, true);
        
        while (i != selection.markers.end ()) {
            
            if ((*i)->location()->is_mark () && (*i)->selected ()) {
                /* ~MarkerListItem invokes signal CatchDeletion which call funcation
                 LocationListDialog::remove_location_item () which erase element from _location_items.
                 So do not erase elements from _location_items here. */
                std::list<Marker*>::iterator j = i++;
                ARDOUR_UI::instance ()->the_editor ().externally_remove_marker ((*j)->location ());
                
                continue;
            }
            ++i;
        }

        std::list<MarkerTemplateItem*>::iterator it = _marker_templates.begin ();

        while (it != _marker_templates.end ()) {
            
            if ((*it)->get_selected ()) {
                /* ~MarkerTemplateItem invokes signal CatchDeletion which call funcation
                 LocationListDialog::remove_marker_template_item () which erase element from _marker_templates.
                 So do not erase elements from _marker_templates here. */
                std::list<MarkerTemplateItem*>::iterator j = it++;
                delete *j;
                
                continue;
            }
            ++it;
        }
    }
    
    sync_order_keys ();
}

void
LocationListDialog::clear_selection_for_marker_items ()
{
    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
        (*i)->set_selected (false);
    }
    
    for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin (); i != _marker_templates.end (); ++i) {
        (*i)->set_selected (false);
    }
}

/*
 Remove all Selection Items before close the session
 */
void
LocationListDialog::clear_gui_objects ()
{
    Gtk::Box_Helpers::BoxList* childList = &_midi_marker_list_home.children ();
    childList->erase (childList->begin (), childList->end ());
    
	_marker_templates.clear ();
    _root = NULL;
    
    gui_object_state ().remove_node (xml_node_midi_marker_templates);
}

void
LocationListDialog::begin_edit_next_marker_list_item (MemoryLocationItem* current_item)
{
    std::vector<Gtk::Widget*> children = _midi_marker_list_home.get_children();
    for (std::vector<Gtk::Widget*>::iterator it = children.begin (); it != children.end (); ++it) {
        if (*it == current_item) {
            if (++it != children.end ()) {
                MemoryLocationItem* item = dynamic_cast<MemoryLocationItem*> (*it);
                if (item) {
                    ensure_item_is_visible (item);
                    item->begin_name_edit ();
                }
            }
            break;
        }
    }
}

void
LocationListDialog::ensure_item_is_visible (const MemoryLocationItem* cur_item)
{
    Gtk::Adjustment* vertical_adjustment = _marker_scroll.get_vadjustment ();
    Gtk::Adjustment* using_adjustment;
    
    Gtk::Box* the_box = dynamic_cast <Gtk::Box*> (&_midi_marker_list_home);
    
    double current_view_min_pos, current_view_max_pos;
    double item_min_pos, item_max_pos;
    
    if (the_box) {
        const int ITEM_HEIGHT = cur_item->get_height ();
        
        current_view_min_pos = vertical_adjustment->get_value ();
        current_view_max_pos = current_view_min_pos + vertical_adjustment->get_page_size ();
        
        // Count number of items
        std::vector<Gtk::Widget*> items = _midi_marker_list_home.get_children();
        std::size_t number_of_items = -1;
        for (std::vector<Gtk::Widget*>::iterator it = items.begin (); it != items.end (); ++it) {
            if (*it == cur_item) {
                number_of_items = std::distance (items.begin (), it);
                break;
            }
        }
        
        item_min_pos = number_of_items * (ITEM_HEIGHT + the_box->get_spacing ());
        item_max_pos = item_min_pos + ITEM_HEIGHT;
        
        using_adjustment = vertical_adjustment;
    } else {
        return;
    }
    
    if ( item_min_pos >= current_view_min_pos &&
         item_max_pos < current_view_max_pos ) {
        // already visible
        return;
    }
    
    double new_value = 0.0;
    
    new_value = item_max_pos - using_adjustment->get_page_size ();

    using_adjustment->set_value (new_value);
}

bool
LocationListDialog::exists_selected_item_in_marker_list ()
{
    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
        if ((*i)->get_selected ()) {
            return true;
        }
    }
    
    for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin (); i != _marker_templates.end (); ++i) {
        if ((*i)->get_selected ()) {
            return true;
        }
    }
    
    return false;
}

bool
LocationListDialog::multiple_selection_in_marker_list ()
{
    std::size_t number_of_selected = 0;
    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
        if ((*i)->get_selected ()) {
            number_of_selected++;
            if (number_of_selected >= 2) {
                return true;
            }
        }
    }
    
    for (std::list<MarkerTemplateItem*>::iterator i = _marker_templates.begin (); i != _marker_templates.end (); ++i) {
        if ((*i)->get_selected ()) {
            number_of_selected++;
            if (number_of_selected >= 2) {
                return true;
            }
        }
    }
    
    return false;
}
