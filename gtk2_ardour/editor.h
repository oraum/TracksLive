/*
    Copyright (C) 2000-2003 Paul Davis

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

#ifndef __ardour_editor_h__
#define __ardour_editor_h__

#include <list>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <sys/time.h>
#include <cmath>

#include <boost/optional.hpp>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/layout.h>

#include "gtkmm2ext/selector.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/dndtreeview.h"
#include "gtkmm2ext/stateful_button.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/fader.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/import_status.h"
#include "ardour/tempo.h"
#include "ardour/location.h"
#include "ardour/types.h"

#include "canvas/fwd.h"
#include "canvas/ruler.h"

#include "ardour_button.h"
#include "ardour_dialog.h"
#include "ardour_dropdown.h"
#include "public_editor.h"
#include "editing.h"
#include "enums.h"
#include "editor_items.h"
#include "region_selection.h"
#include "compact_meter_bridge.h"
#include "mixer_bridge_view.h"
#include "waves_zoom_control.h"

namespace Gtkmm2ext {
	class TearOff;
	class Bindings;
}

namespace ARDOUR {
	class RouteGroup;
	class Playlist;
	class AudioPlaylist;
	class AudioRegion;
	class Region;
	class Location;
	class TempoSection;
	class Session;
	class Filter;
	class ChanCount;
	class MidiOperator;
	class Track;
	class MidiTrack;
	class AudioTrack;
}

namespace LADSPA {
	class Plugin;
}

class AnalysisWindow;
class AudioClock;
class AudioRegionView;
class AudioStreamView;
class AudioTimeAxisView;
class AutomationLine;
class AutomationSelection;
class AutomationTimeAxisView;
class BundleManager;
class ButtonJoiner;
class ControlPoint;
class DragManager;
class EditNoteDialog;
class EditorCursor;
class EditorGroupTabs;
class EditorLocations;
class EditorRegions;
class EditorRoutes;
class EditorRouteGroups;
class EditorSnapshots;
class EditorSummary;
class GroupedButtons;
class GUIObjectState;
class LocationListSelection;
class Marker;
class MarkerIODialog;
class MidiRegionView;
class MixerStrip;
class MouseCursors;
class NoteBase;
class PlaylistSelector;
class PluginSelector;
class ProgressReporter;
class RhythmFerret;
class RulerDialog;
class Selection;
class SoundFileOmega;
class StreamView;
class TempoLines;
class TimeAxisView;
class TimeFXDialog;
class TimeSelection;
class RegionLayeringOrderEditor;
class VerboseCursor;
class MasterBusUI;

class Editor : public PublicEditor, public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr
{
  public:
	Editor ();
	~Editor ();

	void	     set_session (ARDOUR::Session *);
    bool set_session_in_progress() const { return _set_session_in_progress; }
	ARDOUR::Session* session() const { return _session; }

    void restore_session_to_default ();
    
	void	     first_idle ();
	virtual bool     have_idled () const { return _have_idled; }

	framepos_t leftmost_sample() const { return leftmost_frame; }

	framecnt_t current_page_samples() const {
		return (framecnt_t) _visible_canvas_width * samples_per_pixel;
	}

	double visible_canvas_height () const {
		return _visible_canvas_height;
	}
	double trackviews_height() const;

	void cycle_snap_mode ();
	void next_snap_choice ();
	void next_snap_choice_music_only ();
	void next_snap_choice_music_and_time ();
	void prev_snap_choice ();
	void prev_snap_choice_music_only ();
	void prev_snap_choice_music_and_time ();
	void set_snap_to (Editing::SnapType);
	void set_snap_mode (Editing::SnapMode);
	void set_snap_threshold (double pixel_distance) {snap_threshold = pixel_distance;}

	Editing::SnapMode  snap_mode () const;
	Editing::SnapType  snap_type () const;

	void undo (uint32_t n = 1);
	void redo (uint32_t n = 1);
	
	//	UNDOable commands:
	
	void move_markers_command (std::list<Marker*>&, const std::list<ARDOUR::Location*>&);
	void toggle_location_skipping_command (Marker*);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void set_mouse_mode (Editing::MouseMode, bool force=true);
	void step_mouse_mode (bool next);
	Editing::MouseMode current_mouse_mode () const { return mouse_mode; }
	Editing::MidiEditMode current_midi_edit_mode () const;
	void remove_midi_note (ArdourCanvas::Item *, GdkEvent *);

	bool internal_editing() const { return _internal_editing ; }
	void set_internal_edit (bool yn);
	bool toggle_internal_editing_from_double_click (GdkEvent*);

	void foreach_time_axis_view (sigc::slot<void,TimeAxisView&>);
	void add_to_idle_resize (TimeAxisView*, int32_t);

	RouteTimeAxisView* get_route_view_by_route_id (const PBD::ID& id) const;

	void consider_auditioning (boost::shared_ptr<ARDOUR::Region>);
	void hide_a_region (boost::shared_ptr<ARDOUR::Region>);
	void show_a_region (boost::shared_ptr<ARDOUR::Region>);

#ifdef USE_RUBBERBAND
	std::vector<std::string> rb_opt_strings;
	int rb_current_opt;
#endif

	/* things that need to be public to be used in the main menubar */

	void new_region_from_selection ();
	void separate_regions_between (const TimeSelection&);
	void separate_region_from_selection ();
	void separate_under_selected_regions ();
	void separate_region_from_punch ();
	void separate_region_from_loop ();
	void separate_regions_using_location (ARDOUR::Location&);
	void transition_to_rolling (bool forward);
    /* this need to be public to be used on tracks remove */
    void update_edit_selection_menu ();

    void toggle_enable_group_edit ();
    void set_enable_group_edit (bool enabled);
    bool is_group_edit_enabled () {return _enable_group_edit;}
	/* NOTE: these functions assume that the "pixel" coordinate is
	   in canvas coordinates. These coordinates already take into
	   account any scrolling offsets.
	*/

	framepos_t pixel_to_sample_from_event (double pixel) const {

		/* pixel can be less than zero when motion events
		   are processed. since we've already run the world->canvas
		   affine, that means that the location *really* is "off
		   to the right" and thus really is "before the start".
		*/

		if (pixel >= 0) {
			return pixel * samples_per_pixel;
		} else {
			return 0;
		}
	}

	framepos_t pixel_to_sample (double pixel) const {
		return pixel * samples_per_pixel;
	}

	double sample_to_pixel (framepos_t sample) const {
		return round (sample / (double) samples_per_pixel);
	}

	double sample_to_pixel_unrounded (framepos_t sample) const {
		return sample / (double) samples_per_pixel;
	}

	/* selection */

	Selection& get_selection() const { return *selection; }
    void restore_selection (LocationListSelection*);
	Selection& get_cut_buffer() const { return *cut_buffer; }
	void track_mixer_selection ();

    MixerBridgeView& get_mixer_bridge () { return _mixer_bridge_view; }
    MixerBridgeView& get_meter_bridge () { return _meter_bridge_view; }
    
	bool extend_selection_to_track (TimeAxisView&);
    bool extend_time_selection_to_track (TimeAxisView&);

	void play_selection ();
	framepos_t get_preroll ();
	void maybe_locate_with_edit_preroll (framepos_t);
	void play_with_preroll ();
	void select_all_in_track (Selection::Operation op);
	void select_all_objects (Selection::Operation op);
    void select_all_regions ();
    void invert_region_selection ();
	void invert_selection_in_track ();
	void invert_selection ();
	void deselect_all ();
	long select_range (framepos_t, framepos_t);
    bool has_regionviews ();
    bool allow_invert_regionviews_selection ();

	void set_selected_regionview_from_region_list (boost::shared_ptr<ARDOUR::Region> region, Selection::Operation op = Selection::Set);

	/* tempo */

	void set_show_measures (bool yn);
	bool show_measures () const { return _show_measures; }

	/* analysis window */

	void analyze_region_selection();
	void analyze_range_selection();

	/* export */

	void export_audio ();
	void stem_export ();
	void export_selection ();
	void export_range ();
	void export_region ();

	void add_toplevel_controls (Gtk::Container&);

	void	       set_zoom_focus (Editing::ZoomFocus);
	Editing::ZoomFocus get_zoom_focus () const { return zoom_focus; }
	framecnt_t	 get_current_zoom () const { return samples_per_pixel; }
	void	       cycle_zoom_focus ();
	void temporal_zoom_step (bool coarser);
    void waveform_zoom_step (bool coarser);
	void ensure_time_axis_view_is_visible (TimeAxisView const & tav, bool at_top);
	void tav_zoom_step (bool coarser);
	void tav_zoom_smooth (bool coarser, bool force_all);

	/* stuff that AudioTimeAxisView and related classes use */

	PlaylistSelector& playlist_selector() const;
	void clear_playlist (boost::shared_ptr<ARDOUR::Playlist>);

	void new_playlists (TimeAxisView* v);
	void copy_playlists (TimeAxisView* v);
	void clear_playlists (TimeAxisView* v);

	void get_onscreen_tracks (TrackViewList&);

	void show_editor_mixer (bool yn);
	void create_editor_mixer ();
	void show_editor_list (bool yn);
	void set_selected_mixer_strip (TimeAxisView&);
	void hide_track_in_display (TimeAxisView* tv, bool apply_to_selection = false);

	/* nudge is initiated by transport controls owned by ARDOUR_UI */

	framecnt_t get_nudge_distance (framepos_t pos, framecnt_t& next);
	unsigned get_grid_beat_divisions(framepos_t position);
	Evoral::Beats get_grid_type_as_beats (bool& success, framepos_t position);

	void nudge_forward (bool next, bool force_playhead);
	void nudge_backward (bool next, bool force_playhead);

	/* nudge initiated from context menu */

	void nudge_forward_capture_offset ();
	void nudge_backward_capture_offset ();

	void sequence_regions ();

	/* playhead/screen stuff */

    void update_playhead_modes ();
    void toggle_playhead_mode (ARDOUR::AutoReturnTarget t);
    
	void set_stationary_playhead (bool yn);
	void toggle_stationary_playhead ();
	bool stationary_playhead() const { return _stationary_playhead; }

	void set_follow_playhead (bool yn, bool catch_up = true);
	void toggle_follow_playhead ();
	bool follow_playhead() const { return _follow_playhead; }
	bool dragging_playhead () const { return _dragging_playhead; }

    framepos_t get_playhead_position () const
    {
	if (_session) {
	    return _session->audible_frame();
	} else {
	    return 0;
	}
    }
    
	void toggle_zero_line_visibility ();
	void set_summary ();
	void set_group_tabs ();
	void toggle_measure_visibility ();
	void toggle_logo_visibility ();

	/* fades */

 	void toggle_region_fades (int dir);
 	void update_region_fade_visibility ();

	/* redirect shared ops menu. caller must free returned menu */

	Gtk::Menu* redirect_menu ();

	/* floating windows/transient */

	void ensure_float (Gtk::Window&);

	void show_window ();

	void scroll_tracks_down_line ();
	void scroll_tracks_up_line ();
	bool scroll_up_one_track ();
	bool scroll_down_one_track ();

	void prepare_for_cleanup ();
	void finish_cleanup ();

	void maximise_editing_space();
	void restore_editing_space();

	void update_tearoff_visibility();

	void reset_x_origin (framepos_t);
	void reset_x_origin_to_follow_playhead ();
	void reset_y_origin (double);
	void reset_zoom (framecnt_t);
	void reposition_and_zoom (framepos_t, double);

	framepos_t get_preferred_edit_position (bool ignore_playhead = false, bool use_context_click = false);

	bool update_mouse_speed ();
	bool decelerate_mouse_speed ();

	void toggle_meter_updating();

	void show_rhythm_ferret();

	void goto_visual_state (uint32_t);
	void save_visual_state (uint32_t);

	void queue_draw_resize_line (int at);
	void start_resize_line_ops ();
	void end_resize_line_ops ();

	TrackViewList const & get_track_views () {
		return track_views;
	}

	int get_regionview_count_from_region_list (boost::shared_ptr<ARDOUR::Region>);

	void do_import (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode, ARDOUR::SrcQuality, framepos_t&);
	void do_embed (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode,  framepos_t&);

	void get_regions_corresponding_to (boost::shared_ptr<ARDOUR::Region> region, std::vector<RegionView*>& regions, bool src_comparison);

	void center_screen (framepos_t);

	TrackViewList axis_views_from_routes (boost::shared_ptr<ARDOUR::RouteList>) const;

	Gtkmm2ext::TearOff* mouse_mode_tearoff () const { return _mouse_mode_tearoff; }
	Gtkmm2ext::TearOff* tools_tearoff () const { return _tools_tearoff; }

	void snap_to (framepos_t&       first,
	              ARDOUR::RoundMode direction = ARDOUR::RoundNearest,
	              bool              for_mark  = false);

	void snap_to_with_modifier (framepos_t&       first,
	                            GdkEvent const *  ev,
	                            ARDOUR::RoundMode direction = ARDOUR::RoundNearest,
	                            bool              for_mark  = false);

	void snap_to (framepos_t&       first,
	              framepos_t&       last,
	              ARDOUR::RoundMode direction = ARDOUR::RoundNearest,
	              bool              for_mark  = false);

	void begin_reversible_command (std::string cmd_name);
	void begin_reversible_command (GQuark);
	void commit_reversible_command ();

	DragManager* drags () const {
		return _drags;
	}

    void maybe_autoscroll (bool, bool, bool);
    void start_autoscroll_for_headers ();
    bool autoscroll_active() const;

	void set_canvas_cursor (Gdk::Cursor*);
	
	void push_canvas_cursor (Gdk::Cursor*);
	void push_canvas_cursor ();
	void pop_canvas_cursor ();

	void set_current_trimmable (boost::shared_ptr<ARDOUR::Trimmable>);
	void set_current_movable (boost::shared_ptr<ARDOUR::Movable>);

	MouseCursors const * cursors () const {
		return _cursors;
	}

	VerboseCursor* verbose_cursor () const {
		return _verbose_cursor;
	}

	double clamp_verbose_cursor_x (double);
	double clamp_verbose_cursor_y (double);

	void get_pointer_position (double &, double &) const;

	TimeAxisView* stepping_axis_view () {
		return _stepping_axis_view;
	}
	
	void set_stepping_axis_view (TimeAxisView* v) {
		_stepping_axis_view = v;
	}

 	ArdourCanvas::Container* get_trackview_group () const { return _trackview_group; }
	ArdourCanvas::Container* get_noscroll_group () const { return no_scroll_group; }
	ArdourCanvas::ScrollGroup* get_hscroll_group () const { return h_scroll_group; }
	ArdourCanvas::ScrollGroup* get_vscroll_group () const { return v_scroll_group; }
	ArdourCanvas::ScrollGroup* get_hvscroll_group () const { return hv_scroll_group; }

	ArdourCanvas::GtkCanvasViewport* get_track_canvas () const;

	bool inside_track_area (double x, double y) const;
	
	void override_visible_track_count ();

	/* Ruler metrics methods */

	void metric_get_timecode (std::vector<ArdourCanvas::Ruler::Mark>&, gdouble, gdouble, gint);
	void metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>&, gdouble, gdouble, gint);
	void metric_get_samples (std::vector<ArdourCanvas::Ruler::Mark>&, gdouble, gdouble, gint);
	void metric_get_minsec (std::vector<ArdourCanvas::Ruler::Mark>&, gdouble, gdouble, gint);

	/* editing operations that need to be public */
	void mouse_add_new_marker (framepos_t where, bool is_cd=false, bool is_xrun=false);
    void externally_remove_marker (ARDOUR::Location* loc);
	void split_regions_at (framepos_t, RegionSelection&);
	void split_region_at_points (boost::shared_ptr<ARDOUR::Region>, ARDOUR::AnalysisFeatureList&, bool can_ferret, bool select_new = false);
	RegionSelection get_regions_from_selection_and_mouse (framepos_t);
	
  protected:
	void map_transport_state ();
	void map_position_change (framepos_t);
	void on_realize();
	MasterBusUI* master_bus_ui () { return _master_bus_ui; }

	void suspend_route_redisplay ();
	void resume_route_redisplay ();
	void _set_canvas_cursor (GdkCursor*);

  private:

	void color_handler ();

	bool		 constructed;
    bool		 _set_session_in_progress;

	// to keep track of the playhead position for control_scroll
	boost::optional<framepos_t> _control_scroll_target;

	PlaylistSelector* _playlist_selector;

	typedef std::pair<TimeAxisView*,XMLNode*> TAVState;

	struct VisualState {
	    VisualState (bool with_tracks);
	    ~VisualState ();
	    double	      y_position;
	    framecnt_t	  samples_per_pixel;
	    framepos_t	  leftmost_frame;
	    Editing::ZoomFocus  zoom_focus;
	    GUIObjectState*     gui_state;
	};

	std::list<VisualState*> undo_visual_stack;
	std::list<VisualState*> redo_visual_stack;
	VisualState* current_visual_state (bool with_tracks = true);
	void undo_visual_state ();
	void redo_visual_state ();
	void use_visual_state (VisualState&);
	bool no_save_visual;
	void swap_visual_state ();

	std::vector<VisualState*> visual_states;
	void start_visual_state_op (uint32_t n);
	void cancel_visual_state_op (uint32_t n);

	framepos_t	 leftmost_frame;
	framecnt_t	 samples_per_pixel;
	Editing::ZoomFocus zoom_focus;

	void set_samples_per_pixel (framecnt_t);

	Editing::MouseMode mouse_mode;
	Editing::MouseMode pre_internal_mouse_mode;
	Editing::SnapType  pre_internal_snap_type;
	Editing::SnapMode  pre_internal_snap_mode;
	Editing::SnapType  internal_snap_type;
	Editing::SnapMode  internal_snap_mode;
	bool _internal_editing;
	Editing::MouseMode effective_mouse_mode () const;

	enum JoinObjectRangeState {
		JOIN_OBJECT_RANGE_NONE,
		/** `join object/range' mode is active and the mouse is over a place where object mode should happen */
		JOIN_OBJECT_RANGE_OBJECT,
		/** `join object/range' mode is active and the mouse is over a place where range mode should happen */
		JOIN_OBJECT_RANGE_RANGE
	};

	JoinObjectRangeState _join_object_range_state;

	void update_join_object_range_location (double);

	boost::optional<int>  pre_notebook_shrink_pane_width;

	void pane_allocation_handler (Gtk::Allocation&, Gtk::Paned*);

	Gtk::Notebook _the_notebook;
	bool _notebook_shrunk;
	void add_notebook_page (std::string const &, Gtk::Widget &);
	bool notebook_tab_clicked (GdkEventButton *, Gtk::Widget *);

	Gtk::HPaned&   edit_pane;
	Gtk::VPaned&   editor_summary_pane;

	void location_changed (ARDOUR::Location *);
	void location_flags_changed (ARDOUR::Location *);
	void refresh_location_display ();
	void refresh_location_display_internal (const ARDOUR::Locations::LocationList&);
	void add_new_location (ARDOUR::Location *);
	ArdourCanvas::Container* add_new_location_internal (ARDOUR::Location *);
	void location_gone (ARDOUR::Location *);
	void remove_selected_markers ();
	void remove_selected_range_markers ();
	void remove_marker (ArdourCanvas::Item&, GdkEvent*);
	gint really_remove_marker (ARDOUR::Location* loc);
    gint really_remove_selected_markers (Marker::Type);
	void goto_nth_marker (int nth);
	void toggle_marker_lines ();
	void set_marker_line_visibility (bool);

	struct LocationMarkers {
		Marker* start;
		Marker* end;
		bool    valid;

		LocationMarkers () : start(0), end(0), valid (true) {}
		
		~LocationMarkers ();

		void set_selected (bool);
		void canvas_height_set (double);
	};

	LocationMarkers  *find_location_markers (ARDOUR::Location *) const;
	ARDOUR::Location* find_location_from_marker (Marker *, bool& is_start) const;
	Marker* find_marker_from_location_id (PBD::ID const &, bool) const;
	Marker* entered_marker;
	bool _show_marker_lines;

	typedef std::map<ARDOUR::Location*,LocationMarkers *> LocationMarkerMap;
	LocationMarkerMap location_markers;

	void update_marker_labels ();
	void update_marker_labels (ArdourCanvas::Container *);
	void check_marker_label (Marker *);

	/** A set of lists of Markers that are in each of the canvas groups
	 *  for the marker sections at the top of the editor.  These lists
	 *  are kept sorted in time order between marker movements, so that after
	 *  a marker has moved we can decide whether we need to update the labels
	 *  for all markers or for just a few.
	 */
	std::map<ArdourCanvas::Container *, std::list<Marker *> > _sorted_marker_lists;
	void remove_sorted_marker (Marker *);

	void hide_marker (ArdourCanvas::Item*, GdkEvent*);
	void clear_marker_display ();
	void mouse_add_new_range (framepos_t);
	bool choose_new_marker_name(std::string &name);
	void update_cd_marker_display ();
	void ensure_cd_marker_updated (LocationMarkers * lam, ARDOUR::Location * location);

	TimeAxisView*      clicked_axisview;
	RouteTimeAxisView* clicked_routeview;
	/** The last RegionView that was clicked on, or 0 if the last click was not
	 * on a RegionView.  This is set up by the canvas event handlers in
	 * editor_canvas_events.cc
	 */
	RegionView*	clicked_regionview;
	RegionSelection    latest_regionviews;
	uint32_t	   clicked_selection;
	ControlPoint*      clicked_control_point;

	void sort_track_selection (TrackViewList&);

	void get_equivalent_regions (RegionView* rv, std::vector<RegionView*> &, PBD::PropertyID) const;
	RegionSelection get_equivalent_regions (RegionSelection &, PBD::PropertyID) const;
	void mapover_tracks (sigc::slot<void,RouteTimeAxisView&,uint32_t> sl, TimeAxisView*, PBD::PropertyID) const;
	void mapover_tracks_with_unique_playlists (sigc::slot<void,RouteTimeAxisView&,uint32_t> sl, TimeAxisView*, PBD::PropertyID) const;

	/* functions to be passed to mapover_tracks(), possibly with sigc::bind()-supplied arguments */
	void mapped_get_equivalent_regions (RouteTimeAxisView&, uint32_t, RegionView *, std::vector<RegionView*>*) const;
	void mapped_use_new_playlist (RouteTimeAxisView&, uint32_t, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void mapped_use_copy_playlist (RouteTimeAxisView&, uint32_t, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void mapped_clear_playlist (RouteTimeAxisView&, uint32_t);
	
	void button_selection (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type);
	bool button_release_can_deselect;

	void catch_vanishing_regionview (RegionView *);

	void set_selected_track (TimeAxisView&, Selection::Operation op = Selection::Set, bool no_remove=false);
	void select_all_tracks ();
	void select_all_internal_edit (Selection::Operation);

	bool set_selected_control_point_from_click (bool press, Selection::Operation op = Selection::Set);
	void set_selected_track_from_click (bool press, Selection::Operation op = Selection::Set, bool no_remove=false);
	void set_selected_track_as_side_effect (Selection::Operation op);
	bool set_selected_regionview_from_click (bool press, Selection::Operation op = Selection::Set);

	bool set_selected_regionview_from_map_event (GdkEventAny*, StreamView*, boost::weak_ptr<ARDOUR::Region>);
	void collect_new_region_view (RegionView *);
	void collect_and_select_new_region_view (RegionView *);

	Gtk::Menu track_context_menu;
	Gtk::Menu track_region_context_menu;
	Gtk::Menu track_selection_context_menu;

	Gtk::MenuItem* region_edit_menu_split_item;
	Gtk::MenuItem* region_edit_menu_split_multichannel_item;
	Gtk::Menu * track_region_edit_playlist_menu;
	Gtk::Menu * track_edit_playlist_submenu;
	Gtk::Menu * track_selection_edit_playlist_submenu;

	GdkEvent context_click_event;

	void popup_track_context_menu (int, int, ItemType, bool);
	Gtk::Menu* build_track_context_menu ();
	Gtk::Menu* build_track_bus_context_menu ();
	Gtk::Menu* build_track_region_context_menu ();
	Gtk::Menu* build_track_selection_context_menu ();
	void add_bus_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_selection_context_items (Gtk::Menu_Helpers::MenuList&);
    void add_region_context_items (Gtk::Menu_Helpers::MenuList&);
    void add_track_context_items (Gtk::Menu_Helpers::MenuList&);
    void activate_track_context_menu_action (Glib::RefPtr<Gtk::Action>& );
    
    Gtk::MenuItem* _popup_region_menu_item;

	void popup_control_point_context_menu (ArdourCanvas::Item *, GdkEvent *);
	Gtk::Menu _control_point_context_menu;

	void add_routes (ARDOUR::RouteList&);
	void timeaxisview_deleted (TimeAxisView *);

    Gtk::Container& inspector_home;
	Gtk::Container& _master_bus_ui_home;
	MasterBusUI*    _master_bus_ui;
	Gtk::VBox&      vpacker;
    WavesButton&    skip_button;

	std::stack <GdkCursor*> _cursor_stack;
	Gdk::Cursor* which_grabber_cursor () const;
	Gdk::Cursor* which_track_cursor () const;
	Gdk::Cursor* which_mode_cursor () const;
	Gdk::Cursor* which_trim_cursor (bool left_side) const;
	bool reset_canvas_cursor ();
	void choose_canvas_cursor_on_entry (GdkEventCrossing*, ItemType);

	ArdourCanvas::GtkCanvas* _track_canvas;
	ArdourCanvas::GtkCanvasViewport* _track_canvas_viewport;

	bool within_track_canvas;

	friend class VerboseCursor;
	VerboseCursor* _verbose_cursor;

	void parameter_changed (std::string);
	void ui_parameter_changed (std::string);

	ArdourCanvas::Pixbuf     *logo_item;
#if 0    
    /* these will be needed when we have canvas rulers */
	ArdourCanvas::Container      *minsec_group;
	ArdourCanvas::Container      *bbt_group;
	ArdourCanvas::Container      *timecode_group;
	ArdourCanvas::Container      *frame_group;
#endif

	ArdourCanvas::Container      *tempo_group;
	ArdourCanvas::Container      *meter_group;
	ArdourCanvas::Container      *ruler_group;
	ArdourCanvas::Container      *marker_group;
	ArdourCanvas::Container      *range_marker_group;
	ArdourCanvas::Container      *transport_marker_group;
	ArdourCanvas::Container      *skip_group;
	ArdourCanvas::Container*      cd_marker_group;

	/* parent for groups which themselves contain time markers */
	ArdourCanvas::Container*     _time_markers_group;

	/* The group containing all other groups that are scrolled vertically
	   and horizontally.
	*/
	ArdourCanvas::ScrollGroup* hv_scroll_group;

	/* The group containing all other groups that are scrolled vertically ONLY
	*/
	ArdourCanvas::ScrollGroup* v_scroll_group;

	/* The group containing all other groups that are scrolled horizontally ONLY
	*/
	ArdourCanvas::ScrollGroup* h_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* no_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* _trackview_group;

	/* The group holding things (mostly regions) while dragging so they
	 * are on top of everything else
	 */
	ArdourCanvas::Container* _drag_motion_group;

	/* a rect that sits at the bottom of all tracks to act as a drag-no-drop/clickable
	 * target area.
	 */
	ArdourCanvas::Rectangle* _canvas_drop_zone;
	bool canvas_drop_zone_event (GdkEvent* event);

	enum RulerType {
		ruler_metric_timecode = 0,
		ruler_metric_bbt = 1,
		ruler_metric_samples = 2,
		ruler_metric_minsec = 3,

		ruler_time_tempo = 4,
		ruler_time_meter = 5,
		ruler_time_marker = 6,
		ruler_time_range_marker = 7,
		ruler_time_punch_loop = 8,
		ruler_time_cd_marker = 9,
		ruler_video_timeline = 10,
		ruler_time_skip = 11,
	};

	Glib::RefPtr<Gtk::ToggleAction> ruler_timecode_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_bbt_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_samples_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_minsec_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_tempo_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_meter_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_marker_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_skip_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_range_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_loop_punch_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_cd_marker_action;
	bool			    no_ruler_shown_update;

	Gtk::Widget * ruler_grabbed_widget;

	RulerDialog* ruler_dialog;

	void initialize_rulers ();
	void update_just_timecode ();
	void compute_fixed_ruler_scale (); //calculates the RulerScale of the fixed rulers
	void update_fixed_rulers ();
	void update_tempo_based_rulers (ARDOUR::TempoMap::BBTPointList::const_iterator& begin,
					ARDOUR::TempoMap::BBTPointList::const_iterator& end);
	void popup_ruler_menu (framepos_t where = 0, ItemType type = RegionItem);
	void update_ruler_visibility ();
	void set_ruler_visible (RulerType, bool);
	void toggle_ruler_visibility (RulerType rt);
	void ruler_toggled (int);
	bool ruler_label_button_release (GdkEventButton*);
	void store_ruler_visibility ();
	void restore_ruler_visibility ();

	enum MinsecRulerScale {
		minsec_show_msecs,
		minsec_show_seconds,
		minsec_show_minutes,
		minsec_show_hours,
		minsec_show_many_hours
	};

	MinsecRulerScale minsec_ruler_scale;

	framecnt_t minsec_mark_interval;
	gint minsec_mark_modulo;
	gint minsec_nmarks;
	void set_minsec_ruler_scale (framepos_t, framepos_t);

	enum TimecodeRulerScale {
		timecode_show_bits,
		timecode_show_frames,
		timecode_show_seconds,
		timecode_show_minutes,
		timecode_show_hours,
		timecode_show_many_hours
	};

	TimecodeRulerScale timecode_ruler_scale;

	gint timecode_mark_modulo;
	gint timecode_nmarks;
	void set_timecode_ruler_scale (framepos_t, framepos_t);

	framecnt_t _samples_ruler_interval;
	void set_samples_ruler_scale (framepos_t, framepos_t);

	enum BBTRulerScale {
		bbt_show_many,
		bbt_show_64,
		bbt_show_16,
		bbt_show_4,
		bbt_show_1,
		bbt_show_beats,
		bbt_show_ticks,
		bbt_show_ticks_detail,
		bbt_show_ticks_super_detail
	};

	BBTRulerScale bbt_ruler_scale;

	uint32_t bbt_bars;
	gint bbt_nmarks;
	uint32_t bbt_bar_helper_on;
	uint32_t bbt_accent_modulo;
    void compute_bbt_ruler_scale (framepos_t lower, framepos_t upper,
				    ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_begin,
				    ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_end);

	ArdourCanvas::Ruler* clock_ruler;

	const double timebar_height;
	const double ruler_height;
	const double ruler_divide_height;
	const double marker_height;
	const double skipbar_height;
	
	Gtk::Menu *editor_ruler_menu;

	ArdourCanvas::Rectangle* tempo_bar;
	ArdourCanvas::Rectangle* meter_bar;
	ArdourCanvas::Rectangle* marker_bar;
	ArdourCanvas::Rectangle* range_marker_bar;
	ArdourCanvas::Rectangle* skip_bar;
	ArdourCanvas::Rectangle* cd_marker_bar;

	Gtk::Label  minsec_label;
	Gtk::Label  bbt_label;
	Gtk::Label  timecode_label;
	Gtk::Label  samples_label;
	Gtk::Label  tempo_label;
	Gtk::Label  meter_label;
	Gtk::Label  mark_label;
	Gtk::Label  range_mark_label;
	Gtk::Label  transport_mark_label;
	Gtk::Label  cd_mark_label;

	/* videtimline related actions */
	Gtk::Label		videotl_label;
	ArdourCanvas::Container*      videotl_group;
	Glib::RefPtr<Gtk::ToggleAction> ruler_video_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_proc_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_ontop_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_timecode_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_frame_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_osdbg_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_fullscreen_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_letterbox_action;
	Glib::RefPtr<Gtk::Action> xjadeo_zoom_100;
	void set_xjadeo_proc ();
	void toggle_xjadeo_proc (int state=-1);
	void set_close_video_sensitive (bool onoff);
	void set_xjadeo_sensitive (bool onoff);
	void set_xjadeo_viewoption (int);
	void toggle_xjadeo_viewoption (int what, int state=-1);
	void toggle_ruler_video (bool onoff) {ruler_video_action->set_active(onoff);}
	int videotl_bar_height; /* in units of timebar_height; default: 4 */
	int get_videotl_bar_height () const { return videotl_bar_height; }
	void export_video (bool range = false);
	void toggle_region_video_lock ();

	friend class EditorCursor;

	EditorCursor*	playhead_cursor;

	framepos_t get_region_boundary (framepos_t pos, int32_t dir, bool with_selection, bool only_onscreen);

	void    cursor_to_region_boundary (bool with_selection, int32_t dir);
	void    cursor_to_next_region_boundary (bool with_selection);
	void    cursor_to_previous_region_boundary (bool with_selection);
	void    cursor_to_next_region_point (EditorCursor*, ARDOUR::RegionPoint);
	void    cursor_to_previous_region_point (EditorCursor*, ARDOUR::RegionPoint);
	void    cursor_to_region_point (EditorCursor*, ARDOUR::RegionPoint, int32_t dir);
	void    cursor_to_selection_start (EditorCursor *);
	void    cursor_to_selection_end   (EditorCursor *);

	void    selected_marker_to_region_boundary (bool with_selection, int32_t dir);
	void    selected_marker_to_next_region_boundary (bool with_selection);
	void    selected_marker_to_previous_region_boundary (bool with_selection);
	void    selected_marker_to_next_region_point (ARDOUR::RegionPoint);
	void    selected_marker_to_previous_region_point (ARDOUR::RegionPoint);
	void    selected_marker_to_region_point (ARDOUR::RegionPoint, int32_t dir);
	void    selected_marker_to_selection_start ();
	void    selected_marker_to_selection_end   ();

	void    select_all_selectables_using_cursor (EditorCursor *, bool);
	void    select_all_selectables_using_edit (bool);
	void    select_all_selectables_between (bool within);
	void    select_range_between ();

	boost::shared_ptr<ARDOUR::Region> find_next_region (ARDOUR::framepos_t, ARDOUR::RegionPoint, int32_t dir, TrackViewList&, TimeAxisView ** = 0);
	ARDOUR::framepos_t find_next_region_boundary (ARDOUR::framepos_t, int32_t dir, const TrackViewList&);

	std::vector<ARDOUR::framepos_t> region_boundary_cache;
	void build_region_boundary_cache ();

	Gtk::Table&	  edit_packer;

	/** the adjustment that controls the overall editor vertical scroll position */
	Gtk::Adjustment&     vertical_adjustment;
    Gtk::Adjustment&     horizontal_adjustment;
    Gtk::Adjustment&     unused_adjustment; // yes, really; Gtk::Layout constructor requires refs
	Gtk::Layout&	 controls_layout;
	bool control_layout_scroll (GdkEventScroll* ev);
	void reset_controls_layout_width ();
	void reset_controls_layout_height (int32_t height);

	enum Direction {
		LEFT,
		RIGHT,
		UP,
		DOWN
	};

	bool scroll_press (Direction);
	void scroll_release ();
	sigc::connection _scroll_connection;
	int _scroll_callbacks;

	double _visible_canvas_width;
	double _visible_canvas_height; ///< height of the visible area of the track canvas
	double _full_canvas_height;    ///< full height of the canvas

	bool track_canvas_map_handler (GdkEventAny*);

	bool edit_controls_button_release (GdkEventButton*);
    bool edit_controls_button_press (GdkEventButton*);
	Gtk::Menu *edit_controls_left_menu;
	Gtk::Menu *edit_controls_right_menu;

	Gtk::VBox	   track_canvas_vbox;
	Gtk::VBox&	  edit_controls_vbox;
	Gtk::HBox	   edit_controls_hbox;

    void vertical_zoom_step_in ();
    void vertical_zoom_step_out ();
	void control_vertical_zoom_in_all ();
	void control_vertical_zoom_out_all ();
	void control_vertical_zoom_in_selected ();
	void control_vertical_zoom_out_selected ();
	void control_step_tracks_up ();
	void control_step_tracks_down ();
	void control_view (uint32_t);
	void control_scroll (float);
	void control_select (uint32_t rid, Selection::Operation);
	void control_unselect ();
	void access_action (std::string,std::string);
	bool deferred_control_scroll (framepos_t);
	sigc::connection control_scroll_connection;

	void tie_vertical_scrolling ();
    void tie_horizontal_scrolling ();
	double horizontal_position () const;

	struct VisualChange {
		enum Type {
			TimeOrigin = 0x1,
			ZoomLevel = 0x2,
			YOrigin = 0x4
		};

		Type       pending;
		framepos_t time_origin;
		framecnt_t samples_per_pixel;
		double     y_origin;

		int idle_handler_id;
		/** true if we are currently in the idle handler */
		bool being_handled;

		VisualChange() : pending ((VisualChange::Type) 0), time_origin (0), samples_per_pixel (0), idle_handler_id (-1), being_handled (false) {}
		void add (Type t) {
			pending = Type (pending | t);
		}
	};

	VisualChange pending_visual_change;

	static int _idle_visual_changer (void *arg);
	int idle_visual_changer ();
	void visual_changer (const VisualChange&);
	void ensure_visual_change_idle_handler ();

	/* track views */
	TrackViewList track_views;
	std::pair<TimeAxisView*, double> trackview_by_y_position (double, bool trackview_relative_offset = true) const;
	RouteTimeAxisView* axis_view_from_route (boost::shared_ptr<ARDOUR::Route>) const;

	TrackViewList get_tracks_for_range_action () const;

	sigc::connection super_rapid_screen_update_connection;
	framepos_t last_update_frame;
	void center_screen_internal (framepos_t, float);

	void super_rapid_screen_update ();

	void session_going_away ();

	framepos_t cut_buffer_start;
	framecnt_t cut_buffer_length;

	boost::weak_ptr<ARDOUR::Trimmable> _trimmable;
	boost::weak_ptr<ARDOUR::Movable> _movable;

	bool typed_event (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_1 (ArdourCanvas::Item *, GdkEvent *, ItemType);
	bool button_press_handler_2 (ArdourCanvas::Item *, GdkEvent *, ItemType);
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_double_click_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_dispatch (GdkEventButton*);
	bool button_release_dispatch (GdkEventButton*);
	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false, ItemType item = NoItem);
	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);

	Gtkmm2ext::Bindings* button_bindings;
	XMLNode* button_settings () const;

	/* KEYMAP HANDLING */

	void register_actions ();
	void register_region_actions ();

	void load_bindings ();
	Gtkmm2ext::ActionMap editor_action_map;
	Gtkmm2ext::Bindings  key_bindings;

	int ensure_cursor (framepos_t* pos);

	void cut_copy (Editing::CutCopyOp);
	bool can_cut_copy () const;
	void cut_copy_points (Editing::CutCopyOp);
	void cut_copy_regions (Editing::CutCopyOp, RegionSelection&);
	void cut_copy_ranges (Editing::CutCopyOp);
	void cut_copy_midi (Editing::CutCopyOp);

	void mouse_paste ();
	void paste_internal (framepos_t position, float times);

	/* EDITING OPERATIONS */

	void reset_point_selection ();
	void toggle_region_lock ();
	void toggle_opaque_region ();
	void toggle_record_enable ();
	void toggle_solo ();
	void toggle_solo_isolate ();
	void toggle_mute ();
	void toggle_region_lock_style ();

	enum LayerOperation {
		Raise,
		RaiseToTop,
		Lower,
		LowerToBottom
	};

	void do_layer_operation (LayerOperation);
	void raise_region ();
	void raise_region_to_top ();
	void change_region_layering_order (bool from_context_menu);
	void lower_region ();
	void lower_region_to_bottom ();
	void split_region_at_transients ();
	void crop_region_to_selection ();
	void crop_region_to (framepos_t start, framepos_t end);
	void set_sync_point (framepos_t, const RegionSelection&);
	void set_region_sync_position ();
	void remove_region_sync();
	void align_regions (ARDOUR::RegionPoint);
	void align_regions_relative (ARDOUR::RegionPoint point);
	void align_region (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, framepos_t position);
	void align_region_internal (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, framepos_t position);
	void remove_selected_regions ();
	void remove_clicked_region ();
	void show_midi_list_editor ();
	void rename_region ();
	void duplicate_some_regions (RegionSelection&, float times);
	void duplicate_selection (float times);
	void region_fill_selection ();
	void combine_regions ();
	void uncombine_regions ();

	void region_fill_track ();
	void audition_playlist_region_standalone (boost::shared_ptr<ARDOUR::Region>);
	void audition_playlist_region_via_route (boost::shared_ptr<ARDOUR::Region>, ARDOUR::Route&);
	void split_multichannel_region();
	void reverse_region ();
	void strip_region_silence ();
	void normalize_region ();
	void reset_region_scale_amplitude ();
	void adjust_region_gain (bool up);
	void quantize_region ();
	void quantize_regions (const RegionSelection& rs);
	void insert_patch_change (bool from_context);
	void fork_region ();

	void do_insert_time ();
	void insert_time (framepos_t, framecnt_t, Editing::InsertTimeOption, bool, bool, bool, bool, bool, bool);

	void tab_to_transient (bool forward);

	void set_tempo_from_region ();
	void use_range_as_bar ();

	void define_one_bar (framepos_t start, framepos_t end);

	void audition_region_from_region_list ();
	void hide_region_from_region_list ();
	void show_region_in_region_list ();

	void naturalize_region ();
    void regions_to_playhead ();

	void reset_focus ();

	void split_region ();
    void split_region_on_playhead ();

	void delete_ ();
	void cut ();
	void copy ();
	void paste (float times, bool from_context_menu = false);

	void place_transient ();
	void remove_transient (ArdourCanvas::Item* item);
	void snap_regions_to_grid ();
	void close_region_gaps ();

	void keyboard_paste ();

	void region_from_selection ();
	void create_region_from_selection (std::vector<boost::shared_ptr<ARDOUR::Region> >&);
    void cut_copy_region_from_selection (RegionSelection& new_regions, bool copy = false);
    
	void play_from_start ();
	void play_from_edit_point ();
	void play_from_edit_point_and_return ();
	void play_selected_region ();
	void play_edit_range ();
	void play_location (ARDOUR::Location&);
	void loop_location (ARDOUR::Location&);

	void calc_extra_zoom_edges(framepos_t &start, framepos_t &end);
	void temporal_zoom_selection (bool both_axes = false);
	void temporal_zoom_region (bool both_axes);
	void zoom_to_region (bool both_axes);
	void temporal_zoom_session ();
	void temporal_zoom (framecnt_t samples_per_pixel);
	void temporal_zoom_by_frame (framepos_t start, framepos_t end);
	void temporal_zoom_to_frame (bool coarser, framepos_t frame);
	void temporal_zoom_by_slider ();
	void wave_form_zoom ();
	void update_temporal_zoom_slider ();
    void update_waveform_color ();

	void insert_region_list_selection (float times);

	/* import & embed */

	void add_external_audio_action (Editing::ImportMode);
	void external_audio_dialog ();
	void session_import_dialog ();

	int  check_whether_and_how_to_import(std::string, bool all_or_nothing = true);
	bool check_multichannel_status (const std::vector<std::string>& paths);

	void bring_in_external_audio (Editing::ImportMode mode,  framepos_t& pos);

	bool  idle_drop_paths  (std::vector<std::string> paths, framepos_t frame, double ypos, bool copy);
	void  drop_paths_part_two  (const std::vector<std::string>& paths, framepos_t frame, double ypos, bool copy);

	int  import_sndfiles (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode,  
			      ARDOUR::SrcQuality, framepos_t& pos,
			      int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::Track>&, bool);
	int  embed_sndfiles (std::vector<std::string> paths, bool multiple_files, bool& check_sample_rate, 
			     Editing::ImportDisposition disposition, Editing::ImportMode mode,
			     framepos_t& pos, int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::Track>&);

	int add_sources (std::vector<std::string> paths, ARDOUR::SourceList& sources, framepos_t& pos, 
			 Editing::ImportDisposition, Editing::ImportMode,
			 int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::Track>&, bool add_channel_suffix);

	int finish_bringing_in_material (boost::shared_ptr<ARDOUR::Region> region, uint32_t, uint32_t,  framepos_t& pos, Editing::ImportMode mode,
					 boost::shared_ptr<ARDOUR::Track>& existing_track, const std::string& new_track_name);

	boost::shared_ptr<ARDOUR::AudioTrack> get_nth_selected_audio_track (int nth) const;
	boost::shared_ptr<ARDOUR::MidiTrack> get_nth_selected_midi_track (int nth) const;

	void toggle_midi_input_active (bool flip_others);

	ARDOUR::InterThreadInfo* current_interthread_info;

	AnalysisWindow* analysis_window;

	/* import specific info */

	struct EditorImportStatus : public ARDOUR::ImportStatus {
	    Editing::ImportMode mode;
	    framepos_t pos;
	    int target_tracks;
	    int target_regions;
	    boost::shared_ptr<ARDOUR::Track> track;
	    bool replace;
	};

	EditorImportStatus import_status;
	static void *_import_thread (void *);
	void* import_thread ();
	void finish_import ();

	/* to support this ... */

	void import_audio (bool as_tracks);
	void do_import (std::vector<std::string> paths, bool split, bool as_tracks);

	void move_to_start ();
	void move_to_end ();
	void center_playhead ();
	void center_edit_point ();
	void playhead_forward_to_grid ();
	void playhead_backward_to_grid ();
	void scroll_playhead (bool forward);
	void scroll_backward (float pages=0.8f);
	void scroll_forward (float pages=0.8f);
	void scroll_tracks_down ();
	void scroll_tracks_up ();
	void set_mark ();
	void clear_markers ();
	void clear_ranges ();
	void clear_locations ();
	void unhide_markers ();
	void unhide_ranges ();
	void jump_forward_to_mark ();
	void jump_backward_to_mark ();
	void cursor_align (bool playhead_to_edit);
	void toggle_skip_playback ();

	void remove_last_capture ();
	void select_all_selectables_using_time_selection ();
	void select_all_selectables_using_loop();
	void select_all_selectables_using_punch();
	void set_selection_from_range (ARDOUR::Location&);
	void set_selection_from_punch ();
	void set_selection_from_loop ();
	void set_selection_from_region ();

	void add_location_mark (framepos_t where);
	void add_location_from_region ();
	void add_locations_from_region ();
	void add_location_from_selection ();
	void set_loop_from_selection (bool play);
    void add_to_location_list ();
    void update_current_selection ();
	void set_punch_from_selection ();
	void set_punch_from_region ();

	void set_loop_from_edit_range (bool play);
	void set_loop_from_region (bool play);
	void set_punch_from_edit_range ();

	void set_loop_range (framepos_t start, framepos_t end, std::string cmd);
	void set_punch_range (framepos_t start, framepos_t end, std::string cmd);

	void add_location_from_playhead_cursor ();
	void remove_location_at_playhead_cursor ();
	bool select_new_marker;

	void reverse_selection ();
	void edit_envelope ();

	double last_scrub_x;
	int scrubbing_direction;
	int scrub_reversals;
	int scrub_reverse_distance;
	void scrub (framepos_t, double);

	void keyboard_selection_begin ();
	void keyboard_selection_finish (bool add);
	bool have_pending_keyboard_selection;
	framepos_t pending_keyboard_selection_start;

	void move_range_selection_start_or_end_to_region_boundary (bool, bool);

	Editing::SnapType _snap_type;
	Editing::SnapMode _snap_mode;

	/// Snap threshold in pixels
	double snap_threshold;

	bool ignore_gui_changes;

	DragManager* _drags;

	void escape ();
    
	void lock ();

	/* This dialog must NOT forward events */
	Gtk::Dialog *lock_dialog;

	struct timeval last_event_time;
	bool generic_event_handler (GdkEvent*);
	sigc::connection timeout_connection;
	bool lock_timeout_callback ();
	void start_lock_event_timing ();
    
	// Session auto save staff
	struct timeval _start_recording_time;
	sigc::connection _session_auto_save_timeout_connection;
	bool session_auto_save_timeout_callback();
	void start_session_auto_save_event_timing ();
    
	void on_ardour_ui_config_changed (const std::string&);

	Gtk::Menu fade_context_menu;

	Gtk::Menu xfade_in_context_menu;
	Gtk::Menu xfade_out_context_menu;
	void popup_xfade_in_context_menu (int, int, ArdourCanvas::Item*, ItemType);
	void popup_xfade_out_context_menu (int, int, ArdourCanvas::Item*, ItemType);
	void fill_xfade_menu (Gtk::Menu_Helpers::MenuList& items, bool start);

	void set_fade_in_shape (ARDOUR::FadeShape);
	void set_fade_out_shape (ARDOUR::FadeShape);

	void set_fade_length (bool in);
	void set_fade_in_active (bool);
	void set_fade_out_active (bool);

	void fade_range ();

	std::set<boost::shared_ptr<ARDOUR::Playlist> > motion_frozen_playlists;

	bool _dragging_playhead;
	bool _dragging_edit_point;

	void marker_drag_motion_callback (GdkEvent*);
	void marker_drag_finished_callback (GdkEvent*);

	gint mouse_rename_region (ArdourCanvas::Item*, GdkEvent*);

	void add_region_drag (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void start_create_region_grab (ArdourCanvas::Item*, GdkEvent*);
	void add_region_copy_drag (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void add_region_brush_drag (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void start_selection_grab (ArdourCanvas::Item*, GdkEvent*, bool copy = false);

	void region_view_item_click (AudioRegionView&, GdkEventButton*);

	bool can_remove_control_point (ArdourCanvas::Item *);
	void remove_control_point (ArdourCanvas::Item *);

	void mouse_brush_insert_region (RegionView*, framepos_t pos);

	/* Canvas event handlers */

	bool canvas_scroll_event (GdkEventScroll* event, bool from_canvas);
	bool canvas_control_point_event (GdkEvent* event,ArdourCanvas::Item*, ControlPoint*);
	bool canvas_line_event (GdkEvent* event,ArdourCanvas::Item*, AutomationLine*);
	bool canvas_selection_rect_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_selection_start_trim_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_selection_end_trim_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_start_xfade_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_end_xfade_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_in_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_in_handle_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*, bool trim = false);
	bool canvas_fade_out_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_out_handle_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*, bool trim = false);
	bool canvas_region_view_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_wave_view_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_frame_handle_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_feature_line_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*);
	bool canvas_stream_view_event (GdkEvent* event,ArdourCanvas::Item*, RouteTimeAxisView*);
	bool canvas_marker_event (GdkEvent* event,ArdourCanvas::Item*, Marker*);
	bool canvas_range_marker_handle_event (GdkEvent* event,ArdourCanvas::Item*, bool leftside);
	bool canvas_zoom_rect_event (GdkEvent* event,ArdourCanvas::Item*);
	bool canvas_tempo_marker_event (GdkEvent* event,ArdourCanvas::Item*, TempoMarker*);
	bool canvas_meter_marker_event (GdkEvent* event,ArdourCanvas::Item*, MeterMarker*);
	bool canvas_automation_track_event(GdkEvent* event, ArdourCanvas::Item*, AutomationTimeAxisView*);
	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item *);

	bool canvas_ruler_event (GdkEvent* event, ArdourCanvas::Item *, ItemType);
	bool canvas_tempo_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_meter_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_range_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_skip_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_cd_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);

	bool canvas_videotl_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	void update_video_timeline (bool flush = false);
	void set_video_timeline_height (const int);
	bool is_video_timeline_locked ();
	void toggle_video_timeline_locked ();
	void set_video_timeline_locked (const bool);
	void queue_visual_videotimeline_update ();
	void embed_audio_from_video (std::string, framepos_t n = 0, bool lock_position_to_video = true);

	PBD::Signal0<void> EditorFreeze;
	PBD::Signal0<void> EditorThaw;

  private:
	friend class DragManager;
	friend class EditorRouteGroups;
	friend class EditorRegions;

	/* non-public event handlers */

	bool canvas_playhead_cursor_event (GdkEvent* event, ArdourCanvas::Item*);
	bool track_canvas_scroll (GdkEventScroll* event);

	bool track_canvas_button_press_event (GdkEventButton* event);
	bool track_canvas_button_release_event (GdkEventButton* event);
	bool track_canvas_motion_notify_event (GdkEventMotion* event);

	Gtk::Allocation _canvas_viewport_allocation;
    void update_horizontal_adjustment_limits ();
	void track_canvas_viewport_allocate (Gtk::Allocation alloc);
	void track_canvas_viewport_size_allocated ();
	bool track_canvas_drag_motion (Glib::RefPtr<Gdk::DragContext> const &, int, int, guint);
	bool track_canvas_key_press (GdkEventKey *);
	bool track_canvas_key_release (GdkEventKey *);

	void set_playhead_cursor ();

	void toggle_region_mute ();

	void initialize_canvas ();

	/* display control */

	bool _show_measures;
	/// true if the editor should follow the playhead, otherwise false
	bool _follow_playhead;
	/// true if we scroll the tracks rather than the playhead
	bool _stationary_playhead;
	/// true if we are in fullscreen mode
	bool _maximised;
    /// true if we have group edit enabled
    bool _enable_group_edit;
    
	TempoLines* tempo_lines;

	ArdourCanvas::Container* global_rect_group;
	ArdourCanvas::Container* time_line_group;

	void hide_measures ();
	void draw_measures (ARDOUR::TempoMap::BBTPointList::const_iterator& begin,
			    ARDOUR::TempoMap::BBTPointList::const_iterator& end);

	void new_tempo_section ();

	void mouse_add_new_tempo_event (framepos_t where);
	void mouse_add_new_meter_event (framepos_t where);

	void remove_tempo_marker (ArdourCanvas::Item*);
	void remove_meter_marker (ArdourCanvas::Item*);
	gint real_remove_tempo_marker (ARDOUR::TempoSection*);
	gint real_remove_meter_marker (ARDOUR::MeterSection*);

	void edit_tempo_section (ARDOUR::TempoSection*);
	void edit_meter_section (ARDOUR::MeterSection*);
	void edit_tempo_marker (TempoMarker&);
	void edit_meter_marker (MeterMarker&);
	void edit_control_point (ArdourCanvas::Item*);
	void edit_notes (TimeAxisViewItem&);

	void marker_menu_edit ();
	void marker_menu_remove ();
	void range_marker_menu_remove ();
	void marker_menu_rename ();
	void rename_marker (Marker *marker);
	void finish_rename_marker (std::string, Marker *marker);
	void toggle_marker_menu_lock (ARDOUR::Location*, bool);
	void toggle_marker_menu_glue ();
	void marker_menu_hide ();
	void marker_menu_loop_range ();
	void marker_menu_select_all_selectables_using_range ();
	void marker_menu_select_using_range ();
	void marker_menu_separate_regions_using_location ();
	void marker_menu_play_from ();
	void marker_menu_play_range ();
	void marker_menu_set_playhead ();
	void marker_menu_set_from_playhead ();
	void marker_menu_set_from_selection (bool force_regions);
	void marker_menu_range_to_next ();
	void marker_menu_zoom_to_range ();
	void new_transport_marker_menu_set_loop ();
	void new_transport_marker_menu_set_punch ();
	void update_loop_range_view ();
	void update_punch_range_view ();
	void new_transport_marker_menu_popdown ();
	void marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void tempo_or_meter_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void new_transport_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void build_range_marker_menu (bool, bool);
	void build_marker_menu (ARDOUR::Location *);
	bool get_marker_menu_lock_items_locked (const ARDOUR::Location*, bool&);
	void build_tempo_or_meter_marker_menu (bool);
	void build_new_transport_marker_menu ();
	void dynamic_cast_marker_object (void*, MeterMarker**, TempoMarker**) const;

	Gtk::Menu* tempo_or_meter_marker_menu;
	Gtk::Menu* marker_menu;
	Gtk::Menu* range_marker_menu;
	Gtk::Menu* transport_marker_menu;
	Gtk::Menu* new_transport_marker_menu;
	Gtk::Menu* cd_marker_menu;
	ArdourCanvas::Item* marker_menu_item;

	typedef std::list<Marker*> Marks;
	Marks metric_marks;

	void remove_metric_marks ();
	void draw_metric_marks (const ARDOUR::Metrics& metrics);

	void compute_current_bbt_points (framepos_t left, framepos_t right, 
					 ARDOUR::TempoMap::BBTPointList::const_iterator& begin,
					 ARDOUR::TempoMap::BBTPointList::const_iterator& end);

	void tempo_map_changed (const PBD::PropertyChange&);
	void redisplay_tempo (bool immediate_redraw);

	uint32_t bbt_beat_subdivision;

	/* toolbar */

	Gtk::ToggleButton editor_mixer_button;
	Gtk::ToggleButton editor_list_button;
	void editor_mixer_button_toggled ();
	void editor_list_button_toggled ();

	AudioClock*	       zoom_range_clock;
	WavesZoomControl	  _waves_zoom_control;

	ArdourDropdown	    visible_tracks_selector;
	ArdourDropdown	    zoom_preset_selector;

    int32_t		   _visible_track_count;
    void build_track_count_menu ();
    void set_visible_track_count (int32_t);
    
    void set_zoom_preset(int64_t);

	Gtk::VBox		toolbar_clock_vbox;
	Gtk::VBox		toolbar_selection_clock_vbox;
	Gtk::Table	       toolbar_selection_clock_table;
	Gtk::Label	       toolbar_selection_cursor_label;

    MarkerIODialog* marker_io_dialog;
    WavesButton& add_marker_button;
    WavesButton& global_solo_button;
    void solo_blink (bool);
    void global_solo_clicked (WavesButton*);
    
    // Global record staff
    WavesButton& _global_rec_button;
    bool check_all_tracks_are_record_armed ();
    bool check_all_tracks_are_record_safe ();
    void connect_routes_and_update_global_rec_button(ARDOUR::RouteList& tracks);
    PBD::ScopedConnectionList _route_state_connections;
    
    void record_state_changed ();
    void global_rec_clicked (WavesButton*);

	Gtkmm2ext::TearOff* _mouse_mode_tearoff;
	WavesButton& _tool_marker_button;
	ArdourButton mouse_draw_button;
	WavesButton& _tool_arrow_button;
	ArdourButton mouse_gain_button;
	WavesButton& _tool_zoom_button;
	WavesButton& _tool_cut_button;
	Gtk::Adjustment& _temporal_zoom_adjustment;
	Gtk::Adjustment& _vertical_zoom_adjustment;
    Gtk::Adjustment& _waves_zoom_adjustment;
    Gtkmm2ext::Fader& _vertical_zoom_fader;

	ArdourButton mouse_timefx_button;
	ArdourButton mouse_audition_button;

	ArdourButton smart_mode_button;
	Glib::RefPtr<Gtk::ToggleAction> smart_mode_action;

	void		     mouse_mode_toggled (Editing::MouseMode m);
	void			 mouse_mode_object_range_toggled ();
	bool		     ignore_mouse_mode_toggle;

	ArdourButton internal_edit_button;
	void	 toggle_internal_editing ();

	bool		     mouse_select_button_release (GdkEventButton*);

	Gtk::VBox		automation_box;
	Gtk::Button	      automation_mode_button;

	//edit mode menu stuff
	ArdourDropdown	edit_mode_selector;
	void edit_mode_selection_done ( ARDOUR::EditMode m );
	void build_edit_mode_menu ();
	Gtk::VBox	 edit_mode_box;

	void set_edit_mode (ARDOUR::EditMode);
	void cycle_edit_mode ();

	ArdourDropdown snap_type_selector;
	void build_snap_type_menu ();

	ArdourDropdown snap_mode_selector;
	void build_snap_mode_menu ();
	Gtk::HBox	 snap_box;

	std::vector<std::string> snap_type_strings;
	std::vector<std::string> snap_mode_strings;

	void snap_type_selection_done (Editing::SnapType);
	void snap_mode_selection_done (Editing::SnapMode);
	void snap_mode_chosen (Editing::SnapMode);
	void snap_type_chosen (Editing::SnapType);

	Glib::RefPtr<Gtk::RadioAction> snap_type_action (Editing::SnapType);
	Glib::RefPtr<Gtk::RadioAction> snap_mode_action (Editing::SnapMode);

	//zoom focus meu stuff
	ArdourDropdown	zoom_focus_selector;
	void zoom_focus_selection_done ( Editing::ZoomFocus f );
	void build_zoom_focus_menu ();
	std::vector<std::string> zoom_focus_strings;

	void zoom_focus_chosen (Editing::ZoomFocus);

	Glib::RefPtr<Gtk::RadioAction> zoom_focus_action (Editing::ZoomFocus);

	Gtk::HBox	   _zoom_box;
	Gtkmm2ext::TearOff* _zoom_tearoff;
	void		zoom_adjustment_changed();

    bool vertical_fader_pressed(GdkEventButton* ev);
	void setup_toolbar ();

	void setup_tooltips ();

	Gtkmm2ext::TearOff*     _tools_tearoff;
	Gtk::HBox		toolbar_hbox;
	Gtk::EventBox	    toolbar_base;
	Gtk::Frame	       toolbar_frame;
	Gtk::Viewport	   _toolbar_viewport;

	/* midi toolbar */

	Gtk::HBox		panic_box;

	void setup_midi_toolbar ();

	/* selection process */

	Selection* selection;
	Selection* cut_buffer;

	void time_selection_changed ();
	void update_time_selection_display ();
	void track_selection_changed ();
    bool track_selected ();
    void output_connection_mode_changed ();
	void region_selection_changed ();
	sigc::connection editor_regions_selection_changed_connection;
	void sensitize_all_region_actions (bool);
	void sensitize_the_right_region_actions ();
	bool _all_region_actions_sensitized;
	/** Flag to block region action handlers from doing what they normally do;
	 *  I tried Gtk::Action::block_activate() but this doesn't work (ie it doesn't
	 *  block) when setting a ToggleAction's active state.
	 */
	bool _ignore_region_action;
	bool _last_region_menu_was_main;
	void point_selection_changed ();
	void marker_selection_changed ();

	bool _ignore_follow_edits;

	void cancel_selection ();
	void cancel_time_selection ();

	bool get_smart_mode() const;

	bool audio_region_selection_covers (framepos_t where);

	/* transport range select process */

	ArdourCanvas::Rectangle*  cd_marker_bar_drag_rect;
	ArdourCanvas::Rectangle*  range_bar_drag_rect;
	ArdourCanvas::Rectangle*  skip_drag_rect;
	ArdourCanvas::Rectangle*  transport_bar_drag_rect;

