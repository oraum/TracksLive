/*
    Copyright (C) 2000 Paul Davis

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

/* This file contains any ARDOUR_UI methods that require knowledge of
   the various dialog boxes, and exists so that no compilation dependency
   exists between the main ARDOUR_UI modules and their respective classes.
   This is to cut down on the compile times.  It also helps with my sanity.
*/

#include "ardour/audioengine.h"
#include "ardour/automation_watch.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "control_protocol/control_protocol.h"

#include "actions.h"
#include "add_route_dialog.h"
#include "add_video_dialog.h"
#include "ardour_ui.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "main_clock.h"
#include "meter_patterns.h"
#include "midi_tracer.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "shuttle_control.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "splash.h"
#include "theme_manager.h"
#include "time_info_box.h"

#include <gtkmm2ext/keyboard.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;

void
ARDOUR_UI::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		WM::Manager::instance().set_session (s);
		/* Session option editor cannot exist across change-of-session */
		session_option_editor.drop_window ();
		/* Ditto for AddVideoDialog */
		add_video_dialog.drop_window ();
		return;
	}

	const XMLNode* node = _session->extra_xml (X_("UI"));

	if (node) {
		const XMLNodeList& children = node->children();
		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((*i)->name() == GUIObjectState::xml_node_name) {
				gui_object_state->load (**i);
				break;
			}
		}
	}

	WM::Manager::instance().set_session (s);

	AutomationWatch::instance().set_session (s);

	if (shuttle_box) {
		shuttle_box->set_session (s);
	}

	primary_clock->set_session (s);
	secondary_clock->set_session (s);
	big_clock->set_session (s);
	time_info_box->set_session (s);
	video_timeline->set_session (s);

	/* sensitize menu bar options that are now valid */

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, true);
	ActionManager::set_sensitive (ActionManager::write_sensitive_actions, _session->writable());

	if (_session->locations()->num_range_markers()) {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
	} else {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
	}

	if (!_session->monitor_out()) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), X_("SoloViaBus"));
		if (act) {
			act->set_sensitive (false);
		}
	}

	/* allow wastebasket flush again */

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("FlushWastebasket"));
	if (act) {
		act->set_sensitive (true);
	}

	/* there are never any selections on startup */

	ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::line_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::point_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::playlist_selection_sensitive_actions, false);
    ActionManager::set_sensitive (ActionManager::region_list_selection_sensitive_actions, false);
    ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
    ActionManager::set_sensitive (ActionManager::region_exists_sensitive_actions, false);
    ActionManager::set_sensitive (ActionManager::region_or_time_selection_sensitive_actions, false);

	editor->get_waves_button ("transport_record_button").set_sensitive (true);

	setup_session_options ();

	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::transport_rec_enable_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::solo_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::sync_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::audition_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::feedback_blink));

    _session->SessionSaveUnderway.connect_same_thread (_session_connections, boost::bind (&ARDOUR_UI::save_session_gui_state, this));
	_session->SaveSessionRequested.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::save_session_at_its_request, this, _1), gui_context());
	_session->RecordStateChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::record_state_changed, this), gui_context());
    _session->RecordArmStateChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::record_state_changed, this), gui_context());
    _session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::transport_state_changed, this), gui_context());
	_session->StepEditStatusChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::map_transport_state, this), gui_context());
	_session->DirtyChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_autosave, this), gui_context());

    _session->MTCSyncStateChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::set_mtc_indicator_active, this, _1), gui_context());
    
    _session->LTCSyncStateChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::set_ltc_indicator_active, this, _1), gui_context());
    
	_session->Xrun.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::xrun_handler, this, _1), gui_context());
	_session->SoloActive.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::soloing_changed, this, _1), gui_context());
	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::auditioning_changed, this, _1), gui_context());
	_session->locations()->added.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::handle_locations_change, this, _1), gui_context());
	_session->locations()->removed.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::handle_locations_change, this, _1), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_parameter_changed, this, _1), gui_context ());
    
    _session->MtcOrLtcInputPortChanged.connect(_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_timecode_source_dropdown_items, this), gui_context ());
    
    /* Clocks are on by default after we are connected to a session, so show that here.
	*/

	connect_dependents_to_session (s);

	/* listen to clock mode changes. don't do this earlier because otherwise as the clocks
	   restore their modes or are explicitly set, we will cause the "new" mode to be saved
	   back to the session XML ("Extra") state.
	 */

	AudioClock::ModeChanged.connect (sigc::mem_fun (*this, &ARDOUR_UI::store_clock_modes));

	Glib::signal_idle().connect (sigc::mem_fun (*this, &ARDOUR_UI::first_idle));

	start_clocking ();
	start_blinking ();

	map_transport_state ();

	second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_second), 1000);
	point_one_second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_one_seconds), 100);
	point_zero_something_second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_zero_something_seconds), 40);
	set_fps_timeout_connection();

	if (_session && 
	    _session->master_out() && 
	    _session->master_out()->n_outputs().n(DataType::AUDIO) > 0) {

        ArdourMeter::ResetAllPeakDisplays.connect_same_thread (global_meter_connections, boost::bind(&ARDOUR_UI::reset_peak_display, this));
		ArdourMeter::ResetRoutePeakDisplays.connect_same_thread (global_meter_connections, boost::bind(&ARDOUR_UI::reset_route_peak_display, this, _1));
		ArdourMeter::ResetGroupPeakDisplays.connect_same_thread (global_meter_connections, boost::bind(&ARDOUR_UI::reset_group_peak_display, this, _1));
	}
    
    update_bit_depth_button ();
    update_sample_rate_dropdown ();
    update_frame_rate_button ();
    
    populate_display_format_dropdown ();
    populate_timecode_source_dropdown ();

    _location_list_dialog->load_gui_objects ();
}

