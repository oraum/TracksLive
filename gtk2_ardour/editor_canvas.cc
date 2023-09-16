/*
    Copyright (C) 2005 Paul Davis

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

#include "gtkmm2ext/utils.h"

#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/smf_source.h"
#include "ardour/audiofilesource.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/pixbuf.h"
#include "canvas/scroll_group.h"
#include "canvas/text.h"
#include "canvas/utils.h"
#include "canvas/debug.h"

#include "ardour_ui.h"
#include "automation_time_axis.h"
#include "editor.h"
#include "global_signals.h"
#include "editing.h"
#include "rgb_macros.h"
#include "utils.h"
#include "audio_time_axis.h"
#include "editor_drag.h"
#include "region_view.h"
#include "editor_group_tabs.h"
#include "editor_summary.h"
#include "video_timeline.h"
#include "keyboard.h"
#include "editor_cursors.h"
#include "mouse_cursors.h"
#include "verbose_cursor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

void
Editor::initialize_canvas ()
{
	_track_canvas_viewport = new ArdourCanvas::GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);
	_track_canvas = _track_canvas_viewport->canvas ();

        _track_canvas->set_background_color (ARDOUR_UI::config()->get_canvasvar_ArrangeBase());

	/* scroll group for items that should not automatically scroll
	 *  (e.g verbose cursor). It shares the canvas coordinate space.
	*/
	no_scroll_group = new ArdourCanvas::Container (_track_canvas->root());

	ArdourCanvas::ScrollGroup* hsg; 
	ArdourCanvas::ScrollGroup* hg;
	ArdourCanvas::ScrollGroup* vg;

	hv_scroll_group = hsg = new ArdourCanvas::ScrollGroup (_track_canvas->root(), 
							       ArdourCanvas::ScrollGroup::ScrollSensitivity (ArdourCanvas::ScrollGroup::ScrollsVertically|
													     ArdourCanvas::ScrollGroup::ScrollsHorizontally));
	CANVAS_DEBUG_NAME (hv_scroll_group, "canvas hv scroll");
	_track_canvas->add_scroller (*hsg);

	v_scroll_group = vg = new ArdourCanvas::ScrollGroup (_track_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsVertically);
	CANVAS_DEBUG_NAME (v_scroll_group, "canvas v scroll");
	_track_canvas->add_scroller (*vg);

	h_scroll_group = hg = new ArdourCanvas::ScrollGroup (_track_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (h_scroll_group, "canvas h scroll");
	_track_canvas->add_scroller (*hg);

	_verbose_cursor = new VerboseCursor (this);

	/* on the bottom, an image */

	if (Profile->get_sae()) {
		Image img (::get_icon (X_("saelogo")));
		// logo_item = new ArdourCanvas::Pixbuf (_track_canvas->root(), 0.0, 0.0, img.get_pixbuf());
		// logo_item->property_height_in_pixels() = true;
		// logo_item->property_width_in_pixels() = true;
		// logo_item->property_height_set() = true;
		// logo_item->property_width_set() = true;
		// logo_item->show ();
	}

	/*a group to hold global rects like punch/loop indicators */
	global_rect_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (global_rect_group, "global rect group");

        transport_loop_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_loop_range_rect, "loop rect");
	transport_loop_range_rect->hide();

	transport_punch_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_punch_range_rect, "punch rect");
	transport_punch_range_rect->hide();

	/*a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (time_line_group, "time line group");

	_trackview_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (_trackview_group, "Canvas TrackViews");
	
	// used to show zoom mode active zooming
	zoom_rect = new ArdourCanvas::Rectangle (hv_scroll_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	zoom_rect->hide();
	zoom_rect->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_zoom_rect_event), (ArdourCanvas::Item*) 0));

	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::Rectangle (hv_scroll_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	rubberband_rect->hide();

	/* a group to hold stuff while it gets dragged around. Must be the
	 * uppermost (last) group with hv_scroll_group as a parent
	 */
	_drag_motion_group = new ArdourCanvas::Container (hv_scroll_group);                                                                                                                                     
        CANVAS_DEBUG_NAME (_drag_motion_group, "Canvas Drag Motion");

	/* TIME BAR CANVAS */
	
        /* this group is part of the canvas "top-level" hscroll group. This group
           responds only to horizontal scroll, so vertical scrolling does not move
           it.
        */

	_time_markers_group = new ArdourCanvas::Container (h_scroll_group);
	CANVAS_DEBUG_NAME (_time_markers_group, "time bars");

        /* items (containers) in the time_markers_group. Each group holds a background rectangle ("bar"),
           and may then also hold zero or more markers of various kinds, depending on the state of the
           session (does it have any locations defined?) and visibility options. In some cases, eg. the ruler
           group, it may hold other kinds of items also (e.g. a ruler).
         */

	cd_marker_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (cd_marker_group, "cd marker group");
	videotl_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (videotl_group, "videotl group");
	marker_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (marker_group, "marker group");
	transport_marker_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (transport_marker_group, "transport marker group");
	skip_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (skip_group, "skip group");
	range_marker_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (range_marker_group, "range marker group");
	tempo_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (tempo_group, "tempo group");
	meter_group = new ArdourCanvas::Container (_time_markers_group);
	CANVAS_DEBUG_NAME (meter_group, "meter group");
	ruler_group = new ArdourCanvas::Container (_time_markers_group);
        CANVAS_DEBUG_NAME (ruler_group, "ruler group");

        /* bars (background rectangles for each kind of marker/ruler */

	skip_bar = new ArdourCanvas::Rectangle (skip_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, skipbar_height));
	CANVAS_DEBUG_NAME (skip_bar, "skip Bar");
	skip_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	marker_bar = new ArdourCanvas::Rectangle (marker_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, marker_height));
	CANVAS_DEBUG_NAME (marker_bar, "Marker Bar");
	marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

        /* Rectangles displayed on the bars during drag operations */

	skip_drag_rect = new ArdourCanvas::Rectangle (skip_group, ArdourCanvas::Rect (0.0, 0.0, 100, skipbar_height));
	CANVAS_DEBUG_NAME (skip_drag_rect, "skip drag");
	skip_drag_rect->set_outline (false);
	skip_drag_rect->hide ();

	range_bar_drag_rect = new ArdourCanvas::Rectangle (range_marker_group, ArdourCanvas::Rect (0.0, 0.0, 100, marker_height));
	CANVAS_DEBUG_NAME (range_bar_drag_rect, "range drag");
	range_bar_drag_rect->set_outline (false);
	range_bar_drag_rect->hide ();

	transport_bar_drag_rect = new ArdourCanvas::Rectangle (ruler_group, ArdourCanvas::Rect (0.0, 0.0, 100, 
                                                                                                ruler_height - ruler_divide_height - 2.0));
	CANVAS_DEBUG_NAME (transport_bar_drag_rect, "transport drag");
	transport_bar_drag_rect->set_outline (false);
	transport_bar_drag_rect->hide ();

        /* the following bars and rects are not used in Tracks Live */

	meter_bar = new ArdourCanvas::Rectangle (meter_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (meter_bar, "meter Bar");
	meter_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	tempo_bar = new ArdourCanvas::Rectangle (tempo_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (tempo_bar, "Tempo  Bar");
	tempo_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	range_marker_bar = new ArdourCanvas::Rectangle (range_marker_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (range_marker_bar, "Range Marker Bar");
	range_marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	cd_marker_bar = new ArdourCanvas::Rectangle (cd_marker_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (cd_marker_bar, "CD Marker Bar");
 	cd_marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	ARDOUR_UI::instance()->video_timeline = new VideoTimeLine(this, videotl_group, (timebar_height * videotl_bar_height));
	
	cd_marker_bar_drag_rect = new ArdourCanvas::Rectangle (cd_marker_group, ArdourCanvas::Rect (0.0, 0.0, 100, timebar_height));
	CANVAS_DEBUG_NAME (cd_marker_bar_drag_rect, "cd marker drag");
	cd_marker_bar_drag_rect->set_outline (false);
	cd_marker_bar_drag_rect->hide ();

        /* end of bars + rects */
        
	transport_punchin_line = new ArdourCanvas::Line (hv_scroll_group);
	transport_punchin_line->set_x0 (0);
	transport_punchin_line->set_y0 (0);
	transport_punchin_line->set_x1 (0);
	transport_punchin_line->set_y1 (ArdourCanvas::COORD_MAX);
	transport_punchin_line->hide ();

	transport_punchout_line  = new ArdourCanvas::Line (hv_scroll_group);
	transport_punchout_line->set_x0 (0);
	transport_punchout_line->set_y0 (0);
	transport_punchout_line->set_x1 (0);
	transport_punchout_line->set_y1 (ArdourCanvas::COORD_MAX);
	transport_punchout_line->hide();

	tempo_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_tempo_bar_event), tempo_bar));
	meter_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_meter_bar_event), meter_bar));
	marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_marker_bar_event), marker_bar));
	cd_marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_cd_marker_bar_event), cd_marker_bar));
	videotl_group->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_videotl_bar_event), videotl_group));
	range_marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_range_marker_bar_event), range_marker_bar));
	skip_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_skip_bar_event), skip_bar));

	playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event);

	if (logo_item) {
		logo_item->lower_to_bottom ();
	}


	_canvas_drop_zone = new ArdourCanvas::Rectangle (hv_scroll_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, 0.0));
	/* this thing is transparent */
	_canvas_drop_zone->set_fill (false);
	_canvas_drop_zone->set_outline (false);
	_canvas_drop_zone->Event.connect (sigc::mem_fun (*this, &Editor::canvas_drop_zone_event));

	/* these signals will initially be delivered to the canvas itself, but if they end up remaining unhandled, they are passed to Editor-level
	   handlers.
	*/

	_track_canvas->signal_scroll_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_scroll_event), true));
	_track_canvas->signal_motion_notify_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_motion_notify_event));
	_track_canvas->signal_button_press_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_button_press_event));
	_track_canvas->signal_button_release_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_button_release_event));
	_track_canvas->signal_drag_motion().connect (sigc::mem_fun (*this, &Editor::track_canvas_drag_motion));
	_track_canvas->signal_key_press_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_key_press));
	_track_canvas->signal_key_release_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_key_release));

	_track_canvas->set_name ("EditorMainCanvas");
	_track_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	_track_canvas->signal_leave_notify_event().connect (sigc::mem_fun(*this, &Editor::left_track_canvas), false);
	_track_canvas->signal_enter_notify_event().connect (sigc::mem_fun(*this, &Editor::entered_track_canvas), false);
	_track_canvas->set_flags (CAN_FOCUS);

	/* set up drag-n-drop */

	vector<TargetEntry> target_table;

	// Drag-N-Drop from the region list can generate this target
	target_table.push_back (TargetEntry ("regions"));

	target_table.push_back (TargetEntry ("text/plain"));
	target_table.push_back (TargetEntry ("text/uri-list"));
	target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	_track_canvas->drag_dest_set (target_table);
	_track_canvas->signal_drag_data_received().connect (sigc::mem_fun(*this, &Editor::track_canvas_drag_data_received));

	_track_canvas_viewport->signal_size_allocate().connect (sigc::mem_fun(*this, &Editor::track_canvas_viewport_allocate));

	initialize_rulers ();

	ColorsChanged.connect (sigc::mem_fun (*this, &Editor::color_handler));
	color_handler();

}

