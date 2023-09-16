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

#ifndef __location_list_dialog_h__
#define __location_list_dialog_h__

#include "waves_dialog.h"
#include "ardour_button.h"
#include "audio_clock.h"

#include "pbd/xml++.h"

class GUIObjectState;
class LocationListItem;
class LocationListSelection;
class Marker;
class MarkerTemplateItem;
class MarkerListItem;
class MemoryLocationItem;
class RangeMarker;
class Selection;
class TimeAxisView;

namespace ARDOUR {
    class Location;
    class Session;
}

class LocationListDialog : public Gtk::Window, public WavesUI, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
public:
    LocationListDialog ();
    ~LocationListDialog ();
    
    void add_midi_marker (Marker* marker);
    void set_clock_mode (AudioClock::Mode mode);
    
    void add_skip_marker (RangeMarker* skip_marker);
    void add_selection (Selection& selection);
    void load_gui_objects ();
    void clear_gui_objects ();
    void end_all_editing ();
    
    bool multiple_selection_in_location_list ();
    bool multiple_selection_in_marker_list ();
    bool exists_selected_item_in_location_list ();
    bool exists_selected_item_in_marker_list ();
    
    static const std::string xml_node_name;
    static const std::string xml_node_midi_marker_templates;
    
protected:
    void on_realize ();
    void on_hide ();
    
private:    
    std::list<MarkerListItem*> _marker_locations;
    std::list<MarkerTemplateItem*> _marker_templates;
    std::list<LocationListItem*> _location_items;

    Gtk::Container& _tabs_home;
    Gtk::Box& _midi_marker_list_home;
    Gtk::Box& _location_list_home;
    WavesButton& _midi_marker_list_tab_button;
    WavesButton& _location_marker_list_tab_button;
    Gtk::EventBox& _marker_list_event_box;
    Gtk::EventBox& _location_list_event_box;
    WavesButton& _add_midi_marker_template;
    Gtk::ScrolledWindow& _marker_scroll;
    
    void load_selection_list ();
    void load_marker_template_list ();

    void delete_selected_ranges (LocationListItem*);
    void clear_selection_for_ranges (LocationListItem*);
    void extend_selection_for_ranges (LocationListItem*);
    void do_extend_selection_for_ranges (std::size_t start_num, std::size_t end_num, bool select_first_skip);

    void delete_all_selected_marker_items ();
    void clear_selection_for_marker_items ();
    void do_extend_midi_selection (std::size_t start_num, std::size_t end_num);
    void find_extend_range_and_extend (MemoryLocationItem*);
    void apply_marker_template (MarkerListItem*);
    
    void show_tab (Gtk::Widget&);
    void hide_tab (Gtk::Widget&);
    
    void on_midi_marker_list_tab_button_clicked (WavesButton*);
    void on_location_marker_list_tab_button_clicked (WavesButton*);
    bool marker_list_item_button_pressed (GdkEventButton* ev, MarkerListItem* strip);
    bool location_list_item_button_pressed (GdkEventButton* ev, LocationListItem* strip);
    
    gboolean on_button_pressed (GdkEventButton* ev);
    void sync_order_keys ();
    void on_add_midi_marker_template_button_clicked (WavesButton*);
    void update_midi_marker_template_in_xml (MarkerTemplateItem* marker_template_item);
    void remove_item (MarkerListItem* item);
    void remove_marker_template_item (MarkerTemplateItem*);
    void remove_location_item (LocationListItem* item);
    void update_order ();
    void sort_item_according_position ();
    void sort_marker_items_according_position ();
    void add_new_id_to_locatoins_order_list (PBD::ID id);
    void remove_id_from_locatoins_order_list (PBD::ID id);
    void add_new_id_to_markers_order_list (PBD::ID id);
    void remove_id_from_markers_order_list (PBD::ID id);
    void replac_id_in_markers_order_list (PBD::ID old_id, PBD::ID new_id);
    void on_selection_name_changed (LocationListSelection*);
    void timeaxisview_deleted (TimeAxisView *tv);
    std::string next_available_selection_name ();
    
    void begin_edit_next_marker_list_item (MemoryLocationItem*);
    void ensure_item_is_visible (const MemoryLocationItem* cur_item);
    
    LocationListSelection* create_selection (ARDOUR::framepos_t start, ARDOUR::framepos_t end, std::string name);
    MarkerTemplateItem* create_marker_template ();
    
    bool _items_deletion_in_progress;
    bool _marker_template_item_load_in_progress;
    
    XMLNode* _root;
    GUIObjectState& gui_object_state ();
    
    bool session_is_recording ();
    static const std::string xml_node_locations_order_list;
    static const std::string xml_node_markers_order_list;
};

#endif /* __location_list_dialog_h__ */
