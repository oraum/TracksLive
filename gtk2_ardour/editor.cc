/*
    Copyright (C) 2000-2009 Paul Davis

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

/* Note: public Editor methods are documented in public_editor.h */

#include "product_definition.h"

#include <stdint.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>
#include <map>

#include "ardour_ui.h"
/*
 * ardour_ui.h include was moved to the top of the list
 * due to a conflicting definition of 'Style' between
 * Apple's MacTypes.h and BarController.
 */

#include <boost/none.hpp>

#include <sigc++/bind.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/unknown_type.h"
#include "pbd/unwind.h"
#include "pbd/stacktrace.h"
#include "pbd/timersub.h"

#include <glibmm/miscutils.h>
#include <glibmm/uriutils.h>
#include <gtkmm/image.h>
#include <gdkmm/color.h>
#include <gdkmm/bitmap.h>

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/grouped_buttons.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/tearoff.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/choice.h"
#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/engine_state_controller.h"
#include "ardour/audioregion.h"
#include "ardour/location.h"
#include "ardour/midi_scene_changer.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session_playlists.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"

#include "canvas/debug.h"
#include "canvas/scroll_group.h"
#include "canvas/text.h"

#include "control_protocol/control_protocol.h"

#include "actions.h"
#include "analysis_window.h"
#include "audio_clock.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "audio_time_axis.h"
#include "automation_time_axis.h"
#include "bundle_manager.h"
#include "crossfade_edit.h"
#include "debug.h"
#include "editing.h"
#include "editor.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "editor_group_tabs.h"
#include "editor_locations.h"
#include "editor_regions.h"
#include "editor_route_groups.h"
#include "editor_routes.h"
#include "editor_snapshots.h"
#include "editor_summary.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "marker.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "mouse_cursors.h"
#include "playlist_selector.h"
#include "public_editor.h"
#include "region_layering_order_editor.h"
#include "rgb_macros.h"
#include "rhythm_ferret.h"
#include "selection.h"
#include "tempo_lines.h"
#include "time_axis_view.h"
#include "master_bus_ui.h"
#include "utils.h"
#include "verbose_cursor.h"
#include "waves_numeric_edit_dialog.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

using PBD::internationalize;
using PBD::atoi;
using Gtkmm2ext::Keyboard;

static const gchar *_snap_type_strings[] = {
	N_("CD Frames"),
	N_("TC Frames"),
	N_("TC Seconds"),
	N_("TC Minutes"),
	N_("Seconds"),
	N_("Minutes"),
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/28"),
	N_("Beats/24"),
	N_("Beats/20"),
	N_("Beats/16"),
	N_("Beats/14"),
	N_("Beats/12"),
	N_("Beats/10"),
	N_("Beats/8"),
	N_("Beats/7"),
	N_("Beats/6"),
	N_("Beats/5"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats/2"),
	N_("Beats"),
	N_("Bars"),
	N_("Marks"),
	N_("Region starts"),
	N_("Region ends"),
	N_("Region syncs"),
	N_("Region bounds"),
	0
};

static const gchar *_snap_mode_strings[] = {
	N_("No Grid"),
	N_("Grid"),
	N_("Magnetic"),
	0
};

static const gchar *_edit_point_strings[] = {
	N_("Playhead"),
	N_("Marker"),
	N_("Mouse"),
	0
};

static const gchar *_edit_mode_strings[] = {
	N_("Slide"),
	N_("Splice"),
	N_("Ripple"),
	N_("Lock"),
	0
};

static const gchar *_zoom_focus_strings[] = {
	N_("Left"),
	N_("Right"),
	N_("Center"),
	N_("Playhead"),
 	N_("Mouse"),
 	N_("Edit point"),
	0
};

#ifdef USE_RUBBERBAND
static const gchar *_rb_opt_strings[] = {
	N_("Mushy"),
	N_("Smooth"),
	N_("Balanced multitimbral mixture"),
	N_("Unpitched percussion with stable notes"),
	N_("Crisp monophonic instrumental"),
	N_("Unpitched solo percussion"),
	N_("Resample without preserving pitch"),
	0
};
#endif

static void
pane_size_watcher (Paned* pane)
{
	/* if the handle of a pane vanishes into (at least) the tabs of a notebook,
	   it is:

	      X: hard to access
	      Quartz: impossible to access
	      
	   so stop that by preventing it from ever getting too narrow. 35
	   pixels is basically a rough guess at the tab width.

	   ugh.
	*/

	int max_width_of_lhs = GTK_WIDGET(pane->gobj())->allocation.width - 35;

	gint pos = pane->get_position ();

	if (pos > max_width_of_lhs) {
		pane->set_position (max_width_of_lhs);
	}
}

Editor::Editor ()
	: PublicEditor ("editor_window.xml")
	, _join_object_range_state (JOIN_OBJECT_RANGE_NONE)
	, timebar_height (xml_property (*xml_tree ()->root (), "timebarheight", 17.0))
	, ruler_height (xml_property (*xml_tree ()->root (), "rulerheight", 26.0))
	, ruler_divide_height (xml_property (*xml_tree ()->root (), "rulerdivideheight", 13.0))
	, marker_height (xml_property (*xml_tree ()->root (), "markerheight", 18))
	, skipbar_height (xml_property (*xml_tree ()->root (), "skipbarheight", 17))
	  /* time display buttons */
	, minsec_label (_("Mins:Secs"))
	, bbt_label (_("Bars:Beats"))
	, timecode_label (_("Timecode"))
	, samples_label (_("Samples"))
	, tempo_label (_("Tempo"))
	, meter_label (_("Meter"))
	, mark_label (_("Location Markers"))
	, range_mark_label (_("Range Markers"))
	, transport_mark_label (_("Loop/Punch Ranges"))
	, cd_mark_label (_("CD Markers"))
	, videotl_label (_("Video Timeline"))
	, edit_pane (get_h_paned ("edit_pane"))
	, editor_summary_pane (get_v_paned ("editor_summary_pane"))
	, inspector_home (get_container ("inspector_home"))
	, _master_bus_ui_home (get_container ("master_bus_ui_home"))
	, vpacker (get_v_box ("vpacker"))
	, skip_button (get_waves_button ("skip_button"))
    , add_marker_button (get_waves_button ("add_marker_button"))
    , global_solo_button (get_waves_button ("global_solo_button"))
    , _global_rec_button (get_waves_button ("global_rec_button"))
	, _tool_marker_button (get_waves_button ("tool_marker_button"))
	, _tool_arrow_button (get_waves_button ("tool_arrow_button"))
	, _tool_zoom_button (get_waves_button ("tool_zoom_button"))
	, _tool_cut_button (get_waves_button ("tool_cut_button"))
	, _temporal_zoom_adjustment (get_adjustment ("temporal_zoom_adjustment"))
	, _vertical_zoom_adjustment (get_adjustment ("vertical_zoom_adjustment"))
    , _vertical_zoom_fader (get_fader ("vertical_zoom_fader"))
	, vertical_adjustment (get_adjustment ("vertical_adjustment"))
    , _waves_zoom_adjustment (get_adjustment ("waves_zoom_adjustment"))
	, _waves_zoom_control (get_adjustment ("waves_zoom_adjustment"))
    , horizontal_adjustment (get_adjustment ("horizontal_adjustment"))
    , unused_adjustment (get_adjustment ("unused_adjustment"))
	, edit_packer (get_table ("edit_packer"))
	, edit_controls_vbox (get_v_box ("edit_controls_vbox"))
	, controls_layout (get_layout ("controls_layout"))

	  /* the values here don't matter: layout widgets
	     reset them as needed.
	  */

	  /* tool bar related */

	, toolbar_selection_clock_table (2,3)
	, _mouse_mode_tearoff (0)
	, automation_mode_button (_("mode"))
	, _zoom_tearoff (0)
	, _tools_tearoff (0)

	, _toolbar_viewport (*manage (new Gtk::Adjustment (0, 0, 1e10)), *manage (new Gtk::Adjustment (0, 0, 1e10)))

	  /* nudge */

	, nudge_clock (new AudioClock (X_("nudge"), false, X_("nudge"), true, false, true))
    , _zoom_tool_was_used (false)
	, meters_running(false)
	, _pending_locate_request (false)
	, _pending_initial_locate (false)
	, _region_selection_change_updates_region_list (true)
	, _following_mixer_bridge_view_selection (false)
    , _following_meter_bridge_selection (false)
	, _mixer_bridge_view ("mixer_bridge_view.xml", "mixer_strip.xml")
	, _meter_bridge_view ("meter_bridge_view.xml", "meter_strip.xml")
	, _control_point_toggled_on_press (false)
	, _stepping_axis_view (0)
	, current_mixer_strip (0)
	, _master_bus_ui (0)
    , _set_session_in_progress(false)
    , _midi_input_dropdown (get_waves_dropdown ("midi_input_dropdown"))
    , _midi_output_dropdown (get_waves_dropdown ("midi_output_dropdown"))
    , midi_marker_input_activity_image (get_widget ("midi_input_activity_indicator"))
    , midi_marker_output_activity_image (get_widget ("midi_output_activity_indicator"))
    , midi_marker_input_enabled_image (get_widget ("midi_input_enabled_indicator"))
    , midi_marker_input_disabled_image (get_widget ("midi_input_disabled_indicator"))
    , midi_marker_output_enabled_image (get_widget ("midi_output_enabled_indicator"))
    , midi_marker_output_disabled_image (get_widget ("midi_output_disabled_indicator"))
{
        constructed = false;

	/* we are a singleton */

	PublicEditor::_instance = this;
	get_container ("compact_meter_bridge_home").add (_compact_meter_bridge);
	get_container ("mixer_bridge_view_home").add (_mixer_bridge_view);
	get_container ("meter_bridge_view_home").add (_meter_bridge_view);
	get_container ("waves_zoom_control_home").add (_waves_zoom_control);
	_waves_zoom_control.show();

	_have_idled = false;
	
	selection = new Selection (this);
	cut_buffer = new Selection (this);

	clicked_regionview = 0;
	clicked_axisview = 0;
	clicked_routeview = 0;
	clicked_control_point = 0;
	last_update_frame = 0;
	_drags = new DragManager (this);
	lock_dialog = 0;
	ruler_dialog = 0;
	current_mixer_strip = 0;
	tempo_lines = 0;

	snap_type_strings =  I18N (_snap_type_strings);
	snap_mode_strings =  I18N (_snap_mode_strings);
	zoom_focus_strings = I18N (_zoom_focus_strings);
	edit_mode_strings = I18N (_edit_mode_strings);
	edit_point_strings = I18N (_edit_point_strings);
#ifdef USE_RUBBERBAND
	rb_opt_strings = I18N (_rb_opt_strings);
	rb_current_opt = 4;
#endif

	build_edit_mode_menu();
	build_zoom_focus_menu();
	build_track_count_menu();
	build_snap_mode_menu();
	build_snap_type_menu();
	build_edit_point_menu();

	snap_threshold = 5.0;
	bbt_beat_subdivision = 4;
	_visible_canvas_width = 0;
	_visible_canvas_height = 0;
	autoscroll_horizontal_allowed = false;
	autoscroll_vertical_allowed = false;
	logo_item = 0;

	analysis_window = 0;

	current_interthread_info = 0;
	_show_measures = true;
	_maximised = false;
    _enable_group_edit = false;
	show_gain_after_trim = false;

	have_pending_keyboard_selection = false;
	_follow_playhead = true;
        _stationary_playhead = false;
	editor_ruler_menu = 0;
	no_ruler_shown_update = false;
	marker_menu = 0;
	range_marker_menu = 0;
	marker_menu_item = 0;
	tempo_or_meter_marker_menu = 0;
	transport_marker_menu = 0;
	new_transport_marker_menu = 0;
	show_editor_mixer_when_tracks_arrive = false;
	region_edit_menu_split_multichannel_item = 0;
	region_edit_menu_split_item = 0;
	temp_location = 0;
	leftmost_frame = 0;
	current_stepping_trackview = 0;
	entered_track = 0;
	entered_regionview = 0;
	entered_marker = 0;
	clear_entered_track = false;
	current_timefx = 0;
	playhead_cursor = 0;
	button_release_can_deselect = true;
	_dragging_playhead = false;
	_dragging_edit_point = false;
	select_new_marker = false;
	rhythm_ferret = 0;
	layering_order_editor = 0;
	no_save_visual = false;
	resize_idle_id = -1;
	within_track_canvas = false;

	scrubbing_direction = 0;

	zoom_focus = ZoomFocusLeft;
	_edit_point = EditAtMouse;
	_internal_editing = false;
	_visible_track_count = -1;

	samples_per_pixel = 2048; /* too early to use reset_zoom () */

	_scroll_callbacks = 0;

	bbt_label.set_name ("EditorRulerLabel");
	bbt_label.set_size_request (-1, (int)timebar_height);
	bbt_label.set_alignment (1.0, 0.5);
	bbt_label.set_padding (5,0);
	bbt_label.hide ();
	bbt_label.set_no_show_all();
	minsec_label.set_name ("EditorRulerLabel");
	minsec_label.set_size_request (-1, (int)timebar_height);
	minsec_label.set_alignment (1.0, 0.5);
	minsec_label.set_padding (5,0);
	minsec_label.hide ();
	minsec_label.set_no_show_all();
	timecode_label.set_name ("EditorRulerLabel");
	timecode_label.set_size_request (-1, (int)timebar_height);
	timecode_label.set_alignment (1.0, 0.5);
	timecode_label.set_padding (5,0);
	timecode_label.hide ();
	timecode_label.set_no_show_all();
	samples_label.set_name ("EditorRulerLabel");
	samples_label.set_size_request (-1, (int)timebar_height);
	samples_label.set_alignment (1.0, 0.5);
	samples_label.set_padding (5,0);
	samples_label.hide ();
	samples_label.set_no_show_all();

	tempo_label.set_name ("EditorRulerLabel");
	tempo_label.set_size_request (-1, (int)timebar_height);
	tempo_label.set_alignment (1.0, 0.5);
	tempo_label.set_padding (5,0);
	tempo_label.hide();
	tempo_label.set_no_show_all();

	meter_label.set_name ("EditorRulerLabel");
	meter_label.set_size_request (-1, (int)timebar_height);
	meter_label.set_alignment (1.0, 0.5);
	meter_label.set_padding (5,0);
	meter_label.hide();
	meter_label.set_no_show_all();

	if (Profile->get_trx()) {
		mark_label.set_text (_("Markers"));
	}
	mark_label.set_name ("EditorRulerLabel");
	mark_label.set_size_request (-1, (int)timebar_height);
	mark_label.set_alignment (1.0, 0.5);
	mark_label.set_padding (5,0);
	mark_label.hide();
	mark_label.set_no_show_all();

	cd_mark_label.set_name ("EditorRulerLabel");
	cd_mark_label.set_size_request (-1, (int)timebar_height);
	cd_mark_label.set_alignment (1.0, 0.5);
	cd_mark_label.set_padding (5,0);
	cd_mark_label.hide();
	cd_mark_label.set_no_show_all();

	videotl_bar_height = 4;
	videotl_label.set_name ("EditorRulerLabel");
	videotl_label.set_size_request (-1, (int)timebar_height * videotl_bar_height);
	videotl_label.set_alignment (1.0, 0.5);
	videotl_label.set_padding (5,0);
	videotl_label.hide();
	videotl_label.set_no_show_all();

	range_mark_label.set_name ("EditorRulerLabel");
	range_mark_label.set_size_request (-1, (int)timebar_height);
	range_mark_label.set_alignment (1.0, 0.5);
	range_mark_label.set_padding (5,0);
	range_mark_label.hide();
	range_mark_label.set_no_show_all();

	transport_mark_label.set_name ("EditorRulerLabel");
	transport_mark_label.set_size_request (-1, (int)timebar_height);
	transport_mark_label.set_alignment (1.0, 0.5);
	transport_mark_label.set_padding (5,0);
	transport_mark_label.hide();
	transport_mark_label.set_no_show_all();

	initialize_canvas ();

    CairoWidget::set_focus_handler (sigc::mem_fun (*this, &Editor::reset_focus));

	_summary = new EditorSummary (this);

    this->signal_hide().connect ( sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::on_editor_hiding));
	selection->TimeChanged.connect (sigc::mem_fun(*this, &Editor::time_selection_changed));
	selection->TracksChanged.connect (sigc::mem_fun(*this, &Editor::track_selection_changed));

	EngineStateController::instance()->SampleRateChanged.connect (*this, invalidator (*this), boost::bind (&Editor::update_ruler_visibility, this), gui_context());
    EngineStateController::instance()->OutputConnectionModeChanged.connect (*this, invalidator (*this), boost::bind (&Editor::output_connection_mode_changed, this), gui_context() );

        /* Connect to relevant signal so that we will be notified of port registration changes */
	ARDOUR::EngineStateController::instance()->PortRegistrationChanged.connect (port_state_connection_list, invalidator (*this), boost::bind (&Editor::port_registration_handler, this), gui_context());

        /* Connect to relevant signal so that we will be notified of midi port connection changes */
	ARDOUR::EngineStateController::instance()->MIDISceneInputConnectionChanged.connect (port_state_connection_list, invalidator (*this), boost::bind (&Editor::midi_input_connection_changed, this, _1, _2), gui_context());
    ARDOUR::EngineStateController::instance()->MIDISceneOutputConnectionChanged.connect (port_state_connection_list, invalidator (*this), boost::bind (&Editor::midi_output_connection_changed, this, _1, _2), gui_context());
    
    ARDOUR::EngineStateController::instance()->MIDIInputConfigChanged.connect (port_state_connection_list, invalidator (*this), boost::bind (&Editor::midi_input_configuration_changed, this), gui_context());
    ARDOUR::EngineStateController::instance()->MIDIOutputConfigChanged.connect (port_state_connection_list, invalidator (*this), boost::bind (&Editor::midi_output_configuration_changed, this), gui_context());
    
    _midi_input_dropdown.selected_item_changed.connect (mem_fun(*this, &Editor::midi_input_chosen ));
    _midi_output_dropdown.selected_item_changed.connect (mem_fun(*this, &Editor::midi_output_chosen ));
    
	editor_regions_selection_changed_connection = selection->RegionsChanged.connect (sigc::mem_fun(*this, &Editor::region_selection_changed));

	selection->PointsChanged.connect (sigc::mem_fun(*this, &Editor::point_selection_changed));
	selection->MarkersChanged.connect (sigc::mem_fun(*this, &Editor::marker_selection_changed));

	vertical_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &Editor::tie_vertical_scrolling), true);
    horizontal_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &Editor::tie_horizontal_scrolling), true);
    
	_track_canvas->signal_map_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_map_handler));

	_group_tabs = new EditorGroupTabs (this);
	controls_layout.set_name ("EditControlsBase");
	controls_layout.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::SCROLL_MASK);
	controls_layout.signal_button_press_event().connect (sigc::mem_fun(*this, &Editor::edit_controls_button_press));
	controls_layout.signal_button_release_event().connect (sigc::mem_fun(*this, &Editor::edit_controls_button_release));
	controls_layout.signal_scroll_event().connect (sigc::mem_fun(*this, &Editor::control_layout_scroll), false);

	_cursors = new MouseCursors;
	_cursors->set_cursor_set (ARDOUR_UI::config()->get_icon_set());

	ArdourCanvas::GtkCanvas* time_pad = manage (new ArdourCanvas::GtkCanvas ());

	ArdourCanvas::Line* pad_line_1 = new ArdourCanvas::Line (time_pad->root());
	pad_line_1->set (ArdourCanvas::Duple (0.0, 1.0), ArdourCanvas::Duple (100.0, 1.0));
	pad_line_1->set_outline_color (0xFF0000FF);
	pad_line_1->show();

	// CAIROCANVAS
	time_pad->show();

    /* rest of the packing is done in ui/editor_window.xml */

	edit_packer.attach (*_track_canvas_viewport,  1, 2, 1, 3,    FILL|EXPAND, FILL|EXPAND, 0, 0);

	_route_groups = new EditorRouteGroups (this);
	_routes = new EditorRoutes (this);
	_regions = new EditorRegions (this);
	_snapshots = new EditorSnapshots (this);
	_locations = new EditorLocations (this);

	/* these are static location signals */

	Location::start_changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());
	Location::end_changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());
	Location::changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());

	add_notebook_page (_("Regions"), _regions->widget ());
	add_notebook_page (_("Tracks & Busses"), _routes->widget ());
	add_notebook_page (_("Snapshots"), _snapshots->widget ());
	add_notebook_page (_("Track & Bus Groups"), _route_groups->widget ());
	add_notebook_page (_("Ranges & Marks"), _locations->widget ());

	_the_notebook.set_show_tabs (true);
	_the_notebook.set_scrollable (true);
	_the_notebook.popup_disable ();
	_the_notebook.set_tab_pos (Gtk::POS_RIGHT);
	_the_notebook.show_all ();

	_notebook_shrunk = false;

	Button* summary_arrows_left_left = manage (new Button);
	summary_arrows_left_left->add (*manage (new Arrow (ARROW_LEFT, SHADOW_NONE)));
	summary_arrows_left_left->signal_pressed().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), LEFT)));
	summary_arrows_left_left->signal_released().connect (sigc::mem_fun (*this, &Editor::scroll_release));

	Button* summary_arrows_left_right = manage (new Button);
	summary_arrows_left_right->add (*manage (new Arrow (ARROW_RIGHT, SHADOW_NONE)));
	summary_arrows_left_right->signal_pressed().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), RIGHT)));
	summary_arrows_left_right->signal_released().connect (sigc::mem_fun (*this, &Editor::scroll_release));

	VBox* summary_arrows_left = manage (new VBox);
	summary_arrows_left->pack_start (*summary_arrows_left_left);
	summary_arrows_left->pack_start (*summary_arrows_left_right);

	Button* summary_arrows_right_up = manage (new Button);
	summary_arrows_right_up->add (*manage (new Arrow (ARROW_UP, SHADOW_NONE)));
	summary_arrows_right_up->signal_pressed().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), UP)));
	summary_arrows_right_up->signal_released().connect (sigc::mem_fun (*this, &Editor::scroll_release));

	Button* summary_arrows_right_down = manage (new Button);
	summary_arrows_right_down->add (*manage (new Arrow (ARROW_DOWN, SHADOW_NONE)));
	summary_arrows_right_down->signal_pressed().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), DOWN)));
	summary_arrows_right_down->signal_released().connect (sigc::mem_fun (*this, &Editor::scroll_release));

	VBox* summary_arrows_right = manage (new VBox);
	summary_arrows_right->pack_start (*summary_arrows_right_up);
	summary_arrows_right->pack_start (*summary_arrows_right_down);

	Frame* summary_frame = manage (new Frame);
	summary_frame->set_shadow_type (Gtk::SHADOW_ETCHED_IN);

	summary_frame->add (*_summary);
	summary_frame->show ();

	_summary_hbox.pack_start (*summary_arrows_left, false, false);
	_summary_hbox.pack_start (*summary_frame, true, true);
	_summary_hbox.pack_start (*summary_arrows_right, false, false);

	if (!ARDOUR::Profile->get_trx()) {
		editor_summary_pane.pack2 (_summary_hbox);
	}

	if (!ARDOUR::Profile->get_trx()) {
		edit_pane.pack2 (_the_notebook, false, true);
	}

	editor_summary_pane.signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &Editor::pane_allocation_handler), static_cast<Paned*> (&editor_summary_pane)));

	/* XXX: editor_summary_pane might need similar to the edit_pane */

	edit_pane.signal_size_allocate().connect (sigc::bind (sigc::mem_fun(*this, &Editor::pane_allocation_handler), static_cast<Paned*> (&edit_pane)));

	Glib::PropertyProxy<int> proxy = edit_pane.property_position();
	proxy.signal_changed().connect (bind (sigc::ptr_fun (pane_size_watcher), static_cast<Paned*> (&edit_pane)));

	set_name ("EditorWindow");
	add_accel_group (ActionManager::ui_manager->get_accel_group());

	/* register actions now so that set_state() can find them and set toggles/checks etc */

	register_actions ();
	/* when we start using our own keybinding system for the editor, this
	 * will be uncommented
	 */
	// load_bindings ();

	setup_toolbar ();

	RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-skip-playback"));
        assert (act);
	skip_button.set_related_action (act);

	global_solo_button.signal_clicked.connect (sigc::mem_fun(*this,&Editor::global_solo_clicked));
	_global_rec_button.signal_clicked.connect (sigc::mem_fun(*this,&Editor::global_rec_clicked));   

	set_zoom_focus (zoom_focus);
	set_visible_track_count (_visible_track_count);
	_snap_type = SnapToBeat;
	set_snap_to (_snap_type);
	_snap_mode = SnapOff;
	set_snap_mode (_snap_mode);
	set_mouse_mode (MouseObject, true);
        pre_internal_mouse_mode = MouseObject;
        pre_internal_snap_type = _snap_type;
        pre_internal_snap_mode = _snap_mode;
        internal_snap_type = _snap_type;
        internal_snap_mode = _snap_mode;
	set_edit_point_preference (EditAtMouse, true);

	_playlist_selector = new PlaylistSelector();
	_playlist_selector->signal_delete_event().connect (sigc::bind (sigc::ptr_fun (just_hide_it), static_cast<Window *> (_playlist_selector)));

	RegionView::RegionViewGoingAway.connect (*this, invalidator (*this),  boost::bind (&Editor::catch_vanishing_regionview, this, _1), gui_context());

	fade_context_menu.set_name ("ArdourContextMenu");

	/* icons, titles, WM stuff */

	list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;
	Glib::RefPtr<Gdk::Pixbuf> icon;

	if ((icon = ::get_icon ("ardour_icon_16px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_22px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_32px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_48px")) != 0) {
		window_icons.push_back (icon);
	}
	if (!window_icons.empty()) {
		// set_icon_list (window_icons);
		set_default_icon_list (window_icons);
	}

	WindowTitle title(Glib::get_application_name());
	title += _("Editor");
	set_title (title.get_string());
	set_wmclass (X_("ardour_editor"), PROGRAM_NAME);

	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	signal_delete_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::exit_on_main_window_close));
    
	Gtkmm2ext::Keyboard::the_keyboard().ZoomVerticalModifierReleased.connect (sigc::mem_fun (*this, &Editor::zoom_vertical_modifier_released));
	
	/* allow external control surfaces/protocols to do various things */

	ControlProtocol::ZoomToSession.connect (*this, invalidator (*this), boost::bind (&Editor::temporal_zoom_session, this), gui_context());
	ControlProtocol::ZoomIn.connect (*this, invalidator (*this), boost::bind (&Editor::temporal_zoom_step, this, false), gui_context());
	ControlProtocol::ZoomOut.connect (*this, invalidator (*this), boost::bind (&Editor::temporal_zoom_step, this, true), gui_context());
	ControlProtocol::Undo.connect (*this, invalidator (*this), boost::bind (&Editor::undo, this, true), gui_context());
	ControlProtocol::Redo.connect (*this, invalidator (*this), boost::bind (&Editor::redo, this, true), gui_context());
	ControlProtocol::ScrollTimeline.connect (*this, invalidator (*this), boost::bind (&Editor::control_scroll, this, _1), gui_context());
	ControlProtocol::StepTracksUp.connect (*this, invalidator (*this), boost::bind (&Editor::control_step_tracks_up, this), gui_context());
	ControlProtocol::StepTracksDown.connect (*this, invalidator (*this), boost::bind (&Editor::control_step_tracks_down, this), gui_context());
	ControlProtocol::GotoView.connect (*this, invalidator (*this), boost::bind (&Editor::control_view, this, _1), gui_context());
	ControlProtocol::CloseDialog.connect (*this, invalidator (*this), Keyboard::close_current_dialog, gui_context());
	ControlProtocol::VerticalZoomInAll.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_in_all, this), gui_context());
	ControlProtocol::VerticalZoomOutAll.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_out_all, this), gui_context());
	ControlProtocol::VerticalZoomInSelected.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_in_selected, this), gui_context());
	ControlProtocol::VerticalZoomOutSelected.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_out_selected, this), gui_context());

	ControlProtocol::AddRouteToSelection.connect (*this, invalidator (*this), boost::bind (&Editor::control_select, this, _1, Selection::Add), gui_context());
	ControlProtocol::RemoveRouteFromSelection.connect (*this, invalidator (*this), boost::bind (&Editor::control_select, this, _1, Selection::Toggle), gui_context());
	ControlProtocol::SetRouteSelection.connect (*this, invalidator (*this), boost::bind (&Editor::control_select, this, _1, Selection::Set), gui_context());
	ControlProtocol::ToggleRouteSelection.connect (*this, invalidator (*this), boost::bind (&Editor::control_select, this, _1, Selection::Toggle), gui_context());
	ControlProtocol::ClearRouteSelection.connect (*this, invalidator (*this), boost::bind (&Editor::control_unselect, this), gui_context());

	BasicUI::AccessAction.connect (*this, invalidator (*this), boost::bind (&Editor::access_action, this, _1, _2), gui_context());

	/* problematic: has to return a value and thus cannot be x-thread */

	Session::AskAboutPlaylistDeletion.connect_same_thread (*this, boost::bind (&Editor::playlist_deletion_dialog, this, _1));

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&Editor::parameter_changed, this, _1), gui_context());
    
    ARDOUR_UI::config()->ParameterChanged.connect_same_thread (*this, boost::bind (&Editor::ui_parameter_changed, this, _1) );

    ARDOUR_UI::config()->ParameterChanged.connect_same_thread (*this, boost::bind (&Editor::on_ardour_ui_config_changed, this, _1) );
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    ardour_ui->lock_button_was_pressed.connect( *this, invalidator (*this), boost::bind (&Editor::lock, this), gui_context() );
    
	TimeAxisView::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Editor::timeaxisview_deleted, this, _1), gui_context());

	_ignore_region_action = false;
	_last_region_menu_was_main = false;
	_popup_region_menu_item = 0;

	_ignore_follow_edits = false;

	_show_marker_lines = false;

        /* Button bindings */

        button_bindings = new Bindings;

	XMLNode* node = button_settings();
        if (node) {
                for (XMLNodeList::const_iterator i = node->children().begin(); i != node->children().end(); ++i) {
                        button_bindings->load (**i);
                }
        }

	constructed = true;

	/* grab current parameter state */
	boost::function<void (string)> pc (boost::bind (&Editor::ui_parameter_changed, this, _1));
	ARDOUR_UI::config()->map_parameters (pc);
    
	setup_fade_images ();

	instant_save ();
}

