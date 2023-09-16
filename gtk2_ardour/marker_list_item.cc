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

#include "marker_list_item.h"

#include "ardour_ui.h"
#include "editor.h"
#include "keyboard.h"
#include "location_list_dialog.h"
#include "main_clock.h"
#include "marker.h"
#include "memory_location_item.h"
#include "rgb_macros.h"
#include "waves_ui.h"

#include "ardour/midi_scene_change.h"
#include "ardour/location.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>
#include <gtkmm/menu_elems.h>

#include "gtkmm2ext/keyboard.h"

#include "pbd/compose.h"
#include "pbd/whitespace.h"
#include "gui_thread.h"

PBD::Signal0<void> MarkerListItem::DeleteAllSelectedMarkerItems;
PBD::Signal0<void> MarkerListItem::ClearSelection;
PBD::Signal1<void, MemoryLocationItem*> MarkerListItem::ExtendItemSelection;
PBD::Signal1<void, MarkerListItem*> MarkerListItem::CatchDeletion;
PBD::Signal1<void, MemoryLocationItem*> MarkerListItem::EditNext;

MarkerListItem::MarkerListItem (Marker* marker, LocationListDialog* location_list_dialog)
    : WavesUI ("marker_list_item.xml", *this)
    , _marker (marker)
    , _location_list_dialog (location_list_dialog)
    , _clock ("primary", true, "marker list", true, true, true, false, false)
    , _clock_button (get_waves_button ("clock_button"))
    , _controls_event_box (WavesUI::root ())
    , _order_number_label (get_label ("order_number_label"))
    , _go_button (get_waves_button ("go_button"))
    , _name_button (get_waves_button ("name_button"))
    , _name_entry (get_entry ("name_entry"))
    , _name_entry_eventbox (get_event_box ("name_entry_eventbox"))
    , _lock_button (get_waves_button ("lock_button"))
    , _stop_button (get_waves_button ("stop_button"))
    , _color_dropdown (get_waves_dropdown ("color_dropdown"))
    , _program_change_on_off_button (get_waves_button ("program_change_on_off_button"))
	, _program_change_info_panel (get_container ("program_change_info_panel"))
    , _bank_dropdown (get_waves_dropdown ("bank_dropdown"))
    , _program_dropdown (get_waves_dropdown ("program_dropdown"))
    , _channel_dropdown (get_waves_dropdown ("channel_dropdown"))
    , _display_menu (0)
{
    if (_marker) {
        Marker::CatchDeletion.connect (*this, MISSING_INVALIDATOR, boost::bind (&MarkerListItem::self_delete, this, _1), gui_context ());
        _marker->GuiChanged.connect   (*this, MISSING_INVALIDATOR, boost::bind (&MarkerListItem::marker_gui_changed, this, _1, _2), gui_context());
        _marker->SelectedChanged.connect (*this, invalidator (*this), boost::bind (&MarkerListItem::marker_selected_changed, this), gui_context ());
    }
        
    set_location (_marker->location());
    set_color_dropdown_from_marker ();
    
    get_box ("clock_home").pack_start (_clock, false, false);
    _clock.show ();

    _name_entry.add_events (Gdk::FOCUS_CHANGE_MASK);
    _name_entry_eventbox.hide ();
    _name_button.show ();
    
    _controls_event_box.signal_button_press_event ().connect (sigc::mem_fun (*this, &MarkerListItem::marker_list_item_button_press));
    _name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MarkerListItem::marker_list_item_button_press), false);
    _clock_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MarkerListItem::marker_list_item_button_press), false);
    
    _name_button.signal_double_clicked.connect (sigc::mem_fun (*this, &MarkerListItem::on_name_button_double_clicked));
    
    add_events (Gdk::BUTTON_RELEASE_MASK|
                Gdk::ENTER_NOTIFY_MASK|
                Gdk::LEAVE_NOTIFY_MASK|
                Gdk::KEY_PRESS_MASK|
                Gdk::KEY_RELEASE_MASK);
    
	set_flags (get_flags() | Gtk::CAN_FOCUS);
    
    _name_entry.signal_key_press_event ().connect (sigc::mem_fun (*this, &MarkerListItem::name_entry_key_press), false);
	_name_entry.signal_key_release_event ().connect (sigc::mem_fun (*this, &MarkerListItem::name_entry_key_release), false);
	_name_entry.signal_focus_out_event ().connect (sigc::mem_fun (*this, &MarkerListItem::name_entry_focus_out));
    _clock.signal_focus_out_event ().connect (sigc::mem_fun (*this, &MarkerListItem::clock_focus_out));
    
    _clock_button.signal_double_clicked.connect (sigc::mem_fun (*this, &MarkerListItem::on_clock_button_double_clicked));

    
    _name_entry.set_max_length (250);
    
    _lock_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerListItem::lock_button_clicked));
    _stop_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerListItem::stop_button_clicked));
    _go_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerListItem::go_button_clicked));
    _program_change_on_off_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerListItem::program_change_on_off_button_clicked));
	
    _color_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerListItem::on_color_dropdown_item_changed));
    _bank_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerListItem::on_bank_dropdown_item_changed));
	_program_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerListItem::on_program_dropdown_item_changed));
	_channel_dropdown.selected_item_changed.connect (mem_fun (*this, &MarkerListItem::on_channel_dropdown_item_changed));
    
    boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
    if (msc) {
        msc->ActiveChanged.connect (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::program_changed, this), gui_context());
        msc->ChannelChanged.connect (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::display_scene_change_info, this), gui_context());
        msc->ProgramChanged.connect (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::display_scene_change_info, this), gui_context());
        msc->BankChanged.connect    (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::display_scene_change_info, this), gui_context());
    }
    
    _clock.ValueChanged.connect (sigc::mem_fun (*this, &MarkerListItem::clock_value_changed));
}