#ifdef GTKOSX
	ArdourCanvas::Rectangle     *bogus_background_rect;
#endif
	ArdourCanvas::Rectangle     *transport_bar_range_rect;
	ArdourCanvas::Rectangle     *transport_bar_preroll_rect;
	ArdourCanvas::Rectangle     *transport_bar_postroll_rect;
	ArdourCanvas::Rectangle     *transport_loop_range_rect;
	ArdourCanvas::Rectangle     *transport_punch_range_rect;
	ArdourCanvas::Line	  *transport_punchin_line;
	ArdourCanvas::Line	  *transport_punchout_line;
	ArdourCanvas::Rectangle     *transport_preroll_rect;
	ArdourCanvas::Rectangle     *transport_postroll_rect;

	ARDOUR::Location*  transport_loop_location();
	ARDOUR::Location*  transport_punch_location();

	ARDOUR::Location   *temp_location;

	/* object rubberband select process */

	void select_all_within (framepos_t, framepos_t, double, double, TrackViewList const &, Selection::Operation, bool);

	ArdourCanvas::Rectangle   *rubberband_rect;

	/* mouse zoom process */

	ArdourCanvas::Rectangle   *zoom_rect;
	void reposition_zoom_rect (framepos_t start, framepos_t end);

	EditorRouteGroups* _route_groups;
	EditorRoutes* _routes;
	EditorRegions* _regions;
	EditorSnapshots* _snapshots;
	EditorLocations* _locations;

	/* diskstream/route display management */
	Glib::RefPtr<Gdk::Pixbuf> rec_enabled_icon;
	Glib::RefPtr<Gdk::Pixbuf> rec_disabled_icon;

	Glib::RefPtr<Gtk::TreeSelection> route_display_selection;

	bool sync_track_view_list_and_routes ();

	Gtk::VBox	   list_vpacker;

	/* autoscrolling */

	sigc::connection autoscroll_connection;
	bool autoscroll_horizontal_allowed;
	bool autoscroll_vertical_allowed;
	uint32_t autoscroll_cnt;
	Gtk::Widget* autoscroll_widget;
	ArdourCanvas::Rect autoscroll_boundary;

	bool autoscroll_canvas ();
    void start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary);
	void stop_canvas_autoscroll ();

	/* trimming */
	void point_trim (GdkEvent *, framepos_t);

	void trim_region_front();
	void trim_region_back();
	void trim_region (bool front);

	void trim_region_to_loop ();
	void trim_region_to_punch ();
	void trim_region_to_location (const ARDOUR::Location&, const char* cmd);

	void trim_to_region(bool forward);
	void trim_region_to_previous_region_end();
	void trim_region_to_next_region_start();

	bool show_gain_after_trim;

	/* Drag-n-Drop */

	int convert_drop_to_paths (
		std::vector<std::string>&	   paths,
		const Glib::RefPtr<Gdk::DragContext>& context,
		gint				  x,
		gint				  y,
		const Gtk::SelectionData&	     data,
		guint				 info,
		guint				 time);

	void track_canvas_drag_data_received (
		const Glib::RefPtr<Gdk::DragContext>& context,
		gint				  x,
		gint				  y,
		const Gtk::SelectionData&	     data,
		guint				 info,
		guint				 time);

	void drop_paths (
		const Glib::RefPtr<Gdk::DragContext>& context,
		gint				  x,
		gint				  y,
		const Gtk::SelectionData&	     data,
		guint				 info,
		guint				 time);

	void drop_regions (
		const Glib::RefPtr<Gdk::DragContext>& context,
		gint				  x,
		gint				  y,
		const Gtk::SelectionData&	     data,
		guint				 info,
		guint				 time);

	void drop_routes (
		const Glib::RefPtr<Gdk::DragContext>& context,
		gint		x,
		gint		y,
		const Gtk::SelectionData& data,
		guint	       info,
		guint	       time);

	/* audio export */

	int  write_region_selection(RegionSelection&);
	bool write_region (std::string path, boost::shared_ptr<ARDOUR::AudioRegion>);
	void bounce_region_selection (bool with_processing);
	void bounce_range_selection (bool replace, bool enable_processing);
	void external_edit_region ();

	int write_audio_selection (TimeSelection&);
	bool write_audio_range (ARDOUR::AudioPlaylist&, const ARDOUR::ChanCount& channels, std::list<ARDOUR::AudioRange>&);

	void write_selection ();

	XMLNode *before; /* used in *_reversible_command */

	void update_title ();
	void update_title_s (const std::string & snapshot_name);

	void instant_save ();

	boost::shared_ptr<ARDOUR::AudioRegion> last_audition_region;

	/* freeze operations */

	ARDOUR::InterThreadInfo freeze_status;
	static void* _freeze_thread (void*);
	void* freeze_thread ();

	void freeze_route ();
	void unfreeze_route ();

	/* duplication */

	void duplicate_range (bool with_dialog);

	/** computes the timeline frame (sample) of an event whose coordinates
	 * are in canvas units (pixels, scroll offset included).
	 */
	framepos_t canvas_event_sample (GdkEvent const *, double* px = 0, double* py = 0) const;

	/** computes the timeline frame (sample) of an event whose coordinates
	 * are in window units (pixels, no scroll offset).
	 */
	framepos_t window_event_sample (GdkEvent const *, double* px = 0, double* py = 0) const;

	/* returns false if mouse pointer is not in track or marker canvas
	 */
	bool mouse_frame (framepos_t&, bool& in_track_canvas) const;

	TimeFXDialog* current_timefx;
	static void* timefx_thread (void *arg);
	void do_timefx ();

	int time_stretch (RegionSelection&, float fraction);
	int pitch_shift (RegionSelection&, float cents);
	void pitch_shift_region ();

	void transpose_region ();

	/* editor-mixer strip */

	MixerStrip *current_mixer_strip;
	CompactMeterbridge _compact_meter_bridge;
	MixerBridgeView _mixer_bridge_view;
	MixerBridgeView _meter_bridge_view;
	bool show_editor_mixer_when_tracks_arrive;
	void cms_new (boost::shared_ptr<ARDOUR::Route>);
	void current_mixer_strip_hidden ();