Editor::~Editor()
{
    delete button_bindings;
	delete _routes;
	delete _route_groups;
	delete _track_canvas_viewport;
	delete _drags;
	delete nudge_clock;
}

XMLNode*
Editor::button_settings () const
{
	XMLNode* settings = ARDOUR_UI::instance()->editor_settings();
	XMLNode* node = find_named_node (*settings, X_("Buttons"));

	if (!node) {
		node = new XMLNode (X_("Buttons"));
	}

	return node;
}

void
Editor::add_toplevel_controls (Container& cont)
{
	vpacker.pack_start (cont, false, false);
	cont.show_all ();
}

bool
Editor::get_smart_mode () const
{
	return ((current_mouse_mode() == Editing::MouseObject) && smart_mode_action->get_active());
}

void
Editor::catch_vanishing_regionview (RegionView *rv)
{
	/* note: the selection will take care of the vanishing
	   audioregionview by itself.
	*/

	if (_drags->active() && _drags->have_item (rv->get_canvas_group()) && !_drags->ending()) {
		_drags->abort ();
	}

	if (clicked_regionview == rv) {
		clicked_regionview = 0;
	}

	if (entered_regionview == rv) {
		set_entered_regionview (0);
	}

	if (!_all_region_actions_sensitized) {
		sensitize_all_region_actions (true);
	}
}

void
Editor::set_entered_regionview (RegionView* rv)
{
	if (rv == entered_regionview) {
		return;
	}

	if (entered_regionview) {
		entered_regionview->exited ();
	}

	entered_regionview = rv;

	if (entered_regionview  != 0) {
		entered_regionview->entered (internal_editing ());
        
        if (mouse_mode == MouseCut) {
            Gdk::Cursor* cursor = which_mode_cursor ();
            set_canvas_cursor (cursor);
        }
        
    } else {
        if (mouse_mode == MouseCut) {
            Gdk::Cursor* cursor = which_mode_cursor ();
            set_canvas_cursor (cursor);
        }
    }

	if (!_all_region_actions_sensitized && _last_region_menu_was_main) {
		/* This RegionView entry might have changed what region actions
		   are allowed, so sensitize them all in case a key is pressed.
		*/
		sensitize_all_region_actions (true);
	}
}

void
Editor::set_entered_track (TimeAxisView* tav)
{
	if (entered_track) {
		entered_track->exited ();
	}

	entered_track = tav;

	if (entered_track) {
		entered_track->entered ();
	}
}

void
Editor::show_window ()
{
	if (!is_visible ()) {
		show_all ();

		/* XXX: this is a bit unfortunate; it would probably
		   be nicer if we could just call show () above rather
		   than needing the show_all ()
		*/

		/* re-hide stuff if necessary */
		editor_list_button_toggled ();
		parameter_changed ("show-summary");
		parameter_changed ("show-group-tabs");
		parameter_changed ("show-zoom-tools");

		/* now reset all audio_time_axis heights, because widgets might need
		   to be re-hidden
		*/

		TimeAxisView *tv;

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			tv = (static_cast<TimeAxisView*>(*i));
			tv->reset_height ();
		}

		if (current_mixer_strip) {
			current_mixer_strip->hide_things ();
			current_mixer_strip->parameter_changed ("mixer-strip-visibility");
		}
	}

	present ();
}

void
Editor::instant_save ()
{
	if (!constructed || !ARDOUR_UI::instance()->session_loaded) {
		return;
	}

	if (_session) {
		_session->add_instant_xml(get_state());
	} else {
		Config->add_instant_xml(get_state());
	}
}

void
Editor::vertical_zoom_step_in ()
{
    _vertical_zoom_adjustment.set_value(_vertical_zoom_adjustment.get_value()+1);

}

void
Editor::vertical_zoom_step_out ()
{
     _vertical_zoom_adjustment.set_value(_vertical_zoom_adjustment.get_value()-1);
}


void
Editor::control_vertical_zoom_in_all ()
{
	tav_zoom_smooth (false, true);
}

void
Editor::control_vertical_zoom_out_all ()
{
	tav_zoom_smooth (true, true);
}

void
Editor::control_vertical_zoom_in_selected ()
{
	tav_zoom_smooth (false, false);
}

void
Editor::control_vertical_zoom_out_selected ()
{
	tav_zoom_smooth (true, false);
}

void
Editor::control_view (uint32_t view)
{
	goto_visual_state (view);
}

void
Editor::control_unselect ()
{
	selection->clear_tracks ();
}

void
Editor::control_select (uint32_t rid, Selection::Operation op) 
{
	/* handles the (static) signal from the ControlProtocol class that
	 * requests setting the selected track to a given RID
	 */
	 
	if (!_session) {
		return;
	}

	boost::shared_ptr<Route> r = _session->route_by_remote_id (rid);

	if (!r) {
		return;
	}

	TimeAxisView* tav = axis_view_from_route (r);

	if (tav) {
		switch (op) {
		case Selection::Add:
			selection->add (tav);
			break;
		case Selection::Toggle:
			selection->toggle (tav);
			break;
		case Selection::Extend:
			break;
		case Selection::Set:
			selection->set (tav);
			break;
		}
	} else {
		selection->clear_tracks ();
	}
}

void
Editor::control_step_tracks_up ()
{
	scroll_tracks_up_line ();
}

void
Editor::control_step_tracks_down ()
{
	scroll_tracks_down_line ();
}

void
Editor::control_scroll (float fraction)
{
	ENSURE_GUI_THREAD (*this, &Editor::control_scroll, fraction)

	if (!_session) {
		return;
	}

	double step = fraction * current_page_samples();

	/*
		_control_scroll_target is an optional<T>

		it acts like a pointer to an framepos_t, with
		a operator conversion to boolean to check
		that it has a value could possibly use
		playhead_cursor->current_frame to store the
		value and a boolean in the class to know
		when it's out of date
	*/

	if (!_control_scroll_target) {
		_control_scroll_target = _session->transport_frame();
		_dragging_playhead = true;
	}

	if ((fraction < 0.0f) && (*_control_scroll_target < (framepos_t) fabs(step))) {
		*_control_scroll_target = 0;
	} else if ((fraction > 0.0f) && (max_framepos - *_control_scroll_target < step)) {
		*_control_scroll_target = max_framepos - (current_page_samples()*2); // allow room for slop in where the PH is on the screen
	} else {
		*_control_scroll_target += (framepos_t) floor (step);
	}

	/* move visuals, we'll catch up with it later */

	playhead_cursor->set_position (*_control_scroll_target);
	UpdateAllTransportClocks (*_control_scroll_target);

	if (*_control_scroll_target > (current_page_samples() / 2)) {
		/* try to center PH in window */
		reset_x_origin (*_control_scroll_target - (current_page_samples()/2));
	} else {
		reset_x_origin (0);
	}

	/*
		Now we do a timeout to actually bring the session to the right place
		according to the playhead. This is to avoid reading disk buffers on every
		call to control_scroll, which is driven by ScrollTimeline and therefore
		probably by a control surface wheel which can generate lots of events.
	*/
	/* cancel the existing timeout */

	control_scroll_connection.disconnect ();

	/* add the next timeout */

	control_scroll_connection = Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &Editor::deferred_control_scroll), *_control_scroll_target), 250);
}

bool
Editor::deferred_control_scroll (framepos_t /*target*/)
{
	_session->request_locate (*_control_scroll_target, _session->transport_rolling());
	// reset for next stream
	_control_scroll_target = boost::none;
	_dragging_playhead = false;
	return false;
}

void
Editor::access_action (std::string action_group, std::string action_item)
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &Editor::access_action, action_group, action_item)

	RefPtr<Action> act;
	act = ActionManager::get_action( action_group.c_str(), action_item.c_str() );

	if (act) {
		act->activate();
	}
}

void
Editor::on_realize ()
{
	Window::on_realize ();
	Realized ();

    signal_event().connect (sigc::mem_fun (*this, &Editor::generic_event_handler));
}

// Update lock time and auto_save time
void
Editor::on_ardour_ui_config_changed(const std::string& param)
{
    if (param=="auto-lock-timer" && ARDOUR_UI::instance()->screen_lock_is_allowed())
        start_lock_event_timing();

    if (param=="auto-save-timer" && ARDOUR_UI::instance()->session_auto_save_is_allowed())
        start_session_auto_save_event_timing();
}

void
Editor::start_lock_event_timing ()
{    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    
    timeout_connection.disconnect();
    
    if( !ardour_ui->screen_lock_is_allowed() )
        return;
    
    gettimeofday(&last_event_time, 0);
    timeout_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::lock_timeout_callback), 1 * 1000);
}

bool
Editor::lock_timeout_callback ()
{
    struct timeval now, delta;
    
    gettimeofday (&now, 0);
    
    timersub (&now, &last_event_time, &delta);
    
    if( !ARDOUR_UI::instance()->screen_lock_is_allowed() )
        return false; // Returning false will effectively disconnect us from the timer callback.
    
    if (delta.tv_sec >= 60 * ARDOUR_UI::config()->get_auto_lock_timer ())
    {
        lock ();
        /* don't call again. Returning false will effectively
         disconnect us from the timer callback.
         
         unlock() will call start_lock_event_timing() to get things
         started again.
         */
        return false;
    }
    
    return true;
}

void
Editor::start_session_auto_save_event_timing ()
{
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    
    _session_auto_save_timeout_connection.disconnect();
    
    if( !ardour_ui->session_auto_save_is_allowed() )
        return;

    gettimeofday(&_start_recording_time, 0);
    
    _session_auto_save_timeout_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::session_auto_save_timeout_callback), 1 * 1000);
}

void
Editor::on_record_state_changed ()
{
    if (!_session) {
        return;
    }
    
    if (_session->actively_recording() && _drags->active() ) {
        _drags->abort ();
    }
    
    start_lock_event_timing ();
    start_session_auto_save_event_timing ();
    
    /* The same restriction actions are handled when transport starts/stops
     * by on_transport_state_changed () handler
     
    if (_session->actively_recording() && _session->have_rec_enabled_track () ) {
        set_record_and_playback_restricted_actions_sensitive (false);
    } else {
        set_record_and_playback_restricted_actions_sensitive (true);
    }*/
}

void
Editor::on_transport_state_changed ()
{
    if (!_session) {
        return;
    }
    
    if (_session->transport_rolling () ) {
        set_record_and_playback_restricted_actions_sensitive (false);
    } else {
        set_record_and_playback_restricted_actions_sensitive (true);
    }

	if (!_session->step_editing()) {
		get_waves_button ("transport_record_button").set_active (_session->actively_recording());
	}
}

void
Editor::set_record_and_playback_restricted_actions_sensitive (bool yn)
{
    ActionManager::set_sensitive (ActionManager::record_and_playback_restricted_actions, yn);
    set_track_header_dnd_active (yn);
    // update actions we might activated prematurely
    if (yn) {
        // check if we should enable track selection sensitive actions
        ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, track_selected() );
        // check if we should enable region selection sensitive actions
        ActionManager::set_sensitive (ActionManager::region_list_selection_sensitive_actions, !selection->regions.empty ());
        // check if we should enable time selection sensitive actions
        ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, !selection->time.empty ());
        // check if we should update selection sensitive actions
        update_edit_selection_menu ();
    }
}

bool
Editor::session_auto_save_timeout_callback ()
{
    struct timeval now, delta;
    
    gettimeofday (&now, 0);
    
    timersub (&now, &_start_recording_time, &delta);
    
    if( !ARDOUR_UI::instance()->session_auto_save_is_allowed() )
        return false; // Returning false will effectively disconnect us from the timer callback.
    
    if (delta.tv_sec >= 60 * ARDOUR_UI::config()->get_auto_save_timer ())
    {
        ARDOUR_UI::instance()->save_state();
        gettimeofday(&_start_recording_time, 0);
    }
    
    return true;
}

bool
Editor::generic_event_handler (GdkEvent* ev)
{
        switch (ev->type) {
        case GDK_BUTTON_PRESS:
                reset_focus();
        case GDK_BUTTON_RELEASE:
        case GDK_MOTION_NOTIFY:
        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
			gettimeofday (&last_event_time, 0);
			break;
		case GDK_LEAVE_NOTIFY:
			switch (ev->crossing.detail) {
			case GDK_NOTIFY_UNKNOWN:
			case GDK_NOTIFY_INFERIOR:
			case GDK_NOTIFY_ANCESTOR:
			break; 
			case GDK_NOTIFY_VIRTUAL:
			case GDK_NOTIFY_NONLINEAR:
			case GDK_NOTIFY_NONLINEAR_VIRTUAL:
			break;
			}
		break;
		case GDK_FOCUS_CHANGE:
			if (((GdkEventFocus*)ev)->in) {
				reset_focus();
			}
			break;
       default:
			break;
        }
        return false;
}

void
Editor::map_position_change (framepos_t frame)
{
	ENSURE_GUI_THREAD (*this, &Editor::map_position_change, frame)

	if (_session == 0) {
		return;
	}

	if (_follow_playhead) {
		center_screen (frame);
	}
    
	playhead_cursor->set_position (frame);
}

void
Editor::center_screen (framepos_t frame)
{
	framecnt_t const page = _visible_canvas_width * samples_per_pixel;

	/* if we're off the page, then scroll.
	 */

	if (frame < leftmost_frame || frame >= leftmost_frame + page) {
		center_screen_internal (frame, page);
	}
}

void
Editor::center_screen_internal (framepos_t frame, float page)
{
	page /= 2;

	if (frame > page) {
		frame -= (framepos_t) page;
	} else {
		frame = 0;
	}

	reset_x_origin (frame);
}


void
Editor::update_title ()
{
	ENSURE_GUI_THREAD (*this, &Editor::update_title)

	if (_session) {
		bool dirty = _session->dirty();

		string session_name;

		if (_session->snap_name() != _session->name()) {
			session_name = _session->snap_name();
		} else {
			session_name = _session->name();
		}

		if (dirty) {
			session_name = "*" + session_name;
		}

		WindowTitle title(session_name);
		title += Glib::get_application_name();
		set_title (title.get_string());
	} else {
		/* ::session_going_away() will have taken care of it */
	}
}