void
Editor::track_canvas_viewport_allocate (Gtk::Allocation alloc)
{
	_canvas_viewport_allocation = alloc;
	track_canvas_viewport_size_allocated ();
}

void
Editor::update_horizontal_adjustment_limits ()
{
    if (!_session) {
        return;
    }
    
    double lower_limit = sample_to_pixel(_session->locations()->session_range_location()->start() );
    horizontal_adjustment.set_lower(lower_limit);
    
    double session_end_marker_position = sample_to_pixel(_session->locations()->session_range_location()->end() );
    double upper_limit = max (lower_limit + _visible_canvas_width, session_end_marker_position);
	upper_limit = max (upper_limit, sample_to_pixel(leftmost_frame) + _visible_canvas_width);
    
    horizontal_adjustment.set_upper(upper_limit + 10); // 3 pixels offset
}

void
Editor::track_canvas_viewport_size_allocated ()
{
	bool height_changed = _visible_canvas_height != _canvas_viewport_allocation.get_height();
    
    bool width_changed = _visible_canvas_width != _canvas_viewport_allocation.get_width();

	_visible_canvas_width  = _canvas_viewport_allocation.get_width ();
	_visible_canvas_height = _canvas_viewport_allocation.get_height ();

	_canvas_drop_zone->set_y1 (_canvas_drop_zone->y0() + (_visible_canvas_height - 20.0));

	// SHOWTRACKS

	if (height_changed) {

		for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
			i->second->canvas_height_set (_visible_canvas_height);
		}
        
		vertical_adjustment.set_page_size (_visible_canvas_height);
        vertical_adjustment.set_page_increment (_visible_canvas_height/2);
        vertical_adjustment.set_step_increment (_visible_canvas_height/10);
        
		if ((vertical_adjustment.get_value() + _visible_canvas_height) >= vertical_adjustment.get_upper()) {
			/*
			   We're increasing the size of the canvas while the bottom is visible.
			   We scroll down to keep in step with the controls layout.
			*/
			vertical_adjustment.set_value (_full_canvas_height - _visible_canvas_height);
		}

		set_visible_track_count (_visible_track_count);
	}
    
    if (width_changed) {
        horizontal_adjustment.set_page_size (_visible_canvas_width);
        horizontal_adjustment.set_page_increment (_visible_canvas_width/2);
        horizontal_adjustment.set_step_increment (_visible_canvas_width/10);
        
        // set adjustment horizontal value
        update_horizontal_adjustment_limits ();
        
        if ((horizontal_adjustment.get_value() + _visible_canvas_width) >= horizontal_adjustment.get_upper()) {
            
            horizontal_adjustment.set_value (horizontal_adjustment.get_upper() - _visible_canvas_width);
        }
    }

	update_fixed_rulers();
	redisplay_tempo (false);
	_summary->set_overlays_dirty ();
}