#ifdef GTKOSX
	void ensure_all_elements_drawn ();
#endif
	/* nudging tracks */

	void nudge_track (bool use_edit_point, bool forwards);

	static const int32_t default_width = 995;
	static const int32_t default_height = 765;

	/* nudge */

	Gtk::HBox	nudge_hbox;
	Gtk::VBox	nudge_vbox;
	AudioClock*       nudge_clock;

	bool nudge_forward_release (GdkEventButton*);
	bool nudge_backward_release (GdkEventButton*);

	/* audio filters */

	void apply_filter (ARDOUR::Filter&, std::string cmd, ProgressReporter* progress = 0);

	Command* apply_midi_note_edit_op_to_region (ARDOUR::MidiOperator& op, MidiRegionView& mrv);
	void apply_midi_note_edit_op (ARDOUR::MidiOperator& op, const RegionSelection&);

	/* handling cleanup */

	int playlist_deletion_dialog (boost::shared_ptr<ARDOUR::Playlist>);

	PBD::ScopedConnectionList session_connections;

	/* tracking step changes of track height */

	TimeAxisView* current_stepping_trackview;
	ARDOUR::microseconds_t last_track_height_step_timestamp;
	gint track_height_step_timeout();
	sigc::connection step_timeout;

	TimeAxisView* entered_track;
	/** If the mouse is over a RegionView or one of its child canvas items, this is set up
	    to point to the RegionView.  Otherwise it is 0.
	*/
	RegionView*   entered_regionview;

	bool clear_entered_track;
	bool left_track_canvas (GdkEventCrossing*);
	bool entered_track_canvas (GdkEventCrossing*);
	void set_entered_track (TimeAxisView*);
	void set_entered_regionview (RegionView*);
	gint left_automation_track ();

	void reset_canvas_action_sensitivity (bool);
	void set_gain_envelope_visibility ();
	void set_region_gain_visibility (RegionView*);
	void toggle_gain_envelope_active ();
	void reset_region_gain_envelopes ();

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

    void on_record_state_changed ();
    void on_transport_state_changed ();
    
	void session_state_saved (std::string);
    void set_session_dirty ();
    
	Glib::RefPtr<Gtk::Action>	      undo_action;
	Glib::RefPtr<Gtk::Action>	      redo_action;

	void history_changed ();

	Editing::EditPoint _edit_point;

	ArdourDropdown edit_point_selector;
	void build_edit_point_menu();

	void set_edit_point_preference (Editing::EditPoint ep, bool force = false);
	void cycle_edit_point (bool with_marker);
	void set_edit_point ();
	void edit_point_selection_done (Editing::EditPoint);
	void edit_point_chosen (Editing::EditPoint);
	Glib::RefPtr<Gtk::RadioAction> edit_point_action (Editing::EditPoint);
	std::vector<std::string> edit_point_strings;
	std::vector<std::string> edit_mode_strings;

	void selected_marker_moved (ARDOUR::Location*);

	bool get_edit_op_range (framepos_t& start, framepos_t& end) const;

	void get_regions_at (RegionSelection&, framepos_t where, const TrackViewList& ts) const;
	void get_regions_after (RegionSelection&, framepos_t where, const TrackViewList& ts) const;

	RegionSelection get_regions_from_selection_and_edit_point ();
    RegionSelection get_regions_from_selection_and_playhead ();
	RegionSelection get_regions_from_selection_and_entered ();

	void start_updating_meters ();
	void stop_updating_meters ();
	bool meters_running;

	void select_next_route ();
	void select_prev_route ();

	void snap_to_internal (framepos_t&       first,
	                       ARDOUR::RoundMode direction = ARDOUR::RoundNearest,
	                       bool              for_mark  = false);

	void timecode_snap_to_internal (framepos_t&       first,
	                                ARDOUR::RoundMode direction = ARDOUR::RoundNearest,
	                                bool              for_mark  = false);

	RhythmFerret* rhythm_ferret;

	void fit_tracks (TrackViewList &);
	void fit_selected_tracks ();
	void set_track_height (Height);
	void vertical_zoom_by_slider();

    void waveform_zoom_changed();
    
	void remove_tracks ();
	void toggle_tracks_active ();
    void set_track_header_dnd_active (bool yn);
    
    void set_record_and_playback_restricted_actions_sensitive (bool yn);
    
	bool _have_idled;
	int resize_idle_id;
	static gboolean _idle_resize (gpointer);
	bool idle_resize();
	int32_t _pending_resize_amount;
	TimeAxisView* _pending_resize_view;

	void visible_order_range (int*, int*) const;

	void located ();

	/** true if we've made a locate request that hasn't yet been processed */
	bool _pending_locate_request;

	/** if true, there is a pending Session locate which is the initial one when loading a session;
	    we need to know this so that we don't (necessarily) set the viewport to show the playhead
	    initially.
	*/
	bool _pending_initial_locate;

	Gtk::HBox _summary_hbox;
	EditorSummary* _summary;

	void region_view_added (RegionView *);
	void region_view_removed ();

	EditorGroupTabs* _group_tabs;
	void fit_route_group (ARDOUR::RouteGroup *);

	void step_edit_status_change (bool);
	void start_step_editing ();
	void stop_step_editing ();
	bool check_step_edit ();
	sigc::connection step_edit_connection;

	double _last_motion_y;

	RegionLayeringOrderEditor* layering_order_editor;
	void update_region_layering_order_editor ();

	/** Track that was the source for the last cut/copy operation.  Used as a place
	    to paste things iff there is no selected track.
	*/
	TrackViewList _last_cut_copy_source_tracks;

	/** true if a change in Selection->regions should change the selection in the region list.
	    See EditorRegions::selection_changed.
	*/
	bool _region_selection_change_updates_region_list;

	void setup_fade_images ();
	std::map<ARDOUR::FadeShape, Gtk::Image*> _fade_in_images;
	std::map<ARDOUR::FadeShape, Gtk::Image*> _fade_out_images;
	std::map<ARDOUR::FadeShape, Gtk::Image*> _xfade_in_images;
	std::map<ARDOUR::FadeShape, Gtk::Image*> _xfade_out_images;

	Gtk::MenuItem& action_menu_item (std::string const &);
	void action_pre_activated (Glib::RefPtr<Gtk::Action> const &);

	MouseCursors* _cursors;

	void follow_mixer_bridge_view_selection ();
	bool _following_mixer_bridge_view_selection;

	void follow_meter_bridge_selection ();
	bool _following_meter_bridge_selection;

	int time_fx (ARDOUR::RegionList&, float val, bool pitching);
	void note_edit_done (int, EditNoteDialog*);
	void toggle_sound_midi_notes ();

	/** Flag for a bit of a hack wrt control point selection; see set_selected_control_point_from_click */
	bool _control_point_toggled_on_press;

	/** This is used by TimeAxisView to keep a track of the TimeAxisView that is currently being
	    stepped in height using ScrollZoomVerticalModifier+Scrollwheel.  When a scroll event
	    occurs, we do the step on this _stepping_axis_view if it is non-0 (and we set up this
	    _stepping_axis_view with the TimeAxisView underneath the mouse if it is 0).  Then Editor
	    resets _stepping_axis_view when the modifier key is released.  In this (hacky) way,
	    pushing the modifier key and moving the scroll wheel will operate on the same track
	    until the key is released (rather than skipping about to whatever happens to be
	    underneath the mouse at the time).
	*/
	TimeAxisView* _stepping_axis_view;
	void zoom_vertical_modifier_released();

	void bring_in_callback (Gtk::Label*, uint32_t n, uint32_t total, std::string name);
	void update_bring_in_message (Gtk::Label* label, uint32_t n, uint32_t total, std::string name);
	void bring_all_sources_into_session ();
    
    void midi_input_connection_changed (const std::vector<std::string>&, bool);
    void midi_output_connection_changed (const std::vector<std::string>&, bool);
    
    void midi_input_configuration_changed ();
    void midi_output_configuration_changed ();
    void update_midi_dropdowns ();

    void port_registration_handler ();
    void port_connection_handler (boost::weak_ptr<ARDOUR::Port> wa, std::string, boost::weak_ptr<ARDOUR::Port> wb, std::string, bool connected);
    
    PBD::ScopedConnectionList port_state_connection_list;
    
    void update_progress_dialog_of_changing_tracks (bool);
    
    bool _zoom_tool_was_used;

	/* members and methods associated with MIDI + markers */

	WavesDropdown& _midi_input_dropdown;
	WavesDropdown& _midi_output_dropdown;
	void midi_input_chosen (WavesDropdown*, int);
	void midi_output_chosen (WavesDropdown*, int);
	void populate_midi_inout_dropdowns ();
	void populate_midi_inout_dropdown (bool playback);
    
    int64_t get_zoom_from_temporal_adjustment_value (double);

	Gtk::Widget& midi_marker_input_activity_image;
	Gtk::Widget& midi_marker_output_activity_image;
	Gtk::Widget& midi_marker_input_enabled_image;
	Gtk::Widget& midi_marker_input_disabled_image;
	Gtk::Widget& midi_marker_output_enabled_image;
	Gtk::Widget& midi_marker_output_disabled_image;

	void marker_midi_input_activity ();
	void marker_midi_output_activity ();
	bool reset_marker_midi_images (bool input);

	friend class Drag;
	friend class RegionDrag;
	friend class RegionMoveDrag;
	friend class RegionSpliceDrag;
	friend class RegionRippleDrag;
	friend class TrimDrag;
	friend class MeterMarkerDrag;
	friend class TempoMarkerDrag;
	friend class CursorDrag;
	friend class FadeInDrag;
	friend class FadeOutDrag;
	friend class MarkerDrag;
	friend class RegionGainDrag;
	friend class ControlPointDrag;
	friend class LineDrag;
	friend class RubberbandSelectDrag;
	friend class EditorRubberbandSelectDrag;
	friend class TimeFXDrag;
	friend class ScrubDrag;
	friend class SelectionDrag;
	friend class RangeMarkerBarDrag;
	friend class MarkerBarDrag;
	friend class MouseZoomDrag;
	friend class RegionCreateDrag;
	friend class RegionMotionDrag;
	friend class RegionInsertDrag;
	friend class VideoTimeLineDrag;

	friend class EditorSummary;
	friend class EditorGroupTabs;

	friend class EditorRoutes;
	friend class RhythmFerret;
};

#endif /* __ardour_editor_h__ */