namespace {
    const size_t mixer_bridge_strip_max_name_size = 9;
    const size_t meter_bridge_strip_max_name_size = 8;
}
void
Editor::set_session (Session *t)
{
    _set_session_in_progress = true;
    
	SessionHandlePtr::set_session (t);

	if (!_session) {
		return;
	}

    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.4);
	_playlist_selector->set_session (_session);
	nudge_clock->set_session (_session);
	_summary->set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.45);
	_group_tabs->set_session (_session);
	_route_groups->set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.5);
	_regions->set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.55);
	_snapshots->set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.6);
	_routes->set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.65);
	_locations->set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.7);
	_compact_meter_bridge.set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.75);
    _mixer_bridge_view.set_max_name_size(mixer_bridge_strip_max_name_size);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.8);
    _mixer_bridge_view.set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.85);
    _meter_bridge_view.set_max_name_size(meter_bridge_strip_max_name_size);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.9);
	_meter_bridge_view.set_session (_session);
    ARDOUR_UI::instance()->_progress_dialog.set_progress (0.95);
    
	if (rhythm_ferret) {
		rhythm_ferret->set_session (_session);
	}

	if (analysis_window) {
		analysis_window->set_session (_session);
	}

	compute_fixed_ruler_scale ();

	/* Make sure we have auto loop and auto punch ranges */

	Location* loc = _session->locations()->auto_loop_location();
	if (loc != 0) {
		loc->set_name (_("Loop"));
	}

	loc = _session->locations()->auto_punch_location();
	if (loc != 0) {
		// force name
		loc->set_name (_("Punch"));
	}

	refresh_location_display ();

	/* This must happen after refresh_location_display(), as (amongst other things) we restore
	   the selected Marker; this needs the LocationMarker list to be available.
	*/
	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	set_state (*node, Stateful::loading_state_version);

    /* listen for incoming scene change messages so we can indicate relevant MIDI activity in the GUI */
    
    MIDISceneChanger* msc = dynamic_cast<MIDISceneChanger*> (_session->scene_changer());

    if (msc) {
        msc->MIDIInputActivity.connect (_session_connections, invalidator (*this), boost::bind (&Editor::marker_midi_input_activity, this), gui_context());
        msc->MIDIOutputActivity.connect (_session_connections, invalidator (*this), boost::bind (&Editor::marker_midi_output_activity, this), gui_context());
    }

    _session->reconnect_midi_scene_ports (true);
    _session->reconnect_midi_scene_ports (false);
    update_midi_dropdowns ();

	/* catch up with the playhead */

	_session->request_locate (playhead_cursor->current_frame ());
	_pending_initial_locate = true;

	update_title ();

	/* These signals can all be emitted by a non-GUI thread. Therefore the
	   handlers for them must not attempt to directly interact with the GUI,
	   but use PBD::Signal<T>::connect() which accepts an event loop
	   ("context") where the handler will be asked to run.
	*/

	_session->StepEditStatusChange.connect (_session_connections, invalidator (*this), boost::bind (&Editor::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, invalidator (*this), boost::bind (&Editor::map_transport_state, this), gui_context());
	_session->PositionChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::map_position_change, this, _1), gui_context());
	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Editor::add_routes, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::update_title, this), gui_context());
	_session->tempo_map().PropertyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::tempo_map_changed, this, _1), gui_context());
	_session->Located.connect (_session_connections, invalidator (*this), boost::bind (&Editor::located, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::parameter_changed, this, _1), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Editor::session_state_saved, this, _1), gui_context());
	_session->locations()->added.connect (_session_connections, invalidator (*this), boost::bind (&Editor::add_new_location, this, _1), gui_context());
	_session->locations()->removed.connect (_session_connections, invalidator (*this), boost::bind (&Editor::location_gone, this, _1), gui_context());
	_session->locations()->changed.connect (_session_connections, invalidator (*this), boost::bind (&Editor::refresh_location_display, this), gui_context());
	_session->history().Changed.connect (_session_connections, invalidator (*this), boost::bind (&Editor::history_changed, this), gui_context());
    _session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::on_record_state_changed, this), gui_context());
    _session->RecordArmStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::on_record_state_changed, this), gui_context());
    _session->TransportStateChange.connect (_session_connections, invalidator (*this), boost::bind (&Editor::on_transport_state_changed, this), gui_context());
    _session->locations()->session_range_location()->StartChanged.connect(_session_connections, invalidator (*this), boost::bind (&Editor::update_horizontal_adjustment_limits, this), gui_context() );
    _session->locations()->session_range_location()->EndChanged.connect(_session_connections, invalidator (*this), boost::bind (&Editor::update_horizontal_adjustment_limits, this), gui_context() );
    
    ARDOUR_UI::Blink.connect (sigc::mem_fun(*this, &Editor::solo_blink));
    
    update_horizontal_adjustment_limits();
    
    // one route was removed/added
    _session->RouteAddedOrRemoved.connect (_session_connections, invalidator (*this), boost::bind (&Editor::update_progress_dialog_of_changing_tracks, this, _1), gui_context());
    
    // connect existing tracks to Global Record button
    connect_routes_and_update_global_rec_button( *(_session->get_tracks().get()) );
    
	playhead_cursor->show ();

	boost::function<void (string)> pc (boost::bind (&Editor::parameter_changed, this, _1));
	Config->map_parameters (pc);
	_session->config.map_parameters (pc);

	restore_ruler_visibility ();
	//tempo_map_changed (PropertyChange (0));
	_session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks);

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(static_cast<TimeAxisView*>(*i))->set_samples_per_pixel (samples_per_pixel);
	}

	super_rapid_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (
		sigc::mem_fun (*this, &Editor::super_rapid_screen_update)
		);

	switch (_snap_type) {
	case SnapToRegionStart:
	case SnapToRegionEnd:
	case SnapToRegionSync:
	case SnapToRegionBoundary:
		build_region_boundary_cache ();
		break;

	default:
		break;
	}

	/* register for undo history */
	_session->register_with_memento_command_factory(id(), this);

	ActionManager::ui_manager->signal_pre_activate().connect (sigc::mem_fun (*this, &Editor::action_pre_activated));

	start_updating_meters ();
    
    // create or init master bus UI
    if (!_master_bus_ui) {
        _master_bus_ui = new MasterBusUI (_session, *this);
        _master_bus_ui_home.add (*_master_bus_ui);
    } else {
        _master_bus_ui->init(_session);
    }
    
    bool stereo_out_mode_active = Config->get_output_auto_connect() & AutoConnectMaster;
    _master_bus_ui->master_bus_set_visible ( stereo_out_mode_active );
    _mixer_bridge_view.all_gain_sliders_set_visible (stereo_out_mode_active);
    if (current_mixer_strip) {
        // if inspector existed
        current_mixer_strip->gain_slider_set_visible (stereo_out_mode_active);
    }

    // For certain reason the slider is not updated when
	// such a session opened, which does not have instant.xml
	// Newly created sessions have the same problem
	update_temporal_zoom_slider (); // HOT FIX. (REWORK IT)

	_set_session_in_progress = false;
}

void
Editor::restore_session_to_default ()
{
    // restore session to default means:
    // leave only mono tracks and
    // make tracks amount equal to the amount of active inputs
    
    if (_session == 0) {
        /* shouldn't happen: menu item is insensitive */
        return;
    }
    
    // evaluate necessity to prompt user
    // that this operation is going to remove tracks that contain regions
    // user may agree to proceed ro disagree
    
    // we will unconditionaly remove stereo (or other multiple channel) tracks
    // start form this point
    // check if we have multiple channel tracks
    // check if they contain audio regions
    bool have_to_remove_regions = false;
    uint32_t multi_chan_tracks = 0;
    {
        TrackViewList::iterator it = track_views.begin();
        
        for (; it != track_views.end (); ++it) {
            RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*it);
            
            if (rtv && rtv->is_audio_track () ) {
                uint32_t n_inputs = rtv->audio_track()->n_inputs().n_audio();
                
                if (n_inputs >= 2) {
                    ++multi_chan_tracks;
                    
                    if (rtv->has_regionviews () ) {
                        have_to_remove_regions = true;
                        break;
                    }
                }
            }
        }
    }
    
    if (have_to_remove_regions != true) {
        // proceed with evaluation
        // check if after removing stereo (or other multiple channel) tracks
        // we still have to remove mono tracks
        const uint32_t n_inputs = EngineStateController::instance()->get_available_inputs_count();
        const uint32_t n_tracks = _session->ntracks ();
        const uint32_t n_mono_tracks = n_tracks - multi_chan_tracks;
        
        if (n_mono_tracks > n_inputs) {
            list<Selectable *> touched;
            uint32_t tracks_to_remove = n_mono_tracks - n_inputs;
            TrackViewList::reverse_iterator rit = track_views.rbegin();
            
            while (tracks_to_remove > 0) {
                
                // make sure we've found audio track
                RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*rit);
                if (rtv && rtv->is_audio_track () &&
                    rtv->audio_track()->n_inputs().n_audio() == 1) {
                    
                    --tracks_to_remove;
                    if (rtv->has_regionviews () ) {
                        have_to_remove_regions = true;
                        break;
                    }
                }
                
                ++rit;
            }
        }
    }
    
    bool do_restore = true;
    if (have_to_remove_regions) {
        int result = WavesMessageDialog ("", _("Tracks that contain regions will be removed.\n\nProceed?"),
                                         WavesMessageDialog::BUTTON_YES |
                                         WavesMessageDialog::BUTTON_NO).run ();
        
        switch (result) {
                // button "YES" was pressed
            case WavesDialog::RESPONSE_DEFAULT:
            case RESPONSE_YES:
                do_restore = true;
                break;
            default:
				do_restore = false;
                break;
        }
    }
    
    if (do_restore) {
        _session->restore_to_default ();
    }
}

void
Editor::action_pre_activated (Glib::RefPtr<Action> const & a)
{
	if (a->get_name() == "RegionMenu") {
		/* When the main menu's region menu is opened, we setup the actions so that they look right
		   in the menu.  I can't find a way of getting a signal when this menu is subsequently closed,
		   so we resensitize all region actions when the entered regionview or the region selection
		   changes.  HOWEVER we can't always resensitize on entered_regionview change because that
		   happens after the region context menu is opened.  So we set a flag here, too.

		   What a carry on :(
		*/
		sensitize_the_right_region_actions ();
		_last_region_menu_was_main = true;
	}
}

void
Editor::fill_xfade_menu (Menu_Helpers::MenuList& items, bool start)
{
	using namespace Menu_Helpers;

	void (Editor::*emf)(FadeShape);
	std::map<ARDOUR::FadeShape,Gtk::Image*>* images;

	if (start) {
		images = &_xfade_in_images;
		emf = &Editor::set_fade_in_shape;
	} else {
		images = &_xfade_out_images;
		emf = &Editor::set_fade_out_shape;
	}

	items.push_back (
		ImageMenuElem (
			_("Linear (for highly correlated material)"),
			*(*images)[FadeLinear],
			sigc::bind (sigc::mem_fun (*this, emf), FadeLinear)
			)
		);
	
	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();
	
	items.push_back (
		ImageMenuElem (
			_("Constant power"),
			*(*images)[FadeConstantPower],
			sigc::bind (sigc::mem_fun (*this, emf), FadeConstantPower)
			));
	
	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();
	
	items.push_back (
		ImageMenuElem (
			_("Symmetric"),
			*(*images)[FadeSymmetric],
			sigc::bind (sigc::mem_fun (*this, emf), FadeSymmetric)
			)
		);
	
	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();
	
	items.push_back (
		ImageMenuElem (
			_("Slow"),
			*(*images)[FadeSlow],
			sigc::bind (sigc::mem_fun (*this, emf), FadeSlow)
			));
	
	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();
	
	items.push_back (
		ImageMenuElem (
			_("Fast"),
			*(*images)[FadeFast],
			sigc::bind (sigc::mem_fun (*this, emf), FadeFast)
			));
	
	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();
}

/** Pop up a context menu for when the user clicks on a start crossfade */
void
Editor::popup_xfade_in_context_menu (int button, int32_t time, ArdourCanvas::Item* item, ItemType /*item_type*/)
{
	using namespace Menu_Helpers;
    MenuList& items (xfade_in_context_menu.items());
    items.clear ();
	fill_xfade_menu (items, true);

	xfade_in_context_menu.popup (button, time);
}

/** Pop up a context menu for when the user clicks on an end crossfade */
void
Editor::popup_xfade_out_context_menu (int button, int32_t time, ArdourCanvas::Item* item, ItemType /*item_type*/)
{
	using namespace Menu_Helpers;
    MenuList& items (xfade_out_context_menu.items());
    items.clear ();
	fill_xfade_menu (items, false);

	xfade_out_context_menu.popup (button, time);
}

void
Editor::popup_track_context_menu (int button, int32_t time, ItemType item_type, bool with_selection)
{
	using namespace Menu_Helpers;
	Menu* (Editor::*build_menu_function)();
	Menu *menu;

	switch (item_type) {
	case RegionItem:
	case RegionViewName:
	case RegionViewNameHighlight:
	case LeftFrameHandle:
	case RightFrameHandle:
		if (with_selection) {
			build_menu_function = &Editor::build_track_selection_context_menu;
		} else {
			build_menu_function = &Editor::build_track_region_context_menu;
		}
		break;

	case SelectionItem:
		if (with_selection) {
			build_menu_function = &Editor::build_track_selection_context_menu;
		} else {
			build_menu_function = &Editor::build_track_context_menu;
		}
		break;

	case StreamItem:
		if (clicked_routeview->track()) {
			build_menu_function = &Editor::build_track_context_menu;
		} else {
			build_menu_function = &Editor::build_track_bus_context_menu;
		}
		break;

	default:
		/* probably shouldn't happen but if it does, we don't care */
		return;
	}

	menu = (this->*build_menu_function)();
	menu->set_name ("ArdourContextMenu");

	/* now handle specific situations */

	switch (item_type) {
	case RegionItem:
	case RegionViewName:
	case RegionViewNameHighlight:
	case LeftFrameHandle:
	case RightFrameHandle:
		if (!with_selection) {
			if (region_edit_menu_split_item) {
				if (clicked_regionview && clicked_regionview->region()->covers (get_preferred_edit_position())) {
					ActionManager::set_sensitive (ActionManager::edit_point_in_region_sensitive_actions, true);
				} else {
					ActionManager::set_sensitive (ActionManager::edit_point_in_region_sensitive_actions, false);
				}
			}
			if (region_edit_menu_split_multichannel_item) {
				if (clicked_regionview && clicked_regionview->region()->n_channels() > 1) {
					region_edit_menu_split_multichannel_item->set_sensitive (true);
				} else {
					region_edit_menu_split_multichannel_item->set_sensitive (false);
				}
			}
		}
		break;

	case SelectionItem:
		break;

	case StreamItem:
		break;

	default:
		/* probably shouldn't happen but if it does, we don't care */
		return;
	}

	if (item_type == StreamItem && clicked_routeview) {
		clicked_routeview->build_underlay_menu(menu);
	}

	/* When the region menu is opened, we setup the actions so that they look right
	   in the menu.
	*/
	sensitize_the_right_region_actions ();
	_last_region_menu_was_main = false;

	menu->signal_hide().connect (sigc::bind (sigc::mem_fun (*this, &Editor::sensitize_all_region_actions), true));
	menu->popup (button, time);
}

Menu*
Editor::build_track_context_menu ()
{
	using namespace Menu_Helpers;

 	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

    add_track_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_bus_context_menu ()
{
	using namespace Menu_Helpers;

 	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_bus_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_region_context_menu ()
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_region_context_menu.items();
	edit_items.clear();

    add_region_context_items (edit_items);
	return &track_region_context_menu;
}

void
Editor::analyze_region_selection ()
{
	if (analysis_window == 0) {
		analysis_window = new AnalysisWindow();

		if (_session != 0)
			analysis_window->set_session(_session);

		analysis_window->show_all();
	}

	analysis_window->set_regionmode();
	analysis_window->analyze();

	analysis_window->present();
}

void
Editor::analyze_range_selection()
{
	if (analysis_window == 0) {
		analysis_window = new AnalysisWindow();

		if (_session != 0)
			analysis_window->set_session(_session);

		analysis_window->show_all();
	}

	analysis_window->set_rangemode();
	analysis_window->analyze();

	analysis_window->present();
}

Menu*
Editor::build_track_selection_context_menu ()
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_selection_context_menu.items();
	edit_items.clear ();

	add_selection_context_items (edit_items);
	// edit_items.push_back (SeparatorElem());
	// add_dstream_context_items (edit_items);

	return &track_selection_context_menu;
}


/** Add context menu items relevant to selection ranges.
 * @param edit_items List to add the items to.
 */
void
Editor::add_selection_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	edit_items.push_back (MenuElem (_("Play Range"), sigc::mem_fun(*this, &Editor::play_selection)));
	edit_items.push_back (MenuElem (_("Loop Range"), sigc::bind (sigc::mem_fun(*this, &Editor::set_loop_from_selection), true)));

	edit_items.push_back (SeparatorElem());

	edit_items.push_back (
		MenuElem (
			_("Move Range Start to Previous Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), false, false)
			)
		);

	edit_items.push_back (
		MenuElem (
			_("Move Range Start to Next Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), false, true)
			)
		);

	edit_items.push_back (
		MenuElem (
			_("Move Range End to Previous Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), true, false)
			)
		);

	edit_items.push_back (
		MenuElem (
			_("Move Range End to Next Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), true, true)
			)
		);

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Set Loop from Range"), sigc::bind (sigc::mem_fun(*this, &Editor::set_loop_from_selection), false)));

    edit_items.push_back (SeparatorElem());
    edit_items.push_back (MenuElem (_("Add to locations list "), sigc::mem_fun (*this, &Editor::add_to_location_list)));
    
    if (selection->time.update_current_selection_is_allowed ()) {
        edit_items.push_back (MenuElem (_("Update current selection"), sigc::mem_fun (*this, &Editor::update_current_selection)));
    }
    
	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Crop Region to Range"), sigc::mem_fun(*this, &Editor::crop_region_to_selection)));
	
	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Consolidate Range"), sigc::bind (sigc::mem_fun(*this, &Editor::bounce_range_selection), true, false)));
	edit_items.push_back (MenuElem (_("Export Range..."), sigc::mem_fun(*this, &Editor::export_selection)));
	if (ARDOUR_UI::instance()->video_timeline->get_duration() > 0) {
		edit_items.push_back (MenuElem (_("Export Video Range..."), sigc::bind (sigc::mem_fun(*this, &Editor::export_video), true)));
	}
}

void
Editor::add_region_context_items (Gtk::Menu_Helpers::MenuList& edit_items)
{
    using namespace Menu_Helpers;
    
    // Cut, copy, paste, delete
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("editor-cut")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("editor-copy")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("editor-paste")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("editor-delete")->create_menu_item ()));  // or 'remove-region'
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("rename-region")->create_menu_item ()));
    edit_items.push_back (SeparatorElem());

    edit_items.push_back (*manage (ActionManager::get_action_from_name ("mute-region")->create_menu_item ()));
    edit_items.push_back (SeparatorElem());

    // Align Selected Regions
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("set-loop-from-region")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("region-to-session-start")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("regions-relative-to-session-start")->create_menu_item ()));

    edit_items.push_back (*manage (ActionManager::get_action_from_name ("naturalize-region")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("regions-to-playhead")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("toggle-region-lock")->create_menu_item ()));
    edit_items.push_back (SeparatorElem());
    
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("import-audio-files")->create_menu_item ()));
    edit_items.push_back (*manage (ActionManager::get_action_from_name ("export-region")->create_menu_item ()));

//    edit_items.push_back (*manage (ActionManager::get_action_from_name ("reverse-region")->create_menu_item ()));

//    Tracks Live doesn't use it
//    edit_items.push_back (*manage (ActionManager::get_action_from_name ("strip-region-silence")->create_menu_item ()));
//    edit_items.back().set_label ("Strip Silence");
//    edit_items.push_back (*manage (ActionManager::get_action_from_name ("pitch-shift-region")->create_menu_item ()));
//    edit_items.back().set_label ("Pitch Shift");
    
    // Gain
    
}


void
Editor::add_track_context_items (Menu_Helpers::MenuList& edit_items)
{
    using namespace Menu_Helpers;
    
    // Tools menu
    Glib::RefPtr<Gtk::Action> act = ActionManager::get_action_from_name ("set-mouse-mode-range");
    assert (act);
    Image* icon;
    icon = manage (new Gtk::Image (get_icon_path (X_ ("tool_marker_idle"))));
    edit_items.push_back (ImageMenuElem ("Range Select Tool", *icon, sigc::bind (sigc::mem_fun (*this, &Editor::activate_track_context_menu_action), act)));
    
    act = ActionManager::get_action_from_name ("set-mouse-mode-object");
    assert (act);
    icon = manage (new Gtk::Image (get_icon_path (X_ ("tool_arrow_idle"))));
    edit_items.push_back (ImageMenuElem ("Pointer Tool", *icon, sigc::bind (sigc::mem_fun (*this, &Editor::activate_track_context_menu_action), act)));
  
    act = ActionManager::get_action_from_name ("set-mouse-mode-cut");
    assert (act);
    icon = manage (new Gtk::Image (get_icon_path (X_ ("tool_cut_idle"))));
	edit_items.push_back (ImageMenuElem ("Split Tool", *icon, sigc::bind (sigc::mem_fun (*this, &Editor::activate_track_context_menu_action), act)));
    
    act = ActionManager::get_action_from_name ("set-mouse-mode-zoom");
    assert (act);
    icon = manage (new Gtk::Image (get_icon_path (X_ ("tool_zoom_idle"))));
    edit_items.push_back (ImageMenuElem ("Zoom Tool", *icon, sigc::bind (sigc::mem_fun (*this, &Editor::activate_track_context_menu_action), act)));
}

void
Editor::activate_track_context_menu_action (Glib::RefPtr<Gtk::Action>& act)
{
    act->activate ();
}