int
ARDOUR_UI::unload_session (bool hide_stuff)
{
	if (_session) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();
	}

	if (_session && _session->dirty()) {
		std::vector<std::string> actions;
		actions.push_back (_("Don't close"));
		actions.push_back (_("Just close"));
		actions.push_back (_("Save and close"));
		switch (ask_about_saving_session (actions)) {
		case -1:
			// cancel
			return 1;

		case 1:
			_session->save_state ("");
			break;
		}
	}

	{
		// tear down session specific CPI (owned by rc_config_editor which can remain)
		ControlProtocolManager& m = ControlProtocolManager::instance ();
		for (std::list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {
			if (*i && (*i)->protocol && (*i)->protocol->has_editor ()) {
				(*i)->protocol->tear_down_gui ();
			}
		}
	}

	if (hide_stuff) {
		editor->hide ();
		theme_manager->hide ();
//		audio_port_matrix->hide();
//		midi_port_matrix->hide();
		route_params->hide();
	}

	second_connection.disconnect ();
	point_one_second_connection.disconnect ();
	point_zero_something_second_connection.disconnect();
        connection_with_session_config.disconnect();
	fps_connection.disconnect();

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, false);

	editor->get_waves_button ("transport_record_button").set_sensitive (false);

	WM::Manager::instance().set_session ((ARDOUR::Session*) 0);

	if (ARDOUR_UI::instance()->video_timeline) {
		ARDOUR_UI::instance()->video_timeline->close_session();
	}

	stop_blinking ();
	stop_clocking ();

	/* drop everything attached to the blink signal */

	Blink.clear ();

	delete _session;
	_session = 0;

	session_loaded = false;
    
    _location_list_dialog->clear_gui_objects ();

	return 0;
}

static bool
_hide_splash (gpointer arg)
{
	((ARDOUR_UI*)arg)->hide_splash();
	return false;
}

void
ARDOUR_UI::minimize_window ()
{
    if (editor)
        editor->iconify();
}

void
ARDOUR_UI::maximize_window ()
{
    if (editor)
        editor->maximize();
}


