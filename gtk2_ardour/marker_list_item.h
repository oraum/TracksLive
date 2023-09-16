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

#ifndef __marker_list_item__
#define __marker_list_item__

#include "location_list_dialog.h"
#include "waves_ui.h"
#include "memory_location_item.h"
#include "pbd/signals.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "main_clock.h"

namespace ARDOUR {
    class Location;
}

class Marker;

namespace Gtk {
	class Menu;
}

class MarkerListItem : public MemoryLocationItem, public WavesUI, public PBD::ScopedConnectionList
{
public:
	MarkerListItem (Marker* marker, LocationListDialog* dialog);
	~MarkerListItem ();
    
    virtual void set_order_number (std::size_t num);
    virtual void set_selected (bool yn);
    virtual bool get_selected ();
    virtual void end_all_editing ();
    virtual PBD::ID get_id ();
    
    static PBD::Signal0<void> DeleteAllSelectedMarkerItems;
    static PBD::Signal0<void> ClearSelection;
    static PBD::Signal1<void, MemoryLocationItem*> ExtendItemSelection;
    static PBD::Signal1<void, MarkerListItem*> CatchDeletion;
    static PBD::Signal1<void, MemoryLocationItem*> EditNext;
    ARDOUR::Location* location () { return _location; }
    Marker* get_marker () const { return _marker; }
    void set_clock_mode (AudioClock::Mode mode);
    void display_marker_data ();
    
    virtual void begin_name_edit ();
protected:
    void self_delete (Marker*);
private:
    ARDOUR::Location* _location;
    Marker* _marker;
    LocationListDialog* _location_list_dialog;
    MainClock _clock;
    WavesButton& _clock_button;
    
    Gtk::Container& _controls_event_box;
    Gtk::Label& _order_number_label;
    WavesButton& _go_button;
    WavesButton& _name_button;
    Gtk::Entry& _name_entry;
    Gtk::EventBox& _name_entry_eventbox;
    WavesButton&  _lock_button;
    WavesButton&  _stop_button;
    WavesDropdown& _color_dropdown;
    WavesButton&   _program_change_on_off_button;
    Gtk::Container& _program_change_info_panel;
    WavesDropdown& _bank_dropdown;
	WavesDropdown& _program_dropdown;
	WavesDropdown& _channel_dropdown;
    
    Gtk::Menu* _display_menu;
    
    bool marker_list_item_button_press (GdkEventButton*);
    void popup_display_menu (guint32 when);
    void build_display_menu ();
    bool show_only_delete_menu_item ();
    
    void set_name (std::string name);
    void set_location (ARDOUR::Location* location);
    void update_name (ARDOUR::Location* location);
    void end_name_edit (int response);
    
    bool name_entry_key_release (GdkEventKey *ev);
	bool name_entry_key_press (GdkEventKey *ev);
 	bool name_entry_focus_out (GdkEventFocus *ev);
    void name_entry_changed ();
    
    void on_name_button_double_clicked (WavesButton* button);
    
    void enable_program_change (bool yn);
    void program_changed ();
    void display_scene_change_info ();
    void on_set_scene_change_changed ();
    void set_color_dropdown_from_marker ();
    void marker_selected_changed ();
    void delete_selected_items_permanently ();
    
    void lock_button_clicked (WavesButton *button);
    void stop_button_clicked (WavesButton *button);
    void program_change_on_off_button_clicked (WavesButton *button);
    void go_button_clicked (WavesButton *button);
    void on_color_dropdown_item_changed (WavesDropdown*, int);
	void on_bank_dropdown_item_changed (WavesDropdown*, int);
	void on_program_dropdown_item_changed (WavesDropdown*, int);
	void on_channel_dropdown_item_changed (WavesDropdown*, int);
    void clock_value_changed ();
    
    void on_clock_button_double_clicked (WavesButton*);
    bool clock_focus_out (GdkEventFocus *ev);
    
    void marker_gui_changed (std::string, void*);
    
    PBD::ScopedConnectionList _location_connections;
    PBD::ScopedConnectionList _scene_change_connection;
};

#endif /* __marker_list_item__ */
