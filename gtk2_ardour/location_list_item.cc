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
#include "main_clock.h"
#include "waves_ui.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "gtkmm2ext/keyboard.h"

#include "pbd/compose.h"
#include "pbd/whitespace.h"
#include "gui_thread.h"

#include "i18n.h"

PBD::Signal1<void, LocationListItem*>LocationListItem::DeleteAllSelectedRanges;
PBD::Signal1<void, LocationListItem*>LocationListItem::ExtendItemSelection;
PBD::Signal1<void, LocationListItem*>LocationListItem::ClearSelection;

LocationListItem::LocationListItem (LocationListDialog* lld)
    : MemoryLocationItem ()
    , WavesUI ("location_list_item.xml", *this)
    , _controls_event_box (WavesUI::root ())
    , _order_number_label (get_label ("order_number_label"))
    , _type_label (get_label ("type_label"))
    , _on_off_button (get_waves_button ("on_off_button"))
    , _start_clock ("primary", true, "marker list", true, true, true, false, false)
    , _end_clock ("primary", true, "marker list", true, true, true, false, false)
    , _start_clock_button (get_waves_button ("start_clock_button"))
    , _end_clock_button (get_waves_button ("end_clock_button"))
    , _name_entry (get_entry ("name_entry"))
    , _name_button (get_waves_button ("name_button"))
    , _name_entry_eventbox (get_event_box ("name_entry_eventbox"))
    , _display_menu (0)
    , _location_list_dialog (lld)
{
    // set_session must be invoked before set position
    _start_clock.set_session (ARDOUR_UI::instance ()->the_session ());
    _end_clock.set_session (ARDOUR_UI::instance ()->the_session ());
    
    get_box ("start_clock_home").pack_start (_start_clock, false, false);
    _start_clock.show ();
    
    get_box ("end_clock_home").pack_start (_end_clock, false, false);
    _end_clock.show ();
    
    _name_entry.add_events (Gdk::FOCUS_CHANGE_MASK);
    _name_entry_eventbox.hide ();
    _name_button.show ();
    
    _controls_event_box.signal_button_press_event ().connect (sigc::mem_fun (*this, &LocationListItem::location_list_item_button_press));
    _name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &LocationListItem::location_list_item_button_press), false);
    _start_clock_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &LocationListItem::location_list_item_button_press), false);
    _end_clock_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &LocationListItem::location_list_item_button_press), false);
    
    _name_button.signal_double_clicked.connect (sigc::mem_fun (*this, &LocationListItem::on_name_button_double_clicked));
    
    add_events (Gdk::BUTTON_RELEASE_MASK|
                Gdk::ENTER_NOTIFY_MASK|
                Gdk::LEAVE_NOTIFY_MASK|
                Gdk::KEY_PRESS_MASK|
                Gdk::KEY_RELEASE_MASK);
    
	set_flags (get_flags () | Gtk::CAN_FOCUS);
    
    _name_entry.signal_key_press_event ().connect (sigc::mem_fun (*this, &LocationListItem::name_entry_key_press), false);
	_name_entry.signal_key_release_event ().connect (sigc::mem_fun (*this, &LocationListItem::name_entry_key_release), false);
	_name_entry.signal_focus_out_event ().connect (sigc::mem_fun (*this, &LocationListItem::name_entry_focus_out));
   
    _start_clock.signal_focus_out_event ().connect (sigc::mem_fun (*this, &LocationListItem::start_clock_focus_out));
    _end_clock.signal_focus_out_event ().connect (sigc::mem_fun (*this, &LocationListItem::end_clock_focus_out));
    
    _start_clock_button.signal_double_clicked.connect (sigc::mem_fun (*this, &LocationListItem::on_start_clock_button_double_clicked));
    _end_clock_button.signal_double_clicked.connect (sigc::mem_fun (*this, &LocationListItem::on_end_clock_button_double_clicked));   
   
    _name_entry.set_max_length (250);
    
    _start_clock.ValueChanged.connect (sigc::mem_fun (*this, &LocationListItem::start_clock_value_changed));
    _end_clock.ValueChanged.connect (sigc::mem_fun (*this, &LocationListItem::end_clock_value_changed));
}

LocationListItem::~LocationListItem ()
{
    delete _display_menu;
}

