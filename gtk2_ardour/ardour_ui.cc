/*
    Copyright (C) 1999-2013 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cerrno>
#include <fstream>

#ifndef PLATFORM_WINDOWS
#include <sys/resource.h>
#endif

#include <stdint.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <boost/assign/list_of.hpp>

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/fileutils.h>

#include <gtkmm/messagedialog.h>
#include <gtkmm/accelmap.h>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/openuri.h"
#include "pbd/stl_delete.h"
#include "pbd/file_utils.h"
#include "pbd/localtime_r.h"
#include "pbd/system_exec.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/waves_fastmeter.h"
#include "gtkmm2ext/popup.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/ardour.h"
#include "ardour/audio_backend.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/automation_watch.h"
#include "ardour/diskstream.h"
#include "ardour/engine_state_controller.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port.h"
#include "ardour/plugin_manager.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/recent_sessions.h"
#include "ardour/session_directory.h"
#include "ardour/session_route.h"
#include "ardour/session_state_utils.h"
#include "ardour/session_utils.h"
#include "ardour/slave.h"
#include "ardour/system_exec.h"
#include "ardour/directory_names.h"
#include "ardour/filename_extensions.h"
#include "waves_prompter.h"
#include "dbg_msg.h"

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif

#include "timecode/time.h"

typedef uint64_t microseconds_t;

#include "about_dialog.h"
#include "actions.h"
#include "add_tracks_dialog.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "gain_meter.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "main_clock.h"
#include "waves_missing_file_dialog.h"
#include "missing_plugin_dialog.h"
#include "mixer_ui.h"
#include "mouse_cursors.h"
#include "nsm.h"
#include "opts.h"
#include "pingback.h"
#include "processor_box.h"
#include "prompter.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "route_time_axis.h"
#include "route_params_ui.h"
#include "session_metadata_dialog.h"
#include "session_option_editor.h"
#include "shuttle_control.h"
#include "speaker_dialog.h"
#include "splash.h"
#include "theme_manager.h"
#include "time_axis_view_item.h"
#include "mixer_bridge_view.h"
#include "utils.h"
#include "video_server_dialog.h"
#include "add_video_dialog.h"
#include "transcode_video_dialog.h"

#include "i18n.h"

#include "open_file_dialog.h"
#include "waves_message_dialog.h"
#include "waves_ambiguous_file_dialog.h"
#include "crash_recovery_dialog.h"
#include "selection.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

ARDOUR_UI *ARDOUR_UI::theArdourUI = 0;
UIConfiguration *ARDOUR_UI::ui_config = 0;

sigc::signal<void,bool> ARDOUR_UI::Blink;
sigc::signal<void>      ARDOUR_UI::RapidScreenUpdate;
sigc::signal<void>      ARDOUR_UI::SuperRapidScreenUpdate;
sigc::signal<void>      ARDOUR_UI::FPSUpdate;
sigc::signal<void, framepos_t, bool, framepos_t> ARDOUR_UI::Clock;
sigc::signal<void>      ARDOUR_UI::CloseAllDialogs;

ARDOUR_UI::ARDOUR_UI (int *argcp, char **argvp[], const char* localedir)

    : Gtkmm2ext::UI (PROGRAM_NAME, argcp, argvp)
	, gui_object_state (new GUIObjectState)
	, primary_clock (new MainClock (X_("primary"), false, X_("transport"), true, true, true, false, false))
	, secondary_clock (new MainClock (X_("secondary"), false, X_("secondary"), true, true, false, false, true))
	  /* big clock */
	, big_clock (new AudioClock (X_("bigclock"), false, "big", true, true, false, false))
    , _ignore_changes (0)
	, video_timeline(0)
	  /* start of private members */
	, nsm (0)
	, _was_dirty (false)
	, _mixer_on_top (false)
	, first_time_engine_run (true)
    , session_dialog_was_hidden (false)
    , program_starting (true)
	, blink_timeout_tag (-1)
	  /* transport */
	, roll_controllable (new TransportControllable ("transport roll", *this, TransportControllable::Roll))
	, stop_controllable (new TransportControllable ("transport stop", *this, TransportControllable::Stop))
	, goto_start_controllable (new TransportControllable ("transport goto start", *this, TransportControllable::GotoStart))
	, goto_end_controllable (new TransportControllable ("transport goto end", *this, TransportControllable::GotoEnd))
	, auto_loop_controllable (new TransportControllable ("transport auto loop", *this, TransportControllable::AutoLoop))
	, play_selection_controllable (new TransportControllable ("transport play selection", *this, TransportControllable::PlaySelection))
	, rec_controllable (new TransportControllable ("transport rec-enable", *this, TransportControllable::RecordEnable))
	, speaker_config_window (X_("speaker-config"), _("Speaker Configuration"))
	, theme_manager (X_("theme-manager"), _("Theme Manager"))
	, key_editor (X_("key-editor"), _("Key Command Editor"))
	, rc_option_editor (X_("rc-options-editor"), _("Preferences"))
	, about (X_("about"), _("About"))
	, location_ui (X_("locations"), _("Locations"))
	, route_params (X_("inspector"), _("Tracks and Busses"))
	, tracks_control_panel (X_("tracks-control-panel"), _("Preferences"))
    , _session_dialog (tracks_control_panel, false, "", "", "", false)
    , _add_tracks_dialog(new AddTracksDialog())
	, track_color_dialog (X_("track_color-dialog"), _("Track Color Dialog"))
    , _location_list_dialog (X_("location-list-dialog"), _("Memory Locations List") )
    , session_option_editor (X_("session-options-editor"), _("Properties"), boost::bind (&ARDOUR_UI::create_session_option_editor, this))
	, add_video_dialog (X_("add-video"), _("Add Tracks/Busses"), boost::bind (&ARDOUR_UI::create_add_video_dialog, this))
	, bundle_manager (X_("bundle-manager"), _("Bundle Manager"), boost::bind (&ARDOUR_UI::create_bundle_manager, this))
	, big_clock_window (X_("big-clock"), _("Big Clock"), boost::bind (&ARDOUR_UI::create_big_clock_window, this))
	, _audio_engine_reset_info_dialog (0) // HOT FIX. (REWORK IT)
    , _audio_engine_reset_menu_disabler (0) // HOT FIX. (REWORK IT)
//	, audio_port_matrix (X_("audio-connection-manager"), _("Audio Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::AUDIO))
//	, midi_port_matrix (X_("midi-connection-manager"), _("MIDI Connections"), boost::bind (&ARDOUR_UI::create_global_port_matrix, this, ARDOUR::DataType::MIDI))
	, _feedback_exists (false)
    , _dsp_load_adjustment (0)
    , _hd_load_adjustment (0)
    , _dsp_load_label (0)
    , _hd_load_label (0)
    , _hd_remained_time_label (0)
	, editor (0)
    , _tracks_button (0)
    , _bit_depth_button (0)
    , _frame_rate_button (0)
    , _sample_rate_dropdown (0)
    , splash (0)
{
    // method Application::instance ()->ready ()
    // must be called before show_splash
    // otherwise signal application:openFile from
    // OS X will not be received
    Application::instance ()->ShouldLoad.connect (sigc::mem_fun (*this, &ARDOUR_UI::load_from_application_api));
    Application::instance ()->ready ();
    
    show_splash ();
    
   	Gtkmm2ext::init(localedir);

	_numpad_locate_happening = false;

	if (theArdourUI == 0) {
		theArdourUI = this;
	}
    
	ui_config = new UIConfiguration();

    ui_config->ParameterChanged.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
    
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	ui_config->map_parameters (pc);

	_session_is_new = false;
	session_selector_window = 0;
	last_key_press_time = 0;
	video_server_process = 0;
	open_session_selector = 0;
	have_configure_timeout = false;
	have_disk_speed_dialog_displayed = false;
	session_loaded = false;
	ignore_dual_punch = false;

	last_configure_time= 0;
	last_peak_grab = 0;

	ARDOUR::Diskstream::DiskOverrun.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::disk_overrun_handler, this), gui_context());
	ARDOUR::Diskstream::DiskUnderrun.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::disk_underrun_handler, this), gui_context());

	ARDOUR::Session::VersionMismatch.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_format_mismatch, this, _1, _2), gui_context());

	/* handle dialog requests */

	ARDOUR::Session::Dialog.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_dialog, this, _1), gui_context());

	/* handle pending state with a dialog (PROBLEM: needs to return a value and thus cannot be x-thread) */

	ARDOUR::Session::AskAboutPendingState.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::pending_state_dialog, this));

	/* handle Audio/MIDI setup when session requires it */

	ARDOUR::Session::AudioEngineSetupRequired.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::do_engine_setup, this, _1));

	/* handle sr mismatch with a dialog (PROBLEM: needs to return a value and thus cannot be x-thread) */

	ARDOUR::Session::AskAboutSampleRateMismatch.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::sr_mismatch_dialog, this, _1, _2));

	/* handle requests to quit (coming from JACK session) */

	ARDOUR::Session::Quit.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::finish, this), gui_context ());

	/* tell the user about feedback */

	ARDOUR::Session::FeedbackDetected.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::feedback_detected, this), gui_context ());
	ARDOUR::Session::SuccessfulGraphSort.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::successful_graph_sort, this), gui_context ());

	/* handle requests to deal with missing files */

	ARDOUR::Session::MissingFile.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::missing_file, this, _1, _2, _3));

	/* and ambiguous files */

	ARDOUR::FileSource::AmbiguousFileName.connect_same_thread (forever_connections, boost::bind (&ARDOUR_UI::ambiguous_file, this, _1, _2));

	/* also plugin scan messages */
	ARDOUR::PluginScanMessage.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::plugin_scan_dialog, this, _1, _2, _3), gui_context());
	ARDOUR::PluginScanTimeout.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::plugin_scan_timeout, this, _1), gui_context());

	ARDOUR::GUIIdle.connect (forever_connections, MISSING_INVALIDATOR, boost::bind(&ARDOUR_UI::gui_idle_handler, this), gui_context());

    // initialize engine state controller
    EngineStateController::instance();
    
    EngineStateController::instance()->SampleRateChanged.connect (update_connections_to_toolbar_buttons, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate_dropdown, this), gui_context());
    EngineStateController::instance()->EngineRunning.connect (update_connections_to_toolbar_buttons, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_sample_rate_dropdown, this), gui_context());
    
	/* lets get this party started */

	setup_gtk_ardour_enums ();
	setup_profile ();

	SessionEvent::create_per_thread_pool ("GUI", 4096);

	/* we like keyboards */

	keyboard = new ArdourKeyboard(*this);

	XMLNode* node = ARDOUR_UI::instance()->keyboard_settings();
	if (node) {
		keyboard->set_state (*node, Stateful::loading_state_version);
	}

	/* we don't like certain modifiers */
	Bindings::set_ignored_state (GDK_LOCK_MASK|GDK_MOD2_MASK|GDK_MOD3_MASK);

	reset_dpi();

	TimeAxisViewItem::set_constant_heights ();

        /* Set this up so that our window proxies can register actions */

	ActionManager::init ();

	/* The following must happen after ARDOUR::init() so that Config is set up */

	const XMLNode* ui_xml = Config->extra_xml (X_("UI"));

	if (ui_xml) {
		theme_manager.set_state (*ui_xml);
		key_editor.set_state (*ui_xml);
		rc_option_editor.set_state (*ui_xml);
		session_option_editor.set_state (*ui_xml);
		speaker_config_window.set_state (*ui_xml);
		about.set_state (*ui_xml);
		add_video_dialog.set_state (*ui_xml);
		route_params.set_state (*ui_xml);
		bundle_manager.set_state (*ui_xml);
		location_ui.set_state (*ui_xml);
		big_clock_window.set_state (*ui_xml);
//		audio_port_matrix.set_state (*ui_xml);
//		midi_port_matrix.set_state (*ui_xml);
	}

	WM::Manager::instance().register_window (&theme_manager);
	WM::Manager::instance().register_window (&key_editor);
	WM::Manager::instance().register_window (&rc_option_editor);
	WM::Manager::instance().register_window (&session_option_editor);
	WM::Manager::instance().register_window (&speaker_config_window);
	WM::Manager::instance().register_window (&about);
	WM::Manager::instance().register_window (&add_video_dialog);
	WM::Manager::instance().register_window (&route_params);
	WM::Manager::instance().register_window (&tracks_control_panel);
	WM::Manager::instance().register_window (&track_color_dialog);
    WM::Manager::instance().register_window (&_location_list_dialog);
	WM::Manager::instance().register_window (&bundle_manager);
	WM::Manager::instance().register_window (&location_ui);
	WM::Manager::instance().register_window (&big_clock_window);
//	WM::Manager::instance().register_window (&audio_port_matrix);
//	WM::Manager::instance().register_window (&midi_port_matrix);
    
    
	/* We need to instantiate the theme manager because it loads our
	   theme files. This should really change so that its window
	   and its functionality are separate 
	*/
	
	(void) theme_manager.get (true);
	
	_process_thread = new ProcessThread ();
	_process_thread->init ();

    // start the engine:
    // attach to the engine signals
	attach_to_engine ();
    
    // start the engine pushing state from the state controller
    EngineStateController::instance()->push_current_state_to_backend(true);
    
    _session_dialog.set_engine_state_controller (EngineStateController::instance());
}

GlobalPortMatrixWindow*
ARDOUR_UI::create_global_port_matrix (ARDOUR::DataType type)
{
	if (!_session) {
		return 0;
	}
	return new GlobalPortMatrixWindow (_session, type);
}

void
ARDOUR_UI::attach_to_engine ()
{
	AudioEngine::instance()->Running.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_running, this), gui_context());
	AudioEngine::instance()->DeviceResetStarted.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::device_reset_started, this), gui_context());
	AudioEngine::instance()->DeviceResetFinished.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::device_reset_finished, this), gui_context());
	ARDOUR::Port::set_connecting_blocked (ARDOUR_COMMAND_LINE::no_connect_ports);
}

void
ARDOUR_UI::engine_stopped ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::engine_stopped)
	ActionManager::set_sensitive (ActionManager::engine_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::engine_opposite_sensitive_actions, true);
}

void
ARDOUR_UI::engine_running ()
{
	if (first_time_engine_run) {
		post_engine();
		first_time_engine_run = false;
	} 
	
	update_disk_space ();
    update_disk_usage ();
	update_cpu_load ();
    populate_sample_rate_dropdown ();
}

void
ARDOUR_UI::device_reset_started ()
{
	if (!_audio_engine_reset_menu_disabler) {
		_audio_engine_reset_menu_disabler = new ActionDisabler ();
	}
	if (!_audio_engine_reset_info_dialog) {
		_audio_engine_reset_info_dialog = new WavesDialog ("audio_engine_reset_info_dialog.xml"); // HOT FIX. (REWORK IT)
	}
	_audio_engine_reset_info_dialog->set_keep_above (true);
    _audio_engine_reset_info_dialog->show ();
}

void
ARDOUR_UI::device_reset_finished ()
{
	if (_audio_engine_reset_menu_disabler) {
		delete _audio_engine_reset_menu_disabler;
		_audio_engine_reset_menu_disabler = 0;
	}
	
	if (_audio_engine_reset_info_dialog) {
		_audio_engine_reset_info_dialog->hide ();
		delete _audio_engine_reset_info_dialog;
		_audio_engine_reset_info_dialog = 0;
	}
}

void
ARDOUR_UI::engine_halted (const char* reason, bool free_reason)
{
	if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {
		/* we can't rely on the original string continuing to exist when we are called
		   again in the GUI thread, so make a copy and note that we need to
		   free it later.
		*/
		char *copy = strdup (reason);
		Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&ARDOUR_UI::engine_halted, this, copy, true));
		return;
	}

	ActionManager::set_sensitive (ActionManager::engine_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::engine_opposite_sensitive_actions, true);

	string msgstr;

	/* if the reason is a non-empty string, it means that the backend was shutdown
	   rather than just Ardour.
	*/

	if (strlen (reason)) {
		msgstr = string_compose (_("The audio backend was shutdown because:\n\n%1"), reason);
	} else {
		msgstr = string_compose (_("\
The audio backend has either been shutdown or it\n\
disconnected %1 because %1\n\
was not fast enough. Try to restart\n\
the audio backend and save the session."), PROGRAM_NAME);
	}

    WavesMessageDialog msg ("", msgstr);
	pop_back_splash (msg);
    msg.run ();
	
	if (free_reason) {
		free (const_cast<char*> (reason));
	}
}

