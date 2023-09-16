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

#ifndef __location_list_item__
#define __location_list_item__

#include "main_clock.h"
#include "memory_location_item.h"
#include "selectable.h"
#include "waves_ui.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "pbd/id.h"
#include "pbd/signals.h"

namespace ARDOUR {
    class Location;
}

namespace Gtk {
	class Menu;
}

class GUIObjectState;
class LocationListDialog;
class Marker;
class RangeMarker;
class Selection;
class TrackViewList;

class LocationListItem : public MemoryLocationItem, public WavesUI, public PBD::ScopedConnectionList
{
public:
    LocationListItem (LocationListDialog*);
    virtual ~LocationListItem ();
    
    virtual void set_order_number (std::size_t num);
    virtual void end_all_editing ();
    
    virtual void set_selected (bool yn) = 0;
    virtual bool get_selected () = 0;
    
    static PBD::Signal1<void, LocationListItem*> DeleteAllSelectedRanges;
    static PBD::Signal1<void, LocationListItem*> ExtendItemSelection;
    static PBD::Signal1<void, LocationListItem*> ClearSelection;
    
    void set_clock_mode (AudioClock::Mode mode);
    virtual void set_name (std::string name);
    virtual std::string get_name () { return _name; };
    virtual PBD::ID get_id () = 0;
protected:
    LocationListItem ();
    
    virtual void begin_name_edit () = 0;
    virtual void name_entry_changed () = 0;
    
    Gtk::Container& _controls_event_box;
    Gtk::Label& _order_number_label;
    Gtk::Label& _type_label;
    WavesButton& _on_off_button;
    
    MainClock _start_clock;
    MainClock _end_clock;
    WavesButton& _start_clock_button;
    WavesButton& _end_clock_button;
    std::string _name;
    
    Gtk::Entry& _name_entry;
    WavesButton& _name_button;
    Gtk::EventBox& _name_entry_eventbox;
    
    Gtk::Menu* _display_menu;
    LocationListDialog* _location_list_dialog;
    
    bool location_list_item_button_press (GdkEventButton*);    
    virtual void popup_display_menu (guint32 when) = 0;
    virtual void build_display_menu () = 0;
    
    virtual void start_clock_value_changed () = 0;
    virtual void end_clock_value_changed () = 0;
    
    PBD::ScopedConnectionList _location_connections;
private:
    void update_name (ARDOUR::Location* location);
    void end_name_edit (int response);
    
    bool name_entry_key_release (GdkEventKey *ev);
	bool name_entry_key_press (GdkEventKey *ev);
 	bool name_entry_focus_out (GdkEventFocus *ev);
    
    bool start_clock_focus_out (GdkEventFocus *ev);
    bool end_clock_focus_out (GdkEventFocus *ev);
    
    void on_name_button_double_clicked (WavesButton* button);
    void on_start_clock_button_double_clicked (WavesButton* button);
    void on_end_clock_button_double_clicked (WavesButton* button);
};

class LocationListSkipMarker : public LocationListItem
{
public:
    static PBD::Signal1<void,LocationListSkipMarker*> CatchDeletion;
    LocationListSkipMarker (LocationListDialog* lld, RangeMarker* skip_marker);
    virtual ~LocationListSkipMarker ();
    RangeMarker* get_skip_marker () { return _skip_marker; }
    virtual std::string get_name ();
    virtual PBD::ID get_id ();
    virtual bool get_selected ();
    virtual void set_selected (bool yn);
protected:
    void self_delete (Marker*);
    void set_location (ARDOUR::Location* location);
    virtual void begin_name_edit ();
    virtual void name_entry_changed ();
    virtual void popup_display_menu (guint32 when);
    virtual void build_display_menu ();
    virtual void start_clock_value_changed ();
    virtual void end_clock_value_changed ();
private:
    RangeMarker* _skip_marker;
    ARDOUR::Location* _location;
    void parameter_changed (std::string p);
    void on_off_button_clicked (WavesButton*);
    void display_item_data ();
    void marker_selected_changed ();
    void delete_selected_ranges ();
};

class LocationListSelection : public LocationListItem
{
public:
    static PBD::Signal1<void,LocationListSelection*> CatchDeletion;
    LocationListSelection (LocationListDialog* lld, ARDOUR::framepos_t start, ARDOUR::framepos_t end, std::string name);
    ~LocationListSelection ();
    ARDOUR::framepos_t start () { return _start; }
    ARDOUR::framepos_t end () { return _end; }
    PBD::ID get_id () { return _id; }
    void set_id (PBD::ID id) { _id = id; }
    void set_name (std::string name);
    std::string get_name () { return _name; };
    virtual bool get_selected ();
    virtual void set_selected (bool yn);
    void update_current_selection_time_and_route_ids ();
    std::list<std::string> get_routes_id ();
    bool allow_to_show_update_current_selection_menu_on_canvas ();
    
    static PBD::Signal1<void,LocationListSelection*> NameChanged;
protected:
    virtual void begin_name_edit ();
    virtual void name_entry_changed ();
    virtual void popup_display_menu (guint32 when);
    virtual void build_display_menu ();
    virtual void start_clock_value_changed ();
    virtual void end_clock_value_changed ();
private:
    void order_number_button_clicked (WavesButton* button);
    void delete_selected_ranges ();
    void update_time_in_gui_object ();
    bool update_current_selection_is_allowed ();
    void init ();
    ARDOUR::framepos_t _start;
    ARDOUR::framepos_t _end;
    PBD::ID  _id;
    
    GUIObjectState& gui_object_state ();
};

#endif /* __location_list_item__ */