void
Editor::add_bus_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	/* Playback */

	Menu *play_menu = manage (new Menu);
	MenuList& play_items = play_menu->items();
	play_menu->set_name ("ArdourContextMenu");

	play_items.push_back (MenuElem (_("Play From Edit Point"), sigc::mem_fun(*this, &Editor::play_from_edit_point)));
	play_items.push_back (MenuElem (_("Play From Start"), sigc::mem_fun(*this, &Editor::play_from_start)));
	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");

	select_items.push_back (MenuElem (_("Select All in Track"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_in_track), Selection::Set)));
	select_items.push_back (MenuElem (_("Select All Objects"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_objects), Selection::Set)));
	select_items.push_back (MenuElem (_("Invert Selection in Track"), sigc::mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert Selection"), sigc::mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select All After Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), true)));
	select_items.push_back (MenuElem (_("Select All Before Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), false)));
	select_items.push_back (MenuElem (_("Select All After Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, true)));
	select_items.push_back (MenuElem (_("Select All Before Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, false)));

	edit_items.push_back (MenuElem (_("Select"), *select_menu));

	/* Cut-n-Paste */

	Menu *cutnpaste_menu = manage (new Menu);
	MenuList& cutnpaste_items = cutnpaste_menu->items();
	cutnpaste_menu->set_name ("ArdourContextMenu");

	cutnpaste_items.push_back (MenuElem (_("Cut"), sigc::mem_fun(*this, &Editor::cut)));
	cutnpaste_items.push_back (MenuElem (_("Copy"), sigc::mem_fun(*this, &Editor::copy)));
	cutnpaste_items.push_back (MenuElem (_("Paste"), sigc::bind (sigc::mem_fun(*this, &Editor::paste), 1.0f, true)));

	Menu *nudge_menu = manage (new Menu());
	MenuList& nudge_items = nudge_menu->items();
	nudge_menu->set_name ("ArdourContextMenu");

	edit_items.push_back (SeparatorElem());
	nudge_items.push_back (MenuElem (_("Nudge Entire Track Later"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), false, true))));
	nudge_items.push_back (MenuElem (_("Nudge Track After Edit Point Later"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge Entire Track Earlier"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge Track After Edit Point Earlier"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), true, false))));

	edit_items.push_back (MenuElem (_("Nudge"), *nudge_menu));
}

SnapType
Editor::snap_type() const
{
	return _snap_type;
}

SnapMode
Editor::snap_mode() const
{
	return _snap_mode;
}

void
Editor::set_snap_to (SnapType st)
{
	unsigned int snap_ind = (unsigned int)st;

	_snap_type = st;

	if (snap_ind > snap_type_strings.size() - 1) {
		snap_ind = 0;
		_snap_type = (SnapType)snap_ind;
	}

	string str = snap_type_strings[snap_ind];

	if (str != snap_type_selector.get_text()) {
		snap_type_selector.set_text (str);
	}

	instant_save ();

	switch (_snap_type) {
	case SnapToBeatDiv128:
	case SnapToBeatDiv64:
	case SnapToBeatDiv32:
	case SnapToBeatDiv28:
	case SnapToBeatDiv24:
	case SnapToBeatDiv20:
	case SnapToBeatDiv16:
	case SnapToBeatDiv14:
	case SnapToBeatDiv12:
	case SnapToBeatDiv10:
	case SnapToBeatDiv8:
	case SnapToBeatDiv7:
	case SnapToBeatDiv6:
	case SnapToBeatDiv5:
	case SnapToBeatDiv4:
	case SnapToBeatDiv3:
	case SnapToBeatDiv2: {
		ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_begin;
		ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_end;
		
		compute_current_bbt_points (leftmost_frame, leftmost_frame + current_page_samples(),
					    current_bbt_points_begin, current_bbt_points_end);
		compute_bbt_ruler_scale (leftmost_frame, leftmost_frame + current_page_samples(),
					 current_bbt_points_begin, current_bbt_points_end);
		update_tempo_based_rulers (current_bbt_points_begin, current_bbt_points_end);
		break;
	}

	case SnapToRegionStart:
	case SnapToRegionEnd:
	case SnapToRegionSync:
	case SnapToRegionBoundary:
		build_region_boundary_cache ();
		break;

	default:
		/* relax */
		break;
	}

	SnapChanged (); /* EMIT SIGNAL */
}

void
Editor::set_snap_mode (SnapMode mode)
{
	string str = snap_mode_strings[(int)mode];

	if (_internal_editing) {
		internal_snap_mode = mode;
	} else {
		pre_internal_snap_mode = mode;
	}

	_snap_mode = mode;

	if (str != snap_mode_selector.get_text ()) {
		snap_mode_selector.set_text (str);
	}

	instant_save ();
}
void
Editor::set_edit_point_preference (EditPoint ep, bool force)
{
	bool changed = (_edit_point != ep);

	_edit_point = ep;
	string str = edit_point_strings[(int)ep];

	if (Profile->get_mixbus())
		if (ep == EditAtSelectedMarker)
			ep = EditAtPlayhead;
	
	if (str != edit_point_selector.get_text ()) {
		edit_point_selector.set_text (str);
	}

	reset_canvas_cursor ();

	if (!force && !changed) {
		return;
	}

	const char* action=NULL;

	switch (_edit_point) {
	case EditAtPlayhead:
		action = "edit-at-playhead";
		break;
	case EditAtSelectedMarker:
		action = "edit-at-marker";
		break;
	case EditAtMouse:
		action = "edit-at-mouse";
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("Editor", action);
	if (act) {
		Glib::RefPtr<RadioAction>::cast_dynamic(act)->set_active (true);
	}

	framepos_t foo;
	bool in_track_canvas;

	if (!mouse_frame (foo, in_track_canvas)) {
		in_track_canvas = false;
	}

	reset_canvas_action_sensitivity (in_track_canvas);

	instant_save ();
}

int
Editor::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;
	XMLNode* geometry;
	int x, y;
	Gdk::Geometry g;

	set_id (node);

	g.base_width = default_width;
	g.base_height = default_height;
	x = 1;
	y = 1;

	if ((geometry = find_named_node (node, "geometry")) != 0) {

		XMLProperty* prop;

		if ((prop = geometry->property("x_size")) == 0) {
			prop = geometry->property ("x-size");
		}
		if (prop) {
			g.base_width = atoi(prop->value());
		}
		if ((prop = geometry->property("y_size")) == 0) {
			prop = geometry->property ("y-size");
		}
		if (prop) {
			g.base_height = atoi(prop->value());
		}

		if ((prop = geometry->property ("x_pos")) == 0) {
			prop = geometry->property ("x-pos");
		}
		if (prop) {
			x = atoi (prop->value());

		}
		if ((prop = geometry->property ("y_pos")) == 0) {
			prop = geometry->property ("y-pos");
		}
		if (prop) {
			y = atoi (prop->value());
		}
	}

	set_default_size (g.base_width, g.base_height);
	move (x, y);

	if (_session && (prop = node.property ("playhead"))) {
		framepos_t pos;
		sscanf (prop->value().c_str(), "%" PRIi64, &pos);
		playhead_cursor->set_position (pos);
	} else {
		playhead_cursor->set_position (0);
	}

	//if ((prop = node.property ("mixer-width"))) {
	//	editor_mixer_strip_width = Width (string_2_enum (prop->value(), editor_mixer_strip_width));
	//}

	if ((prop = node.property ("zoom-focus"))) {
		zoom_focus_selection_done ((ZoomFocus) string_2_enum (prop->value(), zoom_focus));
	}

	if ((prop = node.property ("zoom"))) {
		/* older versions of ardour used floating point samples_per_pixel */
		double f = PBD::atof (prop->value());
		reset_zoom (llrintf (f));
	} else {
		reset_zoom (samples_per_pixel);
	}

	if ((prop = node.property ("visible-track-count"))) {
		set_visible_track_count (PBD::atoi (prop->value()));
	}

	if ((prop = node.property ("snap-to"))) {
		snap_type_selection_done ((SnapType) string_2_enum (prop->value(), _snap_type));
	}

	if ((prop = node.property ("snap-mode"))) {
		snap_mode_selection_done((SnapMode) string_2_enum (prop->value(), _snap_mode));
	}

	if ((prop = node.property ("internal-snap-to"))) {
		internal_snap_type = (SnapType) string_2_enum (prop->value(), internal_snap_type);
	}

	if ((prop = node.property ("internal-snap-mode"))) {
		internal_snap_mode = (SnapMode) string_2_enum (prop->value(), internal_snap_mode);
	}

	if ((prop = node.property ("pre-internal-snap-to"))) {
		pre_internal_snap_type = (SnapType) string_2_enum (prop->value(), pre_internal_snap_type);
	}


	if ((prop = node.property ("pre-internal-snap-mode"))) {
		pre_internal_snap_mode = (SnapMode) string_2_enum (prop->value(), pre_internal_snap_mode);
	}

	if ((prop = node.property ("mouse-mode"))) {
		MouseMode m = str2mousemode(prop->value());
		set_mouse_mode (m, true);
	} else {
		set_mouse_mode (MouseObject, true);
	}

	if ((prop = node.property ("left-frame")) != 0) {
		framepos_t pos;
		if (sscanf (prop->value().c_str(), "%" PRId64, &pos) == 1) {
			if (pos < 0) {
				pos = 0;
			}
			reset_x_origin (pos);
		}
	}

	if ((prop = node.property ("y-origin")) != 0) {
		reset_y_origin (atof (prop->value ()));
	}

	if ((prop = node.property ("internal-edit"))) {
		bool yn = string_is_affirmative (prop->value());
		RefPtr<Action> act = ActionManager::get_action (X_("MouseMode"), X_("toggle-internal-edit"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}

	if ((prop = node.property ("join-object-range"))) {
		RefPtr<Action> act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-object-range"));
		bool yn = string_is_affirmative (prop->value());
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			tact->set_active (!yn);
			tact->set_active (yn);
		}
		set_mouse_mode(mouse_mode, true);
	}

	if ((prop = node.property ("edit-point"))) {
		set_edit_point_preference ((EditPoint) string_2_enum (prop->value(), _edit_point), true);
	}

	if ((prop = node.property ("show-measures"))) {
		bool yn = string_is_affirmative (prop->value());
		_show_measures = yn;
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleMeasureVisibility"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			/* do it twice to force the change */
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}

	if ((prop = node.property ("follow-playhead"))) {
		bool yn = string_is_affirmative (prop->value());
		set_follow_playhead (yn);
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-follow-playhead"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			if (tact->get_active() != yn) {
				tact->set_active (yn);
			}
		}
	}

	if ((prop = node.property ("stationary-playhead"))) {
		bool yn = string_is_affirmative (prop->value());
		set_stationary_playhead (yn);
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-stationary-playhead"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			if (tact->get_active() != yn) {
				tact->set_active (yn);
			}
		}
	}

	if ((prop = node.property ("region-list-sort-type"))) {
		RegionListSortType st;
		_regions->reset_sort_type ((RegionListSortType) string_2_enum (prop->value(), st), true);
	}

	if ((prop = node.property ("show-editor-mixer"))) {

		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
		assert (act);

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		bool yn = string_is_affirmative (prop->value());

		/* do it twice to force the change */

		tact->set_active (!yn);
		tact->set_active (yn);
	}

	if ((prop = node.property ("show-editor-list"))) {

		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-list"));
		assert (act);

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		bool yn = string_is_affirmative (prop->value());

		/* do it twice to force the change */

		tact->set_active (!yn);
		tact->set_active (yn);
	}

	if ((prop = node.property (X_("editor-list-page")))) {
		_the_notebook.set_current_page (atoi (prop->value ()));
	}

	if ((prop = node.property (X_("show-marker-lines")))) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("show-marker-lines"));
		assert (act);
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
		bool yn = string_is_affirmative (prop->value ());

		tact->set_active (!yn);
		tact->set_active (yn);
	}

	XMLNodeList children = node.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		selection->set_state (**i, Stateful::current_state_version);
		_regions->set_state (**i);
	}

	if ((prop = node.property ("maximised"))) {
		bool yn = string_is_affirmative (prop->value());
		if (yn) {
			ActionManager::do_action ("Common", "ToggleMaximalEditor");
		}
	}

	if ((prop = node.property ("nudge-clock-value"))) {
		framepos_t f;
		sscanf (prop->value().c_str(), "%" PRId64, &f);
		nudge_clock->set (f);
	} else {
		nudge_clock->set_mode (AudioClock::Timecode);
		nudge_clock->set (_session->frame_rate() * 5, true);
	}

	return 0;
}

XMLNode&
Editor::get_state ()
{
	XMLNode* node = new XMLNode ("Editor");
	char buf[32];

	id().print (buf, sizeof (buf));
	node->add_property ("id", buf);

	if (is_realized()) {
		Glib::RefPtr<Gdk::Window> win = get_window();

		int x, y, width, height;
		win->get_root_origin(x, y);
		win->get_size(width, height);

		XMLNode* geometry = new XMLNode ("geometry");

		snprintf(buf, sizeof(buf), "%d", width);
		geometry->add_property("x-size", string(buf));
		snprintf(buf, sizeof(buf), "%d", height);
		geometry->add_property("y-size", string(buf));
		snprintf(buf, sizeof(buf), "%d", x);
		geometry->add_property("x-pos", string(buf));
		snprintf(buf, sizeof(buf), "%d", y);
		geometry->add_property("y-pos", string(buf));
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&edit_pane)->gobj()));
		geometry->add_property("edit-horizontal-pane-pos", string(buf));
		geometry->add_property("notebook-shrunk", _notebook_shrunk ? "1" : "0");
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&editor_summary_pane)->gobj()));
		geometry->add_property("edit-vertical-pane-pos", string(buf));

		node->add_child_nocopy (*geometry);
	}

	//maybe_add_mixer_strip_width (*node);

	node->add_property ("zoom-focus", enum_2_string (zoom_focus));

	snprintf (buf, sizeof(buf), "%" PRId64, samples_per_pixel);
	node->add_property ("zoom", buf);
	node->add_property ("snap-to", enum_2_string (_snap_type));
	node->add_property ("snap-mode", enum_2_string (_snap_mode));
	node->add_property ("internal-snap-to", enum_2_string (internal_snap_type));
	node->add_property ("internal-snap-mode", enum_2_string (internal_snap_mode));
	node->add_property ("pre-internal-snap-to", enum_2_string (pre_internal_snap_type));
	node->add_property ("pre-internal-snap-mode", enum_2_string (pre_internal_snap_mode));
	node->add_property ("edit-point", enum_2_string (_edit_point));
	snprintf (buf, sizeof(buf), "%d", _visible_track_count);
	node->add_property ("visible-track-count", buf);

	snprintf (buf, sizeof (buf), "%" PRIi64, playhead_cursor->current_frame ());
	node->add_property ("playhead", buf);
	snprintf (buf, sizeof (buf), "%" PRIi64, leftmost_frame);
	node->add_property ("left-frame", buf);
	snprintf (buf, sizeof (buf), "%f", vertical_adjustment.get_value ());
	node->add_property ("y-origin", buf);

	node->add_property ("show-measures", _show_measures ? "yes" : "no");
	node->add_property ("maximised", _maximised ? "yes" : "no");
	node->add_property ("follow-playhead", _follow_playhead ? "yes" : "no");
	node->add_property ("stationary-playhead", _stationary_playhead ? "yes" : "no");
	node->add_property ("region-list-sort-type", enum_2_string (_regions->sort_type ()));
	node->add_property ("mouse-mode", enum2str(mouse_mode));
	node->add_property ("internal-edit", _internal_editing ? "yes" : "no");
	node->add_property ("join-object-range", smart_mode_action->get_active () ? "yes" : "no");

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		node->add_property (X_("show-editor-mixer"), tact->get_active() ? "yes" : "no");
	}

	act = ActionManager::get_action (X_("Editor"), X_("show-editor-list"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		node->add_property (X_("show-editor-list"), tact->get_active() ? "yes" : "no");
	}

	snprintf (buf, sizeof (buf), "%d", _the_notebook.get_current_page ());
	node->add_property (X_("editor-list-page"), buf);

        if (button_bindings) {
                XMLNode* bb = new XMLNode (X_("Buttons"));
                button_bindings->save (*bb);
                node->add_child_nocopy (*bb);
        }

	node->add_property (X_("show-marker-lines"), _show_marker_lines ? "yes" : "no");

	node->add_child_nocopy (selection->get_state ());
	node->add_child_nocopy (_regions->get_state ());

	snprintf (buf, sizeof (buf), "%" PRId64, nudge_clock->current_duration());
	node->add_property ("nudge-clock-value", buf);

	return *node;
}

/** Return true if coordinate given by (@param x, @param @y) is within what we define as the "track area". 
 *  Return false otherwise.
 *
 * @param x : x-axis coordinate in canvas coordinate space
 * @param y : x-axis coordinate in canvas coordinate space
 */
bool
Editor::inside_track_area (double x, double y) const
{
	return hv_scroll_group->covers (ArdourCanvas::Duple (x, y));
}

/** if @param trackview_relative_offset is true, @param y y is an offset into the trackview area, in pixel units
 *  if @param trackview_relative_offset is false, @param y y is a global canvas *  coordinate, in pixel units
 *
 *  @return pair: TimeAxisView that y is over, layer index.
 *
 *  TimeAxisView may be 0.  Layer index is the layer number if the TimeAxisView is valid and is
 *  in stacked or expanded region display mode, otherwise 0.
 */
std::pair<TimeAxisView *, double>
Editor::trackview_by_y_position (double y, bool trackview_relative_offset) const
{
	if (!trackview_relative_offset) {
		y -= _trackview_group->canvas_origin().y;
	}

	if (y < 0) {
		return std::make_pair ( (TimeAxisView *) 0, 0);
	}

	for (TrackViewList::const_iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
			
		std::pair<TimeAxisView*, double> const r = (*iter)->covers_y_position (y);
			
		if (r.first) {
			return r;
		}
	}

	return std::make_pair ( (TimeAxisView *) 0, 0);
}

/** Snap a position to the grid, if appropriate, taking into account current
 *  grid settings and also the state of any snap modifier keys that may be pressed.
 *  @param start Position to snap.
 *  @param event Event to get current key modifier information from, or 0.
 */
void
Editor::snap_to_with_modifier (framepos_t& start, GdkEvent const * event, RoundMode direction, bool for_mark)
{
	if (!_session || !event) {
		return;
	}

	if (Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		if (_snap_mode == SnapOff) {
			snap_to_internal (start, direction, for_mark);
		}
	} else {
		if (_snap_mode != SnapOff) {
			snap_to_internal (start, direction, for_mark);
		}
	}
}

void
Editor::snap_to (framepos_t& start, RoundMode direction, bool for_mark)
{
	if (!_session || _snap_mode == SnapOff) {
		return;
	}

	snap_to_internal (start, direction, for_mark);
}

void
Editor::timecode_snap_to_internal (framepos_t& start, RoundMode direction, bool /*for_mark*/)
{
	const framepos_t one_timecode_second = (framepos_t)(rint(_session->timecode_frames_per_second()) * _session->frames_per_timecode_frame());
	framepos_t one_timecode_minute = (framepos_t)(rint(_session->timecode_frames_per_second()) * _session->frames_per_timecode_frame() * 60);

	switch (_snap_type) {
	case SnapToTimecodeFrame:
		if ((direction == RoundUpMaybe || direction == RoundDownMaybe) &&
		    fmod((double)start, (double)_session->frames_per_timecode_frame()) == 0) {
			/* start is already on a whole timecode frame, do nothing */
		} else if (((direction == 0) && (fmod((double)start, (double)_session->frames_per_timecode_frame()) > (_session->frames_per_timecode_frame() / 2))) || (direction > 0)) {
			start = (framepos_t) (ceil ((double) start / _session->frames_per_timecode_frame()) * _session->frames_per_timecode_frame());
		} else {
			start = (framepos_t) (floor ((double) start / _session->frames_per_timecode_frame()) *  _session->frames_per_timecode_frame());
		}
		break;

	case SnapToTimecodeSeconds:
		if (_session->config.get_timecode_offset_negative()) {
			start += _session->config.get_timecode_offset ();
		} else {
			start -= _session->config.get_timecode_offset ();
		}
		if ((direction == RoundUpMaybe || direction == RoundDownMaybe) &&
		    (start % one_timecode_second == 0)) {
			/* start is already on a whole second, do nothing */
		} else if (((direction == 0) && (start % one_timecode_second > one_timecode_second / 2)) || direction > 0) {
			start = (framepos_t) ceil ((double) start / one_timecode_second) * one_timecode_second;
		} else {
			start = (framepos_t) floor ((double) start / one_timecode_second) * one_timecode_second;
		}

		if (_session->config.get_timecode_offset_negative()) {
			start -= _session->config.get_timecode_offset ();
		} else {
			start += _session->config.get_timecode_offset ();
		}
		break;

	case SnapToTimecodeMinutes:
		if (_session->config.get_timecode_offset_negative()) {
			start += _session->config.get_timecode_offset ();
		} else {
			start -= _session->config.get_timecode_offset ();
		}
		if ((direction == RoundUpMaybe || direction == RoundDownMaybe) &&
		    (start % one_timecode_minute == 0)) {
			/* start is already on a whole minute, do nothing */
		} else if (((direction == 0) && (start % one_timecode_minute > one_timecode_minute / 2)) || direction > 0) {
			start = (framepos_t) ceil ((double) start / one_timecode_minute) * one_timecode_minute;
		} else {
			start = (framepos_t) floor ((double) start / one_timecode_minute) * one_timecode_minute;
		}
		if (_session->config.get_timecode_offset_negative()) {
			start -= _session->config.get_timecode_offset ();
		} else {
			start += _session->config.get_timecode_offset ();
		}
		break;
	default:
		fatal << "Editor::smpte_snap_to_internal() called with non-timecode snap type!" << endmsg;
		/*NOTREACHED*/
	}
}