void
ARDOUR_UI::post_engine ()
{
	/* Things to be done once (and once ONLY) after we have a backend running in the AudioEngine
	 */

	ARDOUR::init_post_engine ();
	
	/* connect to important signals */

	AudioEngine::instance()->Stopped.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::engine_stopped, this), gui_context());
	AudioEngine::instance()->Halted.connect_same_thread (halt_connection, boost::bind (&ARDOUR_UI::engine_halted, this, _1, false));

	_tooltips.enable();

	if (setup_windows ()) {
		throw failed_constructor ();
	}
	
	check_memory_locking();

	/* this is the first point at which all the keybindings are available */

	if (ARDOUR_COMMAND_LINE::show_key_actions) {
		vector<string> names;
		vector<string> paths;
		vector<string> tooltips;
		vector<string> keys;
		vector<AccelKey> bindings;

		ActionManager::get_all_actions (names, paths, tooltips, keys, bindings);

		vector<string>::iterator n;
		vector<string>::iterator k;
		vector<string>::iterator p;
		for (n = names.begin(), k = keys.begin(), p = paths.begin(); n != names.end(); ++n, ++k, ++p) {
			cout << "Action: '" << (*n) << "' bound to '" << (*k) << "' Path: '" << (*p) << "'" << endl;
		}

		halt_connection.disconnect ();
		AudioEngine::instance()->stop ();
		exit (0);
	}

	blink_timeout_tag = -1;

	/* this being a GUI and all, we want peakfiles */

	AudioFileSource::set_build_peakfiles (true);
	AudioFileSource::set_build_missing_peakfiles (true);

	/* set default clock modes */

	if (Profile->get_sae()) {
		primary_clock->set_mode (AudioClock::BBT);
		secondary_clock->set_mode (AudioClock::MinSec);
	}  else {
		primary_clock->set_mode (AudioClock::Timecode);
		secondary_clock->set_mode (AudioClock::BBT);
	}

	Config->ParameterChanged.connect (forever_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::parameter_changed, this, _1), gui_context());
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	Config->map_parameters (pc);
}

void
ARDOUR_UI::update_output_operation_mode_buttons()
{
    // muti out
    WavesButton& multi_out_button = editor->get_waves_button ("mode_multi_out_button");
    multi_out_button.set_active(Config->get_output_auto_connect() & AutoConnectPhysical);
    
	// stereo out
    WavesButton& stereo_out_button = editor->get_waves_button ("mode_stereo_out_button");
    stereo_out_button.set_active(Config->get_output_auto_connect() & AutoConnectMaster);
}


ARDOUR_UI::~ARDOUR_UI ()
{
    EngineStateController::instance()->save_audio_midi_settings();

	if (ui_config->dirty()) {
		ui_config->save_state();
	}

	stop_video_server();

	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother at 'real' exit. the OS cleans up for us.
		delete big_clock;
		delete primary_clock;
		delete secondary_clock;
		delete _process_thread;
		delete gui_object_state;
	}
}

void
ARDOUR_UI::pop_back_splash (Gtk::Window& win)
{
	if (Splash::instance()) {
		Splash::instance()->pop_back_for (win);
	}
}

gint
ARDOUR_UI::configure_timeout ()
{
	if (last_configure_time == 0) {
		/* no configure events yet */
		return true;
	}

	/* force a gap of 0.5 seconds since the last configure event
	 */

	if (get_microseconds() - last_configure_time < 500000) {
		return true;
	} else {
		have_configure_timeout = false;
		save_application_state ();
		return false;
	}
}

gboolean
ARDOUR_UI::configure_handler (GdkEventConfigure* /*conf*/)
{
	if (have_configure_timeout) {
		last_configure_time = get_microseconds();
	} else {
		Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::configure_timeout), 100);
		have_configure_timeout = true;
	}

	return FALSE;
}

void
ARDOUR_UI::set_transport_controllable_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("roll")) != 0) {
		roll_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("stop")) != 0) {
		stop_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("goto-start")) != 0) {
		goto_start_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("goto-end")) != 0) {
		goto_end_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("auto-loop")) != 0) {
		auto_loop_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("play-selection")) != 0) {
		play_selection_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("rec")) != 0) {
		rec_controllable->set_id (prop->value());
	}
	if ((prop = node.property ("shuttle")) != 0) {
		shuttle_box->controllable()->set_id (prop->value());
	}
}

XMLNode&
ARDOUR_UI::get_transport_controllable_state ()
{
	XMLNode* node = new XMLNode(X_("TransportControllables"));
	char buf[64];

	roll_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("roll"), buf);
	stop_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("stop"), buf);
	goto_start_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("goto_start"), buf);
	goto_end_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("goto_end"), buf);
	auto_loop_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("auto_loop"), buf);
	play_selection_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("play_selection"), buf);
	rec_controllable->id().print (buf, sizeof (buf));
	node->add_property (X_("rec"), buf);
	shuttle_box->controllable()->id().print (buf, sizeof (buf));
	node->add_property (X_("shuttle"), buf);

	return *node;
}

void
ARDOUR_UI::save_session_at_its_request (std::string snapshot_name)
{
	if (_session) {
		_session->save_state (snapshot_name);
	}
}

gint
ARDOUR_UI::autosave_session ()
{
	if (g_main_depth() > 1) {
		/* inside a recursive main loop,
		   give up because we may not be able to
		   take a lock.
		*/
		return 1;
	}

	if (!Config->get_periodic_safety_backups()) {
		return 1;
	}

	if (_session) {
		_session->maybe_write_autosave();
	}

	return 1;
}

void
ARDOUR_UI::update_autosave ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::update_autosave)

	if (_session && _session->dirty()) {
		if (_autosave_connection.connected()) {
			_autosave_connection.disconnect();
		}

		_autosave_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &ARDOUR_UI::autosave_session),
				Config->get_periodic_safety_backup_interval() * 1000);

	} else {
		if (_autosave_connection.connected()) {
			_autosave_connection.disconnect();
		}
	}
}

void
ARDOUR_UI::check_announcements ()
{
#ifdef PHONE_HOME
	string _annc_filename;

#ifdef __APPLE__
	_annc_filename = PROGRAM_NAME "_announcements_osx_";
#else
	_annc_filename = PROGRAM_NAME "_announcements_linux_";
#endif
	_annc_filename.append (VERSIONSTRING);

	std::string path = Glib::build_filename (user_config_directory(), _annc_filename);
	std::ifstream announce_file (path.c_str());
	if ( announce_file.fail() )
		_announce_string = "";
	else {
		std::stringstream oss;
		oss << announce_file.rdbuf();
		_announce_string = oss.str();
	}

	pingback (VERSIONSTRING, path);
#endif
}

int
ARDOUR_UI::starting ()
{  
	Application* app = Application::instance ();
	const char *nsm_url;
    
	app->ShouldQuit.connect (sigc::mem_fun (*this, &ARDOUR_UI::queue_finish));

	if (!Profile->get_trx() && ARDOUR_COMMAND_LINE::check_announcements) {
		check_announcements ();
	}

	/* we need to create this early because it may need to set the
	 *  audio backend end up.
	 */
	
	try {
		//VKPRefs:audio_midi_setup.get (true);
		tracks_control_panel.get(true);
	} catch (...) {
		std::cerr << "audio-midi engine setup failed."<< std::endl;
		return -1;
	}

	if ((nsm_url = g_getenv ("NSM_URL")) != 0) {
		nsm = new NSM_Client;
		if (!nsm->init (nsm_url)) {
			nsm->announce (PROGRAM_NAME, ":dirty:", "tracks");

			unsigned int i = 0;
			// wait for announce reply from nsm server
			for ( i = 0; i < 5000; ++i) {
				nsm->check ();

				Glib::usleep (i);
				if (nsm->is_active()) {
					break;
				}
			}
			if (i == 5000) {
				error << _("NSM server did not announce itself") << endmsg;
				return -1;
			}
			// wait for open command from nsm server
			for ( i = 0; i < 5000; ++i) {
				nsm->check ();
				Glib::usleep (1000);
				if (nsm->client_id ()) {
					break;
				}
			}

			if (i == 5000) {
				error << _("NSM: no client ID provided") << endmsg;
				return -1;
			}

			if (_session && nsm) {
				_session->set_nsm_state( nsm->is_active() );
			} else {
				error << _("NSM: no session created") << endmsg;
				return -1;
			}

			// nsm requires these actions disabled
			vector<string> action_names;
			action_names.push_back("SaveAs");
			action_names.push_back("Rename");
			action_names.push_back("New");
			action_names.push_back("Open");
			action_names.push_back("Recent");
			action_names.push_back("Close");

			for (vector<string>::const_iterator n = action_names.begin(); n != action_names.end(); ++n) {
				Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), (*n).c_str());
				if (act) {
					act->set_sensitive (false);
				}
			}

		} else {
			delete nsm;
			nsm = 0;
			error << _("NSM: initialization failed") << endmsg;
			return -1;
		}

        goto_editor_window ();
	} else  {
        
        bool brand_new_user = !Glib::file_test ( Glib::build_filename (user_config_directory (), ".a3"), Glib::FILE_TEST_EXISTS);
        const bool new_session_required = (ARDOUR_COMMAND_LINE::new_session || brand_new_user);
        
       // checking to see if any event sources are ready to be processed
       // for example, signal application:openFile from OS X
       Glib::MainContext::get_default()->iteration (false);
        
       /* go get a session */
       if (get_session_parameters (false, new_session_required, ARDOUR_COMMAND_LINE::load_template)) {
           std::cerr << "Cannot get session parameters."<< std::endl;
           return -1;
        }
    }

	use_config ();

	WM::Manager::instance().show_visible ();

	/* We have to do this here since goto_editor_window() ends up calling show_all() on the
	 * editor window, and we may want stuff to be hidden.
	 */

	BootMessage (string_compose (_("%1 is ready for use"), PROGRAM_NAME));
	return 0;
}

void
ARDOUR_UI::check_memory_locking ()
{
#if defined(__APPLE__) || defined(PLATFORM_WINDOWS)
	/* OS X doesn't support mlockall(2), and so testing for memory locking capability there is pointless */
	return;
#else // !__APPLE__

	XMLNode* memory_warning_node = Config->instant_xml (X_("no-memory-warning"));

	if (AudioEngine::instance()->is_realtime() && memory_warning_node == 0) {

		struct rlimit limits;
		int64_t ram;
		long pages, page_size;
#ifdef __FreeBSD__
		size_t pages_len=sizeof(pages);
		if ((page_size = getpagesize()) < 0 ||
				sysctlbyname("hw.availpages", &pages, &pages_len, NULL, 0))
#else
		if ((page_size = sysconf (_SC_PAGESIZE)) < 0 ||(pages = sysconf (_SC_PHYS_PAGES)) < 0)
#endif
		{
			ram = 0;
		} else {
			ram = (int64_t) pages * (int64_t) page_size;
		}

		if (getrlimit (RLIMIT_MEMLOCK, &limits)) {
			return;
		}

		if (limits.rlim_cur != RLIM_INFINITY) {

			if (ram == 0 || ((double) limits.rlim_cur / ram) < 0.75) {

				MessageDialog msg (
					string_compose (
						_("WARNING: Your system has a limit for maximum amount of locked memory. "
						  "This might cause %1 to run out of memory before your system "
						  "runs out of memory. \n\n"
						  "You can view the memory limit with 'ulimit -l', "
						  "and it is normally controlled by %2"),
						PROGRAM_NAME, 
#ifdef __FreeBSD__
						X_("/etc/login.conf")
#else
						X_(" /etc/security/limits.conf")
#endif
					).c_str());

				msg.set_default_response (RESPONSE_OK);

				VBox* vbox = msg.get_vbox();
				HBox hbox;
				CheckButton cb (_("Do not show this window again"));
				hbox.pack_start (cb, true, false);
				vbox->pack_start (hbox);
				cb.show();
				vbox->show();
				hbox.show ();

				pop_back_splash (msg);

				editor->ensure_float (msg);
				msg.run ();

				if (cb.get_active()) {
					XMLNode node (X_("no-memory-warning"));
					Config->add_instant_xml (node);
				}
			}
		}
	}
#endif // !__APPLE__
}


void
ARDOUR_UI::queue_finish ()
{
    if (_session) {
        // do not queue finish if we are actively recording
        if (_session->actively_recording () && _session->have_rec_enabled_track () ) {
            return;
        }
    }
    
	Glib::signal_idle().connect (mem_fun (*this, &ARDOUR_UI::idle_finish));
}

bool
ARDOUR_UI::idle_finish ()
{
	finish ();
	return false; /* do not call again */
}

void
ARDOUR_UI::finish()
{
	if (_session) {
        
        if (_session->actively_recording () && _session->have_rec_enabled_track () ) {
            return;
        }
        
		ARDOUR_UI::instance()->video_timeline->sync_session_state();

		if (_session->dirty()) {
			vector<string> actions;
			actions.push_back (_("Don't quit"));
			actions.push_back (_("Just quit"));
			actions.push_back (_("Save and quit"));
			switch (ask_about_saving_session(actions)) {
			case -1:
				return;
				break;
			case 1:
				/* use the default name */
				if (save_state ("")) {
					/* failed - don't quit */
					WavesMessageDialog msg ("",
							   string_compose (_("\
%1 was unable to save your session.\n\n\
If you still wish to quit, please use the\n\n\
\"Don't save\" option."), PROGRAM_NAME));
					pop_back_splash(msg);
					msg.run ();
					return;
				}
				break;
			case 0:
				break;
			}
		}

		second_connection.disconnect ();
		point_one_second_connection.disconnect ();
		point_zero_something_second_connection.disconnect();
		fps_connection.disconnect();
	}

	delete ARDOUR_UI::instance()->video_timeline;
	ARDOUR_UI::instance()->video_timeline = NULL;
	stop_video_server();

	/* Save state before deleting the session, as that causes some
	   windows to be destroyed before their visible state can be
	   saved.
	*/
	save_application_state ();

	close_all_dialogs ();

	if (_session) {
		// _session->set_deletion_in_progress ();
		_session->set_clean ();
		_session->remove_pending_capture_state ();
		delete _session;
		_session = 0;
	}

	halt_connection.disconnect ();
	AudioEngine::instance()->stop ();
#ifdef WINDOWS_VST_SUPPORT
	fst_stop_threading();
#endif
	quit ();
}

int
ARDOUR_UI::ask_about_saving_session (const vector<string>& actions)
{
  
    int result = WavesMessageDialog ("session_close_dialog.xml",
									 _("Session Close"),
									 _session->snap_name(),
									 WavesMessageDialog::BUTTON_YES |
									 WavesMessageDialog::BUTTON_NO |
									 WavesMessageDialog::BUTTON_CANCEL).run ();
  
	switch (result) {
            // button "SAVE" was pressed
		case WavesDialog::RESPONSE_DEFAULT:
        case RESPONSE_YES: // save and get out of here
            return 1;
            // button "DON'T SAVE" was pressed
        case RESPONSE_NO:  // get out of here
            return 0;
        default:
            break;
	}
    
    // button "CANCEL" was pressed
	return -1;
}


gint
ARDOUR_UI::every_second ()
{
	update_cpu_load ();
	update_disk_space ();
    update_disk_usage ();

	if (nsm && nsm->is_active ()) {
		nsm->check ();

		if (!_was_dirty && _session->dirty ()) {
			nsm->is_dirty ();
			_was_dirty = true;
		}
		else if (_was_dirty && !_session->dirty ()){
			nsm->is_clean ();
			_was_dirty = false;
		}
	}
	return TRUE;
}

