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

#ifndef __marker_template_item__
#define __marker_template_item__

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
class LocationListDialog;

namespace Gtk {
	class Menu;
}

class MarkerTemplateItem : public MemoryLocationItem, public WavesUI, public PBD::ScopedConnectionList
{
public:
    MarkerTemplateItem (LocationListDialog*);
	~MarkerTemplateItem ();
    
    virtual void set_order_number (std::size_t num);
    
    virtual void set_selected (bool yn);
    virtual bool get_selected ();
    virtual void end_all_editing ();

    static PBD::Signal0<void> DeleteAllSelectedMarkerItems;
    static PBD::Signal0<void> ClearSelection;
    static PBD::Signal1<void, MemoryLocationItem*> ExtendItemSelection;
    static PBD::Signal1<void, MarkerTemplateItem*> CatchDeletion;
    static PBD::Signal1<void, MarkerTemplateItem*> PropertyChanged;
    static PBD::Signal1<void, MarkerTemplateItem*> EditNext;
    
    virtual PBD::ID get_id () { return _id; }
    void set_id (PBD::ID id) { _id = id; }
    
    std::string get_name () const;
    std::string get_color () const;
    std::size_t get_color_item () const;
    int get_bank () const;
    int get_program () const;
    int get_channel () const;
    
    void set_name_externally (std::string);
    void set_color (std::string);
    void set_bank (int);
    void set_program (int);
    void set_channel (int);
    
    virtual void begin_name_edit ();
protected:
    void self_delete ();
private:
    static const std::string pre_define_marker_name;
    LocationListDialog* _location_list_dialog;
    
    Gtk::Container& _controls_event_box;
    Gtk::Label& _order_number_label;
    WavesButton& _name_button;
    Gtk::Entry& _name_entry;
    Gtk::EventBox& _name_entry_eventbox;

    WavesDropdown& _color_dropdown;
    
    WavesDropdown& _bank_dropdown;
	WavesDropdown& _program_dropdown;
	WavesDropdown& _channel_dropdown;
    
    Gtk::Menu* _display_menu;
    
    std::string _name;
    
    bool marker_list_item_button_press (GdkEventButton*);
    void popup_display_menu (guint32 when);
    void build_display_menu ();
    bool show_only_delete_menu_item ();
    void delete_selected_items_permanently ();
    
    void end_name_edit (int response);
    
    bool name_entry_key_release (GdkEventKey *ev);
	bool name_entry_key_press (GdkEventKey *ev);
 	bool name_entry_focus_out (GdkEventFocus *ev);
    void name_entry_changed ();
    void set_name (std::string name);
    
    void on_name_button_double_clicked (WavesButton* button);
    
    void delete_selected_markers_permanently ();
    
    void program_change_on_button_clicked (WavesButton *button);
	void program_change_off_button_clicked (WavesButton *button);

    void on_color_dropdown_item_changed (WavesDropdown*, int);
	void on_bank_dropdown_item_changed (WavesDropdown*, int);
	void on_program_dropdown_item_changed (WavesDropdown*, int);
	void on_channel_dropdown_item_changed (WavesDropdown*, int);
    
    PBD::ID _id;
    
    PBD::ScopedConnectionList _location_connections;
    PBD::ScopedConnectionList _scene_change_connection;
};

#endif /* __marker_template_item__ */