void
Editor::reset_controls_layout_width ()
{
	GtkRequisition req;
	gint w;

	edit_controls_vbox.size_request (req);
	w = req.width;

        if (_group_tabs->is_mapped()) {
		_group_tabs->size_request (req);
                w += req.width;
        }

        /* the controls layout has no horizontal scrolling, its visible
           width is always equal to the total width of its contents.
        */

        controls_layout.property_width() = w;
        controls_layout.property_width_request() = w;
}

void
Editor::reset_controls_layout_height (int32_t h)
{
	/* ensure that the rect that represents the "bottom" of the canvas
	 * (the drag-n-drop zone) is, in fact, at the bottom.
	 */

	_canvas_drop_zone->set_position (ArdourCanvas::Duple (0, h));

	/* track controls layout must span the full height of "h" (all tracks)
	 * plus the bottom rect.
	 */

	h += _canvas_drop_zone->height ();

        /* set the height of the scrollable area (i.e. the sum of all contained widgets)
	 * for the controls layout. The size request is set elsewhere.
         */

    controls_layout.property_height() = h;

}

bool
Editor::track_canvas_map_handler (GdkEventAny* /*ev*/)
{
	/*
	 if (!MouseCursors::is_invalid (current_canvas_cursor)) {
		set_canvas_cursor (current_canvas_cursor);
	}
	 Rework!!!!
	 */
	return false;
}

/** This is called when something is dropped onto the track canvas */
void
Editor::track_canvas_drag_data_received (const RefPtr<Gdk::DragContext>& context,
					 int x, int y,
					 const SelectionData& data,
					 guint info, guint time)
{
	if (data.get_target() == "regions") {
		drop_regions (context, x, y, data, info, time);
	} else {
		drop_paths (context, x, y, data, info, time);
	}
}