gint
ARDOUR_UI::every_point_one_seconds ()
{
	shuttle_box->update_speed_display ();
	RapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

gint
ARDOUR_UI::every_point_zero_something_seconds ()
{
	// august 2007: actual update frequency: 25Hz (40ms), not 100Hz

	SuperRapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

gint
ARDOUR_UI::every_fps ()
{
	FPSUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

void
ARDOUR_UI::set_fps_timeout_connection ()
{
	unsigned int interval = 40;
	if (!_session) return;
	if (_session->timecode_frames_per_second() != 0) {
		/* ideally we'll use a select() to sleep and not accumulate
		 * idle time to provide a regular periodic signal.
		 * See linux_vst_gui_support.cc 'elapsed_time_ms'.
		 * However, that'll require a dedicated thread and cross-thread
		 * signals to the GUI Thread..
		 */
		interval = floor(500. /* update twice per FPS, since Glib::signal_timeout is very irregular */
				* _session->frame_rate() / _session->nominal_frame_rate()
				/ _session->timecode_frames_per_second()
				);
		interval = std::max(8u, interval); // at most 120Hz.
	}
	fps_connection.disconnect();
	fps_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_fps), interval);
}

void
ARDOUR_UI::update_cpu_load ()
{
	float const c = AudioEngine::instance()->get_dsp_load ();
    _dsp_load_adjustment->set_value (c);
	    
    stringstream ss;
    ss << (int)c;
    _dsp_load_label->set_text ( ss.str() + "%" );
}

void
ARDOUR_UI::count_recenabled_streams (Route& route)
{
	Track* track = dynamic_cast<Track*>(&route);
	if (track && track->record_enabled()) {
		rec_enabled_streams += track->n_inputs().n_total();
	}
}

void
ARDOUR_UI::update_disk_space()
{
    string result;
    
	if (_session == 0) {
		return;
	}

	boost::optional<framecnt_t> opt_frames = _session->available_capture_duration();
	framecnt_t fr = _session->frame_rate();

	if (fr == 0) {
		/* skip update - no SR available */
		return;
	}

	if (!opt_frames) {
		/* Available space is unknown */
        result = "Unknown";
	} else if (opt_frames.get_value_or (0) == max_framecnt) {
        result = "24hrs+";
    } else {
		rec_enabled_streams = 0;
		_session->foreach_route (this, &ARDOUR_UI::count_recenabled_streams, false);

		framecnt_t frames = opt_frames.get_value_or (0);

		if (rec_enabled_streams) {
			frames /= rec_enabled_streams;
		} else {
            frames /= _session->nroutes ();
        }

		int hrs;
		int mins;
		int secs;

		hrs  = frames / (fr * 3600);

		if (hrs > 24) {
            result =">24hrs";
        } else {
			frames -= hrs * fr * 3600;
			mins = frames / (fr * 60);
			frames -= mins * fr * 60;
			secs = frames / fr;
            
            stringstream ss;
            ss << hrs << "h " << mins << "m ";
            result = ss.str();
		}
	}

    _hd_remained_time_label->set_text(result);
}

void
ARDOUR_UI::update_disk_usage ()
{
    if (_session == 0) {
		return;
	}
    
    uint32_t const hd_buffer = 100 - (_session ? _session->capture_load () : 100);
    _hd_load_adjustment->set_value (hd_buffer);
    stringstream ss;
    ss << hd_buffer;
    _hd_load_label->set_text ( ss.str() + "%" );
}

void
ARDOUR_UI::redisplay_recent_sessions ()
{
	std::vector<std::string> session_directories;
	RecentSessionsSorter cmp;

	recent_session_display.set_model (Glib::RefPtr<TreeModel>(0));
	recent_session_model->clear ();

	ARDOUR::RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		recent_session_display.set_model (recent_session_model);
		return;
	}

	// sort them alphabetically
	sort (rs.begin(), rs.end(), cmp);

	for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		session_directories.push_back ((*i).second);
	}

	for (vector<std::string>::const_iterator i = session_directories.begin();
			i != session_directories.end(); ++i)
	{
		std::vector<std::string> state_file_paths;

		// now get available states for this session

		get_state_files_in_directory (*i, state_file_paths);

		vector<string> states;
		vector<const gchar*> item;
		string fullpath = *i;

		/* remove any trailing / */

		if (fullpath[fullpath.length() - 1] == '/') {
			fullpath = fullpath.substr (0, fullpath.length() - 1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(fullpath.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			continue;
		}

		/* now get available states for this session */
		states = Session::possible_states (fullpath);

		if (states.empty()) {
			/* no state file? */
			continue;
		}

		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		Gtk::TreeModel::Row row = *(recent_session_model->append());

		row[recent_session_columns.visible_name] = Glib::path_get_basename (fullpath);
		row[recent_session_columns.fullpath] = fullpath;
		row[recent_session_columns.tip] = Glib::Markup::escape_text (fullpath);

		if (state_file_names.size() > 1) {

			// add the children

			for (std::vector<std::string>::iterator i2 = state_file_names.begin();
					i2 != state_file_names.end(); ++i2)
			{

				Gtk::TreeModel::Row child_row = *(recent_session_model->append (row.children()));

				child_row[recent_session_columns.visible_name] = *i2;
				child_row[recent_session_columns.fullpath] = fullpath;
				child_row[recent_session_columns.tip] = Glib::Markup::escape_text (fullpath);
			}
		}
	}

	recent_session_display.set_tooltip_column(1); // recent_session_columns.tip
	recent_session_display.set_model (recent_session_model);
}

void
ARDOUR_UI::recent_session_row_activated (const TreePath& /*path*/, TreeViewColumn* /*col*/)
{
	session_selector_window->response (RESPONSE_ACCEPT);
}

void
ARDOUR_UI::get_recent_session_names_and_paths(std::vector<std::string>& session_names,std::vector<std::string>& session_paths)
{
    ARDOUR::RecentSessions rs;
    ARDOUR::read_recent_sessions (rs);
    
    /* after reading we should check that    */
    /* recent_session is still existing      */
    
    int i=0;
    for (ARDOUR::RecentSessions::iterator it = rs.begin();
         (i < MAX_RECENT_SESSION_COUNT) && (it != rs.end());
         ++it)
    {
        std::vector<std::string> state_file_paths;
        
        // now get available states for this session
        
        get_state_files_in_directory ((*it).second, state_file_paths);
        
        // vector<const gchar*> item;
        string dirname = (*it).second;
        
        /* remove any trailing / */
        if (dirname[dirname.length()-1] == '/') {
            dirname = dirname.substr (0, dirname.length()-1);
        }
        
        /* check whether session still exists */
        if (!Glib::file_test(dirname.c_str(), Glib::FILE_TEST_EXISTS)) {
            /* session doesn't exist */
            continue;
        }
        
        /* now get available states for this session */
        
        vector<string> states;
        states = Session::possible_states (dirname);
        
        if (states.empty()) {
            /* no state file? */
            continue;
        }
        
        std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));
        
        if (state_file_names.empty()) {
            continue;
        }
        
        session_paths.push_back(Glib::build_filename((*it).second,state_file_names.front() + statefile_suffix) );
        session_names.push_back(state_file_names.front());
        //std::cout<<"state_file_names.front()= "<<state_file_names.front()<<std::endl;
        
        ++i;
    }
}


void
ARDOUR_UI::open_recent_session_from_menuitem(unsigned int num_of_recent_session)
{
    if (num_of_recent_session >= recent_session_full_paths.size())
        return ;
    std::string cur_recent_session_path=recent_session_full_paths[num_of_recent_session];
    
    //check that cur_recent_session_path is still existing
    if ( !Glib::file_test (cur_recent_session_path, Glib::FileTest (G_FILE_TEST_EXISTS)) )
    {
        WavesMessageDialog session_deleted ("",string_compose (_("There is no existing session in \"%1\""), cur_recent_session_path.c_str() ),WavesMessageDialog::BUTTON_OK);
        session_deleted.run ();
        return ;
    }
    
    string path, name;
    bool isnew;
    if (ARDOUR::find_session (cur_recent_session_path, path, name, isnew) == 0) {
        _session_is_new = isnew;
        if (load_session (path, name) < 0) {
            // session file was corrupted
            // close this session and show session dialog
            close_session ();
        }

    }
}

bool
ARDOUR_UI::check_audioengine ()
{
	if (!AudioEngine::instance()->connected()) {
		WavesMessageDialog msg ("", string_compose (
					   _("%1 is not connected to any audio backend.\n"
					     "You cannot open or close sessions in this condition"),
					   PROGRAM_NAME));
		pop_back_splash (msg);
		msg.run ();
		return false;
	}
	return true;
}

void
ARDOUR_UI::open_session ()
{
	if (!check_audioengine()) {
		return;

	}

    string session_path = "";
    session_path = ARDOUR::open_file_dialog(Config->get_default_session_parent_dir(), _("Select Saved Session"));
    
    // cancel was pressed
    if(session_path == "")
        return;
    
    string path, name;
	bool isnew;

	if (session_path.length() > 0) {
		if (ARDOUR::find_session (session_path, path, name, isnew) == 0) {
			_session_is_new = isnew;
            if (load_session (path, name) < 0) {
                // session file was corrupted
                // close this session and show session dialog
                close_session ();
                
            }
		}
	}
}


void
ARDOUR_UI::session_add_mixed_track (const ChanCount& input, const ChanCount& output, RouteGroup* route_group, 
				    uint32_t how_many, const string& name_template, PluginInfoPtr instrument)
{
	list<boost::shared_ptr<MidiTrack> > tracks;

	if (_session == 0) {
		warning << _("You cannot add a track without a session already loaded.") << endmsg;
		return;
	}

	try {
		tracks = _session->new_midi_track (input, output, instrument, ARDOUR::Normal, route_group, how_many, name_template);
		
		if (tracks.size() != how_many) {
			error << string_compose(P_("could not create %1 new mixed track", "could not create %1 new mixed tracks", how_many), how_many) << endmsg;
		}
	}

	catch (...) {
		WavesMessageDialog msg ("",
				   string_compose (_("There are insufficient ports available\n\
to create a new track or bus.\n\
You should save %1, exit and\n\
restart with more ports."), PROGRAM_NAME));
		msg.run ();
	}
}
	

void
ARDOUR_UI::session_add_midi_route (bool disk, RouteGroup* route_group, uint32_t how_many, const string& name_template, PluginInfoPtr instrument)
{
	ChanCount one_midi_channel;
	one_midi_channel.set (DataType::MIDI, 1);

	if (disk) {
		session_add_mixed_track (one_midi_channel, one_midi_channel, route_group, how_many, name_template, instrument);
	}
}

void
ARDOUR_UI::session_add_audio_route (
	bool track,
	int32_t input_channels,
	int32_t output_channels,
	ARDOUR::TrackMode mode,
	RouteGroup* route_group,
	uint32_t how_many,
	string const & name_template
	)
{
	list<boost::shared_ptr<AudioTrack> > tracks;
	RouteList routes;

	if (_session == 0) {
		warning << _("You cannot add a track or bus without a session already loaded.") << endmsg;
		return;
	}

	try {
		if (track) {
			tracks = _session->new_audio_track (input_channels, output_channels, mode, route_group, how_many, name_template);

			if (tracks.size() != how_many) {
				error << string_compose (P_("could not create %1 new audio track", "could not create %1 new audio tracks", how_many), how_many) 
				      << endmsg;
			}

		} else {

			routes = _session->new_audio_route (input_channels, output_channels, route_group, how_many, name_template);

			if (routes.size() != how_many) {
				error << string_compose (P_("could not create %1 new audio bus", "could not create %1 new audio busses", how_many), how_many)
				      << endmsg;
			}
		}
	}

	catch (...) {
		WavesMessageDialog msg ("",
				   string_compose (_("There are insufficient ports available\n\
to create a new track or bus.\n\
You should save %1, exit and\n\
restart with more ports."), PROGRAM_NAME));
		pop_back_splash (msg);
		msg.run ();
	}
}

void
ARDOUR_UI::transport_goto_start ()
{
	if (_session) {
		_session->goto_start();

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (_session->current_start_frame ());
		}
	}
}

void
ARDOUR_UI::transport_goto_zero ()
{
	if (_session) {
		_session->request_locate (0);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->reset_x_origin (0);
		}
	}
}

void
ARDOUR_UI::transport_goto_wallclock ()
{
	if (_session && editor) {

		time_t now;
		struct tm tmnow;
		framepos_t frames;

		time (&now);
		localtime_r (&now, &tmnow);
		
		int frame_rate = _session->frame_rate();
		
		if (frame_rate == 0) {
			/* no frame rate available */
			return;
		}

		frames = tmnow.tm_hour * (60 * 60 * frame_rate);
		frames += tmnow.tm_min * (60 * frame_rate);
		frames += tmnow.tm_sec * frame_rate;

		_session->request_locate (frames, _session->transport_rolling ());

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (frames);
		}
	}
}

void
ARDOUR_UI::transport_goto_end ()
{
	if (_session) {
		framepos_t const frame = _session->current_end_frame();
		_session->request_locate (frame);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/

		if (editor) {
			editor->center_screen (frame);
		}
	}
}

void
ARDOUR_UI::transport_stop ()
{
	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		return;
	}
    
    if (_session->config.get_external_sync()) {
        switch (Config->get_sync_source()) {
            case MTC:
            case LTC:
                _session->set_slave_state (false);
                if (_session->actively_recording ())
                    _session->request_stop (false, true);
                return;
            default:
                /* transport controlled by the master */
                break;
        }
    }

	_session->request_stop (false, true);
}

/** Check if any tracks are record enabled. If none are, record enable all of them.
 * @return true if track record-enabled status was changed, false otherwise.
 */
bool
ARDOUR_UI::trx_record_enable_all_tracks ()
{
        if (!_session) {
                return false;
        }

        boost::shared_ptr<RouteList> rl = _session->get_tracks ();
        bool none_record_enabled = true;

        for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
                boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*r);
                assert (t);

                if (t->record_enabled()) {
                        none_record_enabled = false;
                        break;
                }
        }

        if (none_record_enabled) {
                _session->set_record_enabled (rl, true, Session::rt_cleanup);
        } 

        return none_record_enabled;
}

void
ARDOUR_UI::transport_record (bool roll)
{

	if (_session) {

        // make sure we are ready to change record state
		if (AudioEngine::instance()->state_lock().trylock () ) {
			// release the look
			AudioEngine::instance()->state_lock().unlock ();
		} else {
			return;
		}

		switch (_session->record_status()) {
		case Session::Disabled: {
			if (_session->ntracks() == 0) {
				WavesMessageDialog msg ("", _("Please create one or more tracks before trying to record.\nYou can do this with the \"Add Track\" option in the Track menu."));
				msg.run ();
				return;
			}
                
            if (editor->check_all_tracks_are_record_safe ()) {
                WavesMessageDialog msg ("", _("No record ready tracks available."));
				msg.run ();
				return;
            }
            
            Glib::RefPtr<Action> lockAction;
            // These are two keys - left and right ALT (win)/Option (mac) -- 0xffe9 and 0xffea:
            if (Keyboard::the_keyboard().key_is_down (0xffe9) || Keyboard::the_keyboard().key_is_down (0xffea)) {
                lockAction = ActionManager::get_action (X_("Main"), X_("toggle-session-lock-dialog"));
            }
            roll = trx_record_enable_all_tracks ();
            
            transport_roll ();
			if (_session->maybe_enable_record ()) {
                if (lockAction) {
                    lockAction->activate ();
                }
            }
		    break;
        }
		case Session::Recording:
			if (roll) {
				_session->request_stop();
			} else {
				_session->disable_record (false, true);
			}
			break;

		case Session::Enabled:
			_session->disable_record (false, true);
		}
	}
}

