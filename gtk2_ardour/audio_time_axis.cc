/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <cstdlib>
#include <cmath>
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/memento_command.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include "ardour/event_type_map.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"

#include "ardour_button.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "automation_line.h"
#include "enums.h"
#include "gui_thread.h"
#include "automation_time_axis.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "prompter.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "utils.h"

#include "i18n.h"
#include "dbg_msg.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Editing;

AudioTimeAxisView::AudioTimeAxisView (PublicEditor& ed, Session* sess, ArdourCanvas::Canvas& canvas)
	: AxisView(sess)
	, RouteTimeAxisView(ed, sess, canvas, "audio_time_axis.xml")
{
}

void
AudioTimeAxisView::set_route (boost::shared_ptr<Route> rt)
{
	_route = rt;

	/* RouteTimeAxisView::set_route() sets up some things in the View,
	   so it must be created before RouteTimeAxis::set_route() is
	   called.
	*/
	_view = new AudioStreamView (*this);

	RouteTimeAxisView::set_route (rt);

	_view->apply_color (gdk_color_to_rgba (color()), StreamView::RegionColor);

	// Make sure things are sane...
	assert(!is_track() || is_audio_track());

	subplugin_menu.set_name ("ArdourContextMenu");

	ignore_toggle = false;

	/* if set_state above didn't create a gain automation child, we need to make one */
	if (automation_child (GainAutomation) == 0) {
		create_automation_child (GainAutomation, false);
	}

	/* if set_state above didn't create a mute automation child, we need to make one */
	if (automation_child (MuteAutomation) == 0) {
		create_automation_child (MuteAutomation, false);
	}

	if (_route->panner_shell()) {
		_route->panner_shell()->Changed.connect (*this, invalidator (*this),
                                                         boost::bind (&AudioTimeAxisView::ensure_pan_views, this, false), gui_context());
	}

	/* map current state of the route */
	processors_changed (RouteProcessorChange ());
	reset_processor_automation_curves ();
	ensure_pan_views (false);
	update_control_names ();

	if (is_audio_track()) {

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (sigc::mem_fun(*this, &AudioTimeAxisView::region_view_added));

		if (!_editor.have_idled()) {
			/* first idle will do what we need */
		} else {
			first_idle ();
		}

	} else {
		post_construct ();
	}
}

AudioTimeAxisView::~AudioTimeAxisView ()
{
}

void
AudioTimeAxisView::first_idle ()
{
	_view->attach ();
	post_construct ();
}

AudioStreamView*
AudioTimeAxisView::audio_view()
{
	return dynamic_cast<AudioStreamView*>(_view);
}

guint32
AudioTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	set_gui_property ("visible", true);
	return TimeAxisView::show_at (y, nth, parent);
}

void
AudioTimeAxisView::hide ()
{
	set_gui_property ("visible", false);
	TimeAxisView::hide ();
}

void
AudioTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	if (param.type() == NullAutomation) {
		return;
	}

	AutomationTracks::iterator existing = _automation_tracks.find (param);

	if (existing != _automation_tracks.end()) {
		
		/* automation track created because we had existing data for
		 * the processor, but visibility may need to be controlled
		 * since it will have been set visible by default.
		 */

		existing->second->set_marked_for_display (show);
		
		if (!no_redraw) {
			request_redraw ();
		}

		return;
	}

	if (param.type() == GainAutomation) {

		create_gain_automation_child (param, show);

	} else if (param.type() == PanWidthAutomation ||
                   param.type() == PanElevationAutomation ||
                   param.type() == PanAzimuthAutomation) {

		ensure_pan_views (show);

	} else if (param.type() == PluginAutomation) {

		/* handled elsewhere */

	} else if (param.type() == MuteAutomation) {

		create_mute_automation_child (param, show);
		

	} else {
		error << "AudioTimeAxisView: unknown automation child " << EventTypeMap::instance().to_symbol(param) << endmsg;
	}
}

void
AudioTimeAxisView::show_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_audio_time_axis (boost::bind (&AudioTimeAxisView::show_all_automation, _1, false));
	} else {

		no_redraw = true;

		RouteTimeAxisView::show_all_automation ();

		no_redraw = false;
		request_redraw ();
	}
}

void
AudioTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_audio_time_axis (boost::bind (&AudioTimeAxisView::show_existing_automation, _1, false));
	} else {
		no_redraw = true;

		RouteTimeAxisView::show_existing_automation ();

		no_redraw = false;

		request_redraw ();
	}
}

void
AudioTimeAxisView::hide_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_audio_time_axis (boost::bind (&AudioTimeAxisView::hide_all_automation, _1, false));
	} else {
		no_redraw = true;

		RouteTimeAxisView::hide_all_automation();

		no_redraw = false;
		request_redraw ();
	}
}

void
AudioTimeAxisView::route_active_changed ()
{
	update_control_names ();
}


/**
 *    Set up the names of the controls so that they are coloured
 *    correctly depending on whether this route is inactive or
 *    selected.
 */

void
AudioTimeAxisView::update_control_names ()
{
	if (is_audio_track()) {
		if (_route->active()) {
			controls_base_selected_name = "AudioTrackControlsBaseSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseUnselected";
		} else {
			controls_base_selected_name = "AudioTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseInactiveUnselected";
		}
	} else {
		if (_route->active()) {
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}

	//if (get_selected()) {
	//	controls_ebox.set_name (controls_base_selected_name);
	//} else {
	//	controls_ebox.set_name (controls_base_unselected_name);
	//}
}

void
AudioTimeAxisView::build_automation_action_menu (bool for_selection)
{
	using namespace Menu_Helpers;

	RouteTimeAxisView::build_automation_action_menu (for_selection);

	MenuList& automation_items = automation_action_menu->items ();

	automation_items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (*this, &AudioTimeAxisView::update_gain_track_visibility)));
	gain_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&automation_items.back ());
	gain_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) && 
					  (gain_track && string_is_affirmative (gain_track->gui_property ("visible"))));

	_main_automation_menu_map[Evoral::Parameter(GainAutomation)] = gain_automation_item;

	automation_items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (*this, &AudioTimeAxisView::update_mute_track_visibility)));
	mute_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&automation_items.back ());
	mute_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) && 
					  (mute_track && string_is_affirmative (mute_track->gui_property ("visible"))));

	_main_automation_menu_map[Evoral::Parameter(MuteAutomation)] = mute_automation_item;

	if (!pan_tracks.empty()) {
		automation_items.push_back (CheckMenuElem (_("Pan"), sigc::mem_fun (*this, &AudioTimeAxisView::update_pan_track_visibility)));
		pan_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&automation_items.back ());
		pan_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) &&
						 (!pan_tracks.empty() && string_is_affirmative (pan_tracks.front()->gui_property ("visible"))));

		set<Evoral::Parameter> const & params = _route->pannable()->what_can_be_automated ();
		for (set<Evoral::Parameter>::const_iterator p = params.begin(); p != params.end(); ++p) {
			_main_automation_menu_map[*p] = pan_automation_item;
		}
	}
}

void
AudioTimeAxisView::enter_internal_edit_mode ()
{
        if (audio_view()) {
                audio_view()->enter_internal_edit_mode ();
        }
}

void
AudioTimeAxisView::leave_internal_edit_mode ()
{
        if (audio_view()) {
                audio_view()->leave_internal_edit_mode ();
        }
}