bool
Editor::idle_drop_paths (vector<string> paths, framepos_t frame, double ypos, bool copy)
{
    ARDOUR::SoundFileInfo info;
	std::string errmsg;
    bool go_ahead = true;
    
	for (std::vector<std::string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
        if (ARDOUR::AudioFileSource::get_soundfile_info (*i, info, errmsg)) {
            if (info.channels > 2 ) {
                WavesMessageDialog msg ("", string_compose (_("One or more of the selected files\ncannot be used by %1"), PROGRAM_NAME));
                msg.run ();
                go_ahead = false;
                break;
            }
        }
    }
    if (go_ahead) {
        drop_paths_part_two (paths, frame, ypos, copy);
    }
	return false;
}

void
Editor::drop_paths_part_two (const vector<string>& paths, framepos_t frame, double ypos, bool copy)
{
	RouteTimeAxisView* tv;
	
	/* MIDI files must always be imported, because we consider them
	 * writable. So split paths into two vectors, and follow the import
	 * path on the MIDI part.
	 */

	vector<string> midi_paths;
	vector<string> audio_paths;

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		if (SMFSource::safe_midi_file_extension (*i)) {
            
            // Waves TracksLive does not support midi
            if (Profile->get_trx()) {
                continue;
            }
            
            midi_paths.push_back (*i);
		} else {
			audio_paths.push_back (*i);
		}
	}


	std::pair<TimeAxisView*, int> const tvp = trackview_by_y_position (ypos, false);
	if (tvp.first == 0) {

		/* drop onto canvas background: create new tracks */

		frame = 0;

        // Waves TracksLive does not support midi
        if (!Profile->get_trx()) {
            do_import (midi_paths, Editing::ImportDistinctFiles, ImportAsTrack, SrcBest, frame);
        }
		
        if (Profile->get_sae() || ARDOUR_UI::config()->get_only_copy_imported_files() || copy) {
			do_import (audio_paths, Editing::ImportDistinctFiles, Editing::ImportAsTrack, SrcBest, frame);
		} else {
			do_embed (audio_paths, Editing::ImportDistinctFiles, ImportAsTrack, frame);
		}

	} else if ((tv = dynamic_cast<RouteTimeAxisView*> (tvp.first)) != 0) {

		/* check that its a track, not a bus */

		if (tv->track()) {
			/* select the track, then embed/import */
			selection->set (tv);
            
            // Waves TracksLive does not support midi
            if (!Profile->get_trx()) {
                do_import (midi_paths, Editing::ImportSerializeFiles, ImportToTrack, SrcBest, frame);
            }

            if (Profile->get_sae() || ARDOUR_UI::config()->get_only_copy_imported_files() || copy) {
				do_import (audio_paths, Editing::ImportSerializeFiles, Editing::ImportToTrack, SrcBest, frame);
			} else {
				do_embed (audio_paths, Editing::ImportSerializeFiles, ImportToTrack, frame);
			}
		}
	}
}

void
Editor::drop_paths (const RefPtr<Gdk::DragContext>& context,
		    int x, int y,
		    const SelectionData& data,
		    guint info, guint time)
{
	vector<string> paths;
	GdkEvent ev;
	framepos_t frame;
	double cy;

	if (convert_drop_to_paths (paths, context, x, y, data, info, time) == 0) {

		/* D-n-D coordinates are window-relative, so convert to canvas coordinates
		 */

		ev.type = GDK_BUTTON_RELEASE;
		ev.button.x = x;
		ev.button.y = y;

		frame = window_event_sample (&ev, 0, &cy);

		snap_to (frame);

		bool copy = ((context->get_actions() & (Gdk::ACTION_COPY | Gdk::ACTION_LINK | Gdk::ACTION_MOVE)) == Gdk::ACTION_COPY);
#ifdef GTKOSX
		/* We are not allowed to call recursive main event loops from within
		   the main event loop with GTK/Quartz. Since import/embed wants
		   to push up a progress dialog, defer all this till we go idle.
		*/
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &Editor::idle_drop_paths), paths, frame, cy, copy));
#else
		idle_drop_paths (paths, frame, cy, copy);
#endif
	}

	context->drag_finish (true, false, time);
}

/** @param allow_horiz true to allow horizontal autoscroll, otherwise false.
 *
 *  @param allow_vert true to allow vertical autoscroll, otherwise false.
 *
 */