bool
MarkerListItem::marker_list_item_button_press (GdkEventButton* ev)
{
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();
    
	switch (ev->button) {
        case 1: // left button pressed
            // cmd on MacOS OR ctr on Windows
            if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, (Gtkmm2ext::Keyboard::PrimaryModifier))) {
                if (_marker->selected ()) {
                    selection.remove (_marker);
                } else {
                    selection.add (_marker);
                }
                return false;
            }
            // Shift
            if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, (Gtkmm2ext::Keyboard::TertiaryModifier))) {
                ExtendItemSelection (this);
                return false;
            }
            
            ClearSelection ();
            selection.add (_marker);
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

void
MarkerListItem::set_selected (bool yn)
{
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();
    
    if (yn) {
        selection.add (_marker);
    } else {
        selection.remove (_marker);
    }
    
    _marker->set_selected (yn);
}

bool
MarkerListItem::get_selected ()
{
    return _marker->selected ();
}

void
MarkerListItem::popup_display_menu (guint32 when)
{
    build_display_menu ();
    _display_menu->popup (1, when);
}

void
MarkerListItem::build_display_menu ()
{
    delete _display_menu;
	_display_menu = new Gtk::Menu;
	_display_menu->set_name ("ArdourContextMenu");
    
    Gtk::Menu_Helpers::MenuList& items = _display_menu->items ();
    
    if (!_location_list_dialog->multiple_selection_in_marker_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Rename"), sigc::mem_fun (*this, &MarkerListItem::begin_name_edit)));
    }
    
    if (_location_list_dialog->exists_selected_item_in_marker_list ()) {
        items.push_back (Gtk::Menu_Helpers::MenuElem (("Delete"), sigc::mem_fun (*this, &MarkerListItem::delete_selected_items_permanently)));
    }
}

void
MarkerListItem::clock_value_changed ()
{
    if (!_location->locked ()) {
        _location->set_start (_clock.current_time ());
    } else {
        _clock.set (_location->start (), true);
        _clock_button.set_text (_clock.get_text ());
    }
}

void
MarkerListItem::marker_gui_changed (std::string what_changed, void*)
{
	if (what_changed == "color") {
		set_color_dropdown_from_marker ();
	}
}

void
MarkerListItem::set_color_dropdown_from_marker ()
{
    uint32_t marker_color = _marker->get_color ();
    
    for (std::size_t i = 0; i < Marker::colors_size; ++i) {
        Gdk::Color iter_gdk_color = Marker::XMLColor[i];
        uint32_t iter_canvas_color = RGBA_TO_UINT (iter_gdk_color.get_red () >> 8, iter_gdk_color.get_green () >> 8, iter_gdk_color.get_blue () >> 8, 255);

        if (iter_canvas_color == marker_color) {
            _color_dropdown.set_current_item (i);
            break;
        }
    }
}