void
ARDOUR_UI::transport_roll ()
{
	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		return;
	}

	if (_session->config.get_external_sync()) {
		switch (Config->get_sync_source()) {
		case Engine:
			break;
        case MTC:
        case LTC:
            _session->set_slave_state (true);
            return;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = _session->transport_rolling();

	if (_session->get_play_loop()) {

		/* If loop playback is not a mode, then we should cancel
		   it when this action is requested. If it is a mode
		   we just leave it in place.
		*/

		if (!Config->get_loop_is_mode()) {
			/* XXX it is not possible to just leave seamless loop and keep
			   playing at present (nov 4th 2009)
			*/
			if (!Config->get_seamless_loop()) {
				/* stop loop playback and stop rolling */
				_session->request_play_loop (false, true);
			} else if (rolling) {
				/* stop loop playback but keep rolling */
				_session->request_play_loop (false, false);
			}
		} 

	} else if (_session->get_play_range () ) {
		/* stop playing a range if we currently are */
		_session->request_play_range (0, true);
	}

	if (!rolling) {
		_session->request_transport_speed (1.0f);
	}
}

bool
ARDOUR_UI::get_smart_mode() const
{
	return ( editor->get_smart_mode() );
}


void
ARDOUR_UI::toggle_roll (bool with_abort, bool roll_out_of_bounded_mode)
{

	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		return;
	}

	if (_session->config.get_external_sync()) {
		switch (Config->get_sync_source()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = _session->transport_rolling();
	bool affect_transport = true;

	if (rolling && roll_out_of_bounded_mode) {
		/* drop out of loop/range playback but leave transport rolling */
		if (_session->get_play_loop()) {
			if (Config->get_seamless_loop()) {
				/* the disk buffers contain copies of the loop - we can't
				   just keep playing, so stop the transport. the user
				   can restart as they wish.
				*/
				affect_transport = true;
			} else {
				/* disk buffers are normal, so we can keep playing */
				affect_transport = false;
			}
			_session->request_play_loop (false, affect_transport);
		} else if (_session->get_play_range ()) {
			affect_transport = false;
			_session->request_play_range (0, true);
		}
	}

	if (affect_transport) {
		if (rolling) {
			_session->request_stop (with_abort, true);
		} else {
			if ( config()->get_follow_edits() && ( editor->get_selection().time.front().start == _session->transport_frame() ) ) {  //if playhead is exactly at the start of a range, we can assume it was placed there by follow_edits
				_session->request_play_range (&editor->get_selection().time, true);
				_session->set_requested_return_frame( editor->get_selection().time.front().start );  //force an auto-return here
			}
			_session->request_transport_speed (1.0f);
		}
	}
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (!_session) {
		return;
	}
	
	Location * looploc = _session->locations()->auto_loop_location();

	if (!looploc) {
		return;
	}

	if (_session->get_play_loop()) {

		/* looping enabled, our job is to disable it */

		_session->request_play_loop (false);

	} else {

		/* looping not enabled, our job is to enable it.

		   loop-is-NOT-mode: this action always starts the transport rolling.
		   loop-IS-mode:     this action simply sets the loop play mechanism, but
		                        does not start transport.
		*/
		if (Config->get_loop_is_mode()) {
			_session->request_play_loop (true, false);
		} else {
			_session->request_play_loop (true, true);
		}
	}
	
	//show the loop markers
	looploc->set_hidden (false, this);
}

void ARDOUR_UI::toggle_multi_out_mode ()
{
    if (Config->get_output_auto_connect() & AutoConnectPhysical) {
        // the mode is already enabled, nothing to do here
        return;
    }

    if (!_session) {
        return;
    }
    
    // do not allow to change output connection mode when trasport is moving
    if (_session->transport_rolling() ) {
        return;
    }
    
    // this case is covered by above
    //if (_session->actively_recording() && _session->have_rec_enabled_track ()) {
    //    return;
    //}

    
    Config->set_output_auto_connect(AutoConnectPhysical);
	editor->get_waves_button ("mode_multi_out_button").set_active(true);
	editor->get_waves_button ("mode_stereo_out_button").set_active(false);
}

void ARDOUR_UI::toggle_stereo_out_mode ()
{
    if (Config->get_output_auto_connect() & AutoConnectMaster) {
        // the mode is already enabled, nothing to do here
        return;
    }
    
    if (!_session) {
        return;
    }
    
    // do not allow to change output connection mode when trasport is moving
    if (_session->transport_rolling() ) {
        return;
    }
    
    // this case is covered by above
    //if (_session->actively_recording() && _session->have_rec_enabled_track ()) {
    //    return;
    //}
    
    Config->set_output_auto_connect(AutoConnectMaster);
	editor->get_waves_button ("mode_stereo_out_button").set_active(true);
	editor->get_waves_button ("mode_multi_out_button").set_active(false);
}

void
ARDOUR_UI::transport_play_selection ()
{
	if (!_session) {
		return;
	}

	editor->play_selection ();
}

void
ARDOUR_UI::transport_play_preroll ()
{
	if (!_session) {
		return;
	}
	editor->play_with_preroll ();
}

void
ARDOUR_UI::transport_rewind (int option)
{
	float current_transport_speed;

       	if (_session) {
		current_transport_speed = _session->transport_speed();

		if (current_transport_speed >= 0.0f) {
			switch (option) {
			case 0:
				_session->request_transport_speed (-1.0f);
				break;
			case 1:
				_session->request_transport_speed (-4.0f);
				break;
			case -1:
				_session->request_transport_speed (-0.5f);
				break;
			}
		} else {
			/* speed up */
			_session->request_transport_speed (current_transport_speed * 1.5f);
		}
	}
}

void
ARDOUR_UI::transport_forward (int option)
{
	if (!_session) {
		return;
	}
	
	float current_transport_speed = _session->transport_speed();
	
	if (current_transport_speed <= 0.0f) {
		switch (option) {
		case 0:
			_session->request_transport_speed (1.0f);
			break;
		case 1:
			_session->request_transport_speed (4.0f);
			break;
		case -1:
			_session->request_transport_speed (0.5f);
			break;
		}
	} else {
		/* speed up */
		_session->request_transport_speed (current_transport_speed * 1.5f);
	}
}

void
ARDOUR_UI::toggle_record_enable (uint32_t rid)
{
	if (!_session) {
		return;
	}

	boost::shared_ptr<Route> r;

	if ((r = _session->route_by_remote_id (rid)) != 0) {

		Track* t;

		if ((t = dynamic_cast<Track*>(r.get())) != 0) {
			t->set_record_enabled (!t->record_enabled(), this);
		}
	}
}

void
ARDOUR_UI::map_transport_state ()
{
	if (!_session) {
		editor->get_waves_button ("transport_loop_button").set_active (false);
		editor->get_waves_button ("transport_play_button").set_active (false);
		editor->get_waves_button ("transport_stop_button").set_active (true);
		return;
	}

	shuttle_box->map_transport_state ();

	float sp = _session->transport_speed();

	if (sp != 0.0f) {

		/* we're rolling */

		if (_session->get_play_range()) {
			editor->get_waves_button ("transport_play_button").set_active (true);
			editor->get_waves_button ("transport_loop_button").set_active (false);
		} else if (_session->get_play_loop ()) {

			editor->get_waves_button ("transport_loop_button").set_active (true);
			if (Config->get_loop_is_mode()) {
				editor->get_waves_button ("transport_play_button").set_active (true);
			} else {
				editor->get_waves_button ("transport_play_button").set_active (false);
			}

		} else {

			editor->get_waves_button ("transport_play_button").set_active (true);
			editor->get_waves_button ("transport_loop_button").set_active (false);
		}

		editor->get_waves_button ("transport_stop_button").set_active (false);

	} else {

		editor->get_waves_button ("transport_stop_button").set_active (true);
		editor->get_waves_button ("transport_play_button").set_active (false);

		if (Config->get_loop_is_mode ()) {
			editor->get_waves_button ("transport_loop_button").set_active (_session->get_play_loop());
		} else {
			editor->get_waves_button ("transport_loop_button").set_active (false);
		}
		update_disk_space ();
        update_disk_usage ();
	}
}

void
ARDOUR_UI::update_clocks ()
{
	if (!editor || !editor->dragging_playhead()) {
		Clock (_session->audible_frame(), false, editor->get_preferred_edit_position()); /* EMIT_SIGNAL */
	}
}

void
ARDOUR_UI::start_clocking ()
{
	if (ARDOUR_UI::config()->get_super_rapid_clock_update()) {
		clock_signal_connection = FPSUpdate.connect (sigc::mem_fun(*this, &ARDOUR_UI::update_clocks));
	} else {
		clock_signal_connection = RapidScreenUpdate.connect (sigc::mem_fun(*this, &ARDOUR_UI::update_clocks));
	}
}

void
ARDOUR_UI::stop_clocking ()
{
	clock_signal_connection.disconnect ();
}

gint
ARDOUR_UI::_blink (void *arg)
{
	((ARDOUR_UI *) arg)->blink ();
	return TRUE;
}

void
ARDOUR_UI::blink ()
{
	Blink (blink_on = !blink_on); /* EMIT_SIGNAL */
}

void
ARDOUR_UI::start_blinking ()
{
	/* Start the blink signal. Everybody with a blinking widget
	   uses Blink to drive the widget's state.
	*/

	if (blink_timeout_tag < 0) {
		blink_on = false;
		blink_timeout_tag = g_timeout_add (240, _blink, this);
	}
}

void
ARDOUR_UI::stop_blinking ()
{
	if (blink_timeout_tag >= 0) {
		g_source_remove (blink_timeout_tag);
		blink_timeout_tag = -1;
	}
}

void
ARDOUR_UI::on_lock_button_pressed () {
    
    lock_button_was_pressed();
}

void
ARDOUR_UI::on_lock_session ()
{
    editor->get_waves_button ("lock_session_button").set_active (true);
}

void
ARDOUR_UI::on_unlock_session ()
{
    editor->get_waves_button ("lock_session_button").set_active (false);
}

void
ARDOUR_UI::lock_session () {
    SessionLockDialog session_lock_dialog;
    session_lock_dialog.set_deletable (false);
    session_lock_dialog.set_modal (true);
    while (1) {
        int response = session_lock_dialog.run ();
        switch (response) {
            case Gtk::RESPONSE_OK:
                return; // Unlock button was pressed
            default:
                continue; // close button (on mac) shouldn't close window
        }
    }
}

bool
ARDOUR_UI::screen_lock_is_allowed() const
{
    if(!_session)
        return false;
    
    if( (_session->actively_recording() ) && (ARDOUR_UI::config()->get_auto_lock_timer () != 0) )
        return true;
    else
        return false;
}

bool
ARDOUR_UI::session_auto_save_is_allowed() const
{
    if(!_session)
        return false;
    
    if( (_session->actively_recording () ) && (ARDOUR_UI::config()->get_auto_save_timer () != 0) )
        return true;
    else
        return false;
}

bool
ARDOUR_UI::save_as_progress_update (float fraction, int64_t cnt, int64_t total)
{
	char buf[256];

	snprintf (buf, sizeof (buf), _("Copied %" PRId64 " of %" PRId64), cnt, total);

    _progress_dialog.set_bottom_label (buf);
    _progress_dialog.set_progress (fraction);

	return true; /* continue with save-as */
}

void
ARDOUR_UI::save_session_as ()
{
	if (!_session) {
		return;
	}

    string save_as_session_full_file_name="";
    bool copy_media = false;
    
	#if defined (__APPLE__)
		save_as_session_full_file_name = ARDOUR::save_as_file_dialog (Config->get_default_session_parent_dir(),_("Save As"), copy_media);
		 // Button 'Cancel' was pressed
		if (save_as_session_full_file_name.empty ())
			return ;
	#else
		save_as_session_full_file_name = ARDOUR::save_file_dialog (Config->get_default_session_parent_dir(),_("Save As"));
		// Button 'Cancel' was pressed
		if (save_as_session_full_file_name.empty ())
			return ;
		WavesMessageDialog ask_for_copy_media ("","Do you want to copy external media?", WavesMessageDialog::BUTTON_YES | WavesMessageDialog::BUTTON_NO);
		ask_for_copy_media.set_position (Gtk::WIN_POS_CENTER);
		ask_for_copy_media.set_keep_above (true);
		int response = ask_for_copy_media.run ();
		if (response == Gtk::RESPONSE_YES)
			copy_media = true;
		else 
			copy_media = false;
	#endif

    Session::SaveAs sa;
    sa.new_parent_folder = Glib::path_get_dirname (save_as_session_full_file_name);
    sa.new_name = Glib::path_get_basename (save_as_session_full_file_name);
	sa.copy_media = copy_media;
    
    // always include references to media for TracksLive
    if (Profile->get_trx() ) {
        sa.include_media = true;
    }
   
    // it should be always so
    /* 
        Paul's comment:
        BASED ON DISCUSSIONS WITH IGOR, TRACKS SHOULD NOT PRESENT
        THE "copy_external" option, BUT SHOULD ALWAYS SET IT TO
        TRUE.
     */
    sa.switch_to = true;
	sa.copy_external = true;

	/* this signal will be emitted from within this, the calling thread,
	 * after every file is copied. It provides information on percentage
	 * complete (in terms of total data to copy), the number of files
	 * copied so far, and the total number to copy.
	 */
	ScopedConnection c;
	sa.Progress.connect_same_thread (c, boost::bind (&ARDOUR_UI::save_as_progress_update, this, _1, _2, _3));

    // Show ProgressDialog
    _progress_dialog.set_top_label (string_compose (("Saving session: %1"), sa.new_name));
    _progress_dialog.update_info (0.0, NULL, NULL, NULL);
    _progress_dialog.show_pd ();
	if (_session->save_as (sa)) {
		/* ERROR MESSAGE */
		WavesMessageDialog ("", string_compose (_("Save As failed: %1"), sa.failure_message)).run ();
	} else {
		update_recent_session_menuitems ();
	}
    _progress_dialog.hide ();
}

/** Ask the user for the name of a new snapshot and then take it.
 */

void
ARDOUR_UI::snapshot_session (bool switch_to_it)
{
	ArdourPrompter prompter (true);
	string snapname;

	prompter.set_name ("Prompter");
	prompter.add_button ("SAVE", Gtk::RESPONSE_ACCEPT);
	if (switch_to_it) {
		prompter.set_title (_("Save as..."));
		prompter.set_prompt (_("New session name"));
	} else {
		prompter.set_title (_("Take Snapshot"));
		prompter.set_prompt (_("Name of new snapshot"));
	}

	if (!switch_to_it) {
		char timebuf[128];
		time_t n;
		struct tm local_time;

		time (&n);
		localtime_r (&n, &local_time);
		strftime (timebuf, sizeof(timebuf), "%FT%H.%M.%S", &local_time);
		prompter.set_initial_text (timebuf);
	}

  again:
	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
	{
		prompter.get_result (snapname);

		bool do_save = (snapname.length() != 0);

		if (do_save) {
			char illegal = Session::session_name_is_legal(snapname);
			if (illegal) {
				WavesMessageDialog msg ("", string_compose (_("To ensure compatibility with various systems\n"
				                     "snapshot names may not contain a '%1' character"), illegal));
				msg.run ();
				goto again;
			}
		}

		vector<std::string> p;
		get_state_files_in_directory (_session->session_directory().root_path(), p);
		vector<string> n = get_file_names_no_extension (p);
		if (find (n.begin(), n.end(), snapname) != n.end()) {

			ArdourDialog confirm (_("Confirm Snapshot Overwrite"), true);
			Label m (_("A snapshot already exists with that name.  Do you want to overwrite it?"));
			confirm.get_vbox()->pack_start (m, true, true);
			confirm.add_button ("CANCEL", Gtk::RESPONSE_CANCEL);
			confirm.add_button (_("Overwrite"), Gtk::RESPONSE_ACCEPT);
			confirm.show_all ();
			switch (confirm.run()) {
			case RESPONSE_CANCEL:
				do_save = false;
			}
		}

		if (do_save) {
			save_state (snapname, switch_to_it);
		}
		break;
	}

	default:
		break;
	}
}

/** Ask the user for a new session name and then rename the session to it.
 */

void
ARDOUR_UI::rename_session ()
{
	if (!_session) {
		return;
	}

	ArdourPrompter prompter (true);
	string name;

	prompter.set_name ("Prompter");
	prompter.add_button ("SAVE", Gtk::RESPONSE_ACCEPT);
	prompter.set_title (_("Rename Session"));
	prompter.set_prompt (_("New session name"));

  again:
	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
	{
		prompter.get_result (name);

		bool do_rename = (name.length() != 0);

		if (do_rename) {
			char illegal = Session::session_name_is_legal (name);

			if (illegal) {
				WavesMessageDialog msg ("", string_compose (_("To ensure compatibility with various systems\n"
								     "session names may not contain a '%1' character"), illegal));
				msg.run ();
				goto again;
			}

			switch (_session->rename (name)) {
			case -1: {
				WavesMessageDialog msg ("", _("That name is already in use by another directory/folder. Please try again."));
				msg.set_position (WIN_POS_MOUSE);
				msg.run ();
				goto again;
				break;
			}
			case 0:
				break;
			default: {
				WavesMessageDialog msg ("", _("Renaming this session failed.\nThings could be seriously messed up at this point"));
				msg.set_position (WIN_POS_MOUSE);
				msg.run ();
				break;
			}
			}
		}
		
		break;
	}

	default:
		break;
	}
}

int
ARDOUR_UI::save_session_state (const string & name, bool pending, bool switch_to_it)
{
    if (_session) {
        
        std::string sess_name = name;
        if (sess_name.length() == 0) {
            sess_name = _session->snap_name();
        }
        
        int ret = 0;
        if ((ret = _session->save_state (name, false, switch_to_it)) != 0) {
            return ret;
        }
    }
    
    return 0;
}

/* called as a handler for Session::SaveSession signal */
void
ARDOUR_UI::save_session_gui_state ()
{
    if (_session) {
        
        /* save extra XML with session GUI config */
        XMLNode* node = new XMLNode (X_("UI"));
        
        WM::Manager::instance().add_state (*node);
        
        node->add_child_nocopy (gui_object_state->get_state());
        
        _session->add_extra_xml (*node);
        
        /* save session instant XML */
        XMLNode& enode (static_cast<Stateful*>(editor)->get_state());
        _session->add_instant_xml (enode);
        if (location_ui) {
            _session->add_instant_xml (location_ui->ui().get_state ());
        }
        delete &enode;
    }
}


int
ARDOUR_UI::save_state (const string & name, bool switch_to_it)
{
    int ret = save_session_state (name, switch_to_it);
	save_application_state ();
    return ret;
}


void
ARDOUR_UI::set_session_dirty ()
{
    if (_session) {
        _session->set_dirty ();
    }
}

void
ARDOUR_UI::primary_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (primary_clock->current_time ());
	}
}

void
ARDOUR_UI::big_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (big_clock->current_time ());
	}
}

void
ARDOUR_UI::secondary_clock_value_changed ()
{
	if (_session) {
		_session->request_locate (secondary_clock->current_time ());
	}
}

void
ARDOUR_UI::transport_rec_enable_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->step_editing()) {
		return;
	}

	Session::RecordState const r = _session->record_status ();
	bool const h = _session->have_rec_enabled_track ();

	if (r == Session::Enabled || (r == Session::Recording && !h)) {
		if (onoff) {
			editor->get_waves_button ("transport_record_button").set_active (true);
		} else {
			editor->get_waves_button ("transport_record_button").set_active (false);
	}
	} else if (r == Session::Recording && h) {
		editor->get_waves_button ("transport_record_button").set_active (true);
	} else {
		editor->get_waves_button ("transport_record_button").set_active (false);
	}
}