void
Editor::snap_to_internal (framepos_t& start, RoundMode direction, bool for_mark)
{
	const framepos_t one_second = _session->frame_rate();
	const framepos_t one_minute = _session->frame_rate() * 60;
	framepos_t presnap = start;
	framepos_t before;
	framepos_t after;

	switch (_snap_type) {
	case SnapToTimecodeFrame:
	case SnapToTimecodeSeconds:
	case SnapToTimecodeMinutes:
		return timecode_snap_to_internal (start, direction, for_mark);

	case SnapToCDFrame:
		if ((direction == RoundUpMaybe || direction == RoundDownMaybe) &&
		    start % (one_second/75) == 0) {
			/* start is already on a whole CD frame, do nothing */
		} else if (((direction == 0) && (start % (one_second/75) > (one_second/75) / 2)) || (direction > 0)) {
			start = (framepos_t) ceil ((double) start / (one_second / 75)) * (one_second / 75);
		} else {
			start = (framepos_t) floor ((double) start / (one_second / 75)) * (one_second / 75);
		}
		break;

	case SnapToSeconds:
		if ((direction == RoundUpMaybe || direction == RoundDownMaybe) &&
		    start % one_second == 0) {
			/* start is already on a whole second, do nothing */
		} else if (((direction == 0) && (start % one_second > one_second / 2)) || (direction > 0)) {
			start = (framepos_t) ceil ((double) start / one_second) * one_second;
		} else {
			start = (framepos_t) floor ((double) start / one_second) * one_second;
		}
		break;

	case SnapToMinutes:
		if ((direction == RoundUpMaybe || direction == RoundDownMaybe) &&
		    start % one_minute == 0) {
			/* start is already on a whole minute, do nothing */
		} else if (((direction == 0) && (start % one_minute > one_minute / 2)) || (direction > 0)) {
			start = (framepos_t) ceil ((double) start / one_minute) * one_minute;
		} else {
			start = (framepos_t) floor ((double) start / one_minute) * one_minute;
		}
		break;

	case SnapToBar:
		start = _session->tempo_map().round_to_bar (start, direction);
		break;

	case SnapToBeat:
		start = _session->tempo_map().round_to_beat (start, direction);
		break;

	case SnapToBeatDiv128:
		start = _session->tempo_map().round_to_beat_subdivision (start, 128, direction);
		break;
	case SnapToBeatDiv64:
		start = _session->tempo_map().round_to_beat_subdivision (start, 64, direction);
		break;
	case SnapToBeatDiv32:
		start = _session->tempo_map().round_to_beat_subdivision (start, 32, direction);
		break;
	case SnapToBeatDiv28:
		start = _session->tempo_map().round_to_beat_subdivision (start, 28, direction);
		break;
	case SnapToBeatDiv24:
		start = _session->tempo_map().round_to_beat_subdivision (start, 24, direction);
		break;
	case SnapToBeatDiv20:
		start = _session->tempo_map().round_to_beat_subdivision (start, 20, direction);
		break;
	case SnapToBeatDiv16:
		start = _session->tempo_map().round_to_beat_subdivision (start, 16, direction);
		break;
	case SnapToBeatDiv14:
		start = _session->tempo_map().round_to_beat_subdivision (start, 14, direction);
		break;
	case SnapToBeatDiv12:
		start = _session->tempo_map().round_to_beat_subdivision (start, 12, direction);
		break;
	case SnapToBeatDiv10:
		start = _session->tempo_map().round_to_beat_subdivision (start, 10, direction);
		break;
	case SnapToBeatDiv8:
		start = _session->tempo_map().round_to_beat_subdivision (start, 8, direction);
		break;
	case SnapToBeatDiv7:
		start = _session->tempo_map().round_to_beat_subdivision (start, 7, direction);
		break;
	case SnapToBeatDiv6:
		start = _session->tempo_map().round_to_beat_subdivision (start, 6, direction);
		break;
	case SnapToBeatDiv5:
		start = _session->tempo_map().round_to_beat_subdivision (start, 5, direction);
		break;
	case SnapToBeatDiv4:
		start = _session->tempo_map().round_to_beat_subdivision (start, 4, direction);
		break;
	case SnapToBeatDiv3:
		start = _session->tempo_map().round_to_beat_subdivision (start, 3, direction);
		break;
	case SnapToBeatDiv2:
		start = _session->tempo_map().round_to_beat_subdivision (start, 2, direction);
		break;

	case SnapToMark:
		if (for_mark) {
			return;
		}

		_session->locations()->marks_either_side (start, before, after);

		if (before == max_framepos && after == max_framepos) {
			/* No marks to snap to, so just don't snap */
			return;
		} else if (before == max_framepos) {
			start = after;
		} else if (after == max_framepos) {
			start = before;
		} else if (before != max_framepos && after != max_framepos) {
			/* have before and after */
			if ((start - before) < (after - start)) {
				start = before;
			} else {
				start = after;
			}
		}

		break;

	case SnapToRegionStart:
	case SnapToRegionEnd:
	case SnapToRegionSync:
	case SnapToRegionBoundary:
		if (!region_boundary_cache.empty()) {

			vector<framepos_t>::iterator prev = region_boundary_cache.end ();
			vector<framepos_t>::iterator next = region_boundary_cache.end ();

			if (direction > 0) {
				next = std::upper_bound (region_boundary_cache.begin(), region_boundary_cache.end(), start);
			} else {
				next = std::lower_bound (region_boundary_cache.begin(), region_boundary_cache.end(), start);
			}

			if (next != region_boundary_cache.begin ()) {
				prev = next;
				prev--;
			}

			framepos_t const p = (prev == region_boundary_cache.end()) ? region_boundary_cache.front () : *prev;
			framepos_t const n = (next == region_boundary_cache.end()) ? region_boundary_cache.back () : *next;

			if (start > (p + n) / 2) {
				start = n;
			} else {
				start = p;
			}
		}
		break;
	}

	switch (_snap_mode) {
	case SnapNormal:
		return;

	case SnapMagnetic:

		if (presnap > start) {
			if (presnap > (start + pixel_to_sample(snap_threshold))) {
				start = presnap;
			}

		} else if (presnap < start) {
			if (presnap < (start - pixel_to_sample(snap_threshold))) {
				start = presnap;
			}
		}

	default:
		/* handled at entry */
		return;

	}
}

bool
Editor::vertical_fader_pressed(GdkEventButton* ev)
{
    /* MOD1 == "Alt" */

    if (Keyboard::modifier_state_equals (ev->state, GDK_MOD1_MASK)) {
    
        for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
            (*i)->set_height ( 2*22 ); // set height to default value 2*22 pix
        }
        
        _vertical_zoom_adjustment.set_value(2);
        
        return true; // do not propagate this event
    }
    
    return false; // propagate this event
}

void
Editor::setup_toolbar ()
{
	HBox* mode_box = manage(new HBox);
	mode_box->set_border_width (2);
	mode_box->set_spacing(4);

	HBox* mouse_mode_box = manage (new HBox);
	HBox* mouse_mode_hbox = manage (new HBox);
	VBox* mouse_mode_vbox = manage (new VBox);
	Alignment* mouse_mode_align = manage (new Alignment);

	Glib::RefPtr<SizeGroup> mouse_mode_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	mouse_mode_size_group->add_widget (mouse_timefx_button);
	mouse_mode_size_group->add_widget (mouse_audition_button);
	mouse_mode_size_group->add_widget (mouse_draw_button);
	mouse_mode_size_group->add_widget (internal_edit_button);

	/* make them just a bit bigger */
	internal_edit_button.set_size_request (-1, 30);

	mouse_mode_hbox->set_spacing (2);

	mouse_mode_vbox->pack_start (*mouse_mode_hbox);

	mouse_mode_align->add (*mouse_mode_vbox);
	mouse_mode_align->set (0.5, 1.0, 0.0, 0.0);

	mouse_mode_box->pack_start (*mouse_mode_align, false, false);

	edit_mode_selector.set_name ("mouse mode button");
	edit_mode_selector.add_elements (ArdourButton::Inset);

	if (!ARDOUR::Profile->get_trx()) {
		mode_box->pack_start (edit_mode_selector, false, false);
	}
	mode_box->pack_start (*mouse_mode_box, false, false);

	_mouse_mode_tearoff = manage (new TearOff (*mode_box));
	_mouse_mode_tearoff->set_name ("MouseModeBase");
	_mouse_mode_tearoff->tearoff_window().signal_key_press_event().connect (sigc::bind (sigc::ptr_fun (relay_key_press), &_mouse_mode_tearoff->tearoff_window()), false);

	if (Profile->get_sae() || Profile->get_mixbus() ) {
		_mouse_mode_tearoff->set_can_be_torn_off (false);
	}

	/* Zoom */

	_zoom_box.set_spacing (2);
	_zoom_box.set_border_width (2);

	RefPtr<Action> act;

	_temporal_zoom_adjustment.signal_value_changed().connect (mem_fun (*this, &Editor::temporal_zoom_by_slider));
	_vertical_zoom_adjustment.signal_value_changed().connect (mem_fun (*this, &Editor::vertical_zoom_by_slider));
    _waves_zoom_adjustment.signal_value_changed().connect (mem_fun (*this, &Editor::waveform_zoom_changed));
    
	ZoomChanged.connect (sigc::mem_fun (*this, &Editor::update_temporal_zoom_slider));
        
        _vertical_zoom_fader.signal_button_press_event().connect (sigc::mem_fun(*this, &Editor::vertical_fader_pressed), false);

	/* Track zoom buttons */

	visible_tracks_selector.set_name ("zoom button");
//	visible_tracks_selector.add_elements ( ArdourButton::Inset );
	if (Profile->get_mixbus()) {
		visible_tracks_selector.set_size_request (42, -1);
	} else {
		set_size_request_to_display_given_text (visible_tracks_selector, _("All"), 40, 2);
	}

	if (ARDOUR::Profile->get_mixbus()) {
		_zoom_box.pack_start (visible_tracks_selector);
	} else if (ARDOUR::Profile->get_trx()) {
	} else {
		_zoom_box.pack_start (visible_tracks_selector);
	}

	if (!ARDOUR::Profile->get_trx()) {
		_zoom_tearoff = manage (new TearOff (_zoom_box));
		
	} 

	if (Profile->get_sae() || Profile->get_mixbus() ) {
		_zoom_tearoff->set_can_be_torn_off (false);
	}

	snap_box.set_spacing (2);
	snap_box.set_border_width (2);

	snap_type_selector.set_name ("mouse mode button");
	//snap_type_selector.add_elements (ArdourButton::Inset);

	snap_mode_selector.set_name ("mouse mode button");
	//snap_mode_selector.add_elements (ArdourButton::Inset);

	edit_point_selector.set_name ("mouse mode button");
	//edit_point_selector.add_elements (ArdourButton::Inset);

	snap_box.pack_start (snap_mode_selector, false, false);
	snap_box.pack_start (snap_type_selector, false, false);
	snap_box.pack_start (edit_point_selector, false, false);

	/* Nudge */

	HBox *nudge_box = manage (new HBox);
	nudge_box->set_spacing (2);
	nudge_box->set_border_width (2);

	nudge_box->pack_start (*nudge_clock, false, false);


	/* Pack everything in... */

	HBox* hbox = manage (new HBox);
	hbox->set_spacing(10);

	_tools_tearoff = manage (new TearOff (*hbox));
	_tools_tearoff->set_name ("MouseModeBase");
	_tools_tearoff->tearoff_window().signal_key_press_event().connect (sigc::bind (sigc::ptr_fun (relay_key_press), &_tools_tearoff->tearoff_window()), false);

	if (Profile->get_sae() || Profile->get_mixbus()) {
		_tools_tearoff->set_can_be_torn_off (false);
	}

	toolbar_hbox.set_spacing (10);
	toolbar_hbox.set_border_width (1);

	toolbar_hbox.pack_start (*_mouse_mode_tearoff, false, false);
	if (!ARDOUR::Profile->get_trx()) {
		toolbar_hbox.pack_start (*_zoom_tearoff, false, false);
		toolbar_hbox.pack_start (*_tools_tearoff, false, false);
	}

	if (!ARDOUR::Profile->get_trx()) {
		hbox->pack_start (snap_box, false, false);
		if (!Profile->get_small_screen()) {
			hbox->pack_start (*nudge_box, false, false);
		} else {
			ARDOUR_UI::instance()->editor_transport_box().pack_start (*nudge_box, false, false);
		}
	}
	hbox->pack_start (panic_box, false, false);

	hbox->show_all ();

	if (!ARDOUR::Profile->get_trx()) {
		toolbar_base.set_name ("ToolBarBase");
		toolbar_base.add (toolbar_hbox);
	}

	_toolbar_viewport.add (toolbar_base);
	/* stick to the required height but allow width to vary if there's not enough room */
	_toolbar_viewport.set_size_request (1, -1);

	toolbar_frame.set_shadow_type (SHADOW_OUT);
	toolbar_frame.set_name ("BaseFrame");
	toolbar_frame.add (_toolbar_viewport);
}

void
Editor::build_edit_point_menu ()
{
	using namespace Menu_Helpers;

	edit_point_selector.AddMenuElem (MenuElem ( edit_point_strings[(int)EditAtPlayhead], sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_selection_done), (EditPoint) EditAtPlayhead)));
	if(!Profile->get_mixbus())
		edit_point_selector.AddMenuElem (MenuElem ( edit_point_strings[(int)EditAtSelectedMarker], sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_selection_done), (EditPoint) EditAtSelectedMarker)));
	edit_point_selector.AddMenuElem (MenuElem ( edit_point_strings[(int)EditAtMouse], sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_selection_done), (EditPoint) EditAtMouse)));

	set_size_request_to_display_given_text (edit_point_selector, edit_point_strings, 30, 2);
}

void
Editor::build_edit_mode_menu ()
{
	using namespace Menu_Helpers;
	
	edit_mode_selector.AddMenuElem (MenuElem ( edit_mode_strings[(int)Slide], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode) Slide)));
//	edit_mode_selector.AddMenuElem (MenuElem ( edit_mode_strings[(int)Splice], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode) Splice)));
	edit_mode_selector.AddMenuElem (MenuElem ( edit_mode_strings[(int)Ripple], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode) Ripple)));
	edit_mode_selector.AddMenuElem (MenuElem ( edit_mode_strings[(int)Lock], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode)  Lock)));

	set_size_request_to_display_given_text (edit_mode_selector, edit_mode_strings, 30, 2);
}

void
Editor::build_snap_mode_menu ()
{
	using namespace Menu_Helpers;

	snap_mode_selector.AddMenuElem (MenuElem ( snap_mode_strings[(int)SnapOff], sigc::bind (sigc::mem_fun(*this, &Editor::snap_mode_selection_done), (SnapMode) SnapOff)));
	snap_mode_selector.AddMenuElem (MenuElem ( snap_mode_strings[(int)SnapNormal], sigc::bind (sigc::mem_fun(*this, &Editor::snap_mode_selection_done), (SnapMode) SnapNormal)));
	snap_mode_selector.AddMenuElem (MenuElem ( snap_mode_strings[(int)SnapMagnetic], sigc::bind (sigc::mem_fun(*this, &Editor::snap_mode_selection_done), (SnapMode) SnapMagnetic)));

	set_size_request_to_display_given_text (snap_mode_selector, snap_mode_strings, 34, 2);
}

void
Editor::build_snap_type_menu ()
{
	using namespace Menu_Helpers;

	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToCDFrame], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToCDFrame)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToTimecodeFrame], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToTimecodeFrame)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToTimecodeSeconds], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToTimecodeSeconds)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToTimecodeMinutes], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToTimecodeMinutes)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToSeconds], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToSeconds)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToMinutes], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToMinutes)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv128)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv64], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv64)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv32], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv32)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv28], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv28)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv24], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv24)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv20], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv20)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv16], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv16)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv14], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv14)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv12], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv12)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv10], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv10)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv8], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv8)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv7], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv7)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv6], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv6)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv5], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv5)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv4], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv4)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv3], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv3)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeatDiv2], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeatDiv2)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBeat], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBeat)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToBar], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToBar)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToMark], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToMark)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToRegionStart], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToRegionStart)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToRegionEnd], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToRegionEnd)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToRegionSync], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToRegionSync)));
	snap_type_selector.AddMenuElem (MenuElem ( snap_type_strings[(int)SnapToRegionBoundary], sigc::bind (sigc::mem_fun(*this, &Editor::snap_type_selection_done), (SnapType) SnapToRegionBoundary)));

	set_size_request_to_display_given_text (snap_type_selector, snap_type_strings, 34, 2);

}

void
Editor::setup_tooltips ()
{
	ARDOUR_UI::instance()->set_tip (smart_mode_button, _("Smart Mode (add Range functions to Object mode)"));
	ARDOUR_UI::instance()->set_tip (mouse_draw_button, _("Draw/Edit MIDI Notes"));
	ARDOUR_UI::instance()->set_tip (mouse_gain_button, _("Draw Region Gain"));
	ARDOUR_UI::instance()->set_tip (mouse_timefx_button, _("Stretch/Shrink Regions and MIDI Notes"));
	ARDOUR_UI::instance()->set_tip (mouse_audition_button, _("Listen to Specific Regions"));
	ARDOUR_UI::instance()->set_tip (internal_edit_button, _("Note Level Editing"));
	ARDOUR_UI::instance()->set_tip (*_group_tabs, _("Groups: click to (de)activate\nContext-click for other operations"));
	ARDOUR_UI::instance()->set_tip (visible_tracks_selector, _("Number of visible tracks"));
	ARDOUR_UI::instance()->set_tip (snap_type_selector, _("Snap/Grid Units"));
	ARDOUR_UI::instance()->set_tip (snap_mode_selector, _("Snap/Grid Mode"));
	ARDOUR_UI::instance()->set_tip (edit_point_selector, _("Edit point"));
	ARDOUR_UI::instance()->set_tip (edit_mode_selector, _("Edit Mode"));
	ARDOUR_UI::instance()->set_tip (nudge_clock, _("Nudge Clock\n(controls distance used to nudge regions and selections)"));
}

int
Editor::convert_drop_to_paths (
		vector<string>&                paths,
		const RefPtr<Gdk::DragContext>& /*context*/,
		gint                            /*x*/,
		gint                            /*y*/,
		const SelectionData&            data,
		guint                           /*info*/,
		guint                           /*time*/)
{
	if (_session == 0) {
		return -1;
	}

	vector<string> uris = data.get_uris();

	if (uris.empty()) {

		/* This is seriously fucked up. Nautilus doesn't say that its URI lists
		   are actually URI lists. So do it by hand.
		*/

		if (data.get_target() != "text/plain") {
			return -1;
		}

		/* Parse the "uri-list" format that Nautilus provides,
		   where each pathname is delimited by \r\n.

		   THERE MAY BE NO NULL TERMINATING CHAR!!!
		*/

		string txt = data.get_text();
		char* p;
		const char* q;

		p = (char *) malloc (txt.length() + 1);
		txt.copy (p, txt.length(), 0);
		p[txt.length()] = '\0';

		while (p)
		{
			if (*p != '#')
			{
				while (g_ascii_isspace (*p))
					p++;

				q = p;
				while (*q && (*q != '\n') && (*q != '\r')) {
					q++;
				}

				if (q > p)
				{
					q--;
					while (q > p && g_ascii_isspace (*q))
						q--;

					if (q > p)
					{
						uris.push_back (string (p, q - p + 1));
					}
				}
			}
			p = strchr (p, '\n');
			if (p)
				p++;
		}

		free ((void*)p);

		if (uris.empty()) {
			return -1;
		}
	}

	for (vector<string>::iterator i = uris.begin(); i != uris.end(); ++i) {
		if ((*i).substr (0,7) == "file://") {
			paths.push_back (Glib::filename_from_uri (*i));
		}
	}

	return 0;
}

void
Editor::new_tempo_section ()
{
}

void
Editor::map_transport_state ()
{
	ENSURE_GUI_THREAD (*this, &Editor::map_transport_state);

	if (_session && _session->transport_stopped()) {
		have_pending_keyboard_selection = false;
	}

	update_loop_range_view ();
}

/* UNDO/REDO */

void
Editor::begin_reversible_command (string name)
{
	if (_session) {
		_session->begin_reversible_command (name);
	}
}

void
Editor::begin_reversible_command (GQuark q)
{
	if (_session) {
		_session->begin_reversible_command (q);
	}
}

void
Editor::commit_reversible_command ()
{
	if (_session) {
		_session->commit_reversible_command ();
	}
}

void
Editor::history_changed ()
{
	string label;

	if (undo_action && _session) {
		if (_session->undo_depth() == 0) {
			label = S_("Command|Undo");
			undo_action->set_sensitive (false);
		} else {
			label = string_compose(S_("Command|Undo (%1)"), _session->next_undo());
			undo_action->set_sensitive (true);
		}
		undo_action->property_label() = label;
	}

	if (redo_action && _session) {
		if (_session->redo_depth() == 0) {
			label = _("Redo");
			redo_action->set_sensitive (false);
		} else {
			label = string_compose(_("Redo (%1)"), _session->next_redo());
			redo_action->set_sensitive (true);
		}
		redo_action->property_label() = label;
	}
}

void
Editor::duplicate_range (bool with_dialog)
{
	float times = 1.0f;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if ( selection->time.length() == 0 && rs.empty()) {
		return;
	}

    // disable Main menu
    ActionDisabler m; // HOT FIX. (REWORK IT)

	if (with_dialog) {

        WavesNumericEditDialog dialog("");
 
		switch (dialog.run ()) {
        case WavesDialog::RESPONSE_DEFAULT:
			break;
		default:
			return;
		}

		times = dialog.get_count ();
	}

	if ((current_mouse_mode() == Editing::MouseRange)) {
		if (selection->time.length()) {
			duplicate_selection (times);
		}
	} else if (get_smart_mode()) {
		if (selection->time.length()) {
			duplicate_selection (times);
		} else 
			duplicate_some_regions (rs, times);
	} else {
		duplicate_some_regions (rs, times);
	}
}

void
Editor::set_edit_mode (EditMode m)
{
	Config->set_edit_mode (m);
}

void
Editor::cycle_edit_mode ()
{
	switch (Config->get_edit_mode()) {
	case Slide:
		if (Profile->get_sae()) {
			Config->set_edit_mode (Lock);
		} else {
			Config->set_edit_mode (Ripple);
		}
		break;
	case Splice:
	case Ripple:
		Config->set_edit_mode (Lock);
		break;
	case Lock:
		Config->set_edit_mode (Slide);
		break;
	}
}

void
Editor::edit_mode_selection_done ( EditMode m )
{
	Config->set_edit_mode ( m );
}

void
Editor::snap_type_selection_done (SnapType snaptype)
{
	RefPtr<RadioAction> ract = snap_type_action (snaptype);
	if (ract) {
		ract->set_active ();
	}
}