void
MarkerListItem::set_clock_mode (AudioClock::Mode mode)
{
    _clock.set_mode (mode);
    _clock_button.set_text (_clock.get_text ());
}

void
MarkerListItem::on_clock_button_double_clicked (WavesButton*)
{
    _clock_button.hide ();
    get_box ("clock_home").show ();
    _clock.show ();
    _clock.start_edit ();
}

bool
MarkerListItem::clock_focus_out (GdkEventFocus *ev)
{
    _clock.hide ();
    get_box ("clock_home").hide ();
    _clock_button.show ();
    _clock_button.set_text (_clock.get_text ());
    grab_focus ();
    
    return false;
}

void
MarkerListItem::set_order_number (std::size_t num)
{
    _order_number_label.set_text (string_compose ("%1", num));
    _order_number = num;
}

void
MarkerListItem::lock_button_clicked (WavesButton* button)
{
    if (!_location->locked ()) {
        _location->lock ();
    } else {
        _location->unlock ();
    }
}

void
MarkerListItem::stop_button_clicked (WavesButton* button)
{
    _location->set_stop_playhead (!_location->get_stop_playhead () );
}

void
MarkerListItem::go_button_clicked (WavesButton*)
{
    ARDOUR::Session* session = ARDOUR_UI::instance ()->the_session ();
	if (session) {
		session->request_locate (_location->start ());
	}
}

void
MarkerListItem::set_location (ARDOUR::Location* location)
{
    _location_connections.drop_connections ();
    _scene_change_connection.drop_connections ();

    _location = location;
    
    if (_location) {
        _location->LockChanged.connect (_location_connections, invalidator (*this), boost::bind (&MarkerListItem::display_marker_data, this), gui_context());
        _location->StopChanged.connect (_location_connections, invalidator (*this), boost::bind (&MarkerListItem::display_marker_data, this), gui_context());
        _location->NameChanged.connect (_location_connections, invalidator (*this), boost::bind (&MarkerListItem::display_marker_data, this), gui_context());
        _location->StartChanged.connect (_location_connections, invalidator (*this), boost::bind (&MarkerListItem::display_marker_data, this), gui_context());
        _location->SceneChangeChanged.connect (_location_connections, invalidator (*this), boost::bind (&MarkerListItem::on_set_scene_change_changed, this), gui_context());

        display_marker_data ();
    }
    
    // set_session must be invoked before set position
    _clock.set_session (ARDOUR_UI::instance ()->the_session ());
    _clock.set (_location->start (), true);
    _clock_button.set_text (_clock.get_text ());
}

PBD::ID
MarkerListItem::get_id ()
{
    return _location->id ();
}

void
MarkerListItem::on_set_scene_change_changed ()
{
    boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
    if (msc) {
        msc->ActiveChanged.connect  (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::program_changed, this), gui_context());
        msc->ChannelChanged.connect (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::display_scene_change_info, this), gui_context());
        msc->ProgramChanged.connect (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::display_scene_change_info, this), gui_context());
        msc->BankChanged.connect    (_scene_change_connection, invalidator(*this), boost::bind (&MarkerListItem::display_scene_change_info, this), gui_context());
        program_changed ();
    }
}

MarkerListItem::~MarkerListItem ()
{
    ARDOUR_UI::instance ()->gui_object_state->remove_node (_marker->location_state_id ());
    CatchDeletion (this);
    delete _display_menu;
}

void
MarkerListItem::marker_selected_changed ()
{
    set_state (_marker->selected () ? Gtk::STATE_SELECTED : Gtk::STATE_NORMAL);
}

/*
 Delete only gui element.
 */
void
MarkerListItem::self_delete (Marker* marker)
{
    if (marker && (marker == _marker)) {
        delete this;
    }
}

/*
 Delete marker permanently. Gui element will be deleted automaticly by invoking MarkerListItem::self_delete.
 */
void
MarkerListItem::delete_selected_items_permanently ()
{
    DeleteAllSelectedMarkerItems ();
}