void
ARDOUR_UI::save_template ()
{
	if (!check_audioengine()) {
		return;
	}

    std::string template_path = ARDOUR::save_file_dialog(boost::assign::list_of (ARDOUR::template_suffix + 1),
														 g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS),
														 _("Save Template"));
	if (!template_path.empty()) {
		bool add_suffix = true;
		std::string basename = Glib::path_get_basename (template_path);
		std::string::size_type pos = basename.find_last_of (".");
	
		if (pos != std::string::npos) {
			basename = basename.substr (pos);
			std::string template_suffix = ARDOUR::template_suffix;
			boost::to_lower (template_suffix);
			boost::to_lower (basename);
			if (basename == template_suffix) {
				add_suffix = false;
			}
		}
		
		if (add_suffix) {
			// add ".template" as in tracks PRD says store to selected FILE
			// doing not create any directory for it. Mention chosen file as of type .template.
			template_path += ARDOUR::template_suffix;
		}
		
		if (_session->save_template (template_path)) {
			 WavesMessageDialog ("", string_compose(_("Could not save Session template\n%1"), template_path)).run ();
		}
	}
}

void
ARDOUR_UI::edit_metadata ()
{
	SessionMetadataEditor dialog;
	dialog.set_session (_session);
	editor->ensure_float (dialog);
	dialog.run ();
}

void
ARDOUR_UI::import_metadata ()
{
	SessionMetadataImporter dialog;
	dialog.set_session (_session);
	editor->ensure_float (dialog);
	dialog.run ();
}

bool
ARDOUR_UI::ask_about_loading_existing_session (const std::string& session_path)
{
	std::string str = string_compose (_("This session\n%1\nalready exists. Do you want to open it?"), session_path);

	WavesMessageDialog msg ("",str, WavesMessageDialog::BUTTON_YES | WavesMessageDialog::BUTTON_NO);
	msg.set_position (Gtk::WIN_POS_MOUSE);
	pop_back_splash (msg);

	switch (msg.run()) {
	case RESPONSE_YES:
		return true;
		break;
	}
	return false;
}

int
ARDOUR_UI::build_session_from_dialog (SessionDialog& sd, const std::string& session_path, const std::string& session_name)
{
	BusProfile bus_profile;

	if (nsm || Profile->get_sae()) {

		bus_profile.master_out_channels = 2;
		bus_profile.input_ac = AutoConnectPhysical;
		bus_profile.output_ac = AutoConnectMaster;
		bus_profile.requested_physical_in = 0; // use all available
		bus_profile.requested_physical_out = 0; // use all available

	} else {

		/* get settings from advanced section of NSD */

		if (sd.create_master_bus()) {
			bus_profile.master_out_channels = (uint32_t) sd.master_channel_count();
		} else {
			bus_profile.master_out_channels = 0;
		}

		if (sd.connect_inputs()) {
			bus_profile.input_ac = AutoConnectPhysical;
		} else {
			bus_profile.input_ac = AutoConnectOption (0);
		}

		bus_profile.output_ac = AutoConnectOption (0);

		if (sd.connect_outputs ()) {
			if (sd.connect_outs_to_master()) {
				bus_profile.output_ac = AutoConnectMaster;
			} else if (sd.connect_outs_to_physical()) {
				bus_profile.output_ac = AutoConnectPhysical;
			}
		}

		bus_profile.requested_physical_in = (uint32_t) sd.input_limit_count();
		bus_profile.requested_physical_out = (uint32_t) sd.output_limit_count();
	}

	if (build_session (session_path, session_name, bus_profile)) {
		return -1;
	}

	return 0;
}

void
ARDOUR_UI::load_from_application_api (const std::string& path_from_user_choice)
{
    // this method could be called just on OS X
    // as a result of opening folder/file
    // with help of application api (dbl-click, drag-n-drop)
    
    string path_for_loading;
    
    // folder was chosen
    if (Glib::file_test (path_from_user_choice, Glib::FILE_TEST_IS_DIR)) {
        // get full name of session file
        string full_name_to_session_file = string_compose ("%1/%2.ardour", path_from_user_choice, basename_nosuffix (path_from_user_choice));
        
        // check that session file is really existing in folder
        if ( Glib::file_test (full_name_to_session_file, Glib::FileTest (G_FILE_TEST_EXISTS))) {
            ARDOUR_COMMAND_LINE::session_name = full_name_to_session_file;
            path_for_loading = path_from_user_choice;
        }
        else {
            if ( !program_starting) {
                WavesMessageDialog session_deleted ("",string_compose (_("There is no existing session in \"%1\""), full_name_to_session_file),WavesMessageDialog::BUTTON_OK);
                session_deleted.run ();
            }
            return ;
        }
    }
    // file was chosen
    else {
        // check that user chose right file format
        string temp_path, temp_name;
        bool isnew;
        if (ARDOUR::find_session (path_from_user_choice, temp_path, temp_name, isnew) == 0) {
            ARDOUR_COMMAND_LINE::session_name = path_from_user_choice;
            path_for_loading = Glib::path_get_dirname (path_from_user_choice);
        }
        else {
            if ( !program_starting) {
                WavesMessageDialog session_wrong_format ("",string_compose (_("\"%1\" has wrong file format"), path_from_user_choice), WavesMessageDialog::BUTTON_OK);
                session_wrong_format.run ();
            }
            return ;
        }
    }
    
    // if the program was starting from application api
    // we should not do nothing here
    if ( !program_starting) {
        if ( _session_dialog.get_visible () ) {
            session_dialog_was_hidden = true;
            _session_dialog.hide ();
        }
        // we should save status session_loaded
        // because it will be changed in load_session
        bool temp_session_loaded = session_loaded;
        if (load_session (path_for_loading, basename_nosuffix (path_from_user_choice)) < 0
            && temp_session_loaded) {
            close_session ();
        }
    }
}

namespace
{
    void run_message_dialog(std::string message)
    {
        WavesMessageDialog msg ("", message.c_str());
        msg.set_position (Gtk::WIN_POS_MOUSE);
        msg.run();
    }
    
    void init_suffices(std::vector<std::string>& suffices)
    {
        suffices.push_back(template_suffix);
        suffices.push_back(statefile_suffix);
        suffices.push_back(pending_suffix);
        suffices.push_back(peakfile_suffix);
        suffices.push_back(backup_suffix);
        suffices.push_back(temp_suffix);
        suffices.push_back(history_suffix);
        suffices.push_back(export_preset_suffix);
        suffices.push_back(export_format_suffix);
        suffices.push_back("instant.xml");
    }
    
    void init_directories(std::vector<std::string>& directories)
    {
        directories.push_back(string(peak_dir_name));
        directories.push_back(string(dead_dir_name));
        directories.push_back(string(export_dir_name));
        directories.push_back(string(analysis_dir_name)); 
        directories.push_back(string(plugins_dir_name));
        directories.push_back(string(externals_dir_name));
        directories.push_back(string(".DS_Store"));
    } 
    
    bool is_tracks_file(std::string full_file_name)
    {
        std::vector<std::string> suffices;
        init_suffices(suffices);
        
        for(int i = 0; i < suffices.size(); i++)
        {
            int pos = full_file_name.rfind(suffices[i]);
            // if suffix locates at the end of the full_file_name
            if( pos == full_file_name.size() - suffices[i].size() )
                return true;
        }
        
        return false;
    }
    
    /*@directory_name for example is "analysis", "example", etc. It is not a full path. */
    bool is_tracks_directory(std::string directory_name)
    {
        std::vector<std::string> directories;
        init_directories(directories);
        
        return std::find(directories.begin(), directories.end(), directory_name) != directories.end();
    }

}


// return true if session was successfully deleted
bool
ARDOUR_UI::delete_session_files(std::string session_path)
{
    GDir *dir;
    GError *error;
    const gchar *file_name;
    
    dir = g_dir_open(session_path.c_str(), 0, &error);
    
    bool result = true;
    
    while ((file_name = g_dir_read_name(dir)))
    {
        string object_name = string(file_name);
        string full_object_name = Glib::build_filename( session_path, string(file_name) );
        
        bool is_directory = Glib::file_test(full_object_name, Glib::FileTest (G_FILE_TEST_IS_DIR));
        
        if( is_directory && is_tracks_directory(file_name) )
        {
            result = result && delete_session_files(full_object_name);
            g_remove(full_object_name.c_str());
        } else {
            if( !is_directory && is_tracks_file(full_object_name) )
                if( g_remove(full_object_name.c_str()) == -1 )
                {
                    run_message_dialog("Can't remove file: " + full_object_name);
                    result = false;
                }
        }
    } 
    
    return result;
}

/** @param quit_on_cancel true if exit() should be called if the user clicks `cancel' in the new session dialog */
int
ARDOUR_UI::get_session_parameters (bool quit_on_cancel, bool should_be_new, string load_template)
{
	string session_name;
	string session_path;
	string template_name;
    string full_session_name;
	int ret = -1;
	bool likely_new = false;
	bool cancel_not_quit;
    
    program_starting = false;

	/* deal with any existing DIRTY session now, rather than later. don't
	 * treat a non-dirty session this way, so that it stays visible
	 * as we bring up the new session dialog.
	 */

	if (_session && ARDOUR_UI::instance()->video_timeline) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();
	}

	/* if there is already a session, relabel the button
	   on the SessionDialog so that we don't Quit directly
	*/
	cancel_not_quit = (_session != 0);
    
    if (_session) {
        if (unload_session (true)) {
    		/* unload cancelled by user */
            return 0;
        }
        ARDOUR_COMMAND_LINE::session_name = "";
    }

	if (!load_template.empty()) {
		should_be_new = true;
		template_name = load_template;
	}

	session_name = basename_nosuffix (ARDOUR_COMMAND_LINE::session_name);
	session_path = ARDOUR_COMMAND_LINE::session_name;
	
	if (!session_path.empty()) {
		if (Glib::file_test (session_path.c_str(), Glib::FILE_TEST_EXISTS)) {
			if (Glib::file_test (session_path.c_str(), Glib::FILE_TEST_IS_REGULAR)) {
				/* session/snapshot file, change path to be dir */
				session_path = Glib::path_get_dirname (session_path);
			}
		}
	}
	//SessionDialog _session_dialog (tracks_control_panel, should_be_new, session_name, session_path, load_template, cancel_not_quit);
    

	while (ret != 0) {
        _session_dialog.set_session_info (should_be_new, session_name, session_path);
        _session_dialog.redisplay ();

		if (!ARDOUR_COMMAND_LINE::session_name.empty()) {

			/* if they named a specific statefile, use it, otherwise they are
			   just giving a session folder, and we want to use it as is
			   to find the session.
			*/

			string::size_type suffix = ARDOUR_COMMAND_LINE::session_name.find (statefile_suffix);

			if (suffix != string::npos) {
				session_path = Glib::path_get_dirname (ARDOUR_COMMAND_LINE::session_name);
				session_name = ARDOUR_COMMAND_LINE::session_name.substr (0, suffix);
				session_name = Glib::path_get_basename (session_name);
			} else {
				session_path = ARDOUR_COMMAND_LINE::session_name;
				session_name = Glib::path_get_basename (ARDOUR_COMMAND_LINE::session_name);
			}
		} else {
			session_path = "";
			session_name = "";
			_session_dialog.clear_given ();
		}
		
		if (ARDOUR_COMMAND_LINE::session_name.size ()) { 
			// session to open is already chosen by user
			// from command line 
			// we shouldn't run SessionDialog
			// just open chosen session
			_session_dialog.set_selected_session_full_path (ARDOUR_COMMAND_LINE::session_name);
		}
		else { 
			// session wasn't preliminary chosen
			// so we must show SessionDialog
			if (should_be_new || session_name.empty()) {
				/* need the dialog to get info from user */
				int response = _session_dialog.run();
				switch (response) {
					case Gtk::RESPONSE_ACCEPT: // existed session was choosen or new session was created
						break;
					case Gtk::RESPONSE_CANCEL: // cancel was pressed
					case WavesDialog::RESPONSE_DEFAULT: // enter was pressed
						continue; // do not act on Esc or Enter button pressed
					case Gtk::RESPONSE_REJECT: // quit button pressed
						UI::quit ();
						return ret;
					default:
                        // if session was chosen from application api (on mac) and
                        // SessionDialog was hidden in method load_from_application_api
                        if (session_dialog_was_hidden) {
                            session_dialog_was_hidden = false;
                            ARDOUR_COMMAND_LINE::session_name = "";
                            if (session_loaded) {
                            // normal continuation of program
                                return 0;
                            } else {
                            // SessionDialg continue running
                                continue;
                            }
                        }
                        // this happens on Close Button pressed (at the top left corner only on Mac)
                        UI::quit ();
                        return ret;
				}

				_session_dialog.hide ();
			}
		}

		/* if we run the startup dialog again, offer more than just "new session" */
		
		should_be_new = false;
        
		full_session_name = session_name = _session_dialog.session_name (likely_new);
        
        /* Check if already created session exists and the path is valid */
        if (!likely_new && !Glib::file_test (full_session_name, Glib::FileTest (G_FILE_TEST_EXISTS))) {
            run_message_dialog ("File " + full_session_name + " does not exist");
            return -1;
        }
        
		session_path = _session_dialog.session_folder ();
        
		if (nsm) {
			likely_new = true;
		}

		string::size_type suffix = session_name.find (statefile_suffix);
		
		if (suffix != string::npos) {
			session_name = session_name.substr (0, suffix);
		}
		
		/* this shouldn't happen, but we catch it just in case it does */
		
		if (session_name.empty()) {
            run_message_dialog("Session name is invalid");
			continue;
		}
		
		if (_session_dialog.use_session_template()) {
			template_name = _session_dialog.session_template_name();
			_session_is_new = true;
		}
		
		if (Glib::path_is_absolute (session_name)) {
			
			/* absolute path or cwd-relative path specified for session name: infer session folder
			   from what was given.
			*/
			
			session_path = Glib::path_get_dirname (session_name);
			session_name = Glib::path_get_basename (session_name);
			
		} else {

			session_path = _session_dialog.session_folder();
			
			char illegal = Session::session_name_is_legal (session_name);
			
			if (illegal) {
				WavesMessageDialog msg ("", string_compose (_("To ensure compatibility with various systems\n"
								                              "session names may not contain a '%1' character"), illegal));
				msg.run ();
				ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
				continue;
			}
		}
	
		if ( Glib::file_test (session_path, Glib::FileTest (G_FILE_TEST_EXISTS)) ) {


			if (likely_new && !nsm) {
                
                // Replace session only if file with extension '.ardour' was chosen
                string suffix = string(statefile_suffix);

			
				// If existed folder was choosen 
				if( Glib::file_test (full_session_name, Glib::FileTest (G_FILE_TEST_IS_DIR)) )
				{
					run_message_dialog("Can not replace session. Folder " + session_path + 
							            " allready exists. If you need to replace session please choose file " + 
							            full_session_name + suffix);
					continue;
				}
                    
                size_t pos = full_session_name.rfind(suffix);
                
                // if not *.ardour file was choosen
                if( !(pos == full_session_name.size() - suffix.size()) )
                {
                    run_message_dialog("Invalid file name. In order to replace session select *" + suffix + " projectfile");
                    
                    continue;
                }
                    
                
                // .ardour file was choosen. Replace session: delete existing session
                if( !delete_session_files(session_path) )
                    continue;
                
                _session_is_new = true;
			} else
                _session_is_new = false;

		} else {

			if (!likely_new) {
				pop_back_splash (_session_dialog);
				WavesMessageDialog msg ("", string_compose (_("There is no existing session at \"%1\""), session_path));
				msg.run ();
				ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
				continue;
			}

			char illegal = Session::session_name_is_legal(session_name);

			if (illegal) {
				pop_back_splash (_session_dialog);
				WavesMessageDialog msg ("", string_compose(_("To ensure compatibility with various systems\n"
										    "session names may not contain a '%1' character"), illegal));
				msg.run ();
				ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
				continue;
			}

			_session_is_new = true;
		}

		if (likely_new && template_name.empty()) {

			ret = build_session_from_dialog (_session_dialog, session_path, session_name);

		} else {

			ret = load_session (session_path, session_name, template_name);

			if (ret == -2) {
				/* not connected to the AudioEngine, so quit to avoid an infinite loop */
				exit (1);
			}

			if (!ARDOUR_COMMAND_LINE::immediate_save.empty()) {
				_session->save_state (ARDOUR_COMMAND_LINE::immediate_save, false);
				exit (1);
			}

			/* clear this to avoid endless attempts to load the
			   same session.
			*/

			ARDOUR_COMMAND_LINE::session_name = "";
            
            if (ret == 0)
                return ret;
		}
	}

    _progress_dialog.set_progress (1);
    _progress_dialog.hide_pd ();

    goto_editor_window ();
    
	return ret;
}