void
Editor::snap_mode_selection_done (SnapMode mode)
{
	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract) {
		ract->set_active (true);
	}
}

void
Editor::cycle_edit_point (bool with_marker)
{
	if(Profile->get_mixbus())
		with_marker = false;

	switch (_edit_point) {
	case EditAtMouse:
		set_edit_point_preference (EditAtPlayhead);
		break;
	case EditAtPlayhead:
		if (with_marker) {
			set_edit_point_preference (EditAtSelectedMarker);
		} else {
			set_edit_point_preference (EditAtMouse);
		}
		break;
	case EditAtSelectedMarker:
		set_edit_point_preference (EditAtMouse);
		break;
	}
}

void
Editor::edit_point_selection_done (EditPoint ep)
{
	set_edit_point_preference ( ep );
}

void
Editor::build_zoom_focus_menu ()
{
	using namespace Menu_Helpers;

	zoom_focus_selector.AddMenuElem (MenuElem ( zoom_focus_strings[(int)ZoomFocusLeft], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusLeft)));
	zoom_focus_selector.AddMenuElem (MenuElem ( zoom_focus_strings[(int)ZoomFocusRight], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusRight)));
	zoom_focus_selector.AddMenuElem (MenuElem ( zoom_focus_strings[(int)ZoomFocusCenter], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusCenter)));
	zoom_focus_selector.AddMenuElem (MenuElem ( zoom_focus_strings[(int)ZoomFocusPlayhead], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusPlayhead)));
	zoom_focus_selector.AddMenuElem (MenuElem ( zoom_focus_strings[(int)ZoomFocusMouse], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusMouse)));
	zoom_focus_selector.AddMenuElem (MenuElem ( zoom_focus_strings[(int)ZoomFocusEdit], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusEdit)));

	set_size_request_to_display_given_text (zoom_focus_selector, longest (zoom_focus_strings), 30, 2);
}

void
Editor::zoom_focus_selection_done ( ZoomFocus f )
{
	RefPtr<RadioAction> ract = zoom_focus_action (f);
	if (ract) {
		ract->set_active ();
	}
}

void
Editor::build_track_count_menu ()
{
	using namespace Menu_Helpers;

	if (!Profile->get_mixbus()) {
		visible_tracks_selector.AddMenuElem (MenuElem (X_("1"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 1)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("2"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 2)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("3"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 3)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("4"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 4)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("8"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 8)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("12"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 12)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("16"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 16)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("20"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 20)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("24"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 24)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("32"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 32)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("64"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 64)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Selected"), sigc::mem_fun(*this, &Editor::fit_selected_tracks)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("All"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 0)));
	} else {
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 1 track"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 1)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 2 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 2)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 4 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 4)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 8 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 8)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 16 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 16)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 24 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 24)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 32 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 32)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 48 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 48)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit All tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 0)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit Selected tracks"), sigc::mem_fun(*this, &Editor::fit_selected_tracks)));

		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 10 ms"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 100 ms"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 100)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 1 sec"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 1 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 10 sec"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 1 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 10 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 1 hour"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 60 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 8 hours"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 8 * 60 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 24 hours"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 24 * 60 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to Session"), sigc::mem_fun(*this, &Editor::temporal_zoom_session)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to Range/Region Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_selection), false)));
	}
}

void
Editor::set_zoom_preset (int64_t ms)
{
	if ( ms <= 0 ) {
		temporal_zoom_session();
		return;
	}
	
	ARDOUR::framecnt_t const sample_rate = ARDOUR::AudioEngine::instance()->sample_rate();
	temporal_zoom( (sample_rate * ms / 1000) / _visible_canvas_width );
}

void
Editor::set_visible_track_count (int32_t n)
{
	_visible_track_count = n;

	/* if the canvas hasn't really been allocated any size yet, just
	   record the desired number of visible tracks and return. when canvas
	   allocation happens, we will get called again and then we can do the
	   real work.
	*/
	
	if (_visible_canvas_height <= 1) {
		return;
	}

	int h;
	string str;
	DisplaySuspender ds;
	
	if (_visible_track_count > 0) {
		h = trackviews_height() / _visible_track_count;
		std::ostringstream s;
		s << _visible_track_count;
		str = s.str();
	} else if (_visible_track_count == 0) {
		uint32_t n = 0;
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((*i)->marked_for_display()) {
				++n;
			}
		}
		h = trackviews_height() / n;
		str = _("All");
	} else {
		/* negative value means that the visible track count has 
		   been overridden by explicit track height changes.
		*/
		visible_tracks_selector.set_text (X_("*"));
		return;
	}

// NP: track's height is inited from .xml
//	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
//		(*i)->set_height (h);
//	}
	
	if (str != visible_tracks_selector.get_text()) {
		visible_tracks_selector.set_text (str);
	}
}

void
Editor::override_visible_track_count ()
{
	_visible_track_count = -1;
	visible_tracks_selector.set_text ( _("*") );
}

bool
Editor::edit_controls_button_press (GdkEventButton* ev)
{
	if ( (ev->button == 1) && (ev->type == GDK_2BUTTON_PRESS) ) {
        ARDOUR_UI::instance()->add_audio_track_instantly ();
        return true;
	}
    
	return false;
}


bool
Editor::edit_controls_button_release (GdkEventButton* ev)
{
	return false;
}

bool
Editor::mouse_select_button_release (GdkEventButton* ev)
{
	/* this handles just right-clicks */

	if (ev->button != 3) {
		return false;
	}

	return true;
}

void
Editor::set_zoom_focus (ZoomFocus f)
{
	string str = zoom_focus_strings[(int)f];

	if (str != zoom_focus_selector.get_text()) {
		zoom_focus_selector.set_text (str);
	}

	if (zoom_focus != f) {
		zoom_focus = f;
		instant_save ();
	}
}

void
Editor::cycle_zoom_focus ()
{
	switch (zoom_focus) {
	case ZoomFocusLeft:
		set_zoom_focus (ZoomFocusRight);
		break;
	case ZoomFocusRight:
		set_zoom_focus (ZoomFocusCenter);
		break;
	case ZoomFocusCenter:
		set_zoom_focus (ZoomFocusPlayhead);
		break;
	case ZoomFocusPlayhead:
		set_zoom_focus (ZoomFocusMouse);
		break;
	case ZoomFocusMouse:
		set_zoom_focus (ZoomFocusEdit);
		break;
	case ZoomFocusEdit:
		set_zoom_focus (ZoomFocusLeft);
		break;
	}
}

void
Editor::ensure_float (Window& win)
{
	win.set_transient_for (*this);
}

void
Editor::pane_allocation_handler (Allocation &alloc, Paned* which)
{
	/* recover or initialize pane positions. do this here rather than earlier because
	   we don't want the positions to change the child allocations, which they seem to do.
	 */

	int pos;
	XMLProperty* prop;
	char buf[32];
	XMLNode* node = ARDOUR_UI::instance()->editor_settings();

	enum Pane {
		Horizontal = 0x1,
		Vertical = 0x2
	};

	static Pane done;

	XMLNode* geometry = find_named_node (*node, "geometry");

	if (which == static_cast<Paned*> (&edit_pane)) {

		if (done & Horizontal) {
			return;
		}

		if (geometry && (prop = geometry->property ("notebook-shrunk"))) {
			_notebook_shrunk = string_is_affirmative (prop->value ());
		}

		if (!geometry || (prop = geometry->property ("edit-horizontal-pane-pos")) == 0) {
			/* initial allocation is 90% to canvas, 10% to notebook */
			pos = (int) floor (alloc.get_width() * 0.90f);
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if (GTK_WIDGET(edit_pane.gobj())->allocation.width > pos) {
			edit_pane.set_position (pos);
		}

		done = (Pane) (done | Horizontal);

	} else if (which == static_cast<Paned*> (&editor_summary_pane)) {

		if (done & Vertical) {
			return;
		}

		if (!geometry || (prop = geometry->property ("edit-vertical-pane-pos")) == 0) {
			/* initial allocation is 90% to canvas, 10% to summary */
			pos = (int) floor (alloc.get_height() * 0.90f);
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {

			pos = atoi (prop->value());
		}

		if (GTK_WIDGET(editor_summary_pane.gobj())->allocation.height > pos) {
			editor_summary_pane.set_position (pos);
		}

		done = (Pane) (done | Vertical);
	}
}

void
Editor::set_show_measures (bool yn)
{
	if (_show_measures != yn) {
		hide_measures ();

		if ((_show_measures = yn) == true) {
			if (tempo_lines) {
				tempo_lines->show();
			}

			ARDOUR::TempoMap::BBTPointList::const_iterator begin;
			ARDOUR::TempoMap::BBTPointList::const_iterator end;
			
			compute_current_bbt_points (leftmost_frame, leftmost_frame + current_page_samples(), begin, end);
			draw_measures (begin, end);
		} 

		instant_save ();
	}
}

void
Editor::update_playhead_modes ()
{
    AutoReturnTarget art = Config->get_auto_return_target_list ();
    
    {   // RangeSelectionStart
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("play-from-selection"));
        
        if (act) {
            Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
            
            if (tact ) {
                tact->set_active(art & RangeSelectionStart);
            }
        }
    }
    
    {   // LastLocate
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("play-from-last-position"));
        
        if (act) {
            Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
            
            if (tact) {
                tact->set_active(art & LastLocate);
            }
        }
    }
    
    {   // Loop
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("play-loop"));
        
        if (act) {
            Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
            
            if (tact) {
                tact->set_active(art & Loop);
            }
        }
    }
    
    {   // RegionSelectionStart
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("play-from-selected-region"));
        
        if (act) {
            Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
            
            if (tact) {
                tact->set_active(art & RegionSelectionStart);
            }
        }
    }
}

void
Editor::toggle_playhead_mode (AutoReturnTarget t)
{
    AutoReturnTarget art = Config->get_auto_return_target_list ();
    
    Glib::RefPtr<Action> act;
    switch (t) {
        case RangeSelectionStart:
            act = ActionManager::get_action (X_("Editor"), X_("play-from-selection"));
            
            if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                
                if (tact ) {
                    if (tact->get_active() ) {
                        Config->set_auto_return_target_list (AutoReturnTarget (art | t));
                    } else {
                        Config->set_auto_return_target_list (AutoReturnTarget (art & ~t));
                    }
                }
            }
            break;
        case LastLocate:
            act = ActionManager::get_action (X_("Editor"), X_("play-from-last-position"));
            
            if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                
                if (tact) {
                    if (tact->get_active() ) {
                        Config->set_auto_return_target_list (AutoReturnTarget (art | t));
                    } else {
                        Config->set_auto_return_target_list (AutoReturnTarget (art & ~t));
                    }
                }
            }
            break;
        case Loop:
            act = ActionManager::get_action (X_("Editor"), X_("play-loop"));
            
            if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                
                if (tact) {
                    if (tact->get_active() ) {
                        Config->set_auto_return_target_list (AutoReturnTarget (art | t));
                    } else {
                        Config->set_auto_return_target_list (AutoReturnTarget (art & ~t));
                    }
                }
            }
            break;
        case RegionSelectionStart:
            act = ActionManager::get_action (X_("Editor"), X_("play-from-selected-region"));
            
            if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                
                if (tact) {
                    if (tact->get_active() ) {
                        Config->set_auto_return_target_list (AutoReturnTarget (art | t));
                    } else {
                        Config->set_auto_return_target_list (AutoReturnTarget (art & ~t));
                    }
                }
            }
            break;
    }
}

void
Editor::toggle_follow_playhead ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-follow-playhead"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
		set_follow_playhead (tact->get_active());
	}
}

/** @param yn true to follow playhead, otherwise false.
 *  @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
 */
void
Editor::set_follow_playhead (bool yn, bool catch_up)
{
	if (_follow_playhead != yn) {
		if ((_follow_playhead = yn) == true && catch_up) {
			/* catch up */
			reset_x_origin_to_follow_playhead ();
		}
		instant_save ();
	}
}

void
Editor::toggle_stationary_playhead ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-stationary-playhead"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
		set_stationary_playhead (tact->get_active());
	}
}

void
Editor::set_stationary_playhead (bool yn)
{
	if (_stationary_playhead != yn) {
		if ((_stationary_playhead = yn) == true) {
			/* catch up */
			// FIXME need a 3.0 equivalent of this 2.X call
			// update_current_screen ();
		}
		instant_save ();
	}
}

void
Editor::toggle_enable_group_edit ()
{
    RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-enable-group-edit"));
    if (act) {
        RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
        set_enable_group_edit (tact->get_active());
    }
}

void Editor::set_enable_group_edit (bool yn)
{
    if (_enable_group_edit != yn) {
        _enable_group_edit = yn;
        
        if (_session) {
            _session->config.set_enable_group_edit(yn);
        }
        
        instant_save ();
    }
}


PlaylistSelector&
Editor::playlist_selector () const
{
	return *_playlist_selector;
}

unsigned
Editor::get_grid_beat_divisions(framepos_t position)
{
	switch (_snap_type) {
	case SnapToBeatDiv128: return 128;
	case SnapToBeatDiv64:  return 64;
	case SnapToBeatDiv32:  return 32;
	case SnapToBeatDiv28:  return 28;
	case SnapToBeatDiv24:  return 24;
	case SnapToBeatDiv20:  return 20;
	case SnapToBeatDiv16:  return 16;
	case SnapToBeatDiv14:  return 14;
	case SnapToBeatDiv12:  return 12;
	case SnapToBeatDiv10:  return 10;
	case SnapToBeatDiv8:   return 8;
	case SnapToBeatDiv7:   return 7;
	case SnapToBeatDiv6:   return 6;
	case SnapToBeatDiv5:   return 5;
	case SnapToBeatDiv4:   return 4;
	case SnapToBeatDiv3:   return 3;
	case SnapToBeatDiv2:   return 2;
	default:               return 0;
	}
	return 0;
}

Evoral::Beats
Editor::get_grid_type_as_beats (bool& success, framepos_t position)
{
	success = true;

	const unsigned divisions = get_grid_beat_divisions(position);
	if (divisions) {
		return Evoral::Beats(1.0 / (double)get_grid_beat_divisions(position));
	}

	switch (_snap_type) {
	case SnapToBeat:
		return Evoral::Beats(1.0);
	case SnapToBar:
		if (_session) {
			return Evoral::Beats(_session->tempo_map().meter_at (position).divisions_per_bar());
		}
		break;
	default:
		success = false;
		break;
	}

	return Evoral::Beats();
}

framecnt_t
Editor::get_nudge_distance (framepos_t pos, framecnt_t& next)
{
	framecnt_t ret;

	ret = nudge_clock->current_duration (pos);
	next = ret + 1; /* XXXX fix me */

	return ret;
}

int
Editor::playlist_deletion_dialog (boost::shared_ptr<Playlist> pl)
{
    /* In Tracks Live playlists doesn't exist. 
     So drop references to all playlists in the function SessionPlaylists::maybe_delete_unused. */
    return 0;
    
	ArdourDialog dialog (_("Playlist Deletion"));
	Label  label (string_compose (_("Playlist %1 is currently unused.\n"
					"If it is kept, its audio files will not be cleaned.\n"
					"If it is deleted, audio files used by it alone will be cleaned."),
				      pl->name()));

	dialog.set_position (WIN_POS_CENTER);
	dialog.get_vbox()->pack_start (label);

	label.show ();

	dialog.add_button (_("Delete Playlist"), RESPONSE_ACCEPT);
	dialog.add_button (_("Keep Playlist"), RESPONSE_REJECT);
	dialog.add_button (_("Cancel"), RESPONSE_CANCEL);

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		/* delete the playlist */
		return 0;
		break;

	case RESPONSE_REJECT:
		/* keep the playlist */
		return 1;
		break;

	default:
		break;
	}

	return -1;
}

bool
Editor::audio_region_selection_covers (framepos_t where)
{
	for (RegionSelection::iterator a = selection->regions.begin(); a != selection->regions.end(); ++a) {
		if ((*a)->region()->covers (where)) {
			return true;
		}
	}

	return false;
}

void
Editor::prepare_for_cleanup ()
{
	cut_buffer->clear_regions ();
	cut_buffer->clear_playlists ();

	selection->clear_regions ();
	selection->clear_playlists ();

	_regions->suspend_redisplay ();
}

void
Editor::finish_cleanup ()
{
	_regions->resume_redisplay ();
}

Location*
Editor::transport_loop_location()
{
	if (_session) {
		return _session->locations()->auto_loop_location();
	} else {
		return 0;
	}
}

Location*
Editor::transport_punch_location()
{
	if (_session) {
		return _session->locations()->auto_punch_location();
	} else {
		return 0;
	}
}

bool
Editor::control_layout_scroll (GdkEventScroll* ev)
{
	/* Just forward to the normal canvas scroll method. The coordinate
	   systems are different but since the canvas is always larger than the
	   track headers, and aligned with the trackview area, this will work.

	   In the not too distant future this layout is going away anyway and
	   headers will be on the canvas.
	*/
	return canvas_scroll_event (ev, false);
}

void
Editor::session_state_saved (string)
{
	update_title ();
	_snapshots->redisplay ();
}

void
Editor::set_session_dirty ()
{
    if (_session) {
        _session->set_dirty ();
    }
}

void
Editor::update_tearoff_visibility()
{
	bool visible = ARDOUR_UI::config()->get_keep_tearoffs();
	_mouse_mode_tearoff->set_visible (visible);
	_tools_tearoff->set_visible (visible);
	if (_zoom_tearoff) {
		_zoom_tearoff->set_visible (visible);
	}
}

void
Editor::maximise_editing_space ()
{
	if (_maximised) {
		return;
	}

	fullscreen ();

	_maximised = true;
}

void
Editor::restore_editing_space ()
{
	if (!_maximised) {
		return;
	}

	unfullscreen();

	_maximised = false;
}

/**
 *  Make new playlists for a given track and also any others that belong
 *  to the same active route group with the `select' property.
 *  @param v Track.
 */

void
Editor::new_playlists (TimeAxisView* v)
{
	begin_reversible_command (_("new playlists"));
	vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
	_session->playlists->get (playlists);
	mapover_tracks (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_use_new_playlist), playlists), v, ARDOUR::Properties::select.property_id);
	commit_reversible_command ();
}

/**
 *  Use a copy of the current playlist for a given track and also any others that belong
 *  to the same active route group with the `select' property.
 *  @param v Track.
 */

void
Editor::copy_playlists (TimeAxisView* v)
{
	begin_reversible_command (_("copy playlists"));
	vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
	_session->playlists->get (playlists);
	mapover_tracks (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_use_copy_playlist), playlists), v, ARDOUR::Properties::select.property_id);
	commit_reversible_command ();
}

/** Clear the current playlist for a given track and also any others that belong
 *  to the same active route group with the `select' property.
 *  @param v Track.
 */

void
Editor::clear_playlists (TimeAxisView* v)
{
	begin_reversible_command (_("clear playlists"));
	vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
	_session->playlists->get (playlists);
	mapover_tracks (sigc::mem_fun (*this, &Editor::mapped_clear_playlist), v, ARDOUR::Properties::select.property_id);
	commit_reversible_command ();
}

void
Editor::mapped_use_new_playlist (RouteTimeAxisView& atv, uint32_t sz, vector<boost::shared_ptr<ARDOUR::Playlist> > const & playlists)
{
	atv.use_new_playlist (sz > 1 ? false : true, playlists);
}

void
Editor::mapped_use_copy_playlist (RouteTimeAxisView& atv, uint32_t sz, vector<boost::shared_ptr<ARDOUR::Playlist> > const & playlists)
{
	atv.use_copy_playlist (sz > 1 ? false : true, playlists);
}

void
Editor::mapped_clear_playlist (RouteTimeAxisView& atv, uint32_t /*sz*/)
{
	atv.clear_playlist ();
}

bool
Editor::on_key_press_event (GdkEventKey* ev)
{
	return key_press_focus_accelerator_handler (*this, ev);
}

bool
Editor::on_key_release_event (GdkEventKey* ev)
{
	return Gtk::Window::on_key_release_event (ev);
	// return key_press_focus_accelerator_handler (*this, ev);
}

/** Queue up a change to the viewport x origin.
 *  @param frame New x origin.
 */
void
Editor::reset_x_origin (framepos_t frame)
{
	pending_visual_change.add (VisualChange::TimeOrigin);
	pending_visual_change.time_origin = frame;
	ensure_visual_change_idle_handler ();
}

void
Editor::reset_y_origin (double y)
{
	pending_visual_change.add (VisualChange::YOrigin);
	pending_visual_change.y_origin = y;
	ensure_visual_change_idle_handler ();
}

void
Editor::reset_zoom (framecnt_t spp)
{
	if (spp == samples_per_pixel) {
		return;
	}

	pending_visual_change.add (VisualChange::ZoomLevel);
	pending_visual_change.samples_per_pixel = spp;
	ensure_visual_change_idle_handler ();
}

void
Editor::reposition_and_zoom (framepos_t frame, double fpu)
{
	reset_x_origin (frame);
	reset_zoom (fpu);

	if (!no_save_visual) {
		undo_visual_stack.push_back (current_visual_state(false));
	}
}

Editor::VisualState::VisualState (bool with_tracks)
	: gui_state (with_tracks ? new GUIObjectState : 0)
{
}

Editor::VisualState::~VisualState ()
{
	delete gui_state;
}

Editor::VisualState*
Editor::current_visual_state (bool with_tracks)
{
	VisualState* vs = new VisualState (with_tracks);
	vs->y_position = vertical_adjustment.get_value();
	vs->samples_per_pixel = samples_per_pixel;
	vs->leftmost_frame = leftmost_frame;
	vs->zoom_focus = zoom_focus;

	if (with_tracks) {	
		*vs->gui_state = *ARDOUR_UI::instance()->gui_object_state;
	}

	return vs;
}

void
Editor::undo_visual_state ()
{
	if (undo_visual_stack.empty()) {
		return;
	}

	VisualState* vs = undo_visual_stack.back();
	undo_visual_stack.pop_back();


	redo_visual_stack.push_back (current_visual_state (vs ? vs->gui_state != 0 : false));

	use_visual_state (*vs);
}

void
Editor::redo_visual_state ()
{
	if (redo_visual_stack.empty()) {
		return;
	}

	VisualState* vs = redo_visual_stack.back();
	redo_visual_stack.pop_back();

	undo_visual_stack.push_back (current_visual_state (vs ? vs->gui_state != 0 : false));

	use_visual_state (*vs);
}

