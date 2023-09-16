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

#include "marker_template_item.h"

#include "ardour_ui.h"
#include "editor.h"
#include "keyboard.h"
#include "location_list_dialog.h"
#include "marker.h"
#include "memory_location_item.h"
#include "rgb_macros.h"
#include "waves_ui.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>
#include <gtkmm/menu_elems.h>

#include "gtkmm2ext/keyboard.h"

#include "pbd/compose.h"
#include "pbd/whitespace.h"
#include "gui_thread.h"
#include "gui_object.h"

PBD::Signal0<void> MarkerTemplateItem::DeleteAllSelectedMarkerItems;
PBD::Signal0<void> MarkerTemplateItem::ClearSelection;
PBD::Signal1<void, MemoryLocationItem*> MarkerTemplateItem::ExtendItemSelection;
PBD::Signal1<void, MarkerTemplateItem*> MarkerTemplateItem::CatchDeletion;
PBD::Signal1<void, MarkerTemplateItem*> MarkerTemplateItem::PropertyChanged;
PBD::Signal1<void, MarkerTemplateItem*> MarkerTemplateItem::EditNext;

const std::string MarkerTemplateItem::pre_define_marker_name = "Marker";

MarkerTemplateItem::MarkerTemplateItem (LocationListDialog* location_list_dialog)
    : WavesUI ("marker_template_item.xml", *this)
    , _controls_event_box (WavesUI::root ())
    , _order_number_label (get_label ("order_number_label"))
    , _name_button (get_waves_button ("name_button"))
    , _name_entry (get_entry ("name_entry"))
    , _name_entry_eventbox (get_event_box ("name_entry_eventbox"))
    , _color_dropdown (get_waves_dropdown ("color_dropdown"))
    , _bank_dropdown (get_waves_dropdown ("bank_dropdown"))
    , _program_dropdown (get_waves_dropdown ("program_dropdown"))
    , _channel_dropdown (get_waves_dropdown ("channel_dropdown"))
    , _location_list_dialog (location_list_dialog)
    , _display_menu (0)
{
    _name_entry.add_events (Gdk::FOCUS_CHANGE_MASK);
    _name_entry_eventbox.hide ();
    _name_button.show ();
    
    _name = pre_define_marker_name;
    set_name (_name);
    
    _color_dropdown.set_current_item (9);
    _bank_dropdown.set_current_item (0);
    _program_dropdown.set_current_item (0);
    _channel_dropdown.set_current_item (0);
    
    _controls_event_box.signal_button_press_event ().connect (sigc::mem_fun (*this, &MarkerTemplateItem::marker_list_item_button_press));
    _name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MarkerTemplateItem::marker_list_item_button_press), false);
    
    _name_button.signal_double_clicked.connect (sigc::mem_fun (*this, &MarkerTemplateItem::on_name_button_double_clicked));
    
    add_events (Gdk::BUTTON_RELEASE_MASK|
                Gdk::ENTER_NOTIFY_MASK|
                Gdk::LEAVE_NOTIFY_MASK|
                Gdk::KEY_PRESS_MASK|
                Gdk::KEY_RELEASE_MASK);
    
	set_flags (get_flags() | Gtk::CAN_FOCUS);
    
    _name_entry.signal_key_press_event ().connect (sigc::mem_fun (*this, &MarkerTemplateItem::name_entry_key_press), false);
	_name_entry.signal_key_release_event ().connect (sigc::mem_fun (*this, &MarkerTemplateItem::name_entry_key_release), false);
	_name_entry.signal_focus_out_event ().connect (sigc::mem_fun (*this, &MarkerTemplateItem::name_entry_focus_out));
    
    _name_entry.set_max_length (250);
	
    _color_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerTemplateItem::on_color_dropdown_item_changed));
    _bank_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerTemplateItem::on_bank_dropdown_item_changed));
	_program_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerTemplateItem::on_program_dropdown_item_changed));
	_channel_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerTemplateItem::on_channel_dropdown_item_changed));
}