void
ARDOUR_UI::close_session()
{
	if (!check_audioengine()) {
		return;
	}

	if (unload_session (true)) {
		return;
	}

	ARDOUR_COMMAND_LINE::session_name = "";

	if (get_session_parameters (true, false)) {
		UI::quit ();
	}
}

/** @param snap_name Snapshot name (without .ardour suffix).
 *  @return -2 if the load failed because we are not connected to the AudioEngine.
 */
int
ARDOUR_UI::load_session (const std::string& path, const std::string& snap_name, std::string mix_template)
{
    _progress_dialog.set_top_label ("Loading session: "+snap_name);
    _progress_dialog.update_info (0.0, NULL, NULL, "Loading audio...");
    _progress_dialog.show_pd ();
    
	Session *new_session;
	int unload_status;
	int retval = -1;

	if (_session) {
		unload_status = unload_session ();
		
		if (unload_status < 0) {
			goto out;
		} else if (unload_status > 0) {
			retval = 0;
			goto out;
		}
	}

	session_loaded = false;

    _progress_dialog.set_progress (0.1);
	try {
		new_session = new Session (*AudioEngine::instance(), path, snap_name, 0, mix_template);
        _progress_dialog.update_info (0.4, NULL, NULL, "Loading elements...");
	}

	/* this one is special */

	catch (AudioEngine::PortRegistrationFailure& err) {

		WavesMessageDialog msg ("Port Registration Error", string( err.what() ) + "\nClick the Close button to try again.", WavesMessageDialog::BUTTON_CLOSE );
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (msg);
		msg.present ();
		int response = msg.run ();
		msg.hide ();
		switch (response) {
		case RESPONSE_CLOSE:
			exit (1);
		default:
			break;
		}
		goto out;
	}

	catch (...) {

        WavesMessageDialog message_dialog ("Loading Error", string_compose(_("Session \"%1 (snapshot %2)\"\ndid not load successfully"), path, snap_name));
        message_dialog.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash (message_dialog);
		message_dialog.run ();
        
		goto out;
	}

	{
		list<string> const u = new_session->unknown_processors ();
		if (!u.empty()) {
			MissingPluginDialog d (_session, u);
			d.run ();
		}
	}
    
	if (!new_session->writable()) {
        
        WavesMessageDialog read_only_session_dialog ("",
                                                     "This session has been opened in read-only mode.\nYou will not be able to record or save.",
                                                     WavesMessageDialog::BUTTON_OK);
        read_only_session_dialog.run ();
        read_only_session_dialog.set_keep_above (true);
        read_only_session_dialog.set_position (Gtk::WIN_POS_CENTER);
	}

	/* Now the session been created, add the transport controls */
	new_session->add_controllable(roll_controllable);
	new_session->add_controllable(stop_controllable);
	new_session->add_controllable(goto_start_controllable);
	new_session->add_controllable(goto_end_controllable);
	new_session->add_controllable(auto_loop_controllable);
	new_session->add_controllable(play_selection_controllable);
	new_session->add_controllable(rec_controllable);

	set_session (new_session);

	session_loaded = true;
    
    tracks_control_panel->refresh_session_settings_info();

    _progress_dialog.set_progress (1);
    _progress_dialog.hide_pd ();
	goto_editor_window ();

#ifdef WINDOWS_VST_SUPPORT
	fst_stop_threading();
#endif
	flush_pending ();
#ifdef WINDOWS_VST_SUPPORT
	fst_start_threading();
#endif
    
    if (_session) {
        _session->set_clean ();
    }
    
	retval = 0;

  out:
   _progress_dialog.set_progress (1);
   _progress_dialog.hide_pd ();
	return retval;
}

int
ARDOUR_UI::build_session (const std::string& path, const std::string& snap_name, BusProfile& bus_profile)
{
	Session *new_session;
	int x;

	session_loaded = false;
	x = unload_session ();

	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 0;
	}

	_session_is_new = true;

	try {
		new_session = new Session (*AudioEngine::instance(), path, snap_name, &bus_profile);
	}

	catch (...) {

		WavesMessageDialog msg ("", string_compose(_("Could not create session in \"%1\""), path));
		pop_back_splash (msg);
		msg.run ();
		return -1;
	}

	/* Give the new session the default GUI state, if such things exist */

	XMLNode* n;
	n = Config->instant_xml (X_("Editor"));
	if (n) {
		new_session->add_instant_xml (*n, false);
	}
	n = Config->instant_xml (X_("Mixer"));
	if (n) {
		new_session->add_instant_xml (*n, false);
	}

	/* Put the playhead at 0 and scroll fully left */
	n = new_session->instant_xml (X_("Editor"));
	if (n) {
		n->add_property (X_("playhead"), X_("0"));
		n->add_property (X_("left-frame"), X_("0"));
	}

    if (new_session) {
        new_session->config.set_native_file_header_format (this->_header_format);
        new_session->config.set_native_file_data_format   (this->_sample_format);
        new_session->config.set_timecode_format (this->_timecode_format);
    }
    
	set_session (new_session);
    session_loaded = true;
    
	new_session->save_state(new_session->name());

	return 0;
}

void
ARDOUR_UI::launch_chat ()
{
#ifdef __APPLE__
	open_uri("http://webchat.freenode.net/?channels=ardour-osx");
#else
	open_uri("http://webchat.freenode.net/?channels=ardour");
#endif
}

void
ARDOUR_UI::launch_manual ()
{
	PBD::open_uri (Config->get_tutorial_manual_url());
}

void
ARDOUR_UI::launch_reference ()
{
	PBD::open_uri (Config->get_reference_manual_url());
}

void
ARDOUR_UI::loading_message (const std::string& msg)
{
	if (ARDOUR_COMMAND_LINE::no_splash) {
		return;
	}

	if (!splash) {
		show_splash ();
	}

	splash->message (msg);
}

void
ARDOUR_UI::show_splash ()
{
	if (splash == 0) {
		try {
			splash = new Splash;
		} catch (...) {
			return;
		}
	}

	splash->display ();
}

void
ARDOUR_UI::hide_splash ()
{
        delete splash;
        splash = 0;
}

void
ARDOUR_UI::display_cleanup_results (ARDOUR::CleanupReport& rep, const gchar* list_title, const bool msg_delete)
{
	size_t removed;

	removed = rep.paths.size();

	if (removed == 0) {
		WavesMessageDialog msgd ("", _("Nothing to clean-up."));
		
		msgd.run ();
		return;
	}

	return;

	ArdourDialog results (_("Clean-up"), true, false);

	struct CleanupResultsModelColumns : public Gtk::TreeModel::ColumnRecord {
	    CleanupResultsModelColumns() {
		    add (visible_name);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<std::string> visible_name;
	    Gtk::TreeModelColumn<std::string> fullpath;
	};


	CleanupResultsModelColumns results_columns;
	Glib::RefPtr<Gtk::ListStore> results_model;
	Gtk::TreeView results_display;

	results_model = ListStore::create (results_columns);
	results_display.set_model (results_model);
	results_display.append_column (list_title, results_columns.visible_name);

	results_display.set_name ("CleanupResultsList");
	results_display.set_headers_visible (true);
	results_display.set_headers_clickable (false);
	results_display.set_reorderable (false);

	Gtk::ScrolledWindow list_scroller;
	Gtk::Label txt;
	Gtk::VBox dvbox;
	Gtk::HBox dhbox;  // the hbox for the image and text
	Gtk::HBox ddhbox; // the hbox we eventually pack into the dialog's vbox
	Gtk::Image* dimage = manage (new Gtk::Image(Stock::DIALOG_INFO,  Gtk::ICON_SIZE_DIALOG));

	dimage->set_alignment(ALIGN_LEFT, ALIGN_TOP);

	const string dead_directory = _session->session_directory().dead_path();

	/* subst:
	   %1 - number of files removed
	   %2 - location of "dead"
	   %3 - size of files affected
	   %4 - prefix for "bytes" to produce sensible results (e.g. mega, kilo, giga)
	*/

	const char* bprefix;
	double space_adjusted = 0;

	if (rep.space < 1000) {
		bprefix = X_("");
		space_adjusted = rep.space;
	} else if (rep.space < 1000000) {
		bprefix = _("kilo");
		space_adjusted = floorf((float)rep.space / 1000.0);
	} else if (rep.space < 1000000 * 1000) {
		bprefix = _("mega");
		space_adjusted = floorf((float)rep.space / (1000.0 * 1000.0));
	} else {
		bprefix = _("giga");
		space_adjusted = floorf((float)rep.space / (1000.0 * 1000 * 1000.0));
	}

	if (msg_delete) {
		txt.set_markup (string_compose (P_("\
The following file was deleted from %2,\n\
releasing %3 %4bytes of disk space", "\
The following %1 files were deleted from %2,\n\
releasing %3 %4bytes of disk space", removed),
					removed, Glib::Markup::escape_text (dead_directory), space_adjusted, bprefix, PROGRAM_NAME));
	} else {
		txt.set_markup (string_compose (P_("\
The following file was not in use and \n\
has been moved to: %2\n\n\
After a restart of %5\n\n\
<span face=\"mono\">Session -> Clean-up -> Flush Wastebasket</span>\n\n\
will release an additional %3 %4bytes of disk space.\n", "\
The following %1 files were not in use and \n\
have been moved to: %2\n\n\
After a restart of %5\n\n\
<span face=\"mono\">Session -> Clean-up -> Flush Wastebasket</span>\n\n\
will release an additional %3 %4bytes of disk space.\n", removed),
					removed, Glib::Markup::escape_text (dead_directory), space_adjusted, bprefix, PROGRAM_NAME));
	}

	dhbox.pack_start (*dimage, true, false, 5);
	dhbox.pack_start (txt, true, false, 5);

	for (vector<string>::iterator i = rep.paths.begin(); i != rep.paths.end(); ++i) {
		TreeModel::Row row = *(results_model->append());
		row[results_columns.visible_name] = *i;
		row[results_columns.fullpath] = *i;
	}

	list_scroller.add (results_display);
	list_scroller.set_size_request (-1, 150);
	list_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	dvbox.pack_start (dhbox, true, false, 5);
	dvbox.pack_start (list_scroller, true, false, 5);
	ddhbox.pack_start (dvbox, true, false, 5);

	results.get_vbox()->pack_start (ddhbox, true, false, 5);
	results.add_button ("CLOSE", RESPONSE_CLOSE);
	results.set_default_response (RESPONSE_CLOSE);
	results.set_position (Gtk::WIN_POS_MOUSE);

	results_display.show();
	list_scroller.show();
	txt.show();
	dvbox.show();
	dhbox.show();
	ddhbox.show();
	dimage->show();

	//results.get_vbox()->show();
	results.set_resizable (false);

	results.run ();

}

void
ARDOUR_UI::cleanup ()
{
	if (_session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}
	   
    WavesMessageDialog msg("waves_clean_up_dialog.xml","", _("Are you sure want to clean-up?\n Clean-up is a destructive operation.\n\
All data and undo/redo information will be lost."),
                         WavesMessageDialog::BUTTON_CANCEL | WavesMessageDialog::BUTTON_ACCEPT);

	switch (msg.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}
    
    WavesMessageDialog msg2 ("", "This operation can not be undone, proceed", WavesMessageDialog::BUTTON_YES | WavesMessageDialog::BUTTON_NO );
    switch (msg2.run()) {
        case RESPONSE_YES:
            break;
        default:
            return;
    };

	ARDOUR::CleanupReport rep;

	editor->prepare_for_cleanup ();

	/* do not allow flush until a session is reloaded */

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("FlushWastebasket"));
	if (act) {
		act->set_sensitive (false);
	}

    // move unused sources to dead folder
 	if (_session->cleanup_sources (rep)) {
		editor->finish_cleanup ();
		return;
	}

	editor->finish_cleanup ();

    // clear dead folder
    if (_session->cleanup_trash_sources (rep)) {
        return;
    }

    msg.hide ();
	display_cleanup_results (rep, _("Cleaned Files"), false);
}

void
ARDOUR_UI::open_dead_folder ()
{
    const string dead_directory = _session->session_directory().dead_path();

#if defined (PLATFORM_WINDOWS)
   ShellExecute (NULL, "open", dead_directory.c_str(), NULL, NULL, SW_SHOW);
#elif defined (__APPLE__)
    std::string command = "open \"" + dead_directory + "\"";
    system (command.c_str ());
#else
    
    /* nix */
    /* XXX what to do here ? */
    
#endif
}

void
ARDOUR_UI::flush_trash ()
{
	if (_session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}

	ARDOUR::CleanupReport rep;

	if (_session->cleanup_trash_sources (rep)) {
		return;
	}

	display_cleanup_results (rep, _("deleted file"), true);
}

void
ARDOUR_UI::setup_order_hint ()
{
	uint32_t order_hint = 0;

	/*
	  we want the new routes to have their order keys set starting from 
	  the highest order key in the selection + 1 (if available).
	*/

	for (TrackSelection::iterator s = editor->get_selection().tracks.begin(); s != editor->get_selection().tracks.end(); ++s) {
		RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (*s);
		if (tav && tav->route() && tav->route()->order_key() > order_hint) {
			order_hint = tav->route()->order_key();
		}
	}
    
    
    /* If no tracks are selected or only master bus is selected do not create a gap in the existing route order keys.
     Set order hint to default.*/
    if (editor->get_selection().tracks.empty ()) {
        _session->set_order_hint (Session::default_order_hint);
        return;
    } else if (editor->get_selection().tracks.size () == 1) {
        TimeAxisView* selected_tav = editor->get_selection().tracks.front();
        RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (selected_tav);
        if (tav && tav->route() && tav->route()->is_master () ) {
            _session->set_order_hint (Session::default_order_hint);
            return;
        }
    }

	order_hint++;
    _session->set_order_hint (order_hint);
	
	/* create a gap in the existing route order keys to accomodate new routes.*/

	boost::shared_ptr <RouteList> rd = _session->get_routes();
	for (RouteList::iterator ri = rd->begin(); ri != rd->end(); ++ri) {
		boost::shared_ptr<Route> rt (*ri);
			
		if (rt->is_monitor()) {
			continue;
		}

		if (rt->order_key () >= order_hint) {
			rt->set_order_key (rt->order_key () + _add_tracks_dialog->get_track_count());
		}
	}
}

void
ARDOUR_UI::add_route (Gtk::Window* float_window)
{
	if (!_session) {
		return;
	}

	unsigned int existing_tracks_count = _session->get_tracks ()->size ();
	if (existing_tracks_count >= _add_tracks_dialog->max_tracks_count ()) {
		WavesMessageDialog("", "Impossible to add more tracks!").run ();
		return;
	}

	_add_tracks_dialog->setup(_add_tracks_dialog->max_tracks_count () - existing_tracks_count);
	_add_tracks_dialog->set_position (WIN_POS_CENTER);
    

    // disable Main menu
    ActionDisabler m; // HOT FIX. (REWORK IT)

    int r = _add_tracks_dialog->run();
   
	switch (r) {
		case WavesDialog::RESPONSE_DEFAULT:
			break;
		default:
			return;
	}
    
	setup_order_hint();

	ChanCount input_chan = _add_tracks_dialog->input_channels ();
	//DisplaySuspender ds;
	ChanCount output_chan;

    // NP: output_channels amount will be validated and changed accordingly to Master BUS config in Session::reconnect_existing_routes
    // during AudioTracks auto connection process
	/*if (Config->get_output_auto_connect() & AutoConnectMaster) {
        output_chan.set (DataType::AUDIO, (_session->master_out() ? _session->master_out()->n_inputs().n_audio() : input_chan.n_audio()));
		output_chan.set (DataType::MIDI, 0);
	} else */ {
		output_chan = input_chan;
	}

 
    _progress_dialog.set_top_label ("Adding tracks...");
    _progress_dialog.set_num_of_steps (_add_tracks_dialog->get_track_count () * 4);
    _progress_dialog.show_pd ();
	
    session_add_audio_route (true, input_chan.n_audio(), output_chan.n_audio(), ARDOUR::Normal, NULL, _add_tracks_dialog->get_track_count (), "");

	_progress_dialog.hide_pd ();
}