void
Editor::swap_visual_state ()
{
	if (undo_visual_stack.empty()) {
		redo_visual_state ();
	} else {
		undo_visual_state ();
	}
}

void
Editor::use_visual_state (VisualState& vs)
{
	PBD::Unwinder<bool> nsv (no_save_visual, true);
	DisplaySuspender ds;

	vertical_adjustment.set_value (vs.y_position);

	set_zoom_focus (vs.zoom_focus);
	reposition_and_zoom (vs.leftmost_frame, vs.samples_per_pixel);
	
	if (vs.gui_state) {
		*ARDOUR_UI::instance()->gui_object_state = *vs.gui_state;
		
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {	
			(*i)->reset_visual_state ();
		}
	}

	_routes->update_visibility ();
}

/** This is the core function that controls the zoom level of the canvas. It is called
 *  whenever one or more calls are made to reset_zoom().  It executes in an idle handler.
 *  @param spp new number of samples per pixel
 */
void
Editor::set_samples_per_pixel (framecnt_t spp)
{
	if (spp < 1) {
		return;
	}

#if !UNLIMITED_TEMPORAL_ZOOM // TRACKS needs no limitations
	const framecnt_t three_days = 3 * 24 * 60 * 60 * (_session ? _session->frame_rate() : 48000);
	const framecnt_t lots_of_pixels = 4000;

	/* if the zoom level is greater than what you'd get trying to display 3
	 * days of audio on a really big screen, then it's too big.
	 */
	if (spp * lots_of_pixels > three_days) {
		return;
	}
#endif

	samples_per_pixel = spp;
    update_horizontal_adjustment_limits();
    
	if (tempo_lines) {
		tempo_lines->tempo_map_changed();
	}

	bool const showing_time_selection = selection->time.length() > 0;

	if (showing_time_selection && selection->time.start () != selection->time.end_frame ()) {
		for (TrackViewList::iterator i = selection->time.tracks_in_range.begin(); i != selection->time.tracks_in_range.end(); ++i) {
			(*i)->show_selection (selection->time);
		}
	}

	ZoomChanged (); /* EMIT_SIGNAL */

	ArdourCanvas::GtkCanvasViewport* c;

	c = get_track_canvas();
	if (c) {
		c->canvas()->zoomed ();
	}

	if (playhead_cursor) {
		playhead_cursor->set_position (playhead_cursor->current_frame ());
	}

	refresh_location_display();
	_summary->set_overlays_dirty ();

	set_session_dirty ();
}

void
Editor::queue_visual_videotimeline_update ()
{
	/* TODO:
	 * pending_visual_change.add (VisualChange::VideoTimeline);
	 * or maybe even more specific: which videotimeline-image
	 * currently it calls update_video_timeline() to update
	 * _all outdated_ images on the video-timeline.
	 * see 'exposeimg()' in video_image_frame.cc
	 */
	ensure_visual_change_idle_handler ();
}

void
Editor::ensure_visual_change_idle_handler ()
{
	if (pending_visual_change.idle_handler_id < 0) {
		// see comment in add_to_idle_resize above.
		pending_visual_change.idle_handler_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_visual_changer, this, NULL);
		pending_visual_change.being_handled = false;
	}
}

int
Editor::_idle_visual_changer (void* arg)
{
	return static_cast<Editor*>(arg)->idle_visual_changer ();
}

int
Editor::idle_visual_changer ()
{
	/* set_horizontal_position() below (and maybe other calls) call
	   gtk_main_iteration(), so it's possible that a signal will be handled
	   half-way through this method.  If this signal wants an
	   idle_visual_changer we must schedule another one after this one, so
	   mark the idle_handler_id as -1 here to allow that.  Also make a note
	   that we are doing the visual change, so that changes in response to
	   super-rapid-screen-update can be dropped if we are still processing
	   the last one.
	*/

	pending_visual_change.idle_handler_id = -1;
	pending_visual_change.being_handled = true;
	
	VisualChange vc = pending_visual_change;

	pending_visual_change.pending = (VisualChange::Type) 0;

	visual_changer (vc);

	pending_visual_change.being_handled = false;

	return 0; /* this is always a one-shot call */
}

void
Editor::visual_changer (const VisualChange& vc)
{
	double const last_time_origin = horizontal_position ();

	if (vc.pending & VisualChange::ZoomLevel) {
		if (vc.pending & VisualChange::TimeOrigin) {
			leftmost_frame = vc.time_origin;
		}
		set_samples_per_pixel (vc.samples_per_pixel);

		compute_fixed_ruler_scale ();

		ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_begin;
		ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_end;
		
		compute_current_bbt_points (vc.time_origin, pending_visual_change.time_origin + current_page_samples(),
					    current_bbt_points_begin, current_bbt_points_end);
		compute_bbt_ruler_scale (vc.time_origin, pending_visual_change.time_origin + current_page_samples(),
					 current_bbt_points_begin, current_bbt_points_end);
		update_tempo_based_rulers (current_bbt_points_begin, current_bbt_points_end);

		update_video_timeline();
	}

	if (vc.pending & VisualChange::TimeOrigin) {
		horizontal_adjustment.set_value (vc.time_origin / samples_per_pixel);
	}

	if (vc.pending & VisualChange::YOrigin) {
		vertical_adjustment.set_value (vc.y_origin);
	}

	if (last_time_origin == horizontal_position ()) {
		/* changed signal not emitted */
		update_fixed_rulers ();
		redisplay_tempo (true);
	}

	if (!(vc.pending & VisualChange::ZoomLevel)) {
		update_video_timeline();
	}
    
	_summary->set_overlays_dirty ();
}

struct EditorOrderTimeAxisSorter {
    bool operator() (const TimeAxisView* a, const TimeAxisView* b) const {
	    return a->order () < b->order ();
    }
};

void
Editor::sort_track_selection (TrackViewList& sel)
{
	EditorOrderTimeAxisSorter cmp;
	sel.sort (cmp);
}

framepos_t
Editor::get_preferred_edit_position (bool ignore_playhead, bool from_context_menu)
{
	bool ignored;
	framepos_t where = 0;
	EditPoint ep = _edit_point;

	if(Profile->get_mixbus())
		if (ep == EditAtSelectedMarker)
			ep=EditAtPlayhead;
		
	if (from_context_menu && (ep == EditAtMouse)) {
		return  canvas_event_sample (&context_click_event, 0, 0);
	}

	if (entered_marker) {
                DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use entered marker @ %1\n", entered_marker->position()));
		return entered_marker->position();
	}

	if (ignore_playhead && ep == EditAtPlayhead) {
		ep = EditAtSelectedMarker;
	}

	switch (ep) {
	case EditAtPlayhead:
		where = _session->audible_frame();
                DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use playhead @ %1\n", where));
		break;

	case EditAtSelectedMarker:
		if (!selection->markers.empty()) {
			bool is_start;
			Location* loc = find_location_from_marker (selection->markers.front(), is_start);
			if (loc) {
				if (is_start) {
					where =  loc->start();
				} else {
					where = loc->end();
				}
                                DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use selected marker @ %1\n", where));
				break;
			}
		}
		/* fallthru */

	default:
	case EditAtMouse:
		if (!mouse_frame (where, ignored)) {
			/* XXX not right but what can we do ? */
			return 0;
		}
		snap_to (where);
                DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use mouse @ %1\n", where));
		break;
	}

	return where;
}

void
Editor::set_loop_range (framepos_t start, framepos_t end, string cmd)
{
	if (!_session) return;

	begin_reversible_command (cmd);

	Location* tll;

	if ((tll = transport_loop_location()) == 0) {
		Location* loc = new Location (*_session, start, end, _("Loop"),  Location::IsAutoLoop);
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (loc, true);
		_session->set_auto_loop_location (loc);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	} else {
		XMLNode &before = tll->get_state();
		tll->set_hidden (false, this);
		tll->set (start, end);
		XMLNode &after = tll->get_state();
		_session->add_command (new MementoCommand<Location>(*tll, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::set_punch_range (framepos_t start, framepos_t end, string cmd)
{
	if (!_session) return;

	begin_reversible_command (cmd);

	Location* tpl;

	if ((tpl = transport_punch_location()) == 0) {
		Location* loc = new Location (*_session, start, end, _("Punch"),  Location::IsAutoPunch);
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (loc, true);
		_session->set_auto_punch_location (loc);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	}
	else {
		XMLNode &before = tpl->get_state();
		tpl->set_hidden (false, this);
		tpl->set (start, end);
		XMLNode &after = tpl->get_state();
		_session->add_command (new MementoCommand<Location>(*tpl, &before, &after));
	}

	commit_reversible_command ();
}

/** Find regions which exist at a given time, and optionally on a given list of tracks.
 *  @param rs List to which found regions are added.
 *  @param where Time to look at.
 *  @param ts Tracks to look on; if this is empty, all tracks are examined.
 */
void
Editor::get_regions_at (RegionSelection& rs, framepos_t where, const TrackViewList& ts) const
{
	const TrackViewList* tracks;

	if (ts.empty()) {
		tracks = &track_views;
	} else {
		tracks = &ts;
	}

	for (TrackViewList::const_iterator t = tracks->begin(); t != tracks->end(); ++t) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*t);

		if (rtv) {
			boost::shared_ptr<Track> tr;
			boost::shared_ptr<Playlist> pl;

			if ((tr = rtv->track()) && ((pl = tr->playlist()))) {

				boost::shared_ptr<RegionList> regions = pl->regions_at (
						(framepos_t) floor ( (double) where * tr->speed()));

				for (RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {
					RegionView* rv = rtv->view()->find_view (*i);
					if (rv) {
						rs.add (rv);
					}
				}
			}
		}
	}
}

void
Editor::get_regions_after (RegionSelection& rs, framepos_t where, const TrackViewList& ts) const
{
	const TrackViewList* tracks;

	if (ts.empty()) {
		tracks = &track_views;
	} else {
		tracks = &ts;
	}

	for (TrackViewList::const_iterator t = tracks->begin(); t != tracks->end(); ++t) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*t);
		if (rtv) {
			boost::shared_ptr<Track> tr;
			boost::shared_ptr<Playlist> pl;

			if ((tr = rtv->track()) && ((pl = tr->playlist()))) {

				boost::shared_ptr<RegionList> regions = pl->regions_touched (
					(framepos_t) floor ( (double)where * tr->speed()), max_framepos);

				for (RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {

					RegionView* rv = rtv->view()->find_view (*i);

					if (rv) {
						rs.add (rv);
					}
				}
			}
		}
	}
}

/** Get regions using the following method:
 *
 *  Make a region list using:
 *   (a) any selected regions
 *   (b) the intersection of any selected tracks and the edit point(*)
 *   (c) if neither exists, and edit_point == mouse, then whatever region is under the mouse
 *
 *  (*) NOTE: in this case, if 'No Selection = All Tracks' is active, search all tracks
 *
 *  Note that we have forced the rule that selected regions and selected tracks are mutually exclusive
 */

RegionSelection
Editor::get_regions_from_selection_and_edit_point ()
{
	RegionSelection regions;

	if (_edit_point == EditAtMouse && entered_regionview && selection->regions.empty() ) {
		regions.add (entered_regionview);
	} else {
		regions = selection->regions;
	}

	return regions;
}

RegionSelection
Editor::get_regions_from_selection_and_playhead ()
{
    RegionSelection regions;
    TrackViewList involved_tracks;
    
    RegionSelection& selected_regions = selection->regions;
    
    if (!selected_regions.empty() ) {
        
        RegionSelection::iterator iter = selected_regions.begin ();
        for (; iter != selected_regions.end (); ++iter) {
            involved_tracks.push_back ( &(*iter)->get_time_axis_view() );
        }
    }
    
    framepos_t where = get_playhead_position ();
    get_regions_at(regions, where, involved_tracks /*if empty apply to all*/);
    
    return regions;
}

/** Get regions using the following method:
 *
 *  Make a region list using:
 *   (a) any selected regions
 *   (b) the intersection of any selected tracks and the edit point(*)
 *   (c) if neither exists, then whatever region is under the mouse
 *
 *  (*) NOTE: in this case, if 'No Selection = All Tracks' is active, search all tracks
 *
 *  Note that we have forced the rule that selected regions and selected tracks are mutually exclusive
 */
RegionSelection
Editor::get_regions_from_selection_and_mouse (framepos_t pos)
{
	RegionSelection regions;

    if (entered_regionview) {
        if (selection->selected (entered_regionview) ) {
            regions = selection->regions;
        }
        
        regions.add (entered_regionview);
    }
    
    // Greg Zharun: Waves Tracks PRD does not say this.
    // So let's skip this.
	//if ( regions.empty() ) {
	//	TrackViewList tracks = selection->tracks;
    //
	//	if (!tracks.empty()) {
	//		/* no region selected or entered, but some selected tracks:
	//		 * act on all regions on the selected tracks at the edit point
	//		 */
	//		get_regions_at(regions, pos, tracks);
	//	}
	//}

	return regions;
}

/** Start with regions that are selected, or the entered regionview if none are selected.
 *  Then add equivalent regions on tracks in the same active edit-enabled route group as any
 *  of the regions that we started with.
 */

RegionSelection
Editor::get_regions_from_selection_and_entered ()
{
	RegionSelection regions = selection->regions;

	if (regions.empty() && entered_regionview) {
		regions.add (entered_regionview);
	}

	return regions;
}

void
Editor::get_regions_corresponding_to (boost::shared_ptr<Region> region, vector<RegionView*>& regions, bool src_comparison)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

		RouteTimeAxisView* tatv;

		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {

			boost::shared_ptr<Playlist> pl;
			vector<boost::shared_ptr<Region> > results;
			RegionView* marv;
			boost::shared_ptr<Track> tr;

			if ((tr = tatv->track()) == 0) {
				/* bus */
				continue;
			}

			if ((pl = (tr->playlist())) != 0) {
				if (src_comparison) {
					pl->get_source_equivalent_regions (region, results);
				} else {
					pl->get_region_list_equivalent_regions (region, results);
				}
			}

			for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
				if ((marv = tatv->view()->find_view (*ir)) != 0) {
					regions.push_back (marv);
				}
			}

		}
	}
}

void
Editor::show_rhythm_ferret ()
{
	if (rhythm_ferret == 0) {
		rhythm_ferret = new RhythmFerret(*this);
	}

	rhythm_ferret->set_session (_session);
	rhythm_ferret->show ();
	rhythm_ferret->present ();
}

void
Editor::first_idle ()
{
    WavesMessageDialog* dialog = 0;
	
	if (track_views.size() > 1) {
		dialog = new WavesMessageDialog ("", string_compose (_("Please wait while %1 loads visual data."), PROGRAM_NAME) );
		dialog->present ();
		ARDOUR_UI::instance()->flush_pending ();
	}

	for (TrackViewList::iterator t = track_views.begin(); t != track_views.end(); ++t) {
		(*t)->first_idle();
	}

	// first idle adds route children (automation tracks), so we need to redisplay here
	_routes->redisplay ();

	delete dialog;
	_have_idled = true;
}

gboolean
Editor::_idle_resize (gpointer arg)
{
	return ((Editor*)arg)->idle_resize ();
}

void
Editor::add_to_idle_resize (TimeAxisView* view, int32_t h)
{
	if (resize_idle_id < 0) {
		/* https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#G-PRIORITY-HIGH-IDLE:CAPS
		 * GTK+ uses G_PRIORITY_HIGH_IDLE + 10 for resizing operations, and G_PRIORITY_HIGH_IDLE + 20 for redrawing operations.
		 * (This is done to ensure that any pending resizes are processed before any pending redraws, so that widgets are not redrawn twice unnecessarily.)
		 */
		resize_idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_resize, this, NULL);
		_pending_resize_amount = 0;
	}

	/* make a note of the smallest resulting height, so that we can clamp the
	   lower limit at TimeAxisView::hSmall */

	int32_t min_resulting = INT32_MAX;

	_pending_resize_amount += h;
	_pending_resize_view = view;

	min_resulting = min (min_resulting, int32_t (_pending_resize_view->current_height()) + _pending_resize_amount);

	if (selection->tracks.contains (_pending_resize_view)) {
		for (TrackViewList::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			min_resulting = min (min_resulting, int32_t ((*i)->current_height()) + _pending_resize_amount);
		}
	}

	if (min_resulting < 0) {
		min_resulting = 0;
	}

	/* clamp */
	if (uint32_t (min_resulting) < TimeAxisView::preset_height (HeightSmall)) {
		_pending_resize_amount += TimeAxisView::preset_height (HeightSmall) - min_resulting;
	}
}

/** Handle pending resizing of tracks */
bool
Editor::idle_resize ()
{
    int32_t minimal_height = _pending_resize_view->preset_height (HeightSmall);
    
    // could be negative at first
    int32_t new_height = _pending_resize_view->current_height() + _pending_resize_amount;
    
    // if the result height would be less then minimal - set track header height to minimal
    if ( new_height < minimal_height ) {
        new_height = minimal_height;
    }
    
	_pending_resize_view->idle_resize (new_height);

	if (dynamic_cast<AutomationTimeAxisView*> (_pending_resize_view) == 0 &&
	    selection->tracks.contains (_pending_resize_view)) {

		for (TrackViewList::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			if (*i != _pending_resize_view) {
                
                minimal_height = (*i)->preset_height (HeightSmall);
                
                new_height = (*i)->current_height() + _pending_resize_amount;
                // if the result height would be less then minimal - set track header height to minimal
                if ( new_height < minimal_height ) {
                    new_height = minimal_height;
                }
                
                (*i)->idle_resize (new_height);
			}
		}
	}

	_pending_resize_amount = 0;
	_group_tabs->set_dirty ();
	resize_idle_id = -1;

	return false;
}

void
Editor::located ()
{
	ENSURE_GUI_THREAD (*this, &Editor::located);

	if (_session) {
		playhead_cursor->set_position (_session->audible_frame ());
		if (_follow_playhead && !_pending_initial_locate) {
			reset_x_origin_to_follow_playhead ();
		}
	}

	_pending_locate_request = false;
	_pending_initial_locate = false;
}

void
Editor::region_view_added (RegionView *)
{
	_summary->set_background_dirty ();
    update_edit_selection_menu ();
}

void
Editor::region_view_removed ()
{
	_summary->set_background_dirty ();
    update_edit_selection_menu ();
}

RouteTimeAxisView*
Editor::axis_view_from_route (boost::shared_ptr<Route> r) const
{
	TrackViewList::const_iterator j = track_views.begin ();
	while (j != track_views.end()) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*j);
		if (rtv && rtv->route() == r) {
			return rtv;
		}
		++j;
	}

	return 0;
}


TrackViewList
Editor::axis_views_from_routes (boost::shared_ptr<RouteList> r) const
{
	TrackViewList t;

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		TimeAxisView* tv = axis_view_from_route (*i);
		if (tv) {
			t.push_back (tv);
		}
	}

	return t;
}

void
Editor::suspend_route_redisplay ()
{
	if (_routes) {
		_routes->suspend_redisplay();
	}
}

void
Editor::resume_route_redisplay ()
{
	if (_routes) {
		_routes->resume_redisplay();
	}
}

void
Editor::add_routes (RouteList& routes)
{
	ENSURE_GUI_THREAD (*this, &Editor::handle_new_route, routes)

	RouteTimeAxisView *rtv;
	list<RouteTimeAxisView*> new_views;

	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);

        bool is_master_visible = (Config->get_output_auto_connect() & AutoConnectMaster);
        
		if (route->is_auditioner() || route->is_monitor() ||
			(!boost::dynamic_pointer_cast<Track> (route) &&
             (route->is_master() && !is_master_visible ) ) ) {
			continue;
		}

		DataType dt = route->input()->default_type();

		if (dt == ARDOUR::DataType::AUDIO) {
			rtv = new AudioTimeAxisView (*this, _session, *_track_canvas);
			rtv->set_route (route);
		} else if (dt == ARDOUR::DataType::MIDI) {
			rtv = new MidiTimeAxisView (*this, _session, *_track_canvas);
			rtv->set_route (route);
		} else {
			throw unknown_type();
		}

        
        new_views.push_back (rtv);
        track_views.push_back (rtv);

		rtv->effective_gain_display ();

                if (internal_editing()) {
                        rtv->enter_internal_edit_mode ();
                } else {
                        rtv->leave_internal_edit_mode ();
                }
        
		rtv->view()->RegionViewAdded.connect (sigc::mem_fun (*this, &Editor::region_view_added));
		rtv->view()->RegionViewRemoved.connect (sigc::mem_fun (*this, &Editor::region_view_removed));
        ARDOUR_UI::instance()->_progress_dialog.add_progress_step (); //process of tracks addition
	}

	if (new_views.size() > 0) {
		_routes->routes_added (new_views);
		_summary->routes_added (new_views);
	}

	if (show_editor_mixer_when_tracks_arrive) {
		show_editor_mixer (true);
	}

	editor_list_button.set_sensitive (true);
	connect_routes_and_update_global_rec_button (routes);
}

namespace {
    bool tv_not_selected (TimeAxisView *tv) {
        return !tv->get_selected ();
    }
}

void
Editor::timeaxisview_deleted (TimeAxisView *tv)
{
	if (tv == entered_track) {
		entered_track = 0;
	}

	if (_session && _session->deletion_in_progress()) {
		/* the situation is under control */
		return;
	}

	ENSURE_GUI_THREAD (*this, &Editor::timeaxisview_deleted, tv);

	RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (tv);

	_routes->route_removed (tv);

	TimeAxisView::Children c = tv->get_child_list ();
	for (TimeAxisView::Children::const_iterator i = c.begin(); i != c.end(); ++i) {
		if (entered_track == i->get()) {
			entered_track = 0;
		}
	}

	/* remove it from the list of track views */

	TrackViewList::iterator i;

	if ((i = std::find (track_views.begin(), track_views.end(), tv)) != track_views.end()) {
		i = track_views.erase (i);
	}

	/* update whatever the current mixer strip is displaying, if revelant */

	boost::shared_ptr<Route> route;

	if (rtav) {
		route = rtav->route ();
	}

	if (current_mixer_strip && current_mixer_strip->route() == route) {

        // find first non selected track
        TimeAxisView* first_non_selected_tv = 0;
        
        if (!track_views.empty() ) {
            
            i = std::find_if (track_views.begin(), track_views.end(), tv_not_selected);
            
            if (i != track_views.end() ) {
                first_non_selected_tv = (*i);
            }
		}
        
        if (first_non_selected_tv ) {
			set_selected_mixer_strip (*first_non_selected_tv);
		} else {
			/* make the editor mixer strip go away setting the
			 * button to inactive (which also unticks the menu option)
			 */

			ActionManager::uncheck_toggleaction ("<Actions>/Editor/show-editor-mixer");
            delete current_mixer_strip;
            current_mixer_strip = 0;
            ARDOUR_UI::instance()->reset_track_color_dialog ();
		}
	}
}