bool
MarkerTemplateItem::marker_list_item_button_press (GdkEventButton* ev)
{
    switch (ev->button) {
        case 1: // left button pressed
            // cmd on MacOS OR ctr on Windows
            if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, (Gtkmm2ext::Keyboard::PrimaryModifier))) {
                if (get_selected ()) {
                    set_selected (false);
                } else {
                    set_selected (true);
                }
                return false;
            }
            // Shift
            if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, (Gtkmm2ext::Keyboard::TertiaryModifier))) {
                ExtendItemSelection (this);
                return false;
            }
            
            ClearSelection ();
            set_selected (true);
            return false;
        case 3: // right button pressed
            if (session_is_recording ()) {
                return true;
            }
            popup_display_menu (ev->time);
            break;
	}
    
	return true;
}

bool
MarkerTemplateItem::get_selected ()
{
    return get_state () == Gtk::STATE_SELECTED;
}

void
MarkerTemplateItem::set_selected (bool yn)
{
    if (yn) {
        set_state (Gtk::STATE_SELECTED);
    } else {
        set_state (Gtk::STATE_NORMAL);
    }    
}

void
MarkerTemplateItem::popup_display_menu (guint32 when)
{
    build_display_menu ();
    _display_menu->popup (1, when);
}

void
MarkerTemplateItem::build_display_menu ()
{
    delete _display_menu;
	_display_menu = new Gtk::Menu;
	_display_menu->set_name ("ArdourContextMenu");
    
    Gtk::Menu_Helpers::MenuList& items = _display_menu->items ();
    
    if (!_location_list_dialog->multiple_selection_in_marker_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Rename"), sigc::mem_fun (*this, &MarkerTemplateItem::begin_name_edit)));
    }
    
    if (_location_list_dialog->exists_selected_item_in_marker_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Delete"), sigc::mem_fun (*this, &MarkerTemplateItem::delete_selected_items_permanently)));
    }
}

MarkerTemplateItem::~MarkerTemplateItem ()
{
    XMLNode* root = ARDOUR_UI::instance ()->gui_object_state->get_or_add_node (LocationListDialog::xml_node_midi_marker_templates);
    root->remove_nodes_and_delete ("id", string_compose ("%1", _id));
    CatchDeletion (this);
    delete _display_menu;
}

void
MarkerTemplateItem::delete_selected_items_permanently ()
{
    DeleteAllSelectedMarkerItems ();
}

void
MarkerTemplateItem::on_color_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    std::string path = string_compose("inspector_color_%1", selected_item + 1);
    _color_dropdown.set_normal_image (ARDOUR_UI_UTILS::get_icon (path.c_str ()));
    
    PropertyChanged (this);
}

void
MarkerTemplateItem::on_bank_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    PropertyChanged (this);
}

void
MarkerTemplateItem::on_program_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    PropertyChanged (this);
}

void
MarkerTemplateItem::on_channel_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    PropertyChanged (this);
}

std::string
MarkerTemplateItem::get_name () const
{
    return _name;
}

std::string
MarkerTemplateItem::get_color () const
{
    const Gdk::Color& color = Marker::XMLColor [_color_dropdown.get_current_item ()];
    ArdourCanvas::Color c = RGBA_TO_UINT (color.get_red () >> 8, color.get_green () >> 8, color.get_blue () >> 8, 255);
    unsigned int r, g, b, a;
    UINT_TO_RGBA (c, &r, &g, &b, &a);
    char buf[64];
    snprintf (buf, sizeof (buf), "%d:%d:%d", r, g, b);
    
    return std::string (buf);
}

std::size_t
MarkerTemplateItem::get_color_item () const
{
    return _color_dropdown.get_current_item ();
}

int
MarkerTemplateItem::get_bank () const
{
    return _bank_dropdown.get_current_item () - 1;
}