void
ARDOUR_UI::delete_selected_tracks()
{   
    if (!editor) {
        return;
    }
    
    TrackSelection& track_selection =  editor->get_selection().tracks;
	bool run_confirmation_dialog = false;
    for (list<TimeAxisView*>::iterator i = track_selection.begin(); i != track_selection.end(); ++i) {
        RouteTimeAxisView* t = dynamic_cast<RouteTimeAxisView*> (*i);
        if (t->has_regionviews ()) {
            run_confirmation_dialog = true;
            break;
        }
    }
    
    if (run_confirmation_dialog) {
        WavesMessageDialog confirmation_dialog ("waves_track_delete_dialog.xml","", "",
                                                WavesMessageDialog::BUTTON_OK | WavesMessageDialog::BUTTON_CANCEL);
        int response = confirmation_dialog.run ();
        if (response != Gtk::RESPONSE_OK && response != WavesDialog::RESPONSE_DEFAULT) {
            return;
        }
    }
	DisplaySuspender ds;
    editor->get_selection().block_tracks_changed (true);
    
    MixerBridgeView& mixer_view = editor->get_mixer_bridge ();
    mixer_view.selection().block_routes_changed(true);
    
    MixerBridgeView& meter_view = editor->get_meter_bridge ();
    meter_view.selection().block_routes_changed(true);
    
    boost::shared_ptr<RouteList> routes_to_remove(new RouteList);
    for (list<TimeAxisView*>::iterator i = track_selection.begin(); i != track_selection.end(); ++i) {
        RouteUI* t = dynamic_cast<RouteUI*> (*i);
        if (t) {
            if ( t->route()->is_master() || t->route()->is_monitor() )
                continue;
            
            routes_to_remove->push_back(t->route() );
        }
    }

  
    _progress_dialog.set_top_label ("Removing tracks...");
    _progress_dialog.set_num_of_steps (routes_to_remove->size ());
    _progress_dialog.show_pd ();
	
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if (session) {
        Session::StateProtector sp (session);
        session->remove_routes (routes_to_remove);
        routes_to_remove->clear ();
    }
    
    /* restore selection notifications and update the selection */
    editor->get_selection().block_tracks_changed (false);
    mixer_view.selection().block_routes_changed(false);
    meter_view.selection().block_routes_changed(false);
    editor->get_selection().TracksChanged();
    editor->update_edit_selection_menu ();
    
    _progress_dialog.hide_pd ();
}

void
ARDOUR_UI::add_audio_track_instantly ()
{    
	if (!_session) {
		return;
	}
    
    if (_session->actively_recording () && _session->have_rec_enabled_track () ) {
        return;
    }
    
	if (_add_tracks_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	if (_session->get_tracks ()->size () >= _add_tracks_dialog->max_tracks_count ()) {
		WavesMessageDialog("", "Impossible to add more tracks!").run ();
		return;
	}
    
	_session->new_audio_track (1, 1, Normal, 0, 1, string() );
}


void
ARDOUR_UI::stop_video_server (bool ask_confirm)
{
	if (!video_server_process && ask_confirm) {
		warning << string_compose (_("Video-Server was not launched by %1. The request to stop it is ignored."), PROGRAM_NAME) << endmsg;
	}
	if (video_server_process) {
		if(ask_confirm) {
			ArdourDialog confirm (_("Stop Video-Server"), true);
			Label m (_("Do you really want to stop the Video Server?"));
			confirm.get_vbox()->pack_start (m, true, true);
			confirm.add_button ("CANCEL", Gtk::RESPONSE_CANCEL);
			confirm.add_button (_("Yes, Stop It"), Gtk::RESPONSE_ACCEPT);
			confirm.show_all ();
			if (confirm.run() == RESPONSE_CANCEL) {
				return;
			}
		}
		delete video_server_process;
		video_server_process =0;
	}
}

void
ARDOUR_UI::start_video_server_menu (Gtk::Window* float_window)
{
  ARDOUR_UI::start_video_server( float_window, true);
}

bool
ARDOUR_UI::start_video_server (Gtk::Window* float_window, bool popup_msg)
{
	//Tracks live doesn't use this func,
	//if we need this function 
	//we must replace g_lstat on stat for windows or lstat for other OS
	if (!_session) {
		return false;
	}
	if (popup_msg) {
		if (ARDOUR_UI::instance()->video_timeline->check_server()) {
			if (video_server_process) {
				popup_error(_("The Video Server is already started."));
			} else {
				popup_error(_("An external Video Server is configured and can be reached. Not starting a new instance."));
			}
		}
	}

	int firsttime = 0;
	while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
		if (firsttime++) {
			warning << _("Could not connect to the Video Server. Start it or configure its access URL in Edit -> Preferences.") << endmsg;
		}
		VideoServerDialog *video_server_dialog = new VideoServerDialog (_session);
		if (float_window) {
			video_server_dialog->set_transient_for (*float_window);
		}

		if (!Config->get_show_video_server_dialog() && firsttime < 2) {
			video_server_dialog->hide();
		} else {
			ResponseType r = (ResponseType) video_server_dialog->run ();
			video_server_dialog->hide();
			if (r != RESPONSE_ACCEPT) { return false; }
			if (video_server_dialog->show_again()) {
				Config->set_show_video_server_dialog(false);
			}
		}

		std::string icsd_exec = video_server_dialog->get_exec_path();
		std::string icsd_docroot = video_server_dialog->get_docroot();
		if (icsd_docroot.empty()) {icsd_docroot = X_("/");}

		GStatBuf sb;
		if (!g_lstat (icsd_docroot.c_str(), &sb) == 0 || !S_ISDIR(sb.st_mode)) {
			warning << _("Specified docroot is not an existing directory.") << endmsg;
			continue;
		}
#ifndef PLATFORM_WINDOWS
		if ( (!g_lstat (icsd_exec.c_str(), &sb) == 0)
		     || (sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#else
		if ( (!g_lstat (icsd_exec.c_str(), &sb) == 0)
		     || (sb.st_mode & (S_IXUSR)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#endif

		char **argp;
		argp=(char**) calloc(9,sizeof(char*));
		argp[0] = strdup(icsd_exec.c_str());
		argp[1] = strdup("-P");
		argp[2] = (char*) calloc(16,sizeof(char)); snprintf(argp[2], 16, "%s", video_server_dialog->get_listenaddr().c_str());
		argp[3] = strdup("-p");
		argp[4] = (char*) calloc(6,sizeof(char)); snprintf(argp[4], 6, "%i", video_server_dialog->get_listenport());
		argp[5] = strdup("-C");
		argp[6] = (char*) calloc(6,sizeof(char)); snprintf(argp[6], 6, "%i", video_server_dialog->get_cachesize());
		argp[7] = strdup(icsd_docroot.c_str());
		argp[8] = 0;
		stop_video_server();

		if (icsd_docroot == X_("/")) {
			Config->set_video_advanced_setup(false);
		} else {
			std::ostringstream osstream;
			osstream << "http://localhost:" << video_server_dialog->get_listenport() << "/";
			Config->set_video_server_url(osstream.str());
			Config->set_video_server_docroot(icsd_docroot);
			Config->set_video_advanced_setup(true);
		}

		if (video_server_process) {
			delete video_server_process;
		}

		video_server_process = new ARDOUR::SystemExec(icsd_exec, argp);
		if (video_server_process->start()) {
			warning << _("Cannot launch the video-server") << endmsg;
			continue;
		}
		int timeout = 120; // 6 sec
		while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
			Glib::usleep (50000);
			gui_idle_handler();
			if (--timeout <= 0 || !video_server_process->is_running()) break;
		}
		if (timeout <= 0) {
			warning << _("Video-server was started but does not respond to requests...") << endmsg;
		} else {
			if (!ARDOUR_UI::instance()->video_timeline->check_server_docroot()) {
				delete video_server_process;
				video_server_process = 0;
			}
		}
	}
	return true;
}

void
ARDOUR_UI::add_video (Gtk::Window* float_window)
{
	if (!_session) {
		return;
	}

	if (!start_video_server(float_window, false)) {
		warning << _("Could not connect to the Video Server. Start it or configure its access URL in Edit -> Preferences.") << endmsg;
		return;
	}

	if (float_window) {
		add_video_dialog->set_transient_for (*float_window);
	}

	if (add_video_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	ResponseType r = (ResponseType) add_video_dialog->run ();
	add_video_dialog->hide();
	if (r != RESPONSE_ACCEPT) { return; }

	bool local_file, orig_local_file;
	std::string path = add_video_dialog->file_name(local_file);

	std::string orig_path = path;
	orig_local_file = local_file;

	bool auto_set_session_fps = add_video_dialog->auto_set_session_fps();

	if (local_file && !Glib::file_test(path, Glib::FILE_TEST_EXISTS)) {
		warning << string_compose(_("could not open %1"), path) << endmsg;
		return;
	}
	if (!local_file && path.length() == 0) {
		warning << _("no video-file selected") << endmsg;
		return;
	}

	switch (add_video_dialog->import_option()) {
		case VTL_IMPORT_TRANSCODE:
			{
				TranscodeVideoDialog *transcode_video_dialog;
				transcode_video_dialog = new TranscodeVideoDialog (_session, path);
				ResponseType r = (ResponseType) transcode_video_dialog->run ();
				transcode_video_dialog->hide();
				if (r != RESPONSE_ACCEPT) {
					delete transcode_video_dialog;
					return;
				}
				if (!transcode_video_dialog->get_audiofile().empty()) {
					editor->embed_audio_from_video(
							transcode_video_dialog->get_audiofile(),
							video_timeline->get_offset(),
							(transcode_video_dialog->import_option() != VTL_IMPORT_NO_VIDEO)
							);
				}
				switch (transcode_video_dialog->import_option()) {
					case VTL_IMPORT_TRANSCODED:
						path = transcode_video_dialog->get_filename();
						local_file = true;
						break;
					case VTL_IMPORT_REFERENCE:
						break;
					default:
						delete transcode_video_dialog;
						return;
				}
				delete transcode_video_dialog;
			}
			break;
		default:
		case VTL_IMPORT_NONE:
			break;
	}

	/* strip _session->session_directory().video_path() from video file if possible */
	if (local_file && !path.compare(0, _session->session_directory().video_path().size(), _session->session_directory().video_path())) {
		 path=path.substr(_session->session_directory().video_path().size());
		 if (path.at(0) == G_DIR_SEPARATOR) {
			 path=path.substr(1);
		 }
	}

	video_timeline->set_update_session_fps(auto_set_session_fps);
	if (video_timeline->video_file_info(path, local_file)) {
		XMLNode* node = new XMLNode(X_("Videotimeline"));
		node->add_property (X_("Filename"), path);
		node->add_property (X_("AutoFPS"), auto_set_session_fps?X_("1"):X_("0"));
		node->add_property (X_("LocalFile"), local_file?X_("1"):X_("0"));
		if (orig_local_file) {
			node->add_property (X_("OriginalVideoFile"), orig_path);
		} else {
			node->remove_property (X_("OriginalVideoFile"));
		}
		_session->add_extra_xml (*node);
		_session->set_dirty ();

		_session->maybe_update_session_range(
			std::max(video_timeline->get_offset(), (ARDOUR::frameoffset_t) 0),
			std::max(video_timeline->get_offset() + video_timeline->get_duration(), (ARDOUR::frameoffset_t) 0));


		if (add_video_dialog->launch_xjadeo() && local_file) {
			editor->set_xjadeo_sensitive(true);
			editor->toggle_xjadeo_proc(1);
		} else {
			editor->toggle_xjadeo_proc(0);
		}
		editor->toggle_ruler_video(true);
	}
}

void
ARDOUR_UI::remove_video ()
{
	video_timeline->close_session();
	editor->toggle_ruler_video(false);

	/* reset state */
	video_timeline->set_offset_locked(false);
	video_timeline->set_offset(0);

	/* delete session state */
	XMLNode* node = new XMLNode(X_("Videotimeline"));
	_session->add_extra_xml(*node);
	node = new XMLNode(X_("Videomonitor"));
	_session->add_extra_xml(*node);
	stop_video_server();
}

void
ARDOUR_UI::flush_videotimeline_cache (bool localcacheonly)
{
	if (localcacheonly) {
		video_timeline->vmon_update();
	} else {
		video_timeline->flush_cache();
	}
	editor->queue_visual_videotimeline_update();
}

XMLNode*
ARDOUR_UI::mixer_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Mixer"));
	} else {
		node = Config->instant_xml(X_("Mixer"));
	}

	if (!node) {
		node = new XMLNode (X_("Mixer"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::editor_settings () const
{
	XMLNode* node = 0;

	if (_session) {
		node = _session->instant_xml(X_("Editor"));
	} else {
		node = Config->instant_xml(X_("Editor"));
	}

	if (!node) {
		if (getenv("ARDOUR_INSTANT_XML_PATH")) {
			node = Config->instant_xml(getenv("ARDOUR_INSTANT_XML_PATH"));
		}
	}

	if (!node) {
		node = new XMLNode (X_("Editor"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::keyboard_settings () const
{
	XMLNode* node = 0;

	node = Config->extra_xml(X_("Keyboard"));

	if (!node) {
		node = new XMLNode (X_("Keyboard"));
	}

	return node;
}

void
ARDOUR_UI::create_xrun_marker (framepos_t where)
{
	editor->mouse_add_new_marker (where, false, true);
}

void
ARDOUR_UI::halt_on_xrun_message ()
{
	WavesMessageDialog msg ("", _("Recording was stopped because your system could not keep up."));
	msg.run ();
}

void
ARDOUR_UI::xrun_handler (framepos_t where)
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::xrun_handler, where)

	if (_session && Config->get_create_xrun_marker() && _session->actively_recording() ) {
		create_xrun_marker(where);
	}

	if (_session && Config->get_stop_recording_on_xrun() && _session->actively_recording() ) {
		halt_on_xrun_message ();
	}
}

void
ARDOUR_UI::disk_overrun_handler ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::disk_overrun_handler)

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		WavesMessageDialog* msg = new WavesMessageDialog ("", string_compose (_("\
The disk system on your computer\n\
was not able to keep up with %1.\n\
\n\
Specifically, it failed to write data to disk\n\
quickly enough to keep up with recording.\n"), PROGRAM_NAME));
		msg->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show ();
	}
}


/* TODO: this is getting elaborate enough to warrant being split into a dedicated class */
static MessageDialog *scan_dlg = NULL;
static ProgressBar   *scan_pbar = NULL;
static HBox          *scan_tbox = NULL;

void
ARDOUR_UI::cancel_plugin_scan ()
{
	PluginManager::instance().cancel_plugin_scan();
}

void
ARDOUR_UI::cancel_plugin_timeout ()
{
	PluginManager::instance().cancel_plugin_timeout();
	scan_tbox->hide();
}

void
ARDOUR_UI::plugin_scan_timeout (int timeout)
{
	if (!scan_dlg || !scan_dlg->is_mapped() || !scan_pbar) {
		return;
	}
	if (timeout > 0) {
		scan_pbar->set_fraction ((float) timeout / (float) Config->get_vst_scan_timeout());
		scan_tbox->show();
	} else {
		scan_tbox->hide();
	}
	gui_idle_handler();
}

void
ARDOUR_UI::plugin_scan_dialog (std::string type, std::string plugin, bool can_cancel)
{
	if (type == X_("closeme") && !(scan_dlg && scan_dlg->is_mapped())) {
		return;
	}

	const bool cancelled = PluginManager::instance().cancelled();
	if (type != X_("closeme") && !ARDOUR_UI::config()->get_show_plugin_scan_window()) {
		if (cancelled && scan_dlg->is_mapped()) {
			scan_dlg->hide();
			gui_idle_handler();
			return;
		}
		if (cancelled || !can_cancel) {
			return;
		}
	}

	static Gtk::Button *cancel_button;
	static Gtk::Button *timeout_button;
	if (!scan_dlg) {
		scan_dlg = new MessageDialog("", false, MESSAGE_INFO, BUTTONS_NONE); // TODO manage
		VBox* vbox = scan_dlg->get_vbox();
		vbox->set_size_request(400,-1);
		scan_dlg->set_title (_("Scanning for plugins"));

		cancel_button = manage(new Gtk::Button(_("Cancel plugin scan")));
		cancel_button->set_name ("EditorGTKButton");
		cancel_button->signal_clicked().connect ( mem_fun (*this, &ARDOUR_UI::cancel_plugin_scan) );
		cancel_button->show();

		scan_dlg->get_vbox()->pack_start ( *cancel_button, PACK_SHRINK);

		scan_tbox = manage( new HBox() );

		timeout_button = manage(new Gtk::Button(_("Stop Timeout")));
		timeout_button->set_name ("EditorGTKButton");
		timeout_button->signal_clicked().connect ( mem_fun (*this, &ARDOUR_UI::cancel_plugin_timeout) );
		timeout_button->show();

		scan_pbar = manage(new ProgressBar());
		scan_pbar->set_orientation(Gtk::PROGRESS_RIGHT_TO_LEFT);
		scan_pbar->set_text(_("Scan Timeout"));
		scan_pbar->show();

		scan_tbox->pack_start (*scan_pbar, PACK_EXPAND_WIDGET, 4);
		scan_tbox->pack_start (*timeout_button, PACK_SHRINK, 4);

		scan_dlg->get_vbox()->pack_start (*scan_tbox, PACK_SHRINK, 4);
	}

	if (type == X_("closeme")) {
		scan_dlg->hide();
	} else {
		scan_dlg->set_message(type + ": " + Glib::path_get_basename(plugin));
		scan_dlg->show();
	}
	if (!can_cancel || !cancelled) {
		scan_tbox->hide();
	}
	cancel_button->set_sensitive(can_cancel && !cancelled);

	gui_idle_handler();
}

void
ARDOUR_UI::gui_idle_handler ()
{
	int timeout = 30;
	/* due to idle calls, gtk_events_pending() may always return true */
	while (gtk_events_pending() && --timeout) {
		gtk_main_iteration ();
	}
}

void
ARDOUR_UI::disk_underrun_handler ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::disk_underrun_handler)

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		WavesMessageDialog* msg = new WavesMessageDialog (
			"", string_compose (_("The disk system on your computer\n\
was not able to keep up with %1.\n\
\n\
Specifically, it failed to read data from disk\n\
quickly enough to keep up with playback.\n"), PROGRAM_NAME));
		msg->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show ();
	}
}

void
ARDOUR_UI::disk_speed_dialog_gone (int /*ignored_response*/, WavesMessageDialog* msg)
{
	have_disk_speed_dialog_displayed = false;
	delete msg;
}

void
ARDOUR_UI::session_dialog (std::string msg)
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::session_dialog, msg)

	MessageDialog* d;

	if (editor) {
		d = new MessageDialog (*editor, msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	} else {
		d = new MessageDialog (msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	}

	d->show_all ();
	d->run ();
	delete d;
}

int
ARDOUR_UI::pending_state_dialog ()
{
    WavesMessageDialog message_dialog ("crash_recovery_dialog.xml",
									   _("Crash Recovery"), 
									   string_compose (_(
"This session appears to have been in the\n\
middle of recording when %1 or\n\
the computer was shutdown.\n\
\n\
%1 can recover any captured audio for\n\
you, or it can ignore it. Please decide\n\
what you would like to do.\n"), PROGRAM_NAME),
									  WavesMessageDialog::BUTTON_ACCEPT |
									  WavesMessageDialog::BUTTON_NO);
    
	switch (message_dialog.run ()) {
	case WavesDialog::RESPONSE_DEFAULT:
	case RESPONSE_ACCEPT:
		return 1;
	default:
		return 0;
	}
}

int
ARDOUR_UI::sr_mismatch_dialog (framecnt_t desired, framecnt_t actual)
{
// Tracks doesn't use it
/*
     WavesMessageDialog message_dialog (_("Sample Rate Mismatch"),
										string_compose (_("\
This session was created with a sample rate of %1 Hz, but\n\
%2 is currently running at %3 Hz. If you load this session,\n\
device will be switched to the session sample rate value. \n\
If an attemp to switch the device is unsuccessful\n\
audio may be played at the wrong sample rate.\n"),
														desired,
														PROGRAM_NAME,
														actual),
										WavesMessageDialog::BUTTON_ACCEPT|WavesMessageDialog::BUTTON_CANCEL);
    message_dialog.set_position (WIN_POS_CENTER);
    
	int result = message_dialog.run ();
*/
    int result = Gtk::RESPONSE_ACCEPT;
	return (result == Gtk::RESPONSE_ACCEPT || result == WavesDialog::RESPONSE_DEFAULT) ? 0 : 1;
}

int
ARDOUR_UI::disconnect_from_engine ()
{
	/* drop connection to AudioEngine::Halted so that we don't act
	 *  as if the engine unexpectedly shut down
	 */

	halt_connection.disconnect ();
	
	if (AudioEngine::instance()->stop ()) {
		WavesMessageDialog msg ("", _("Could not disconnect from Audio/MIDI engine"));
		msg.run ();
		return -1;
	} else {
		AudioEngine::instance()->Halted.connect_same_thread (halt_connection, boost::bind (&ARDOUR_UI::engine_halted, this, _1, false));
	}

	return 0;
}

int
ARDOUR_UI::reconnect_to_engine ()
{
	if (AudioEngine::instance()->start ()) {
		WavesMessageDialog msg ("", _("Could not reconnect to the Audio/MIDI engine"));
		msg.run ();

		return -1;
	}
	
	return 0;
}

void
ARDOUR_UI::use_config ()
{
	XMLNode* node = Config->extra_xml (X_("TransportControllables"));
	if (node) {
		set_transport_controllable_state (*node);
	}
}

void
ARDOUR_UI::update_transport_clocks (framepos_t pos)
{
	if (ui_config->get_primary_clock_delta_edit_cursor()) {
		primary_clock->set (pos, false, editor->get_preferred_edit_position());
	} else {
		primary_clock->set (pos);
	}

	if (ui_config->get_secondary_clock_delta_edit_cursor()) {
		secondary_clock->set (pos, false, editor->get_preferred_edit_position());
	} else {
		secondary_clock->set (pos);
	}

	if (big_clock_window) {
		big_clock->set (pos);
	}
	ARDOUR_UI::instance()->video_timeline->manual_seek_video_monitor(pos);
}

void
ARDOUR_UI::step_edit_status_change (bool yn)
{
	// XXX should really store pre-step edit status of things
	// we make insensitive
	
	if (yn) {
		editor->get_waves_button ("transport_record_button").set_active_state (Gtkmm2ext::ImplicitActive);
		editor->get_waves_button ("transport_record_button").set_sensitive (false);
	} else {
		editor->get_waves_button ("transport_record_button").set_active (false);
		editor->get_waves_button ("transport_record_button").set_sensitive (true);
	}
}

void
ARDOUR_UI::record_state_changed ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::record_state_changed);

	if (!_session ) {
		return;
	}

	if (_session->actively_recording () && _session->have_rec_enabled_track ()) {

        // The same restriction actions are handled when transport starts/stops
        //tracks_control_panel.action()->set_sensitive(false);
        //key_editor.action()->set_sensitive(false);
        //set_topbar_buttons_sensitive (false);
        
        if (big_clock_window) {
            big_clock->set_active (true);
        }
        
	} else {
        
        // The same restriction actions are handled when transport starts/stops
        //tracks_control_panel.action()->set_sensitive(true);
        //key_editor.action()->set_sensitive(true);
        //set_topbar_buttons_sensitive (true);

        if (big_clock_window) {
            big_clock->set_active (false);
        }

	}
}