bool
LocationListItem::location_list_item_button_press (GdkEventButton* ev)
{
	switch (ev->button) {
        case 1:// left button clicked
            // cmd on MacOS or ctrl on Windows
            if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, (Gtkmm2ext::Keyboard::PrimaryModifier))) {
                set_selected (!get_selected ());
                return false;
            }
            // Shift
            if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, (Gtkmm2ext::Keyboard::TertiaryModifier))) {
                ExtendItemSelection (this);
                return false;
            }

            ClearSelection (this);
            set_selected (true);
            
            return false;
        case 3:// right button clicked
            if (session_is_recording ()) {
                return true;
            }
            if (!get_selected ()) {
                ClearSelection (this);
                set_selected (true);
            }
            popup_display_menu (ev->time);
            break;
	}
    
	return true;
}

void
LocationListItem::set_clock_mode (AudioClock::Mode mode)
{
    _start_clock.set_mode (mode);
    _start_clock_button.set_text (_start_clock.get_text ());
    _end_clock.set_mode (mode);
    _end_clock_button.set_text (_end_clock.get_text());
}

void
LocationListItem::set_order_number (std::size_t num)
{
    _order_number_label.set_text (string_compose ("%1", num));
    _order_number = num;
}

bool
LocationListItem::name_entry_key_press (GdkEventKey *ev)
{
    switch (ev->keyval) {
        case GDK_Escape:
        case GDK_Return:
        case GDK_KP_Enter:
            return true;
	}
	return false;
}

bool
LocationListItem::name_entry_key_release (GdkEventKey *ev)
{
    switch (ev->keyval) {
        case GDK_Escape:
            end_name_edit (Gtk::RESPONSE_CANCEL);
            return true;
        case GDK_Return:
        case GDK_KP_Enter:
            end_name_edit (Gtk::RESPONSE_OK);
            return true;
        default:
            break;
	}
    
	return false;
}

void
LocationListItem::on_name_button_double_clicked (WavesButton*)
{
    begin_name_edit ();
}

void
LocationListItem::on_start_clock_button_double_clicked (WavesButton*)
{
    _start_clock_button.hide ();
    get_box ("start_clock_home").show ();
    _start_clock.show ();
    _start_clock.start_edit ();
}

void
LocationListItem::on_end_clock_button_double_clicked (WavesButton*)
{
    _end_clock_button.hide ();
    get_box ("end_clock_home").show ();
    _end_clock.show ();
    _end_clock.start_edit ();
}


bool
LocationListItem::start_clock_focus_out (GdkEventFocus *ev)
{
    _start_clock.hide ();
    get_box ("start_clock_home").hide ();
    _start_clock_button.show ();
    _start_clock_button.set_text (_start_clock.get_text ());
    grab_focus ();
    
    return false;
}

bool
LocationListItem::end_clock_focus_out (GdkEventFocus *ev)
{
    _end_clock.hide ();
    get_box ("end_clock_home").hide ();
    _end_clock_button.show ();
    _end_clock_button.set_text (_end_clock.get_text ());
    grab_focus ();
    
    return false;
}

bool
LocationListItem::name_entry_focus_out (GdkEventFocus *)
{
    end_name_edit (Gtk::RESPONSE_OK);
    return false;
}

void
LocationListItem::set_name (std::string name)
{
    _name_button.set_text (name);
    _name_entry.set_text (name);
    _name = name;
}

void
LocationListItem::end_all_editing ()
{
    if (_name_entry.has_focus ()) {
        end_name_edit (Gtk::RESPONSE_OK);
    }
    
    if (_start_clock.has_focus ()) {
        _start_clock.end_edit (true);
    }
    
    if (_end_clock.has_focus ()) {
        _end_clock.end_edit (true);
    }
}

void
LocationListItem::end_name_edit (int response)
{
    if (_name_entry.get_visible () == false) {
        return;
    }
    
	switch (response) {
        case Gtk::RESPONSE_CANCEL:
            break;
        case Gtk::RESPONSE_OK:
        case Gtk::RESPONSE_ACCEPT:
        case Gtk::RESPONSE_APPLY:
            name_entry_changed ();
            break;
	}
    
    set_name (_name);
	
    _name_button.show ();
	_name_entry.hide ();
    _name_entry_eventbox.hide ();
}
