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
#include "gui_object.h"
#include "time_axis_view.h"

#include "ardour/session.h"
#include "ardour/location.h"

#include "i18n.h"

LocationListDialog::LocationListDialog ()
	: WavesUI ("location_list_dialog.xml", *this)
    , _tabs_home (get_container ("tabs_home"))
    , _midi_marker_list_home (get_box ("midi_marker_list_home"))
    , _location_list_home (get_box ("location_list_home"))
    , _midi_marker_list_tab_button (get_waves_button ("midi_marker_list_tab_button"))
    , _location_marker_list_tab_button (get_waves_button ("location_marker_list_tab_button"))
    , _marker_list_event_box (get_event_box ("midi_marker_list_event_box"))
    , _location_list_event_box (get_event_box ("location_list_event_box"))
    , _add_midi_marker_template (get_waves_button ("add_midi_marker_template"))
    , _items_deletion_in_progress (false)
    , _marker_template_item_load_in_progress (false)
    , _marker_scroll (get_scrolled_window ("marker_scroll"))
{
    set_title (xml_property (*xml_tree ()->root (), "title", ""));
  	set_resizable(false);
    
    set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
    set_flags (get_flags () | Gtk::CAN_FOCUS);
    
    this->signal_button_press_event ().connect (sigc::mem_fun (*this, &LocationListDialog::on_button_pressed), false);
    _midi_marker_list_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &LocationListDialog::on_midi_marker_list_tab_button_clicked));
    _location_marker_list_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &LocationListDialog::on_location_marker_list_tab_button_clicked));
    
    _add_midi_marker_template.signal_clicked.connect (sigc::mem_fun (*this, &LocationListDialog::on_add_midi_marker_template_button_clicked));

    _midi_marker_list_tab_button.set_active_state (Gtkmm2ext::ExplicitActive);
    _location_marker_list_tab_button.set_active_state (Gtkmm2ext::Off);
    
    hide_tab (_location_list_event_box);
    show_tab (_marker_list_event_box);

    MarkerListItem::ClearSelection.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::clear_selection_for_marker_items, this), gui_context ());
    MarkerListItem::DeleteAllSelectedMarkerItems.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::delete_all_selected_marker_items, this), gui_context ());
    MarkerListItem::ExtendItemSelection.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::find_extend_range_and_extend, this, _1), gui_context ());
    MarkerListItem::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::remove_item, this, _1), gui_context ());
    MarkerListItem::EditNext.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::begin_edit_next_marker_list_item, this, _1), gui_context ());
    
    MarkerTemplateItem::ClearSelection.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::clear_selection_for_marker_items, this), gui_context ());
    MarkerTemplateItem::ExtendItemSelection.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::find_extend_range_and_extend, this, _1), gui_context ());
    MarkerTemplateItem::DeleteAllSelectedMarkerItems.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::delete_all_selected_marker_items, this), gui_context ());
    MarkerTemplateItem::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::remove_marker_template_item, this, _1), gui_context ());
    MarkerTemplateItem::PropertyChanged.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::update_midi_marker_template_in_xml, this, _1), gui_context ());
    MarkerTemplateItem::EditNext.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::begin_edit_next_marker_list_item, this, _1), gui_context ());
    
    LocationListSkipMarker::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::remove_location_item, this, _1), gui_context ());
    LocationListSelection::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::remove_location_item, this, _1), gui_context ());
    TimeAxisView::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::timeaxisview_deleted, this, _1), gui_context ());
    
    LocationListSelection::NameChanged.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::on_selection_name_changed, this, _1), gui_context ());
    
    LocationListItem::ExtendItemSelection.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::extend_selection_for_ranges, this, _1), gui_context ());
    LocationListItem::DeleteAllSelectedRanges.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::delete_selected_ranges, this, _1), gui_context ());
    LocationListItem::ClearSelection.connect (*this, invalidator (*this), boost::bind (&LocationListDialog::clear_selection_for_ranges, this, _1), gui_context ());
}

LocationListDialog::~LocationListDialog ()
{
}

void
LocationListDialog::end_all_editing ()
{
    for (std::list<MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
		(*i)->end_all_editing ();
	}
    
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
		(*i)->end_all_editing ();
	}
    
    for (std::list <MarkerTemplateItem*>::iterator i = _marker_templates.begin (); i != _marker_templates.end (); ++i) {
		(*i)->end_all_editing ();
	}
}

gboolean
LocationListDialog::on_button_pressed (GdkEventButton* ev)
{
    end_all_editing ();
    
    return false;
}

void 
LocationListDialog::on_realize ()
{
	Gtk::Window::on_realize ();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_TITLE|Gdk::DECOR_MENU));
}

void
LocationListDialog::on_hide ()
{
    deiconify ();
    Gtk::Window::on_hide ();
}

void
LocationListDialog::show_tab (Gtk::Widget& widget)
{
    if (!widget.get_parent ()) {
		_tabs_home.add (widget);
		widget.show_all ();
	}
}

void
LocationListDialog::hide_tab (Gtk::Widget& widget)
{
    if (widget.get_parent () == &_tabs_home) {
		_tabs_home.remove (widget);
	}
}

void
LocationListDialog::on_midi_marker_list_tab_button_clicked (WavesButton*)
{
    _midi_marker_list_tab_button.set_active_state (Gtkmm2ext::ExplicitActive);
    _location_marker_list_tab_button.set_active_state (Gtkmm2ext::Off);
    
    hide_tab (_location_list_event_box);
    show_tab (_marker_list_event_box);
    for (std::list <MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
        (*i)->display_marker_data ();
    }
}

void
LocationListDialog::on_location_marker_list_tab_button_clicked (WavesButton*)
{
    _midi_marker_list_tab_button.set_active_state (Gtkmm2ext::Off);
    _location_marker_list_tab_button.set_active_state (Gtkmm2ext::ExplicitActive);

    hide_tab (_marker_list_event_box);
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
        _location_list_home.remove (*(*i));
    }
    show_tab (_location_list_event_box);
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
        _location_list_home.pack_start (*(*i), false, false);
        (*i)->show ();
    }
}

void
LocationListDialog::set_clock_mode (AudioClock::Mode mode)
{
    for (std::list <MarkerListItem*>::iterator i = _marker_locations.begin (); i != _marker_locations.end (); ++i) {
        (*i)->set_clock_mode (mode);
    }
    
    for (std::list <LocationListItem*>::iterator i = _location_items.begin (); i != _location_items.end (); ++i) {
        (*i)->set_clock_mode (mode);
    }
}

void
LocationListDialog::load_gui_objects ()
{
    load_selection_list ();
    load_marker_template_list ();
}

bool
LocationListDialog::session_is_recording ()
{
    return ARDOUR_UI::instance ()->the_session ()->actively_recording ();
}