void
ARDOUR_UI::goto_editor_window ()
{
	if (splash && splash->is_visible()) {
		// in 2 seconds, hide the splash screen
		Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (_hide_splash), this), 2000);
	}

	editor->show_window ();
    show_meterbridge_view ();
    
	editor->present ();
	/* mixer should now be on top */
	WM::Manager::instance().set_transient_for (editor);
	_mixer_on_top = false;

    /* it is neccessary to update recent session menuitems */
    /* because new session could be created                */
    ARDOUR_UI::instance()->update_recent_session_menuitems();
}

void
ARDOUR_UI::toggle_mixer_bridge_view ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("toggle-mixer"));
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);

	act = ActionManager::get_action (X_("Common"), X_("toggle-meterbridge"));
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> meter_tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	if (tact->get_active()) {
		meter_tact->set_active (false);
		editor->get_container ("mixer_bridge_view_home").show ();
	} else {
		editor->get_container ("mixer_bridge_view_home").hide ();
	}
    
    set_session_dirty ();
}

void
ARDOUR_UI::toggle_meterbridge ()
{
    show_meterbridge_view ();
    set_session_dirty ();
}

void
ARDOUR_UI::show_meterbridge_view ()
{
    Glib::RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("toggle-meterbridge"));
	if (!act) {
		return;
	}
    
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
    
	Glib::RefPtr<Action> mixer_act = ActionManager::get_action (X_("Common"), X_("toggle-mixer"));
	if (!mixer_act) {
		return;
	}
    
	Glib::RefPtr<ToggleAction> mixer_tact = Glib::RefPtr<ToggleAction>::cast_dynamic (mixer_act);
	if (tact->get_active()) {
		mixer_tact->set_active(false);
		editor->get_container ("edit_pane").hide ();
        editor->get_container ("compact_meter_bridge_home").hide ();
		editor->get_container ("meter_bridge_view_home").show ();
    } else {
		editor->get_container ("meter_bridge_view_home").hide ();
		editor->get_container ("edit_pane").show ();
		editor->get_container ("compact_meter_bridge_home").show ();
    }
}

void
ARDOUR_UI::new_midi_tracer_window ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("NewMIDITracer"));
	if (!act) {
		return;
	}

	std::list<MidiTracer*>::iterator i = _midi_tracer_windows.begin ();
	while (i != _midi_tracer_windows.end() && (*i)->get_visible() == true) {
		++i;
	}

	if (i == _midi_tracer_windows.end()) {
		/* all our MIDITracer windows are visible; make a new one */
		MidiTracer* t = new MidiTracer ();
		t->show_all ();
		_midi_tracer_windows.push_back (t);
	} else {
		/* re-use the hidden one */
		(*i)->show_all ();
	}
}

BundleManager*
ARDOUR_UI::create_bundle_manager ()
{
	return new BundleManager (_session);
}

AddVideoDialog*
ARDOUR_UI::create_add_video_dialog ()
{
	return new AddVideoDialog (_session);
}

SessionOptionEditor*
ARDOUR_UI::create_session_option_editor ()
{
	return new SessionOptionEditor (_session);
}

BigClockWindow*
ARDOUR_UI::create_big_clock_window ()
{
	return new BigClockWindow (*big_clock);
}

void
ARDOUR_UI::handle_locations_change (Location *)
{
	if (_session) {
		if (_session->locations()->num_range_markers()) {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
		} else {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
		}
	}
}

bool
ARDOUR_UI::main_window_state_event_handler (GdkEventWindowState* ev, bool window_was_editor)
{
	if (window_was_editor) {

		if ((ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
		    (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
			if (big_clock_window) {
				big_clock_window->set_transient_for (*editor);
			}
		}

	}
	return false;
}

bool
ARDOUR_UI::editor_meter_peak_button_release (GdkEventButton* ev)
{
	if (ev->button == 1 && Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier|Gtkmm2ext::Keyboard::TertiaryModifier)) {
		ArdourMeter::ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
		if (_session->master_out()) {
			ArdourMeter::ResetGroupPeakDisplays (_session->master_out()->route_group());
		}
	} else if (_session->master_out()) {
		ArdourMeter::ResetRoutePeakDisplays (_session->master_out().get());
	}
	return true;
}