void
Editor::hide_track_in_display (TimeAxisView* tv, bool apply_to_selection)
{
	if (apply_to_selection) {
		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ) {

			TrackSelection::iterator j = i;
			++j;

			hide_track_in_display (*i, false);

			i = j;
		}
	} else {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

		if (rtv && current_mixer_strip && (rtv->route() == current_mixer_strip->route())) {
			// this will hide the mixer strip
			set_selected_mixer_strip (*tv);
		}

		_routes->hide_track_in_display (*tv);
	}
}

bool
Editor::sync_track_view_list_and_routes ()
{
	track_views = TrackViewList (_routes->views ());

	_summary->set_dirty ();
	_group_tabs->set_dirty ();

	return false; // do not call again (until needed)
}

void
Editor::foreach_time_axis_view (sigc::slot<void,TimeAxisView&> theslot)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		theslot (**i);
	}
}

/** Find a RouteTimeAxisView by the ID of its route */
RouteTimeAxisView*
Editor::get_route_view_by_route_id (const PBD::ID& id) const
{
	RouteTimeAxisView* v;

	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if((v = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
			if(v->route()->id() == id) {
				return v;
			}
		}
	}

	return 0;
}

void
Editor::fit_route_group (RouteGroup *g)
{
	TrackViewList ts = axis_views_from_routes (g->route_list ());
	fit_tracks (ts);
}

void
Editor::consider_auditioning (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);

	if (r == 0) {
		_session->cancel_audition ();
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		if (r == last_audition_region) {
			return;
		}
	}

	_session->audition_region (r);
	last_audition_region = r;
}


void
Editor::hide_a_region (boost::shared_ptr<Region> r)
{
	r->set_hidden (true);
}

void
Editor::show_a_region (boost::shared_ptr<Region> r)
{
	r->set_hidden (false);
}

void
Editor::audition_region_from_region_list ()
{
	_regions->selection_mapover (sigc::mem_fun (*this, &Editor::consider_auditioning));
}

void
Editor::hide_region_from_region_list ()
{
	_regions->selection_mapover (sigc::mem_fun (*this, &Editor::hide_a_region));
}

void
Editor::show_region_in_region_list ()
{
	_regions->selection_mapover (sigc::mem_fun (*this, &Editor::show_a_region));
}

void
Editor::step_edit_status_change (bool yn)
{
	if (yn) {
		start_step_editing ();
	} else {
		stop_step_editing ();
	}
}

void
Editor::start_step_editing ()
{
	step_edit_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::check_step_edit), 20);
}

void
Editor::stop_step_editing ()
{
	step_edit_connection.disconnect ();
}

bool
Editor::check_step_edit ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (*i);
		if (mtv) {
			mtv->check_step_edit ();
		}
	}

	return true; // do it again, till we stop
}

bool
Editor::scroll_press (Direction dir)
{
	++_scroll_callbacks;

	if (_scroll_connection.connected() && _scroll_callbacks < 5) {
		/* delay the first auto-repeat */
		return true;
	}

	switch (dir) {
	case LEFT:
		scroll_backward (1);
		break;

	case RIGHT:
		scroll_forward (1);
		break;

	case UP:
		scroll_up_one_track ();
		break;

	case DOWN:
		scroll_down_one_track ();
		break;
	}

	/* do hacky auto-repeat */
	if (!_scroll_connection.connected ()) {

		_scroll_connection = Glib::signal_timeout().connect (
			sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), dir), 100
			);

		_scroll_callbacks = 0;
	}

	return true;
}

void
Editor::scroll_release ()
{
	_scroll_connection.disconnect ();
}

/** Queue a change for the Editor viewport x origin to follow the playhead */
void
Editor::reset_x_origin_to_follow_playhead ()
{
	framepos_t const frame = playhead_cursor->current_frame ();

	if (frame < leftmost_frame || frame > leftmost_frame + current_page_samples()) {

		if (_session->transport_speed() < 0) {

			if (frame > (current_page_samples() / 2)) {
				center_screen (frame-(current_page_samples()/2));
			} else {
				center_screen (current_page_samples()/2);
			}

		} else {

			framepos_t l = 0;
			
			if (frame < leftmost_frame) {
				/* moving left */
				if (_session->transport_rolling()) {
					/* rolling; end up with the playhead at the right of the page */
					l = frame - current_page_samples ();
				} else {
					/* not rolling: end up with the playhead 1/4 of the way along the page */
					l = frame - current_page_samples() / 4;
				}
			} else {
				/* moving right */
				if (_session->transport_rolling()) {
					/* rolling: end up with the playhead on the left of the page */
					l = frame;
				} else {
					/* not rolling: end up with the playhead 3/4 of the way along the page */
					l = frame - 3 * current_page_samples() / 4;
				}
			}

			if (l < 0) {
				l = 0;
			}
            
			center_screen_internal (l + (current_page_samples() / 2), current_page_samples ());
		}
	}
}

void
Editor::super_rapid_screen_update ()
{
	if (!_session || !_session->engine().running()) {
		return;
	}

	/* METERING */

	/* update track meters, if required */
    _session->update_meters ();

	/* PLAYHEAD AND VIEWPORT */

	framepos_t const frame = _session->audible_frame();

	/* There are a few reasons why we might not update the playhead / viewport stuff:
	 *
	 * 1.  we don't update things when there's a pending locate request, otherwise
	 *     when the editor requests a locate there is a chance that this method
	 *     will move the playhead before the locate request is processed, causing
	 *     a visual glitch.
	 * 2.  if we're not rolling, there's nothing to do here (locates are handled elsewhere).
	 * 3.  if we're still at the same frame that we were last time, there's nothing to do.
	 */

	if (!_pending_locate_request && _session->transport_speed() != 0 && frame != last_update_frame) {

		last_update_frame = frame;

		if (!_dragging_playhead) {
			playhead_cursor->set_position (frame);
		}
        
		if (!_stationary_playhead) {

            if (_session->actively_recording () &&
                sample_to_pixel(frame) > horizontal_adjustment.get_upper() ) {
                horizontal_adjustment.set_upper(sample_to_pixel(frame) + _visible_canvas_width );
            }
            
			if (!_dragging_playhead && _follow_playhead && _session->requested_return_frame() < 0 && !pending_visual_change.being_handled) {
				/* We only do this if we aren't already
				   handling a visual change (ie if
				   pending_visual_change.being_handled is
				   false) so that these requests don't stack
				   up there are too many of them to handle in
				   time.
				*/
				reset_x_origin_to_follow_playhead ();
			}

		} else {

			/* don't do continuous scroll till the new position is in the rightmost quarter of the
			   editor canvas
			*/
#if 0
			// FIXME DO SOMETHING THAT WORKS HERE - this is 2.X code
			double target = ((double)frame - (double)current_page_samples()/2.0) / samples_per_pixel;
			if (target <= 0.0) {
				target = 0.0;
			}
			if (fabs(target - current) < current_page_samples() / samples_per_pixel) {
				target = (target * 0.15) + (current * 0.85);
			} else {
				/* relax */
			}

			current = target;
			horizontal_adjustment.set_value (current);
#endif
		}

	}
}


void
Editor::session_going_away ()
{
	_have_idled = false;

	_session_connections.drop_connections ();

	super_rapid_screen_update_connection.disconnect ();

	selection->clear ();
	cut_buffer->clear ();

	clicked_regionview = 0;
	clicked_axisview = 0;
	clicked_routeview = 0;
	entered_regionview = 0;
	entered_track = 0;
	last_update_frame = 0;
	_drags->abort ();

	playhead_cursor->hide ();

	/* rip everything out of the list displays */

	_regions->clear ();
	_routes->clear ();
	_route_groups->clear ();

	/* do this first so that deleting a track doesn't reset cms to null
	   and thus cause a leak.
	*/

    ARDOUR_UI::instance()->update_track_color_dialog (boost::shared_ptr<ARDOUR::Route>());
	if (current_mixer_strip) {
		if (current_mixer_strip->get_parent() != 0) {
			current_mixer_strip->get_parent()->remove (*current_mixer_strip);
		}
		delete current_mixer_strip;
		current_mixer_strip = 0;
	}

	/* delete all trackviews */

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		delete *i;
	}
	track_views.clear ();

	nudge_clock->set_session (0);

	editor_list_button.set_active(false);
	editor_list_button.set_sensitive(false);

	/* clear tempo/meter rulers */
	remove_metric_marks ();
	hide_measures ();
	clear_marker_display ();

	stop_step_editing ();
	
	/* get rid of any existing editor mixer strip */

	WindowTitle title(Glib::get_application_name());
	title += _("Editor");

	set_title (title.get_string());

	SessionHandlePtr::session_going_away ();
}


void
Editor::show_editor_list (bool yn)
{
	if (yn) {
		_the_notebook.show ();
	} else {
		_the_notebook.hide ();
	}
}

void
Editor::change_region_layering_order (bool from_context_menu)
{
	const framepos_t position = get_preferred_edit_position (false, from_context_menu);

	if (!clicked_routeview) {
		if (layering_order_editor) {
			layering_order_editor->hide ();
		}
		return;
	}

	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (clicked_routeview->route());

	if (!track) {
		return;
	}

	boost::shared_ptr<Playlist> pl = track->playlist();

	if (!pl) {
		return;
	}

	if (layering_order_editor == 0) {
		layering_order_editor = new RegionLayeringOrderEditor (*this);
	}

	layering_order_editor->set_context (clicked_routeview->name(), _session, clicked_routeview, pl, position);
	layering_order_editor->maybe_present ();
}

void
Editor::update_region_layering_order_editor ()
{
	if (layering_order_editor && layering_order_editor->is_visible ()) {
		change_region_layering_order (true);
	}
}

void
Editor::setup_fade_images ()
{
	_fade_in_images[FadeLinear] = new Gtk::Image (get_icon_path (X_("fadein-linear")));
	_fade_in_images[FadeSymmetric] = new Gtk::Image (get_icon_path (X_("fadein-symmetric")));
	_fade_in_images[FadeFast] = new Gtk::Image (get_icon_path (X_("fadein-fast-cut")));
	_fade_in_images[FadeSlow] = new Gtk::Image (get_icon_path (X_("fadein-slow-cut")));
	_fade_in_images[FadeConstantPower] = new Gtk::Image (get_icon_path (X_("fadein-constant-power")));

	_fade_out_images[FadeLinear] = new Gtk::Image (get_icon_path (X_("fadeout-linear")));
	_fade_out_images[FadeSymmetric] = new Gtk::Image (get_icon_path (X_("fadeout-symmetric")));
	_fade_out_images[FadeFast] = new Gtk::Image (get_icon_path (X_("fadeout-fast-cut")));
	_fade_out_images[FadeSlow] = new Gtk::Image (get_icon_path (X_("fadeout-slow-cut")));
	_fade_out_images[FadeConstantPower] = new Gtk::Image (get_icon_path (X_("fadeout-constant-power")));
	
	_xfade_in_images[FadeLinear] = new Gtk::Image (get_icon_path (X_("fadein-linear")));
	_xfade_in_images[FadeSymmetric] = new Gtk::Image (get_icon_path (X_("fadein-symmetric")));
	_xfade_in_images[FadeFast] = new Gtk::Image (get_icon_path (X_("fadein-fast-cut")));
	_xfade_in_images[FadeSlow] = new Gtk::Image (get_icon_path (X_("fadein-slow-cut")));
	_xfade_in_images[FadeConstantPower] = new Gtk::Image (get_icon_path (X_("fadein-constant-power")));

	_xfade_out_images[FadeLinear] = new Gtk::Image (get_icon_path (X_("fadeout-linear")));
	_xfade_out_images[FadeSymmetric] = new Gtk::Image (get_icon_path (X_("fadeout-symmetric")));
	_xfade_out_images[FadeFast] = new Gtk::Image (get_icon_path (X_("fadeout-fast-cut")));
	_xfade_out_images[FadeSlow] = new Gtk::Image (get_icon_path (X_("fadeout-slow-cut")));
	_xfade_out_images[FadeConstantPower] = new Gtk::Image (get_icon_path (X_("fadeout-constant-power")));

}

/** @return Gtk::manage()d menu item for a given action from `editor_actions' */
Gtk::MenuItem&
Editor::action_menu_item (std::string const & name)
{
	Glib::RefPtr<Action> a = editor_actions->get_action (name);
	assert (a);

	return *manage (a->create_menu_item ());
}

void
Editor::add_notebook_page (string const & name, Gtk::Widget& widget)
{
	EventBox* b = manage (new EventBox);
	b->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::notebook_tab_clicked), &widget));
	Label* l = manage (new Label (name));
	l->set_angle (-90);
	b->add (*l);
	b->show_all ();
	_the_notebook.append_page (widget, *b);
}

bool
Editor::notebook_tab_clicked (GdkEventButton* ev, Gtk::Widget* page)
{
	if (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_2BUTTON_PRESS) {
		_the_notebook.set_current_page (_the_notebook.page_num (*page));
	}

	if (ev->type == GDK_2BUTTON_PRESS) {

		/* double-click on a notebook tab shrinks or expands the notebook */

		if (_notebook_shrunk) {
			if (pre_notebook_shrink_pane_width) {
				edit_pane.set_position (*pre_notebook_shrink_pane_width);
			}
			_notebook_shrunk = false;
		} else {
			pre_notebook_shrink_pane_width = edit_pane.get_position();

			/* this expands the LHS of the edit pane to cover the notebook
			   PAGE but leaves the tabs visible.
			 */
			edit_pane.set_position (edit_pane.get_position() + page->get_width());
			_notebook_shrunk = true;
		}
	}

	return true;
}

void
Editor::popup_control_point_context_menu (ArdourCanvas::Item* item, GdkEvent* event)
{
	using namespace Menu_Helpers;
	
	MenuList& items = _control_point_context_menu.items ();
	items.clear ();
	
	items.push_back (MenuElem (_("Edit..."), sigc::bind (sigc::mem_fun (*this, &Editor::edit_control_point), item)));
	items.push_back (MenuElem (_("Delete"), sigc::bind (sigc::mem_fun (*this, &Editor::remove_control_point), item)));
	if (!can_remove_control_point (item)) {
		items.back().set_sensitive (false);
	}

	_control_point_context_menu.popup (event->button.button, event->button.time);
}

void
Editor::zoom_vertical_modifier_released()
{
	_stepping_axis_view = 0;
}

void
Editor::output_connection_mode_changed ()
{
    if (!_session) {
        return;
    }
    
    if (Config->get_output_auto_connect() & AutoConnectMaster) {
        
        _master_bus_ui->master_bus_set_visible (true);
        
        _mixer_bridge_view.all_gain_sliders_set_visible (true);
        if (current_mixer_strip) {
            current_mixer_strip->gain_slider_set_visible (true);
        }
        if (_session->master_out() && !axis_view_from_route(_session->master_out() ) ) {

            selection->block_tracks_changed(true);
            
            // backup selected tracks
            TrackViewList selected_tracks (selection->tracks);
            std::list<RegionView*> selected_regions (selection->regions);
            
            // add master bus view
            RouteList list;
            list.push_back(_session->master_out() );
            add_routes(list);
            
            // restore selection
            selection->tracks.clear();
            selection->tracks.add(selected_tracks);
            selection->regions.clear();
            selection->regions.add(selected_regions);
            
            selection->block_tracks_changed(false);
        }
    } else {
        
        _master_bus_ui->master_bus_set_visible (false);
        
        _mixer_bridge_view.all_gain_sliders_set_visible (false);
        if (current_mixer_strip) {
            current_mixer_strip->gain_slider_set_visible (false);
        }
        TimeAxisView* tv = 0;
        if (_session->master_out() && (tv = axis_view_from_route(_session->master_out() ) ) ) {
            delete tv;
        }
    }
    
    _routes->redisplay ();
}


void
Editor::ui_parameter_changed (string parameter)
{
	if (parameter == "icon-set") {
		while (!_cursor_stack.empty()) {
			_cursor_stack.pop();
		}
		_cursors->set_cursor_set (ARDOUR_UI::config()->get_icon_set());
	} else if (parameter == "draggable-playhead") {
		if (_verbose_cursor) {
			playhead_cursor->set_sensitive (ARDOUR_UI::config()->get_draggable_playhead());
		}
	}
}

void
Editor::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			global_solo_button.set_active (true);
		} else {
			global_solo_button.set_active (false);
		}
	} else {
		global_solo_button.set_active (false);
	}
}

void
Editor::global_solo_clicked (WavesButton*)
{
	if (!_session) {
                return;
        }

        if (_session->soloing()) {
                _session->set_solo (_session->get_routes(), false);
        } else if (_session->listening()) {
                _session->set_listen (_session->get_routes(), false);
        }
}

// Global Record button staff
bool
Editor::check_all_tracks_are_record_armed ()
{
    if( !_session )
        return false;
    
    boost::shared_ptr<RouteList> tracks = _session->get_tracks ();
    
    if(tracks->size() == 0)
        return false;
    
    bool all_tracks_are_record_armed = true;
    std::size_t n_record_enabled = 0;
    for (RouteList::iterator i = tracks->begin(); i != tracks->end(); ++i)
    {
        if ( !(*i)->record_enabled() && !(*i)->record_safe () ) {
            all_tracks_are_record_armed = false;
            break;
        } else if ((*i)->record_enabled()) {
            ++n_record_enabled;
        }
    }
    
    return all_tracks_are_record_armed && n_record_enabled;
}

bool
Editor::check_all_tracks_are_record_safe ()
{
    if (!_session)
        return false;
    
    boost::shared_ptr<RouteList> tracks = _session->get_tracks ();
    
    if (tracks->size () == 0)
        return false;
    
    bool all_tracks_are_record_safe = true;
    for (RouteList::iterator i = tracks->begin(); i != tracks->end(); ++i)
    {
        if (!(*i)->record_safe ()) {
            all_tracks_are_record_safe = false;
            break;
        }
    }
    
    return all_tracks_are_record_safe;
}

void
Editor::connect_routes_and_update_global_rec_button(RouteList& tracks)
{
    for (RouteList::iterator i = tracks.begin(); i != tracks.end(); ++i)
    {
        boost::shared_ptr<Track> t;
        
        if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
            t->RecordEnableChanged.connect (_route_state_connections,
                                            invalidator (*this),
                                            boost::bind (&Editor::record_state_changed, this),
                                            gui_context() );
            t->RecordSafeChanged.connect (_route_state_connections,
                                          invalidator (*this),
                                          boost::bind (&Editor::record_state_changed, this),
                                          gui_context() );
		}
        
        (*i)->DropReferences.connect(_route_state_connections,
                                     invalidator (*this),
                                     boost::bind (&Editor::record_state_changed, this),
                                     gui_context() );
    }
    
    record_state_changed ();
}

void
Editor::record_state_changed ()
{
    _global_rec_button.set_active (check_all_tracks_are_record_armed ());
}

void
Editor::global_rec_clicked (WavesButton*)
{
    if( !_session )
        return;
    
    boost::shared_ptr<RouteList> rl = _session->get_tracks ();
    
    if (rl->empty () || check_all_tracks_are_record_safe ()) {
        _global_rec_button.set_active (false);
        return;
    }
    
    bool all_tracks_are_record_armed = check_all_tracks_are_record_armed();
    DisplaySuspender ds;
    _session->set_record_enabled (rl, !all_tracks_are_record_armed);
    _global_rec_button.set_active(!all_tracks_are_record_armed);
}

void
Editor::midi_input_connection_changed (const std::vector<std::string>& ports, bool connected)
{
    if (!_session) {
        return;
    }
    
    if (ports.empty() ) {
        _session->reconnect_midi_scene_ports(true);
    } else {
        
        std::vector<std::string>::const_iterator iter = ports.begin();
        for (; iter != ports.end(); ++iter) {
            if (connected ) {
                _session->scene_in()->connect(*iter);
            }
            
            if (!connected ) {
                _session->scene_in()->disconnect(*iter);
            }
        }
    }
    
    reset_marker_midi_images (true);
}

void
Editor::midi_output_connection_changed (const std::vector<std::string>& ports, bool connected)
{
    if (!_session) {
        return;
    }
    
    if (ports.empty() ) {
        _session->reconnect_midi_scene_ports(false);
    } else {
    
        std::vector<std::string>::const_iterator iter = ports.begin();
        for (; iter != ports.end(); ++iter) {

            if (connected ) {
                _session->scene_out()->connect(*iter);
            }
            
            if (!connected ) {
                _session->scene_out()->disconnect(*iter);
            }
        }
    }

    reset_marker_midi_images (false);
}

void
Editor::midi_input_configuration_changed ()
{
    if (!_session) {
        return;
    }
    
    _session->reconnect_midi_scene_ports (true);
    update_midi_dropdowns ();
}

void
Editor::midi_output_configuration_changed ()
{
    if (!_session) {
        return;
    }
    
    _session->reconnect_midi_scene_ports (false);
    update_midi_dropdowns ();
}

void
Editor::update_midi_dropdowns ()
{
    populate_midi_inout_dropdowns ();
    reset_marker_midi_images (true);
    reset_marker_midi_images (false);
}

void
Editor::port_registration_handler ()
{
    if (!_session) {
            return;
    }
    
    update_midi_dropdowns ();
}

void
Editor::port_connection_handler (boost::weak_ptr<Port> wa, std::string, boost::weak_ptr<Port> wb, std::string, bool connected)
{
    if (!_session) {
            return;
    }
    
    // add actions here
}


void // true - track was added, false - track was removed
Editor::update_progress_dialog_of_changing_tracks (bool operation)
{
    ARDOUR_UI::instance()->_progress_dialog.add_progress_step (); // process of tracks addition/removal 
}