void
Editor::maybe_autoscroll (bool allow_horiz, bool allow_vert, bool from_headers)
{
	if (!ARDOUR_UI::config()->get_autoscroll_editor () || autoscroll_active ()) {
		return;
	}

	/* define a rectangular boundary for scrolling. If the mouse moves
	 * outside of this area and/or continue to be outside of this area,
	 * then we will continuously auto-scroll the canvas in the appropriate
	 * direction(s)
	 *
	 * the boundary is defined in coordinates relative to the toplevel
	 * window since that is what we're going to call ::get_pointer() on
	 * during autoscrolling to determine if we're still outside the
	 * boundary or not.
	 */

	ArdourCanvas::Rect scrolling_boundary;
	Gtk::Allocation alloc;
	int cx, cy;

	if (from_headers) {
		alloc = controls_layout.get_allocation ();
	} else {	
		alloc = _track_canvas_viewport->get_allocation ();
		cx = alloc.get_x();
		cy = alloc.get_y();

		/* reduce height by the height of the timebars, which happens
		   to correspond to the position of the hv_scroll_group.
		*/
		
		alloc.set_height (alloc.get_height() - hv_scroll_group->position().y);
		alloc.set_y (alloc.get_y() + hv_scroll_group->position().y);

		/* now reduce it again so that we start autoscrolling before we
		 * move off the top or bottom of the canvas
		 */

		alloc.set_height (alloc.get_height() - 20);
		alloc.set_y (alloc.get_y() + 10);

		/* the effective width of the autoscroll boundary so
		   that we start scrolling before we hit the edge.
		   
		   this helps when the window is slammed up against the
		   right edge of the screen, making it hard to scroll
		   effectively.
		*/
		
		if (alloc.get_width() > 20) { 
			alloc.set_width (alloc.get_width() - 20);
			alloc.set_x (alloc.get_x() + 10);
		} 

	}
	
	scrolling_boundary = ArdourCanvas::Rect (alloc.get_x(), alloc.get_y(), alloc.get_x() + alloc.get_width(), alloc.get_y() + alloc.get_height());
	
	int x, y;
	Gdk::ModifierType mask;

	get_window()->get_pointer (x, y, mask);

	if ((allow_horiz && ((x < scrolling_boundary.x0 && leftmost_frame > 0) || x >= scrolling_boundary.x1)) ||
	    (allow_vert && ((y < scrolling_boundary.y0 && vertical_adjustment.get_value() > 0)|| y >= scrolling_boundary.y1))) {
		start_canvas_autoscroll (allow_horiz, allow_vert, scrolling_boundary);
	}
}

void
Editor::start_autoscroll_for_headers ()
{
    if (autoscroll_active () ) {
		return;
	}
    
	Gtk::Allocation alloc = controls_layout.get_allocation ();
    ArdourCanvas::Rect scrolling_boundary = ArdourCanvas::Rect (alloc.get_x(), alloc.get_y(), alloc.get_x() + alloc.get_width(), alloc.get_y() + alloc.get_height());
    
    start_canvas_autoscroll (false/*horizontal disabled*/, true/*vertical enabled*/, scrolling_boundary);
}

bool
Editor::autoscroll_active () const
{
	return autoscroll_connection.connected ();
}

bool
Editor::autoscroll_canvas ()
{
	int x, y;
	Gdk::ModifierType mask;
	frameoffset_t dx = 0;
	bool no_stop = false;
	bool y_motion = false;

	get_window()->get_pointer (x, y, mask);

	VisualChange vc;
	bool vertical_motion = false;

	if (autoscroll_horizontal_allowed) {

		framepos_t new_frame = leftmost_frame;

		/* horizontal */

		if (x > autoscroll_boundary.x1) {

			/* bring it back into view */
			dx = x - autoscroll_boundary.x1;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			if (leftmost_frame < max_framepos - dx) {
				new_frame = leftmost_frame + dx;
			} else {
				new_frame = max_framepos;
			}

			no_stop = true;

		} else if (x < autoscroll_boundary.x0) {
			
			dx = autoscroll_boundary.x0 - x;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			if (leftmost_frame >= dx) {
				new_frame = leftmost_frame - dx;
			} else {
				new_frame = 0;
			}

			no_stop = true;
		}
		
		if (new_frame != leftmost_frame) {
			vc.time_origin = new_frame;
			vc.add (VisualChange::TimeOrigin);
		}
	}

	if (autoscroll_vertical_allowed) {
		
		// const double vertical_pos = vertical_adjustment.get_value();
		const int speed_factor = 10;

		/* vertical */ 
		
		if (y < autoscroll_boundary.y0) {

			/* scroll to make higher tracks visible */

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				y_motion = scroll_up_one_track ();
				vertical_motion = true;
			}

		} else if (y > autoscroll_boundary.y1) {

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				y_motion = scroll_down_one_track ();
				vertical_motion = true;
			}
		}

		no_stop = true;
	}

	if (vc.pending || vertical_motion) {

		/* change horizontal first */

		if (vc.pending) {
			visual_changer (vc);
		}

		/* now send a motion event to notify anyone who cares
		   that we have moved to a new location (because we scrolled)
		*/

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;
		
		/* the motion handler expects events in canvas coordinate space */

		/* we asked for the mouse position above (::get_pointer()) via
		 * our own top level window (we being the Editor). Convert into 
		 * coordinates within the canvas window.
		 */

		int cx;
		int cy;

		translate_coordinates (*_track_canvas, x, y, cx, cy);

		/* clamp x and y to remain within the autoscroll boundary,
		 * which is defined in window coordinates
		 */

		x = min (max ((ArdourCanvas::Coord) cx, autoscroll_boundary.x0), autoscroll_boundary.x1);
		y = min (max ((ArdourCanvas::Coord) cy, autoscroll_boundary.y0), autoscroll_boundary.y1);

		/* now convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		ArdourCanvas::Duple d = _track_canvas->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;

		motion_handler (0, (GdkEvent*) &ev, true);
		
	} else if (no_stop) {

		/* not changing visual state but pointer is outside the scrolling boundary
		 * so we still need to deliver a fake motion event 
		 */

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;
		
		/* the motion handler expects events in canvas coordinate space */

		/* first convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		int cx;
		int cy;

		/* clamp x and y to remain within the visible area. except
		 * .. if horizontal scrolling is allowed, always allow us to
		 * move back to zero
		 */

		if (autoscroll_horizontal_allowed) {
			x = min (max ((ArdourCanvas::Coord) x, 0.0), autoscroll_boundary.x1);
		} else {
			x = min (max ((ArdourCanvas::Coord) x, autoscroll_boundary.x0), autoscroll_boundary.x1);
		}
		y = min (max ((ArdourCanvas::Coord) y, autoscroll_boundary.y0), autoscroll_boundary.y1);

		translate_coordinates (*_track_canvas_viewport, x, y, cx, cy);

		ArdourCanvas::Duple d = _track_canvas->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;

		motion_handler (0, (GdkEvent*) &ev, true);
		
	} else {
		stop_canvas_autoscroll ();
		return false;
	}

	autoscroll_cnt++;

	return true; /* call me again */
}	