int
MarkerTemplateItem::get_program () const
{
    return _program_dropdown.get_current_item ();
}

int
MarkerTemplateItem::get_channel () const
{
    return _channel_dropdown.get_current_item ();
}

void
MarkerTemplateItem::set_name_externally (std::string name)
{
    _name = name;
    set_name (_name);
}

void
MarkerTemplateItem::set_color (std::string str_color)
{
    int r, g, b;
    uint32_t color;
    sscanf (str_color.c_str(), "%d:%d:%d", &r, &g, &b);
    color = RGBA_TO_UINT (r, g, b, 255);
    
    for (std::size_t i = 0; i < Marker::colors_size; ++i) {
        Gdk::Color iter_gdk_color = Marker::XMLColor[i];
        uint32_t iter_color = RGBA_TO_UINT (iter_gdk_color.get_red () >> 8, iter_gdk_color.get_green () >> 8, iter_gdk_color.get_blue () >> 8, 255);
        
        if (iter_color == color) {
            _color_dropdown.set_current_item (i);
            break;
        }
    }
}

void
MarkerTemplateItem::set_bank (int bank)
{
    _bank_dropdown.set_current_item (bank + 1);
}

void
MarkerTemplateItem::set_program (int prg)
{
    _program_dropdown.set_current_item (prg);
}

void
MarkerTemplateItem::set_channel (int ch)
{
    _channel_dropdown.set_current_item (ch);
}

bool
MarkerTemplateItem::name_entry_key_press (GdkEventKey *ev)
{
    switch (ev->keyval) {
        case GDK_Escape:
        case GDK_Return:
        case GDK_KP_Enter:
        case GDK_Tab:
            return true;
	}
	return false;
}

bool
MarkerTemplateItem::name_entry_key_release (GdkEventKey *ev)
{
    switch (ev->keyval) {
        case GDK_Escape:
            end_name_edit (Gtk::RESPONSE_CANCEL);
            return true;
        case GDK_Return:
        case GDK_KP_Enter:
            end_name_edit (Gtk::RESPONSE_OK);
            return true;
        case GDK_Tab:
            end_name_edit (Gtk::RESPONSE_ACCEPT);
            return true;
        default:
            break;
	}
    
	return false;
}

bool
MarkerTemplateItem::name_entry_focus_out (GdkEventFocus *)
{
    end_name_edit (Gtk::RESPONSE_OK);
    return false;
}

void
MarkerTemplateItem::on_name_button_double_clicked (WavesButton* button)
{
    begin_name_edit ();
}

void
MarkerTemplateItem::set_order_number (std::size_t num)
{
    _order_number = num;
    _order_number_label.set_text (string_compose ("%1", _order_number));
}

void
MarkerTemplateItem::set_name (std::string name)
{
    _name_button.set_text (name);
    _name_entry.set_text (name);
}

void
MarkerTemplateItem::begin_name_edit ()
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
MarkerTemplateItem::end_all_editing ()
{
    end_name_edit (Gtk::RESPONSE_OK);
}

void
MarkerTemplateItem::end_name_edit (int response)
{
    if (_name_entry.get_visible () == false) {
        return;
    }
    
    bool edit_next = false;
    
	switch (response) {
        case Gtk::RESPONSE_CANCEL:
            break;
        case Gtk::RESPONSE_OK:
        case Gtk::RESPONSE_APPLY:
            name_entry_changed ();
            break;
        case Gtk::RESPONSE_ACCEPT:
            name_entry_changed ();
            edit_next = true;
            break;
	}
    
    set_name (_name);
    PropertyChanged (this);
	
    _name_button.show ();
	_name_entry.hide ();
    _name_entry_eventbox.hide ();
    
    if (edit_next) {
        EditNext (this);
    }
}

void
MarkerTemplateItem::name_entry_changed ()
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