void
ARDOUR_UI::transport_state_changed ()
{
    ENSURE_GUI_THREAD (*this, &ARDOUR_UI::transport_state_change);
    
    if (!_session ) {
        return;
    }
    
    if (_session->transport_rolling () ) {
        
        tracks_control_panel.action()->set_sensitive(false);
        key_editor.action()->set_sensitive(false);
        set_topbar_buttons_sensitive (false);
        
    } else {
        
        tracks_control_panel.action()->set_sensitive(true);
        key_editor.action()->set_sensitive(true);
        set_topbar_buttons_sensitive (true);
        
    }
}

bool
ARDOUR_UI::first_idle ()
{
	if (_session) {
		_session->allow_auto_play (true);
	}

	if (editor) {
		editor->first_idle();
	}

	Keyboard::set_can_save_keybindings (true);
	return false;
}

void
ARDOUR_UI::store_clock_modes ()
{
	XMLNode* node = new XMLNode(X_("ClockModes"));

	for (vector<AudioClock*>::iterator x = AudioClock::clocks.begin(); x != AudioClock::clocks.end(); ++x) {
		XMLNode* child = new XMLNode (X_("Clock"));
		
		child->add_property (X_("name"), (*x)->name());
		child->add_property (X_("mode"), enum_2_string ((*x)->mode()));
		child->add_property (X_("on"), ((*x)->off() ? X_("no") : X_("yes")));

		node->add_child_nocopy (*child);
	}

	_session->add_extra_xml (*node);
	_session->set_dirty ();
}

ARDOUR_UI::TransportControllable::TransportControllable (std::string name, ARDOUR_UI& u, ToggleType tp)
	: Controllable (name), ui (u), type(tp)
{

}

void
ARDOUR_UI::TransportControllable::set_value (double val)
{
	if (val < 0.5) {
		/* do nothing: these are radio-style actions */
		return;
	}

	const char *action = 0;

	switch (type) {
	case Roll:
		action = X_("Roll");
		break;
	case Stop:
		action = X_("Stop");
		break;
	case GotoStart:
		action = X_("GotoStart");
		break;
	case GotoEnd:
		action = X_("GotoEnd");
		break;
	case AutoLoop:
		action = X_("Loop");
		break;
	case PlaySelection:
		action = X_("PlaySelection");
		break;
	case RecordEnable:
		action = X_("Record");
		break;
	default:
		break;
	}

	if (action == 0) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", action);

	if (act) {
		act->activate ();
	}
}

double
ARDOUR_UI::TransportControllable::get_value (void) const
{
	float val = 0.0;

	switch (type) {
	case Roll:
		break;
	case Stop:
		break;
	case GotoStart:
		break;
	case GotoEnd:
		break;
	case AutoLoop:
		break;
	case PlaySelection:
		break;
	case RecordEnable:
		break;
	default:
		break;
	}

	return val;
}

void
ARDOUR_UI::setup_profile ()
{
	if (gdk_screen_width() < 1200 || getenv ("ARDOUR_NARROW_SCREEN")) {
		Profile->set_small_screen ();
	}

	if (g_getenv ("ARDOUR_SAE")) {
		Profile->set_sae ();
		Profile->set_single_package ();
	}

        Profile->set_trx ();
}

int
ARDOUR_UI::missing_file (Session*s, std::string str, DataType type)
{
	WavesMissingFileDialog dialog (s, str, type);
	ARDOUR_UI::instance()->_progress_dialog.hide (); // HOT FIX. (REWORK IT)
	int result = dialog.run ();
	ARDOUR_UI::instance()->_progress_dialog.show (); // HOT FIX. (REWORK IT)

	switch (result) {
	case RESPONSE_OK:
		break;
	default:
		return 1; // quit entire session load
	}

	result = dialog.get_action ();

	return result;
}

int
ARDOUR_UI::ambiguous_file (std::string file, std::vector<std::string> hits)
{
	WavesAmbiguousFileDialog dialog (file, hits);
	dialog.run ();
	return dialog.get_selected_num ();
}

/** Allocate our thread-local buffers */
void
ARDOUR_UI::get_process_buffers ()
{
	_process_thread->get_buffers ();
}

/** Drop our thread-local buffers */
void
ARDOUR_UI::drop_process_buffers ()
{
	_process_thread->drop_buffers ();
}

void
ARDOUR_UI::feedback_detected ()
{
	_feedback_exists = true;
}

void
ARDOUR_UI::successful_graph_sort ()
{
	_feedback_exists = false;
}

void
ARDOUR_UI::midi_panic ()
{
	if (_session) {
		_session->midi_panic();
	}
}

void
ARDOUR_UI::session_format_mismatch (std::string xml_path, std::string backup_path)
{
	const char* start_big = "<span size=\"x-large\" weight=\"bold\">";
	const char* end_big = "</span>";
	const char* start_mono = "<tt>";
	const char* end_mono = "</tt>";

	MessageDialog msg (string_compose (_("%4This is a session from an older version of %3%5\n\n"
					     "%3 has copied the old session file\n\n%6%1%7\n\nto\n\n%6%2%7\n\n"
					     "From now on, use the -2000 version with older versions of %3"),
					   xml_path, backup_path, PROGRAM_NAME,
					   start_big, end_big,
					   start_mono, end_mono), true);

	msg.run ();
}


void
ARDOUR_UI::reset_peak_display ()
{
}

void
ARDOUR_UI::reset_group_peak_display (RouteGroup* group)
{
	if (!_session || !_session->master_out()) return;
	if (group == _session->master_out()->route_group()) {
		reset_peak_display ();
	}
}

void
ARDOUR_UI::reset_route_peak_display (Route* route)
{
	if (!_session || !_session->master_out()) return;
	if (_session->master_out().get() == route) {
		reset_peak_display ();
	}
}

int
ARDOUR_UI::do_engine_setup (framecnt_t desired_sample_rate)
{
    // nothing to do so far
    
	return 0;
}

gint
ARDOUR_UI::transport_numpad_timeout ()
{
	_numpad_locate_happening = false;
	if (_numpad_timeout_connection.connected() )
		_numpad_timeout_connection.disconnect();
	return 1;
}

void
ARDOUR_UI::transport_numpad_decimal ()
{
	_numpad_timeout_connection.disconnect();

	if (_numpad_locate_happening) {
		if (editor) editor->goto_nth_marker(_pending_locate_num - 1);
		_numpad_locate_happening = false;
	} else {
		_pending_locate_num = 0;
		_numpad_locate_happening = true;
		_numpad_timeout_connection = Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::transport_numpad_timeout), 2*1000);
	}
}

void
ARDOUR_UI::transport_numpad_event (int num)
{
	if ( _numpad_locate_happening ) {
		_pending_locate_num = _pending_locate_num*10 + num;
	} else {
		switch (num) {		
		case 0:  toggle_roll(false, false);  break;
		case 1:  transport_rewind(1);        break;
		case 2:  transport_forward(1);       break;
		case 3:  transport_record(true);     break;
		case 4:  toggle_session_auto_loop(); break;
		case 5:  transport_record(false); toggle_session_auto_loop(); 	break;
		case 6:  toggle_punch(); 					break;
		case 7:  toggle_click();             break;
		case 8:  toggle_all_auto_return ();  break;
		case 9:  toggle_follow_edits();	     break;
		}
	}
}

void
ARDOUR_UI::open_media_folder ()
{
	if (!_session) {
		return;
	}
	
#if defined (PLATFORM_WINDOWS)
	//ShellExecute (NULL, "open", _session->session_directory ().sources_root ().c_str (), NULL, NULL, SW_SHOW);
	ShellExecute (NULL, "open", _session->session_directory ().sound_path ().c_str (), NULL, NULL, SW_SHOW);
#elif defined (__APPLE__)
    //std::string command = "open \"" + _session->session_directory ().sources_root () + "\"";
    std::string command = "open \"" + _session->session_directory ().sound_path () + "\"";
    system (command.c_str ());
#else 

    /* nix */
    /* XXX what to do here ? */

#endif
}

std::string
ARDOUR_UI::format_session_time (framepos_t frame)
{
	char buf[128];
	Timecode::Time timecode;
	Timecode::BBT_Time bbt;

	if (the_session() == 0) {
		return string();
	}

	/* Take clock mode from the primary clock */

	AudioClock::Mode m = primary_clock->mode();

	switch (m) {
	case AudioClock::BBT:
		the_session()->bbt_time (frame, bbt);
		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, bbt.bars, bbt.beats, bbt.ticks);
		break;

	case AudioClock::Timecode:
		the_session()->timecode_time (frame, timecode);
		snprintf (buf, sizeof (buf), "%s", Timecode::timecode_format_time (timecode).c_str());
		break;

	case AudioClock::MinSec:
		AudioClock::print_minsec (frame, buf, sizeof (buf), the_session()->frame_rate());
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, frame);
		break;
	}

    return buf;
}

void
ARDOUR_UI::toggle_auto_return_state (AutoReturnTarget t)
{
	AutoReturnTarget art = Config->get_auto_return_target_list ();
	Gtk::CheckMenuItem* check_menu_item = 0;
	
	switch (t) {
	case LastLocate:
		check_menu_item = auto_return_last_locate;
		break;
	case Loop:
		check_menu_item = auto_return_loop;
		break;
	case RangeSelectionStart:
		check_menu_item = auto_return_range_selection;
		break;
	case RegionSelectionStart:
		check_menu_item = auto_return_region_selection;
		break;
	}
	
	if (!check_menu_item) {
		return;
	}

	if (check_menu_item->get_active()) {
		Config->set_auto_return_target_list (AutoReturnTarget (art | t));
	} else {
		Config->set_auto_return_target_list (AutoReturnTarget (art & ~t));
	}
}

void
ARDOUR_UI::toggle_all_auto_return ()
{
	AutoReturnTarget art = Config->get_auto_return_target_list ();
	if (art) {
		Config->set_auto_return_target_list (AutoReturnTarget (0));
	} else {
		Config->set_auto_return_target_list (AutoReturnTarget (LastLocate|
								       RangeSelectionStart|
								       RegionSelectionStart|
								       Loop));
	}
}
	
void
ARDOUR_UI::hide_application ()
{
    Application::instance ()-> hide ();
}

void
ARDOUR_UI::on_editor_hiding ()
{
    // here we should close
    // all opening graphical elements
    
    tracks_control_panel->deiconify ();
    tracks_control_panel->hide ();

    key_editor->deiconify ();
    key_editor->hide ();
    
    big_clock_window->deiconify ();
    big_clock_window->hide ();
    
    about->deiconify ();
    about->hide ();
    
    track_color_dialog->deiconify ();
    track_color_dialog->hide ();
    
    _location_list_dialog->deiconify ();
    _location_list_dialog->hide ();
}