void
Editor::start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary)
{
	if (!_session) {
		return;
	}

	stop_canvas_autoscroll ();

	autoscroll_cnt = 0;
	autoscroll_horizontal_allowed = allow_horiz;
	autoscroll_vertical_allowed = allow_vert;
	autoscroll_boundary = boundary;

	/* do the first scroll right now
	*/

	autoscroll_canvas ();

	/* scroll again at very very roughly 30FPS */

	autoscroll_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::autoscroll_canvas), 30);
}

void
Editor::stop_canvas_autoscroll ()
{
	autoscroll_connection.disconnect ();
}

bool
Editor::left_track_canvas (GdkEventCrossing */*ev*/)
{
	DropDownKeys ();
	within_track_canvas = false;
	set_entered_track (0);
	set_entered_regionview (0);
	reset_canvas_action_sensitivity (false);
	return false;
}

bool
Editor::entered_track_canvas (GdkEventCrossing */*ev*/)
{
	within_track_canvas = true;
	reset_canvas_action_sensitivity (true);
	return FALSE;
}

void
Editor::ensure_time_axis_view_is_visible (TimeAxisView const & track, bool at_top)
{
	if (track.hidden()) {
		return;
	}

	/* compute visible area of trackview group, as offsets from top of
	 * trackview group.
	 */

	double const current_view_min_y = vertical_adjustment.get_value();
	double const current_view_max_y = current_view_min_y + vertical_adjustment.get_page_size();

	double const track_min_y = track.y_position ();
	double const track_max_y = track.y_position () + track.effective_height ();

	if (!at_top && 
	    (track_min_y >= current_view_min_y &&
	     track_max_y <= current_view_max_y)) {
		/* already visible, and caller did not ask to place it at the
		 * top of the track canvas
		 */
		return;
	}

	double new_value;

	if (at_top) {
		new_value = track_min_y;
	} else {
		if (track_min_y < current_view_min_y) {
			// Track is above the current view
			new_value = track_min_y;
		} else if (track_max_y > current_view_max_y) {
			// Track is below the current view
			new_value = track.y_position () + track.effective_height() - vertical_adjustment.get_page_size();
		} else {
			new_value = track_min_y;
		}
	}

	vertical_adjustment.set_value(new_value);
}

/** Called when the main vertical_adjustment has changed */
void
Editor::tie_vertical_scrolling ()
{
	if (pending_visual_change.idle_handler_id < 0) {
		_summary->set_overlays_dirty ();
	}
}

void
Editor::tie_horizontal_scrolling ()
{
    double p = horizontal_adjustment.get_value ();
      
    leftmost_frame = (framepos_t) floor (p * samples_per_pixel);
    
    update_fixed_rulers ();
    redisplay_tempo (true);
    
    if (pending_visual_change.idle_handler_id < 0) {
        _summary->set_overlays_dirty ();
    }
    
    update_video_timeline();
}

void
Editor::color_handler()
{
	ArdourCanvas::Color base = ARDOUR_UI::config()->get_canvasvar_RulerBase();
	ArdourCanvas::Color text = ARDOUR_UI::config()->get_canvasvar_RulerText();

	clock_ruler->set_fill_color (base);
	clock_ruler->set_outline_color (text);
	
	playhead_cursor->set_color (ARDOUR_UI::config()->get_canvasvar_PlayHead());

	meter_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_MeterBar());
	meter_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	tempo_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TempoBar());
	tempo_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_MarkerBar());
	marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	cd_marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_CDMarkerBar());
	cd_marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	range_marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_RangeMarkerBar());
	range_marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

        skip_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_SkipBar());
	skip_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	cd_marker_bar_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());
	cd_marker_bar_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());

	range_bar_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());
	range_bar_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());

	skip_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_SkipDragBarRect());
	skip_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_SkipDragBarRect());

	transport_bar_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportDragRect());
	transport_bar_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TransportDragRect());

	transport_loop_range_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportLoopRect());
	transport_loop_range_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TransportLoopRect());

	transport_punch_range_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportPunchRect());
	transport_punch_range_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TransportPunchRect());

	transport_punchin_line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_PunchLine());
	transport_punchout_line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_PunchLine());

	zoom_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_ZoomRect());
	zoom_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_ZoomRect());

	rubberband_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RubberBandRect());
	rubberband_rect->set_fill_color ((guint32) ARDOUR_UI::config()->get_canvasvar_RubberBandRect());

	refresh_location_display ();

        /* redraw the whole thing */
        _track_canvas->queue_draw ();
        
/*
	redisplay_tempo (true);

	if (_session)
	      _session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers
*/
}

double
Editor::horizontal_position () const
{
	return sample_to_pixel (leftmost_frame);
}

bool
Editor::track_canvas_key_press (GdkEventKey*)
{
	/* XXX: event does not report the modifier key pressed down, AFAICS, so use the Keyboard object instead */
	if (mouse_mode == Editing::MouseZoom && Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
		set_canvas_cursor (_cursors->zoom_out);
	}

	return false;
}