void
MarkerListItem::display_marker_data ()
{
    if (_location) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
		display_scene_change_info ();
        set_name (_location->name ());
		enable_program_change (msc && msc->active ());
        
        _clock.set (_location->start (), true);
        _clock_button.set_text (_clock.get_text ());
        
        _lock_button.set_active (_location->locked ());
        _stop_button.set_active (_location->get_stop_playhead ());
        if (_location->locked ()) {
            _clock.set_editable (false);
        } else {
            _clock.set_editable (true);
        }
	}
}

void
MarkerListItem::display_scene_change_info ()
{
	boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
	if (msc) {
		_bank_dropdown.set_current_item (msc->bank () + 1);
		_program_dropdown.set_current_item (msc->program ());
		_channel_dropdown.set_current_item (msc->channel ());
	}
}

void
MarkerListItem::program_changed ()
{
    boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
    if (msc) {
        enable_program_change (msc->active ());
    }
}

void
MarkerListItem::enable_program_change (bool yn)
{
	_program_change_on_off_button.set_active (yn);
    _program_change_info_panel.set_visible (yn);
    
    if (yn) {
        display_scene_change_info ();
    }
    
	if (_location) {
		boost::shared_ptr<ARDOUR::SceneChange> sc = _location->scene_change ();
		if (sc) {
			boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (sc);
			if (msc && (msc->active () != yn)) {
				msc->set_active (yn);
                
                set_session_dirty ();
			}
		} else if (yn) {
			_location->set_scene_change (boost::shared_ptr<ARDOUR::MIDISceneChange> (new ARDOUR::MIDISceneChange (0, -1, 1)));
			display_scene_change_info ();
            set_session_dirty ();
		}
	}
    set_session_dirty ();
}

void
MarkerListItem::program_change_on_off_button_clicked (WavesButton *button)
{
    boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
    if (msc) {
        enable_program_change (!msc->active ());
    } else {
        enable_program_change (true);
    }
}

void
MarkerListItem::on_color_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    std::string path = string_compose ("inspector_color_%1", selected_item + 1);
    _color_dropdown.set_normal_image (ARDOUR_UI_UTILS::get_icon (path.c_str ()));
    _marker->set_color (Marker::XMLColor[selected_item]);
    set_session_dirty ();
}

void
MarkerListItem::on_bank_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    if (_location) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
        int bank = selected_item - 1;
		if (msc && (msc->bank () != bank)) {
			msc->set_bank (bank);
            set_session_dirty ();
		}
	}
}

void
MarkerListItem::on_program_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    if (_location) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
		if (msc && (msc->program () != selected_item)) {
			msc->set_program (selected_item);
            set_session_dirty ();
		}
	}
}

void
MarkerListItem::on_channel_dropdown_item_changed (WavesDropdown*, int selected_item)
{
    if (_location) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_location->scene_change ());
		if (msc && (msc->channel () != selected_item)) {
			msc->set_channel (selected_item);
            set_session_dirty ();
		}
	}
}

bool
MarkerListItem::name_entry_key_press (GdkEventKey *ev)
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
MarkerListItem::name_entry_key_release (GdkEventKey *ev)
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
MarkerListItem::name_entry_focus_out (GdkEventFocus *)
{
    end_name_edit (Gtk::RESPONSE_OK);
    return false;
}

void
MarkerListItem::on_name_button_double_clicked (WavesButton* button)
{
    begin_name_edit ();
}

void
MarkerListItem::set_name (std::string name)
{
    _name_button.set_text (name);
    _name_entry.set_text (name);
}

void
MarkerListItem::begin_name_edit ()
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

void
MarkerListItem::end_all_editing ()
{
    if (_name_entry.has_focus ()) {
        end_name_edit (Gtk::RESPONSE_OK);
    }
    
    if (_clock.has_focus ()) {
        _clock.end_edit (true);
    }
}

void
MarkerListItem::end_name_edit (int response)
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
    
    if (_location) {
        set_name (_location->name ());
    }
	
    _name_button.show ();
	_name_entry.hide ();
    _name_entry_eventbox.hide ();
    
    if (edit_next) {
        EditNext (this);
    }
}

void
MarkerListItem::name_entry_changed ()
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