bool
Editor::track_canvas_key_release (GdkEventKey*)
{
	if (mouse_mode == Editing::MouseZoom && !Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
		set_canvas_cursor (_cursors->zoom_in);
	}

	return false;
}

double
Editor::clamp_verbose_cursor_x (double x)
{
	if (x < 0) {
		x = 0;
	} else {
		x = min (_visible_canvas_width - 200.0, x);
	}
	return x;
}

double
Editor::clamp_verbose_cursor_y (double y)
{
	y = max (0.0, y);
	y = min (_visible_canvas_height - 50, y);
	return y;
}

ArdourCanvas::GtkCanvasViewport*
Editor::get_track_canvas() const
{
	return _track_canvas_viewport;
}

void
Editor::set_canvas_cursor (Gdk::Cursor* cursor)
{
	if (!MouseCursors::is_invalid (cursor)) {
		_set_canvas_cursor (cursor ? cursor->gobj () : 0);
	}
}

void
Editor::_set_canvas_cursor (GdkCursor* gcursor)
{
	Glib::RefPtr<Gdk::Window> win = _track_canvas->get_window();
	
	if (win) {
		gdk_window_set_cursor(win->gobj (), gcursor);
	}
}

void
Editor::push_canvas_cursor (Gdk::Cursor* cursor)
{
	Glib::RefPtr<Gdk::Window> win = _track_canvas->get_window();

	if (win) {
		_cursor_stack.push (gdk_window_get_cursor (win->gobj ()));
		set_canvas_cursor (cursor);
	}
}

void
Editor::push_canvas_cursor ()
{
	Glib::RefPtr<Gdk::Window> win = _track_canvas->get_window();
	
	if (win) {
		_cursor_stack.push (gdk_window_get_cursor (win->gobj ()));
	}
}

void
Editor::pop_canvas_cursor ()
{
	if (!_cursor_stack.empty()) {
		GdkCursor* gcursor = _cursor_stack.top ();
        _cursor_stack.pop ();
        _set_canvas_cursor (gcursor);
	}
}

Gdk::Cursor*
Editor::which_grabber_cursor () const
{
	Gdk::Cursor* c = _cursors->grabber;

	if (_internal_editing) {
		switch (mouse_mode) {
		case MouseDraw:
			c = _cursors->midi_pencil;
			break;

		case MouseObject:
			break;

		case MouseTimeFX:
			c = _cursors->midi_resize;
			break;
			
		case MouseRange:
			break;

		default:
			break;
		}

	} else {

		switch (_edit_point) {
		case EditAtMouse:
			c = _cursors->grabber_edit_point;
			break;
		default:
			boost::shared_ptr<Movable> m = _movable.lock();
			if (m && m->locked()) {
				c = _cursors->speaker;
			}
			break;
		}
	}

	return c;
}

Gdk::Cursor*
Editor::which_trim_cursor (bool left) const
{
	if (!entered_regionview) {
		return 0;
	}

	Trimmable::CanTrim ct = entered_regionview->region()->can_trim ();
		
	if (left) {
		
		if (ct & Trimmable::FrontTrimEarlier) {
			return _cursors->left_side_trim;
		} else {
			return _cursors->left_side_trim_right_only;
		}
	} else {
		if (ct & Trimmable::EndTrimLater) {
			return _cursors->right_side_trim;
		} else {
			return _cursors->right_side_trim_left_only;
		}
	}
}

Gdk::Cursor*
Editor::which_mode_cursor () const
{
	Gdk::Cursor* mode_cursor = _cursors->invalid_cursor();

	switch (mouse_mode) {
	case MouseRange:
		mode_cursor = _cursors->selector;
		if (_internal_editing) {
			mode_cursor = which_grabber_cursor();
		}
		break;

	case MouseCut:
        if (entered_regionview) {
            mode_cursor = _cursors->scissors;
        } else {
            mode_cursor = which_grabber_cursor();
        }
        break;
            
	case MouseObject:
		/* don't use mode cursor, pick a grabber cursor based on the item */
		break;

	case MouseDraw:
		mode_cursor = _cursors->midi_pencil;
		break;

	case MouseGain:
		mode_cursor = _cursors->cross_hair;
		break;

	case MouseZoom:
		if (Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
			mode_cursor = _cursors->zoom_out;
		} else {
			mode_cursor = _cursors->zoom_in;
		}
		break;

	case MouseTimeFX:
		mode_cursor = _cursors->time_fx; // just use playhead
		break;

	case MouseAudition:
		mode_cursor = _cursors->speaker;
		break;
	}

	/* up-down cursor as a cue that automation can be dragged up and down when in join object/range mode */
	if (!_internal_editing && get_smart_mode() ) {

		double x, y;
		get_pointer_position (x, y);

		if (x >= 0 && y >= 0) {
			
			vector<ArdourCanvas::Item const *> items;

			/* Note how we choose a specific scroll group to get
			 * items from. This could be problematic.
			 */
			
			hv_scroll_group->add_items_at_point (ArdourCanvas::Duple (x,y), items);
			
			// first item will be the upper most 
			
			if (!items.empty()) {
				const ArdourCanvas::Item* i = items.front();
				
				if (i && i->parent() && i->parent()->get_data (X_("timeselection"))) {
					pair<TimeAxisView*, int> tvp = trackview_by_y_position (_last_motion_y);
					if (dynamic_cast<AutomationTimeAxisView*> (tvp.first)) {
						mode_cursor = _cursors->up_down;
					}
				}
			}
		}
	}

	return mode_cursor;
}

Gdk::Cursor*
Editor::which_track_cursor () const
{
	Gdk::Cursor* cursor = _cursors->invalid_cursor();

	assert (mouse_mode == MouseObject || get_smart_mode());

	if (!_internal_editing) {
		switch (_join_object_range_state) {
		case JOIN_OBJECT_RANGE_NONE:
		case JOIN_OBJECT_RANGE_OBJECT:
			cursor = which_grabber_cursor ();
			break;
		case JOIN_OBJECT_RANGE_RANGE:
			cursor = _cursors->selector;
			break;
		}
	}

	return cursor;
}

bool
Editor::reset_canvas_cursor ()
{
	if (!is_drawable()) {
		return false;
	}

	Gdk::Cursor* cursor = which_mode_cursor ();

	if (MouseCursors::is_invalid (cursor)) {
		cursor = which_grabber_cursor ();
	}
		
	if (!MouseCursors::is_invalid (cursor)) {
		set_canvas_cursor (cursor);
		return true;
	}

	return false;
}

void
Editor::choose_canvas_cursor_on_entry (GdkEventCrossing* /*event*/, ItemType type)
{
	Gdk::Cursor* cursor = _cursors->invalid_cursor ();

	if (_drags->active()) {
		return;
	}

	cursor = which_mode_cursor ();

	if (mouse_mode == MouseObject || get_smart_mode ()) {

		/* find correct cursor to use in object/smart mode */

		switch (type) {
		case RegionItem:
		case RegionViewNameHighlight:
		case RegionViewName:
		case WaveItem:
		case StreamItem:
		case AutomationTrackItem:
			cursor = which_track_cursor ();
			break;
		case PlayheadCursorItem:
			switch (_edit_point) {
			case EditAtMouse:
				cursor = _cursors->grabber_edit_point;
				break;
			default:
				cursor = _cursors->grabber;
				break;
			}
			break;
		case ControlPointItem:
			cursor = _cursors->fader;
			break;
		case GainLineItem:
			cursor = which_track_cursor ();
			break;
		case AutomationLineItem:
			cursor = _cursors->cross_hair;
			break;
		case FadeInItem:
			cursor = _cursors->fade_in;
			break;
		case FadeInHandleItem:
			cursor = _cursors->fade_in;
			break;
		case FadeInTrimHandleItem:
			cursor = _cursors->fade_in;
			break;
		case FadeOutItem:
			cursor = _cursors->fade_out;
			break;
		case FadeOutHandleItem:
			cursor = _cursors->fade_out;
			break;
		case FadeOutTrimHandleItem:
			cursor = _cursors->fade_out;
			break;
		case NoteItem:
			cursor = which_grabber_cursor();
			break;
		case FeatureLineItem:
			cursor = _cursors->cross_hair;
			break;
		case LeftFrameHandle:
			if ( effective_mouse_mode() == MouseObject )  // (smart mode): if the user is in the top half, override the trim cursor, since they are in the range zone
				cursor = which_trim_cursor (true);  //alternatively, one could argue that we _should_ allow trims here, and disallow range selection
			break;
		case RightFrameHandle:
			if ( effective_mouse_mode() == MouseObject )  //see above
				cursor = which_trim_cursor (false);
			break;
		case StartCrossFadeItem:
			cursor = _cursors->fade_in;
			break;
		case EndCrossFadeItem:
			cursor = _cursors->fade_out;
			break;
		case CrossfadeViewItem:
			cursor = _cursors->cross_hair;
			break;
                case SelectionItem:
                        cursor = which_grabber_cursor();
                        break;
		default:
			break;
		}
                
	} else if (mouse_mode == MouseGain) {
		
		/* ControlPointItem is not really specific to region gain mode
		   but it is the same cursor so don't worry about this for now.
		   The result is that we'll see the fader cursor if we enter
		   non-region-gain-line control points while in MouseGain
		   mode, even though we can't edit them in this mode.
		*/

		switch (type) {
		case GainLineItem:
		case ControlPointItem:
			cursor = _cursors->fader;
			break;
		default:
			break;
		}
        
        } else if (mouse_mode == MouseRange) {
        
                switch (type) {
                case SelectionItem:
                        cursor = _cursors->all_direction_move;
                        break;
                default:
                        break;
                }
        }

	switch (type) {
		/* These items use the timebar cursor at all times */
	case TimecodeRulerItem:
	case MinsecRulerItem:
	case BBTRulerItem:
	case SamplesRulerItem:
	case SkipBarItem:
		cursor = _cursors->timebar;
		break;

                /* These items use the grabber cursor at all times */
	case MeterMarkerItem:
	case TempoMarkerItem:
	case MeterBarItem:
	case TempoBarItem:
	case MarkerItem:
	case MarkerBarItem:
	case RangeMarkerBarItem:
	case CdMarkerBarItem:
	case VideoBarItem:
	case DropZoneItem:
    case ClockRulerItem:
    case PlayheadCursorItem:
                cursor = which_grabber_cursor();
                break;
	default:
		break;
	}

	if (!MouseCursors::is_invalid (cursor)) {
		set_canvas_cursor (cursor);
	}
}

double
Editor::trackviews_height() const
{
	if (!_trackview_group) {
		return 0;
	}

	return _visible_canvas_height - _trackview_group->canvas_origin().y;
}
