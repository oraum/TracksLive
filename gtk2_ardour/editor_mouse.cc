/*
    Copyright (C) 2000-2001 Paul Davis

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

#include <cassert>
#include <cstdlib>
#include <stdint.h>
#include <cmath>
#include <set>
#include <string>
#include <algorithm>
#include <bitset>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/basename.h"
#include "pbd/stateful_diff_command.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/tearoff.h"

#include "canvas/canvas.h"

#include "ardour/audioregion.h"
#include "ardour/operations.h"
#include "ardour/playlist.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "actions.h"
#include "editor.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "midi_region_view.h"
#include "marker.h"
#include "streamview.h"
#include "region_gain_line.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "prompter.h"
#include "selection.h"
#include "keyboard.h"
#include "editing.h"
#include "rgb_macros.h"
#include "control_point_dialog.h"
#include "editor_drag.h"
#include "automation_region_view.h"
#include "edit_note_dialog.h"
#include "mouse_cursors.h"
#include "editor_cursors.h"
#include "verbose_cursor.h"
#include "note.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;
using Gtkmm2ext::Keyboard;

bool
Editor::mouse_frame (framepos_t& where, bool& in_track_canvas) const
{
        /* gdk_window_get_pointer() has X11's XQueryPointer semantics in that it only
           pays attentions to subwindows. this means that menu windows are ignored, and 
           if the pointer is in a menu, the return window from the call will be the
           the regular subwindow *under* the menu.

           this matters quite a lot if the pointer is moving around in a menu that overlaps
           the track canvas because we will believe that we are within the track canvas
           when we are not. therefore, we track enter/leave events for the track canvas
           and allow that to override the result of gdk_window_get_pointer().
        */

        if (!within_track_canvas) {
                return false;
        }

	int x, y;
	Glib::RefPtr<Gdk::Window> canvas_window = const_cast<Editor*>(this)->_track_canvas->get_window();

	if (!canvas_window) {
		return false;
	}

	Glib::RefPtr<const Gdk::Window> pointer_window = Gdk::Display::get_default()->get_window_at_pointer (x, y);

	if (!pointer_window) {
		return false;
	}

	if (pointer_window != canvas_window) {
		in_track_canvas = false;
		return false;
	}

	in_track_canvas = true;

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = x;
	event.button.y = y;

	where = window_event_sample (&event, 0, 0);

	return true;
}

framepos_t
Editor::window_event_sample (GdkEvent const * event, double* pcx, double* pcy) const
{
	ArdourCanvas::Duple d;

	if (!gdk_event_get_coords (event, &d.x, &d.y)) {
		return 0;
	}

	/* event coordinates are in window units, so convert to canvas
	 */

	d = _track_canvas->window_to_canvas (d);

	if (pcx) {
		*pcx = d.x;
	}

	if (pcy) {
		*pcy = d.y;
	}

	return pixel_to_sample (d.x);
}

framepos_t
Editor::canvas_event_sample (GdkEvent const * event, double* pcx, double* pcy) const
{
	double x;
	double y;

	/* event coordinates are already in canvas units */

	if (!gdk_event_get_coords (event, &x, &y)) {
		cerr << "!NO c COORDS for event type " << event->type << endl;
		return 0;
	}

	if (pcx) {
		*pcx = x;
	}

	if (pcy) {
		*pcy = y;
	}

	/* note that pixel_to_sample_from_event() never returns less than zero, so even if the pixel
	   position is negative (as can be the case with motion events in particular),
	   the frame location is always positive.
	*/

	return pixel_to_sample_from_event (x);
}

void
Editor::set_current_trimmable (boost::shared_ptr<Trimmable> t)
{
	boost::shared_ptr<Trimmable> st = _trimmable.lock();

	if (!st || st == t) {
		_trimmable = t;
	}
}

void
Editor::set_current_movable (boost::shared_ptr<Movable> m)
{
	boost::shared_ptr<Movable> sm = _movable.lock();

	if (!sm || sm != m) {
		_movable = m;
	}
}

void
Editor::mouse_mode_object_range_toggled()
{
	MouseMode m = mouse_mode;
	
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-range"));
	assert (act);
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	set_mouse_mode(m, true);  //call this so the button styles can get updated
}

void
Editor::set_mouse_mode (MouseMode m, bool force)
{
	if (_drags->active ()) {
		return;
	}

	if (!force && m == mouse_mode) {
		return;
	}

	Glib::RefPtr<Action> act;

	switch (m) {
	case MouseRange:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-range"));
		break;

	case MouseCut:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-cut"));
		break;

	case MouseObject:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-object"));
		break;
	case MouseDraw:
//  Tracks Live doesn't use it
//		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-draw"));
//		break;
        return;

	case MouseGain:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-gain"));
		break;

	case MouseZoom:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-zoom"));
		break;

	case MouseTimeFX:
//  Tracks Live doesn't use it
//		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-timefx"));
//		break;
        return;

	case MouseAudition:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-audition"));
		break;
	}

	assert (act);

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	/* go there and back to ensure that the toggled handler is called to set up mouse_mode */
	tact->set_active (false);
	tact->set_active (true);

	//NOTE:  this will result in a call to mouse_mode_toggled which does the heavy lifting
}

void
Editor::mouse_mode_toggled (MouseMode m)
{
	Glib::RefPtr<Action> act;
	Glib::RefPtr<ToggleAction> tact;

	switch (m) {
	case MouseRange:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-range"));
		break;

	case MouseObject:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-object"));
		break;

	case MouseCut:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-cut"));
		break;

	case MouseDraw:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-draw"));
		break;

	case MouseGain:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-gain"));
		break;

	case MouseZoom:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-zoom"));
		break;

	case MouseTimeFX:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-timefx"));
		break;

	case MouseAudition:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-audition"));
		break;
	}

	assert (act);

	tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	if (!tact->get_active()) {
		/* this was just the notification that the old mode has been
		 * left. we'll get called again with the new mode active in a
		 * jiffy.
		 */
		return;
	}

	switch (m) {
	case MouseDraw:
		act = ActionManager::get_action (X_("MouseMode"), X_("toggle-internal-edit"));
		tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		tact->set_active (true);
		break;
	default:
		break;
	}
	
	if (_session && mouse_mode == MouseAudition) {
		/* stop transport and reset default speed to avoid oddness with
		   auditioning */
		_session->request_transport_speed (0.0, true);
	}

	mouse_mode = m;

	set_session_dirty ();

	/* this should generate a new enter event which will
	   trigger the appropiate cursor.
	*/

	if (_track_canvas) {
		_track_canvas->re_enter ();
	}

	set_gain_envelope_visibility ();
	
	//update_time_selection_display ();

	MouseModeChanged (); /* EMIT SIGNAL */
}

void
Editor::update_time_selection_display ()
{
	if (smart_mode_action->get_active()) {
		/* not sure what to do here */
		if (mouse_mode == MouseObject) {
		} else {
		}
	} else {
		switch (mouse_mode) {
		case MouseRange:
			selection->clear_objects ();
			break;
		default:
			selection->clear_time ();
			break;
		}
	}
}

void
Editor::step_mouse_mode (bool next)
{
	switch (current_mouse_mode()) {
	case MouseObject:
		if (next) {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseZoom);
			} else {
				set_mouse_mode (MouseRange);
			}
		} else {
			set_mouse_mode (MouseTimeFX);
		}
		break;

	case MouseRange:
		if (next) set_mouse_mode (MouseDraw);
		else set_mouse_mode (MouseCut);
		break;

	case MouseCut:
		if (next) set_mouse_mode (MouseRange);
		else set_mouse_mode (MouseDraw);
		break;

	case MouseDraw:
		if (next) set_mouse_mode (MouseCut);
		else set_mouse_mode (MouseRange);
		break;

	case MouseZoom:
		if (next) {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseTimeFX);
			} else {
				set_mouse_mode (MouseGain);
			}
		} else {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseObject);
			} else {
				set_mouse_mode (MouseDraw);
			}
		}
		break;

	case MouseGain:
		if (next) set_mouse_mode (MouseTimeFX);
		else set_mouse_mode (MouseZoom);
		break;

	case MouseTimeFX:
		if (next) {
			set_mouse_mode (MouseAudition);
		} else {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseZoom);
			} else {
				set_mouse_mode (MouseGain);
			}
		}
		break;

	case MouseAudition:
		if (next) set_mouse_mode (MouseObject);
		else set_mouse_mode (MouseTimeFX);
		break;
	}
}

bool
Editor::toggle_internal_editing_from_double_click (GdkEvent* event)
{
	if (_drags->active()) {
		_drags->end_grab (event);
	} 

	ActionManager::do_action ("MouseMode", "toggle-internal-edit");

	return true;
}

void
Editor::button_selection (ArdourCanvas::Item* /*item*/, GdkEvent* event, ItemType item_type)
{
 	/* in object/audition/timefx/gain-automation mode,
	   any button press sets the selection if the object
	   can be selected. this is a bit of hack, because
	   we want to avoid this if the mouse operation is a
	   region alignment.

	   note: not dbl-click or triple-click

	   Also note that there is no region selection in internal edit mode, otherwise
	   for operations operating on the selection (e.g. cut) it is not obvious whether
	   to cut notes or regions.
	*/

	MouseMode eff_mouse_mode = effective_mouse_mode ();

	if (eff_mouse_mode == MouseCut) {
		/* never change selection in cut mode */
		return;
	}

	if (get_smart_mode() && eff_mouse_mode == MouseRange && event->button.button == 3 && item_type == RegionItem) {
		/* context clicks are always about object properties, even if
		   we're in range mode within smart mode.
		*/
		eff_mouse_mode = MouseObject;
	}

	/* special case: allow drag of region fade in/out in object mode with join object/range enabled */
	if (get_smart_mode()) {
		switch (item_type) {
		  case FadeInHandleItem:
		  case FadeInTrimHandleItem:
		  case FadeOutHandleItem:
		  case FadeOutTrimHandleItem:
			  eff_mouse_mode = MouseObject;
			  break;
		default:
			break;
		}
	}

	if (((mouse_mode != MouseObject) &&
	     (mouse_mode != MouseAudition || item_type != RegionItem) &&
	     (mouse_mode != MouseTimeFX || item_type != RegionItem) &&
	     (mouse_mode != MouseGain) &&
	     (mouse_mode != MouseDraw)) ||
	    ((event->type != GDK_BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE) || event->button.button > 3) ||
	    (internal_editing() && mouse_mode != MouseTimeFX)) {

		return;
	}

	if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {

		if ((event->button.state & Keyboard::RelevantModifierKeyMask) && event->button.button != 1) {

			/* almost no selection action on modified button-2 or button-3 events */

			if (item_type != RegionItem && event->button.button != 2) {
				return;
			}
		}
	}

	Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);
	bool press = (event->type == GDK_BUTTON_PRESS);

	switch (item_type) {
	case RegionItem:
        if (event->button.button == 1) {
            if (press) {
                if (eff_mouse_mode != MouseRange) {
                    set_selected_regionview_from_click (press, op);
                } else {
                    /* don't change the selection unless the
                     clicked track is not currently selected. if
                     so, "collapse" the selection to just this
                     track
                     */
                    if (!selection->selected (clicked_axisview)) {
                        set_selected_track_as_side_effect (Selection::Set);
                    }
                }
            } else {
                if (eff_mouse_mode != MouseRange) {
                    set_selected_regionview_from_click (press, op);
                }
            }
        }
        break;
            
 	case RegionViewNameHighlight:
 	case RegionViewName:
	case LeftFrameHandle:
	case RightFrameHandle:
	case FadeInHandleItem:
	case FadeInTrimHandleItem:
	case FadeInItem:
	case FadeOutHandleItem:
	case FadeOutTrimHandleItem:
	case FadeOutItem:
	case StartCrossFadeItem:
	case EndCrossFadeItem:
            if (get_smart_mode() || eff_mouse_mode != MouseRange) {
                set_selected_regionview_from_click (press, op);
            } else if (event->type == GDK_BUTTON_PRESS) {
                set_selected_track_as_side_effect (op);
            }
        break;

	case ControlPointItem:
            set_selected_track_as_side_effect (op);
            if (eff_mouse_mode != MouseRange) {
                set_selected_control_point_from_click (press, op);
            }
        break;

	case StreamItem:
        break;

	case AutomationTrackItem:
            set_selected_track_as_side_effect (op);
		break;

	default:
		break;
	}
}

bool
Editor::button_press_handler_1 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	/* single mouse clicks on any of these item types operate
	   independent of mouse mode, mostly because they are
	   not on the main track canvas or because we want
	   them to be modeless.
	*/

	switch (item_type) {
	case PlayheadCursorItem:
		_drags->set (new CursorDrag (this, *playhead_cursor, true), event);
		return true;

	case MarkerItem:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
                        if (!Profile->get_trx()) {
                                hide_marker (item, event);
                        }
		} else {
                        Marker* m = reinterpret_cast<Marker*> (item->get_data ("marker"));


                        if (m) {
                                Location* l = m->location();
                                if (l && l->is_auto_loop()) {

	                                ArdourCanvas::Duple i;
	                                gdk_event_get_coords (event, &i.x, &i.y);
	                                i = clock_ruler->canvas_to_item (i);
	                                if (i.y >= ruler_divide_height) {
		                                /* lower half: drag/set playhead */
		                                _drags->set (new CursorDrag (this, *playhead_cursor, false), event);
		                                return true;
	                                } else {
		                                /* upper half - control loop playback */

		                                if (Config->get_loop_is_mode() && !_session->get_play_loop() ) {
			                                /* play loop is disabled, use drag to create new Loop range or toggle loop playback */
			                                _drags->set (new RangeMarkerBarDrag (this, clock_ruler, RangeMarkerBarDrag::CreateLoopMarker), event);
			                                return true;
		                                } else {
			                                _drags->set (new MarkerDrag (this, item, MarkerDrag::Move), event);
			                                return true;
		                                }
	                                }
                                }
                        }
                        _drags->set (new MarkerDrag (this, item, MarkerDrag::Move), event);
		}
		return true;

        case LeftDragHandle:
            _drags->set (new MarkerDrag (this, item, MarkerDrag::TrimLeft), event);
            return true;
            break;

        case RightDragHandle:
            _drags->set (new MarkerDrag (this, item, MarkerDrag::TrimRight), event);
            return true;
            break;

	case TempoMarkerItem:
	{
		TempoMarker* m = reinterpret_cast<TempoMarker*> (item->get_data ("marker"));
		assert (m);
		_drags->set (
			new TempoMarkerDrag (
				this,
				item,
				Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)
				),
			event
			);
		return true;
	}

	case MeterMarkerItem:
	{
		MeterMarker* m = reinterpret_cast<MeterMarker*> (item->get_data ("marker"));
		assert (m);
		_drags->set (
			new MeterMarkerDrag (
				this,
				item,
				Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)
				),
			event
			);
		return true;
	}

	case VideoBarItem:
		_drags->set (new VideoTimeLineDrag (this, item), event);
		return true;
		break;

	case MarkerBarItem:
                _drags->set (new MarkerBarDrag (this, item), event);
                return true;
                break;

    case SkipBarItem:
            _drags->set (new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateSkipMarker), event);
            return true;
            break;

	case ClockRulerItem: {
                ArdourCanvas::Duple i;
                gdk_event_get_coords (event, &i.x, &i.y);
                i = clock_ruler->canvas_to_item (i);
                if (i.y < ruler_divide_height) {
                        /* top half: create/edit loop range */
                        _drags->set (new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateLoopMarker), event);
                } else {
                        /* lower half: drag/set playhead */
                        _drags->set (new CursorDrag (this, *playhead_cursor, false), event);
                }
		return true;
        }       break;

	case RangeMarkerBarItem:
	case CdMarkerBarItem:
	case TempoBarItem:
	case MeterBarItem:
	case TimecodeRulerItem:
	case SamplesRulerItem:
	case MinsecRulerItem:
	case BBTRulerItem:
                /* these are not visible/do not exist in Tracks */
		return true;
		break;
	default:
		break;
	}

	if (_join_object_range_state == JOIN_OBJECT_RANGE_OBJECT) {
		/* special case: allow trim of range selections in joined object mode;
		   in theory eff should equal MouseRange in this case, but it doesn't
		   because entering the range selection canvas item results in entered_regionview
		   being set to 0, so update_join_object_range_location acts as if we aren't
		   over a region.
		*/
		if (item_type == StartSelectionTrimItem) {
			_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionStartTrim), event);
		} else if (item_type == EndSelectionTrimItem) {
			_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionEndTrim), event);
		}
	}

	Editing::MouseMode eff = effective_mouse_mode ();

	/* special case: allow drag of region fade in/out in object mode with join object/range enabled */
	if (get_smart_mode()) { 
		switch (item_type) {
		  case FadeInHandleItem:
		  case FadeInTrimHandleItem:
		  case FadeOutHandleItem:
		  case FadeOutTrimHandleItem:
			eff = MouseObject;
			break;
		default:
			break;
		}
	}

	/* there is no Range mode when in internal edit mode */
	if (eff == MouseRange && internal_editing()) {
		eff = MouseObject;
	}

	switch (eff) {
	case MouseRange:
		switch (item_type) {
		case StartSelectionTrimItem:
            if (Keyboard::modifier_state_equals (event->button.state, Keyboard::Keyboard::TertiaryModifier) ) {
                _drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionStartTrim), event);
            }
			break;

		case EndSelectionTrimItem:
            if (Keyboard::modifier_state_equals (event->button.state, Keyboard::Keyboard::TertiaryModifier) ) {
                _drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionEndTrim), event);
            }
			break;

		case SelectionItem:
                if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
                    /* grab selection for moving */
                    _drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionMove), event);
                } else {
                    double const y = event->button.y;
                    pair<TimeAxisView*, int> tvp = trackview_by_y_position (y, false );
                    
                    if (!tvp.first) {
                        break;
                    }
                    
                    if (get_smart_mode() ) {
                        AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (tvp.first);
                        if (atv) {
                            /* smart "join" mode: drag automation */
                            _drags->set (new AutomationRangeDrag (this, atv, selection->time), event, _cursors->up_down);
                        }
                    } else {
                        
                        bool copy = Keyboard::modifier_state_equals (event->button.state, GDK_MOD1_MASK);
                        
                        /* this was debated, but decided the more common action was to
                            separate-drag the selection. Well actually, Igor@Waves
                            decided this, so here it is.
                         */
                        start_selection_grab (item, event, copy);
                        return true;
                    }
                }
			break;

		case StreamItem:
			if (internal_editing()) {
				if (dynamic_cast<MidiTimeAxisView*> (clicked_axisview)) {
					_drags->set (new RegionCreateDrag (this, item, clicked_axisview), event);
					return true;
				} 
			} else {
				if (Keyboard::modifier_state_equals (event->button.state, Keyboard::RangeSelectModifier)) {
                    
                    // range extend must happen on mouse down
                    if (!selection->time.empty() ) {
                        
                        framepos_t pos = canvas_event_sample (event);
                        framepos_t start = min (pos, selection->time.start() );
                        framepos_t end = max (pos, selection->time.end_frame() );
                        
                        //also extend on tracks
                        extend_time_selection_to_track (*clicked_axisview);
                        selection->time.set_ignore_lls_change (true);
                        selection->set (start, end);
                        selection->time.set_ignore_lls_change (false);
                    }
				} else {
                    
                    if (!clicked_selection) {
                        selection->clear_time();
                        selection->clear_objects();
                    }
                    
					_drags->set (new SelectionDrag (this, item, SelectionDrag::CreateSelection), event);
				}
				return true;
			}
			break;

		case RegionViewNameHighlight:
			if (!clicked_regionview->region()->locked()) {
				_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
				return true;
			}
			break;

		default:
			if (!internal_editing()) {
				if (Keyboard::modifier_state_equals (event->button.state, Keyboard::RangeSelectModifier)) {
                    
                    // range extend must happen on mouse down
                    if (!selection->time.empty() ) {
                        
                        framepos_t pos = canvas_event_sample (event);
                        framepos_t start = min (pos, selection->time.start() );
                        framepos_t end = max (pos, selection->time.end_frame() );
                        
                        //also extend on tracks
                        extend_time_selection_to_track (*clicked_axisview);
                        selection->time.set_ignore_lls_change (true);
                        selection->set (start, end);
                        selection->time.set_ignore_lls_change (false);
                    }
				} else {
                    
                    if (!clicked_selection) {
                        selection->clear_time();
                        selection->clear_objects();
                    }
                    
					_drags->set (new SelectionDrag (this, item, SelectionDrag::CreateSelection), event);
				}
			}
		}
		return true;
		break;

	case MouseDraw:
		switch (item_type) {
		case NoteItem:
			/* Existing note: allow trimming/motion */
			if (internal_editing()) {
				/* trim notes if we're in internal edit mode and near the ends of the note */
				NoteBase* cn = reinterpret_cast<NoteBase*>(item->get_data ("notebase"));
				assert (cn);
				if (cn->big_enough_to_trim() && cn->mouse_near_ends()) {
					_drags->set (new NoteResizeDrag (this, item), event, _cursors->invalid_cursor ());
				} else {
					_drags->set (new NoteDrag (this, item), event);
				}
				return true;
			} 
			break;
		case StreamItem:
			if (internal_editing()) {
				if (dynamic_cast<MidiTimeAxisView*> (clicked_axisview)) {
					_drags->set (new RegionCreateDrag (this, item, clicked_axisview), event);
				}
				return true;
			}
			break;

		default:
			break;
		}
		break;

	case MouseCut:
		switch (item_type) {
		case RegionItem:
		case FadeInHandleItem:
		case FadeOutHandleItem:
		case RegionViewNameHighlight:
		case RegionViewName:
            if (entered_regionview) {
                _drags->set (new RegionCutDrag (this, item, canvas_event_sample (event)), event, _cursors->invalid_cursor ());
            }
			return true;
			break;
        case StreamItem:
                
            if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier) &&
                !Keyboard::modifier_state_contains (event->button.state, Keyboard::TertiaryModifier) ) {
                
                selection->clear_regions();
                selection->clear_time ();
                selection->clear_points ();
                selection->clear_lines ();
            }
            
            _drags->set (new EditorRubberbandSelectDrag (this, item), event);

            break;
    case LeftFrameHandle:
    case RightFrameHandle:
    case FeatureLineItem:
    case AutomationTrackItem:
		default:
			break;
		}
		break;

	case MouseObject:
		switch (item_type) {
		case NoteItem:
			/* Existing note: allow trimming/motion */
			if (internal_editing()) {
				NoteBase* cn = reinterpret_cast<NoteBase*> (item->get_data ("notebase"));
				assert (cn);
				if (cn->big_enough_to_trim() && cn->mouse_near_ends()) {
					_drags->set (new NoteResizeDrag (this, item), event, _cursors->invalid_cursor ());
				} else {
					_drags->set (new NoteDrag (this, item), event);
				}
				return true;
			}
			break;
        case SelectionItem:
            if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
                /* grab selection for moving */
                _drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionMove), event);
            } else {
                double const y = event->button.y;
                pair<TimeAxisView*, int> tvp = trackview_by_y_position (y, false );
                
                if (tvp.first) {
                    bool copy = Keyboard::modifier_state_equals (event->button.state, GDK_MOD1_MASK);
                    
                    /* this was debated, but decided the more common action was to
                     separate-drag the selection. Well actually, Igor@Waves
                     decided this, so here it is.
                     */
                    start_selection_grab (item, event, copy);
                    return true;
                }
            }
            break;
		default:
			break;
		}

		if (Keyboard::modifier_state_contains (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) &&
		    event->type == GDK_BUTTON_PRESS) {

			_drags->set (new EditorRubberbandSelectDrag (this, item), event);

		} else if (event->type == GDK_BUTTON_PRESS) {

			switch (item_type) {
			case FadeInHandleItem:
			{
				_drags->set (new FadeInDrag (this, item, reinterpret_cast<RegionView*> (item->get_data("regionview")), selection->regions), event, _cursors->fade_in);
				return true;
			}

			case FadeOutHandleItem:
			{
				_drags->set (new FadeOutDrag (this, item, reinterpret_cast<RegionView*> (item->get_data("regionview")), selection->regions), event, _cursors->fade_out);
				return true;
			}

			case StartCrossFadeItem:
			case EndCrossFadeItem:
				/* we might allow user to grab inside the fade to trim a region with preserve_fade_anchor.  for not this is not fully implemented */ 
//				if (!clicked_regionview->region()->locked()) {
//					_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer(), true), event);
//					return true;
//				}
				break;

			case FeatureLineItem:
			{
				if (Keyboard::modifier_state_contains (event->button.state, Keyboard::TertiaryModifier)) {
					remove_transient(item);
					return true;
				}

				_drags->set (new FeatureLineDrag (this, item), event);
				return true;
				break;
			}

			case RegionItem:
				if (dynamic_cast<AutomationRegionView*> (clicked_regionview)) {
					/* click on an automation region view; do nothing here and let the ARV's signal handler
					   sort it out.
					*/
					break;
				}

				if (internal_editing ()) {
					break;
				}

				/* click on a normal region view */
				if (Keyboard::modifier_state_contains (event->button.state, GDK_MOD1_MASK)) {
					add_region_copy_drag (item, event, clicked_regionview);
				} else if (Keyboard::the_keyboard().key_is_down (GDK_b)) {
					add_region_brush_drag (item, event, clicked_regionview);
				} else {
					add_region_drag (item, event, clicked_regionview);
				}


//				if (!internal_editing() && (_join_object_range_state == JOIN_OBJECT_RANGE_RANGE && !selection->regions.empty())) {
//					_drags->add (new SelectionDrag (this, clicked_axisview->get_selection_rect (clicked_selection)->rect, SelectionDrag::SelectionMove));
//				}

				_drags->start_grab (event);
				return true;
				break;

			case RegionViewNameHighlight:
			case LeftFrameHandle:
			case RightFrameHandle:
				if (!clicked_regionview->region()->locked()) {
					_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
					return true;
				}
				break;

			case FadeInTrimHandleItem:
			case FadeOutTrimHandleItem:
				if (!clicked_regionview->region()->locked()) {
					_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer(), true), event);
					return true;
				}
				break;

			case RegionViewName:
			{
				/* rename happens on edit clicks */
				if (clicked_regionview->get_name_highlight()) {
					_drags->set (new TrimDrag (this, clicked_regionview->get_name_highlight(), clicked_regionview, selection->regions.by_layer()), event);
					return true;
				}
				break;
			}

			case ControlPointItem:
				_drags->set (new ControlPointDrag (this, item), event);
				return true;
				break;

			case AutomationLineItem:
				_drags->set (new LineDrag (this, item), event);
				return true;
				break;

			case StreamItem:
				if (internal_editing()) {
					if (dynamic_cast<MidiTimeAxisView*> (clicked_axisview)) {
						_drags->set (new RegionCreateDrag (this, item, clicked_axisview), event);
					}
					return true;
				} else {
                    
                    if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier) &&
                        !Keyboard::modifier_state_contains (event->button.state, Keyboard::TertiaryModifier) ) {
                    
                        selection->clear_regions();
                        selection->clear_time ();
                        selection->clear_points ();
                        selection->clear_lines ();
                    }
                        
					_drags->set (new EditorRubberbandSelectDrag (this, item), event);
				}
				break;

			case AutomationTrackItem:
			{
				TimeAxisView* parent = clicked_axisview->get_parent ();
				AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (clicked_axisview);
				assert (atv);
				if (parent && dynamic_cast<MidiTimeAxisView*> (parent) && atv->show_regions ()) {

					RouteTimeAxisView* p = dynamic_cast<RouteTimeAxisView*> (parent);
					assert (p);
					boost::shared_ptr<Playlist> pl = p->track()->playlist ();
					if (pl->n_regions() == 0) {
						/* Parent has no regions; create one so that we have somewhere to put automation */
						_drags->set (new RegionCreateDrag (this, item, parent), event);
					} else {
						/* See if there's a region before the click that we can extend, and extend it if so */
						framepos_t const t = canvas_event_sample (event);
						boost::shared_ptr<Region> prev = pl->find_next_region (t, End, -1);
						if (!prev) {
							_drags->set (new RegionCreateDrag (this, item, parent), event);
						} else {
							prev->set_length (t - prev->position ());
						}
					}
				} else {
					/* rubberband drag to select automation points */
					_drags->set (new EditorRubberbandSelectDrag (this, item), event);
				}
				break;
			}

			case SelectionItem:
			{
				if ( get_smart_mode() ) {
					/* we're in "smart" joined mode, and we've clicked on a Selection */
					double const y = event->button.y;
					pair<TimeAxisView*, int> tvp = trackview_by_y_position (y);
					if (tvp.first) {
						/* if we're over an automation track, start a drag of its data */
						AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (tvp.first);
						if (atv) {
							_drags->set (new AutomationRangeDrag (this, atv, selection->time), event, _cursors->up_down);
						}

						/* if we're over a track and a region, and in the `object' part of a region,
						   put a selection around the region and drag both
						*/
/*						RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tvp.first);
						if (rtv && _join_object_range_state == JOIN_OBJECT_RANGE_OBJECT) {
							boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (rtv->route ());
							if (t) {
								boost::shared_ptr<Playlist> pl = t->playlist ();
								if (pl) {

									boost::shared_ptr<Region> r = pl->top_region_at (canvas_event_sample (event));
									if (r) {
										RegionView* rv = rtv->view()->find_view (r);
										clicked_selection = select_range (rv->region()->position(), 
														  rv->region()->last_frame()+1);
										_drags->add (new SelectionDrag (this, item, SelectionDrag::SelectionMove));
										list<RegionView*> rvs;
										rvs.push_back (rv);
										_drags->add (new RegionMoveDrag (this, item, rv, rvs, false, false));
										_drags->start_grab (event);
										return true;
									}
								}
							}
						}
*/
					}
				}
				break;
			}

			case MarkerBarItem:

				break;

			default:
				break;
			}
		}
		return true;
		break;

	case MouseGain:
		switch (item_type) {
		case GainLineItem:
			_drags->set (new LineDrag (this, item), event);
			return true;

		case ControlPointItem:
			_drags->set (new ControlPointDrag (this, item), event);
			return true;
			break;

		case SelectionItem:
		{
			AudioRegionView* arv = dynamic_cast<AudioRegionView *> (clicked_regionview);
			if (arv) {
				_drags->set (new AutomationRangeDrag (this, arv, selection->time), event, _cursors->up_down);
				_drags->start_grab (event);
			}
			return true;
			break;
		}

		case AutomationLineItem:
			_drags->set (new LineDrag (this, item), event);
			break;
			
		default:
			break;
		}
		return true;
		break;

	case MouseZoom:

        if (inside_track_area (event->button.x, event->button.y) && (event->type == GDK_BUTTON_PRESS)) {
			_drags->set (new MouseZoomDrag (this, item), event);
		}

		return true;
		break;

	case MouseTimeFX:
		if (internal_editing() && item_type == NoteItem ) {
			/* drag notes if we're in internal edit mode */
			NoteBase* cn = reinterpret_cast<NoteBase*>(item->get_data ("notebase"));
			assert (cn);
			if (cn->big_enough_to_trim()) {
				_drags->set (new NoteResizeDrag (this, item), event, _cursors->invalid_cursor ());
			}
			return true;
		} else if (clicked_regionview) {
			/* do time-FX  */
			_drags->set (new TimeFXDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
			return true;
		}
		break;

	case MouseAudition:
		_drags->set (new ScrubDrag (this, item), event);
		scrub_reversals = 0;
		scrub_reverse_distance = 0;
		last_scrub_x = event->button.x;
		scrubbing_direction = 0;
		push_canvas_cursor (_cursors->transparent);
		return true;
		break;

	default:
		break;
	}

	return false;
}

bool
Editor::button_press_handler_2 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	Editing::MouseMode const eff = effective_mouse_mode ();
	switch (eff) {
	case MouseObject:
		switch (item_type) {
		case RegionItem:
			if (internal_editing ()) {
				/* no region drags in internal edit mode */
				return false;
			}

			if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
				add_region_copy_drag (item, event, clicked_regionview);
			} else {
				add_region_drag (item, event, clicked_regionview);
			}
			_drags->start_grab (event);
			return true;
			break;
		case ControlPointItem:
			_drags->set (new ControlPointDrag (this, item), event);
			return true;
			break;

		default:
			break;
		}

		switch (item_type) {
		case RegionViewNameHighlight:
			_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
			return true;
			break;

		case LeftFrameHandle:
		case RightFrameHandle:
			if (!internal_editing ()) {
				_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
			}
			return true;
			break;

		case RegionViewName:
			_drags->set (new TrimDrag (this, clicked_regionview->get_name_highlight(), clicked_regionview, selection->regions.by_layer()), event);
			return true;
			break;

		default:
			break;
		}

		break;

	case MouseDraw:
		return false;

	case MouseRange:
		/* relax till release */
		return true;
		break;


	case MouseZoom:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
			temporal_zoom_to_frame (false, canvas_event_sample (event));
		} else {
			temporal_zoom_to_frame (true, canvas_event_sample(event));
		}
		return true;
		break;

	default:
		break;
	}

	return false;
}
   
bool
Editor::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		_drags->mark_double_click ();
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		return true;
	}

	if (event->type != GDK_BUTTON_PRESS) {
		return false;
	}
	
	push_canvas_cursor ();
	
	_track_canvas->grab_focus();

    /* no action if we're recording (except MarkerItem) */
    
	if (item_type != MarkerItem && _session && _session->actively_recording()) {
		return true;
	}

	if (internal_editing()) {
		bool leave_internal_edit_mode = false;

		switch (item_type) {
		case NoteItem:
			break;

		case RegionItem:
			if (!dynamic_cast<MidiRegionView*> (clicked_regionview) && !dynamic_cast<AutomationRegionView*> (clicked_regionview)) {
				leave_internal_edit_mode = true;
			}
			break;

		case PlayheadCursorItem:
		case MarkerItem:
		case TempoMarkerItem:
		case MeterMarkerItem:
		case MarkerBarItem:
		case TempoBarItem:
		case MeterBarItem:
		case RangeMarkerBarItem:
                case SkipBarItem:
		case CdMarkerBarItem:
		case StreamItem:
		case TimecodeRulerItem:
		case SamplesRulerItem:
		case MinsecRulerItem:
		case BBTRulerItem:
			/* button press on these items never does anything to
			   change the editing mode.
			*/
			break;

		default:
			break;
		}
		
		if (leave_internal_edit_mode) {
			ActionManager::do_action ("MouseMode", "toggle-internal-edit");
		}
	}

	button_selection (item, event, item_type);

	if (!_drags->active () &&
	    (Keyboard::is_delete_event (&event->button) ||
	     Keyboard::is_context_menu_event (&event->button) ||
	     Keyboard::is_edit_event (&event->button))) {

		/* handled by button release */
		return true;
	}

	//not rolling, range mode click + join_play_range :  locate the PH here
	if ( !_drags->active () && _session && !_session->transport_rolling() && ( effective_mouse_mode() == MouseRange ) && ARDOUR_UI::config()->get_follow_edits() ) {
		framepos_t where = canvas_event_sample (event);
		snap_to(where);
		_session->request_locate (where, false);
	}

	switch (event->button.button) {
	case 1:
		return button_press_handler_1 (item, event, item_type);
		break;

	case 2:
		// Tracks Live does not use scroll-button press
		break;

	case 3:
		break;

	default:
                return button_press_dispatch (&event->button);
		break;

	}

	return false;
}

bool
Editor::button_press_dispatch (GdkEventButton* ev)
{
        /* this function is intended only for buttons 4 and above.
         */

        Gtkmm2ext::MouseButton b (ev->state, ev->button);
        return button_bindings->activate (b, Gtkmm2ext::Bindings::Press);
}

bool
Editor::button_release_dispatch (GdkEventButton* ev)
{
        /* this function is intended only for buttons 4 and above.
         */

        Gtkmm2ext::MouseButton b (ev->state, ev->button);
        return button_bindings->activate (b, Gtkmm2ext::Bindings::Release);
}

bool
Editor::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	framepos_t where = canvas_event_sample (event);
	AutomationTimeAxisView* atv = 0;

    pop_canvas_cursor ();

	/* no action if we're recording (except MarkerItem) */

	if (item_type != MarkerItem && _session && _session->actively_recording()) {
		return true;
	}

        bool were_dragging = false;

	if (!Keyboard::is_context_menu_event (&event->button)) {

                /* see if we're finishing a drag */
                
                if (_drags->active ()) {
                        bool const r = _drags->end_grab (event);
                        if (r) {
                                /* grab dragged, so do nothing else */
                                return true;
                        }
                        
                        were_dragging = true;
                }

                update_region_layering_order_editor ();
        }

	/* edit events get handled here */

	if (!_drags->active () && Keyboard::is_edit_event (&event->button)) {
		switch (item_type) {

		case TempoMarkerItem: {
			Marker* marker;
			TempoMarker* tempo_marker;
			
			if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
				fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
				/*NOTREACHED*/
			}
			
			if ((tempo_marker = dynamic_cast<TempoMarker*> (marker)) == 0) {
				fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
				/*NOTREACHED*/
			}
			
			edit_tempo_marker (*tempo_marker);
			break;
		}

		case MeterMarkerItem: {
			Marker* marker;
			MeterMarker* meter_marker;
			
			if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
				fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
				/*NOTREACHED*/
			}
			
			if ((meter_marker = dynamic_cast<MeterMarker*> (marker)) == 0) {
				fatal << _("programming error: marker for meter is not a meter marker!") << endmsg;
				/*NOTREACHED*/
			}
			edit_meter_marker (*meter_marker);
			break;
		}

		case RegionViewName:
			if (clicked_regionview->name_active()) {
				return mouse_rename_region (item, event);
			}
			break;

		case ControlPointItem:
			edit_control_point (item);
			break;

		default:
			break;
		}
		return true;
	}

	/* context menu events get handled here */
	if (Keyboard::is_context_menu_event (&event->button)) {

		context_click_event = *event;

		if (!_drags->active ()) {

			/* no matter which button pops up the context menu, tell the menu
			   widget to use button 1 to drive menu selection.
			*/

			switch (item_type) {
			case FadeInItem:
			case FadeInHandleItem:
			case FadeInTrimHandleItem:
			case StartCrossFadeItem:
			case LeftFrameHandle:
				popup_xfade_in_context_menu (1, event->button.time, item, item_type);
				break;

			case FadeOutItem:
			case FadeOutHandleItem:
			case FadeOutTrimHandleItem:
			case EndCrossFadeItem:
			case RightFrameHandle:
				popup_xfade_out_context_menu (1, event->button.time, item, item_type);
				break;

			case StreamItem:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case RegionItem:
                // make selection and fall down
                set_selected_regionview_from_click (false/*button release handler*/, Selection::Set);
			case RegionViewNameHighlight:
			case RegionViewName:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case SelectionItem:
				popup_track_context_menu (1, event->button.time, item_type, true);
				break;
				
			case AutomationTrackItem:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case MarkerBarItem:
			case SkipBarItem:
			case RangeMarkerBarItem:
			case CdMarkerBarItem:
			case TempoBarItem:
			case MeterBarItem:
			case VideoBarItem:
			case TimecodeRulerItem:
			case SamplesRulerItem:
			case MinsecRulerItem:
			case BBTRulerItem:
				/* do nothing in tracks */
				break;

			case MarkerItem:
                /* don't show context menu if we're recording */
                if (_session && !_session->actively_recording()) {
                    marker_context_menu (&event->button, item);
                }
				break;

			case TempoMarkerItem:
				tempo_or_meter_marker_context_menu (&event->button, item);
				break;

			case MeterMarkerItem:
				tempo_or_meter_marker_context_menu (&event->button, item);
				break;

			case CrossfadeViewItem:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case ControlPointItem:
				popup_control_point_context_menu (item, event);
				break;

			default:
				break;
			}

			return true;
		}
	}

	Editing::MouseMode const eff = effective_mouse_mode ();

	switch (event->button.button) {
	case 1:

		switch (item_type) {
		/* see comments in button_press_handler */
		case PlayheadCursorItem:
		case MarkerItem:
		case GainLineItem:
		case AutomationLineItem:
		case StartSelectionTrimItem:
		case EndSelectionTrimItem:
		case MarkerBarItem:
		case CdMarkerBarItem:
		case TempoBarItem:
		case TimecodeRulerItem:
		case SamplesRulerItem:
		case MinsecRulerItem:
		case BBTRulerItem:
                        /* interactions handled by a Drag object, nothing to do */
                        return true;

		case MeterBarItem:
			if (!_dragging_playhead) {
				mouse_add_new_meter_event (pixel_to_sample (event->button.x));
			}
			return true;
			break;

		default:
			break;
		}

		switch (eff) {
		case MouseObject:
			switch (item_type) {
			case AutomationTrackItem:
				atv = dynamic_cast<AutomationTimeAxisView*>(clicked_axisview);
				if (atv) {
					bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
					atv->add_automation_event (event, where, event->button.y, with_guard_points);
				}
				return true;
				break;
			default:
				break;
			}
			break;

		case MouseGain:
			switch (item_type) {
			case RegionItem:
			{
				/* check that we didn't drag before releasing, since
				   its really annoying to create new control
				   points when doing this.
				*/
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (clicked_regionview);
				if (!were_dragging && arv) {
					bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
					arv->add_gain_point_event (item, event, with_guard_points);
				}
				return true;
				break;
			}

			case AutomationTrackItem: {
				bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
				dynamic_cast<AutomationTimeAxisView*>(clicked_axisview)->
					add_automation_event (event, where, event->button.y, with_guard_points);
				return true;
				break;
			}
			default:
				break;
			}
			break;

		case MouseAudition:
			pop_canvas_cursor ();
			if (scrubbing_direction == 0) {
				/* no drag, just a click */
				switch (item_type) {
				case RegionItem:
					play_selected_region ();
					break;
				default:
					break;
				}
			} else {
				/* make sure we stop */
				_session->request_transport_speed (0.0);
			}
			break;

		default:
			break;

		}

                /* do any (de)selection operations that should occur on button release */
                button_selection (item, event, item_type);
		return true;
		break;


	case 2:
		switch (eff) {

		case MouseObject:
			switch (item_type) {
			case RegionItem:
				if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
					raise_region ();
				} else if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask (Keyboard::TertiaryModifier|Keyboard::SecondaryModifier))) {
					lower_region ();
				} else {
					// Button2 click is unused
				}
				return true;

				break;

			default:
				break;
			}
			break;

		case MouseDraw:
			return true;
			
		case MouseRange:
			// x_style_paste (where, 1.0);
			return true;
			break;

		default:
			break;
		}

		break;

	case 3:
		break;

	default:
		break;
	}

	return false;
}

bool
Editor::enter_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	ControlPoint* cp;
	double fraction;
        bool ret = true;

	/* by the time we reach here, entered_regionview and entered trackview
	 * will have already been set as appropriate. Things are done this 
	 * way because this method isn't passed a pointer to a variable type of
	 * thing that is entered (which may or may not be canvas item).
	 * (e.g. the actual entered regionview)
	 */

	choose_canvas_cursor_on_entry (&event->crossing, item_type);

	switch (item_type) {
	case ControlPointItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			cp = static_cast<ControlPoint*>(item->get_data ("control_point"));
			cp->show ();

			fraction = 1.0 - (cp->get_y() / cp->line().height());

			_verbose_cursor->set (cp->line().get_verbose_cursor_string (fraction));
			_verbose_cursor->show ();
		}
		break;

	case GainLineItem:
		if (mouse_mode == MouseGain) {
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line) {
				line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_EnteredGainLine());
			}
		}
		break;

	case AutomationLineItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line) {
				line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_EnteredAutomationLine());
			}
		}
		break;

	case AutomationTrackItem:
		AutomationTimeAxisView* atv;
		if ((atv = static_cast<AutomationTimeAxisView*>(item->get_data ("trackview"))) != 0) {
			clear_entered_track = false;
			set_entered_track (atv);
		}
		break;

	case MarkerItem:
        case MeterMarkerItem:
	case TempoMarkerItem:
		break;

	case FadeInHandleItem:
	case FadeInTrimHandleItem:
		if (mouse_mode == MouseObject && !internal_editing()) {
			ArdourCanvas::Rectangle *rect = dynamic_cast<ArdourCanvas::Rectangle *> (item);
			if (rect) {
				RegionView* rv = static_cast<RegionView*>(item->get_data ("regionview"));
				rect->set_fill_color (rv->get_fill_color());
			}
		}
		break;

	case FadeOutHandleItem:
	case FadeOutTrimHandleItem:
		if (mouse_mode == MouseObject && !internal_editing()) {
			ArdourCanvas::Rectangle *rect = dynamic_cast<ArdourCanvas::Rectangle *> (item);
			if (rect) {
				RegionView* rv = static_cast<RegionView*>(item->get_data ("regionview"));
				rect->set_fill_color (rv->get_fill_color ());
			}
		}
		break;

	case FeatureLineItem:
	{
		ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
		line->set_outline_color (0xFF0000FF);
	}
	break;

	case SelectionItem:
		break;
    
	default:
		break;
	}

	/* third pass to handle entered track status in a comprehensible way.
	 */

	switch (item_type) {
	case GainLineItem:
	case AutomationLineItem:
	case ControlPointItem:
		/* these do not affect the current entered track state */
		clear_entered_track = false;
		break;

	case AutomationTrackItem:
		/* handled above already */
		break;

	default:

		break;
	}

	return ret;
}

bool
Editor::leave_handler (ArdourCanvas::Item* item, GdkEvent*, ItemType item_type)
{
	AutomationLine* al;
	bool ret = true;

	switch (item_type) {
	case ControlPointItem:
		_verbose_cursor->hide (); 
		break;

	case GainLineItem:
	case AutomationLineItem:
		al = reinterpret_cast<AutomationLine*> (item->get_data ("line"));
		{
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line) {
				line->set_outline_color (al->get_line_color());
			}
		}
		break;

	case MarkerItem:
	case MeterMarkerItem:
	case TempoMarkerItem:
		break;

	case FadeInTrimHandleItem:
	case FadeOutTrimHandleItem:
	case FadeInHandleItem:
	case FadeOutHandleItem:
	{
		ArdourCanvas::Rectangle *rect = dynamic_cast<ArdourCanvas::Rectangle *> (item);
		if (rect) {
			rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_InactiveFadeHandle());
		}
    }
    break;
            
	case AutomationTrackItem:
		break;

    case LeftFrameHandle:
        reset_canvas_cursor ();
        break;
    case RightFrameHandle:
        reset_canvas_cursor ();
        break;
            
	case FeatureLineItem:
	{
		ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
		line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_ZeroLine());
	}
	break;

	default:
		break;
	}

	return ret;
}

void
Editor::scrub (framepos_t frame, double current_x)
{
	double delta;

	if (scrubbing_direction == 0) {
		/* first move */
		_session->request_locate (frame, false);
		_session->request_transport_speed (0.1);
		scrubbing_direction = 1;

	} else {

		if (last_scrub_x > current_x) {

			/* pointer moved to the left */

			if (scrubbing_direction > 0) {

				/* we reversed direction to go backwards */

				scrub_reversals++;
				scrub_reverse_distance += (int) (last_scrub_x - current_x);

			} else {

				/* still moving to the left (backwards) */

				scrub_reversals = 0;
				scrub_reverse_distance = 0;

				delta = 0.01 * (last_scrub_x - current_x);
				_session->request_transport_speed_nonzero (_session->transport_speed() - delta);
			}

		} else {
			/* pointer moved to the right */

			if (scrubbing_direction < 0) {
				/* we reversed direction to go forward */

				scrub_reversals++;
				scrub_reverse_distance += (int) (current_x - last_scrub_x);

			} else {
				/* still moving to the right */

				scrub_reversals = 0;
				scrub_reverse_distance = 0;

				delta = 0.01 * (current_x - last_scrub_x);
				_session->request_transport_speed_nonzero (_session->transport_speed() + delta);
			}
		}

		/* if there have been more than 2 opposite motion moves detected, or one that moves
		   back more than 10 pixels, reverse direction
		*/

		if (scrub_reversals >= 2 || scrub_reverse_distance > 10) {

			if (scrubbing_direction > 0) {
				/* was forwards, go backwards */
				_session->request_transport_speed (-0.1);
				scrubbing_direction = -1;
			} else {
				/* was backwards, go forwards */
				_session->request_transport_speed (0.1);
				scrubbing_direction = 1;
			}

			scrub_reverse_distance = 0;
			scrub_reversals = 0;
		}
	}

	last_scrub_x = current_x;
}

bool
Editor::motion_handler (ArdourCanvas::Item* item, GdkEvent* event, bool from_autoscroll, ItemType type)
{
	_last_motion_y = event->motion.y;

	if (event->motion.is_hint) {
		gint x, y;

		/* We call this so that MOTION_NOTIFY events continue to be
		   delivered to the canvas. We need to do this because we set
		   Gdk::POINTER_MOTION_HINT_MASK on the canvas. This reduces
		   the density of the events, at the expense of a round-trip
		   to the server. Given that this will mostly occur on cases
		   where DISPLAY = :0.0, and given the cost of what the motion
		   event might do, its a good tradeoff.
		*/

		_track_canvas->get_pointer (x, y);
	}

	if (current_stepping_trackview) {
		/* don't keep the persistent stepped trackview if the mouse moves */
		current_stepping_trackview = 0;
		step_timeout.disconnect ();
	}
    
	if (type != MarkerItem && _session && _session->actively_recording()) {
		/* Sorry. no dragging stuff around while we record (except MarkerItem) */
		return true;
	}
	
	update_join_object_range_location (event->motion.y);
	
	if (_drags->active ()) {
	 	return _drags->motion_handler (event, from_autoscroll);
	}

	return false;
}

bool
Editor::can_remove_control_point (ArdourCanvas::Item* item)
{
	ControlPoint* control_point;

	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	AutomationLine& line = control_point->line ();
	if (dynamic_cast<AudioRegionGainLine*> (&line)) {
		/* we shouldn't remove the first or last gain point in region gain lines */
		if (line.is_last_point(*control_point) || line.is_first_point(*control_point)) {
			return false;
		}
	}

	return true;
}

void
Editor::remove_control_point (ArdourCanvas::Item* item)
{
	if (!can_remove_control_point (item)) {
		return;
	}

	ControlPoint* control_point;

	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	control_point->line().remove_point (*control_point);
}

void
Editor::edit_control_point (ArdourCanvas::Item* item)
{
	ControlPoint* p = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"));

	if (p == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	ControlPointDialog d (p);
	ensure_float (d);

	if (d.run () != RESPONSE_ACCEPT) {
		return;
	}

	p->line().modify_point_y (*p, d.get_y_fraction ());
}

void
Editor::edit_notes (TimeAxisViewItem& tavi)
{
	MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(&tavi);

	if (!mrv) {
		return;
	}

	MidiRegionView::Selection const & s = mrv->selection();

	if (s.empty ()) {
		return;
	}

	EditNoteDialog* d = new EditNoteDialog (&(*s.begin())->region_view(), s);
	d->show_all ();
	ensure_float (*d);

	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &Editor::note_edit_done), d));
}

void
Editor::note_edit_done (int r, EditNoteDialog* d)
{
	d->done (r);
	delete d;
}

void
Editor::visible_order_range (int* low, int* high) const
{
	*low = TimeAxisView::max_order ();
	*high = 0;

	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);

		if (!rtv->hidden()) {

			if (*high < rtv->order()) {
				*high = rtv->order ();
			}

			if (*low > rtv->order()) {
				*low = rtv->order ();
			}
		}
	}
}

void
Editor::region_view_item_click (AudioRegionView& rv, GdkEventButton* event)
{
	/* Either add to or set the set the region selection, unless
	   this is an alignment click (control used)
	*/

	if (Keyboard::modifier_state_contains (event->state, Keyboard::PrimaryModifier)) {
		TimeAxisView* tv = &rv.get_time_axis_view();
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(tv);
		double speed = 1.0;
		if (rtv && rtv->is_track()) {
			speed = rtv->track()->speed();
		}

		framepos_t where = get_preferred_edit_position();

		if (where >= 0) {

			if (Keyboard::modifier_state_equals (event->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier))) {

				align_region (rv.region(), SyncPoint, (framepos_t) (where * speed));

			} else if (Keyboard::modifier_state_equals (event->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

				align_region (rv.region(), End, (framepos_t) (where * speed));

			} else {

				align_region (rv.region(), Start, (framepos_t) (where * speed));
			}
		}
	}
}

void
Editor::collect_new_region_view (RegionView* rv)
{
	latest_regionviews.push_back (rv);
}

void
Editor::collect_and_select_new_region_view (RegionView* rv)
{
	selection->add(rv);
	latest_regionviews.push_back (rv);
}

void
Editor::cancel_selection ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}

	selection->clear ();
	clicked_selection = 0;
}

void
Editor::cancel_time_selection ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}
	selection->time.clear ();
	clicked_selection = 0;
}	

void
Editor::point_trim (GdkEvent* event, framepos_t new_bound)
{
	RegionView* rv = clicked_regionview;

	/* Choose action dependant on which button was pressed */
	switch (event->button.button) {
	case 1:
		begin_reversible_command (_("start point trim"));

		if (selection->selected (rv)) {
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin();
			     i != selection->regions.by_layer().end(); ++i)
			{
				if (!(*i)->region()->locked()) {
					(*i)->region()->clear_changes ();
					(*i)->region()->trim_front (new_bound);
					_session->add_command(new StatefulDiffCommand ((*i)->region()));
				}
			}

		} else {
			if (!rv->region()->locked()) {
				rv->region()->clear_changes ();
				rv->region()->trim_front (new_bound);
				_session->add_command(new StatefulDiffCommand (rv->region()));
			}
		}

		commit_reversible_command();

		break;
	case 2:
		begin_reversible_command (_("End point trim"));

		if (selection->selected (rv)) {

			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i)
			{
				if (!(*i)->region()->locked()) {
					(*i)->region()->clear_changes();
					(*i)->region()->trim_end (new_bound);
					_session->add_command(new StatefulDiffCommand ((*i)->region()));
				}
			}

		} else {

			if (!rv->region()->locked()) {
				rv->region()->clear_changes ();
				rv->region()->trim_end (new_bound);
				_session->add_command (new StatefulDiffCommand (rv->region()));
			}
		}

		commit_reversible_command();

		break;
	default:
		break;
	}
}

void
Editor::hide_marker (ArdourCanvas::Item* item, GdkEvent* /*event*/)
{
	Marker* marker;
	bool is_start;

	if ((marker = static_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Location* location = find_location_from_marker (marker, is_start);
	location->set_hidden (true, this);
}


void
Editor::reposition_zoom_rect (framepos_t start, framepos_t end)
{
	double x1 = sample_to_pixel (start);
	double x2 = sample_to_pixel (end);
	double y2 = _full_canvas_height - 1.0;

	zoom_rect->set (ArdourCanvas::Rect (x1, 1.0, x2, y2));
}


gint
Editor::mouse_rename_region (ArdourCanvas::Item* /*item*/, GdkEvent* /*event*/)
{
	using namespace Gtkmm2ext;

	ArdourPrompter prompter (false);

	prompter.set_prompt (_("Name for region:"));
	prompter.set_initial_text (clicked_regionview->region()->name());
	prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	prompter.show_all ();
	switch (prompter.run ()) {
	case Gtk::RESPONSE_ACCEPT:
		string str;
		prompter.get_result(str);
		if (str.length()) {
			clicked_regionview->region()->set_name (str);
		}
		break;
	}
	return true;
}


void
Editor::mouse_brush_insert_region (RegionView* rv, framepos_t pos)
{
	/* no brushing without a useful snap setting */

	switch (_snap_mode) {
	case SnapMagnetic:
		return; /* can't work because it allows region to be placed anywhere */
	default:
		break; /* OK */
	}

	switch (_snap_type) {
	case SnapToMark:
		return;

	default:
		break;
	}

	/* don't brush a copy over the original */

	if (pos == rv->region()->position()) {
		return;
	}

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&rv->get_time_axis_view());

	if (rtv == 0 || !rtv->is_track()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist = rtv->playlist();
	double speed = rtv->track()->speed();

	playlist->clear_changes ();
	boost::shared_ptr<Region> new_region (RegionFactory::create (rv->region(), true));
	playlist->add_region (new_region, (framepos_t) (pos * speed));
	_session->add_command (new StatefulDiffCommand (playlist));

	// playlist is frozen, so we have to update manually XXX this is disgusting

	playlist->RegionAdded (new_region); /* EMIT SIGNAL */
}

gint
Editor::track_height_step_timeout ()
{
	if (get_microseconds() - last_track_height_step_timestamp < 250000) {
		current_stepping_trackview = 0;
		return false;
	}
	return true;
}

void
Editor::add_region_drag (ArdourCanvas::Item* item, GdkEvent*, RegionView* region_view)
{
	assert (region_view);

	if (!region_view->region()->playlist()) {
		return;
	}

	switch (Config->get_edit_mode()) {
		case Splice:
			_drags->add (new RegionSpliceDrag (this, item, region_view, selection->regions.by_layer()));
			break;
		case Ripple:
			_drags->add (new RegionRippleDrag (this, item, region_view, selection->regions.by_layer()));
			break;
		default:
			_drags->add (new RegionMoveDrag (this, item, region_view, selection->regions.by_layer(), false, false));
			break;
	}
}

void
Editor::add_region_copy_drag (ArdourCanvas::Item* item, GdkEvent*, RegionView* region_view)
{
	assert (region_view);

	if (!region_view->region()->playlist()) {
		return;
	}

	_drags->add (new RegionMoveDrag (this, item, region_view, selection->regions.by_layer(), false, true));
}

void
Editor::add_region_brush_drag (ArdourCanvas::Item* item, GdkEvent*, RegionView* region_view)
{
	assert (region_view);

	if (!region_view->region()->playlist()) {
		return;
	}

	if (Config->get_edit_mode() == Splice || Config->get_edit_mode() == Ripple) {
		return;
	}

	_drags->add (new RegionMoveDrag (this, item, region_view, selection->regions.by_layer(), true, false));

	begin_reversible_command (Operations::drag_region_brush);
}

/** Start a grab where a time range is selected, track(s) are selected, and the
 *  user clicks and drags a region with a modifier in order to create a new region containing
 *  the section of the clicked region that lies within the time range.
 */
void
Editor::start_selection_grab (ArdourCanvas::Item* item, GdkEvent* event, bool copy/*=false*/)
{
	/* lets try to create new Region for the selection */
	begin_reversible_command (_("new region for selection drag"));
    
    RegionSelection new_regions;
    cut_copy_region_from_selection (new_regions, copy);
    
    commit_reversible_command ();
    
	if (new_regions.empty()) {
		return;
	}

	/* we need to deselect all other regionviews, and select this one
	   i'm ignoring undo stuff, because the region creation will take care of it
	*/
	selection->set (new_regions);

	_drags->set (new RegionMoveDrag (this, new_regions.front()->get_canvas_group(), new_regions.front(), latest_regionviews, false, false), event);
}

void
Editor::escape ()
{
	if (_drags->active ()) {
		_drags->abort ();
	} else {
		selection->clear ();
	}

	reset_focus ();
}

void
Editor::set_internal_edit (bool yn)
{
	if (_internal_editing == yn) {
		return;
	}

	_internal_editing = yn;

	if (yn) {
		pre_internal_mouse_mode = mouse_mode;
		pre_internal_snap_type = _snap_type;
		pre_internal_snap_mode = _snap_mode;

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->enter_internal_edit_mode ();
		}

		set_snap_to (internal_snap_type);
		set_snap_mode (internal_snap_mode);

	} else {

		internal_snap_mode = _snap_mode;
		internal_snap_type = _snap_type;

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->leave_internal_edit_mode ();
		}

		if (mouse_mode == MouseDraw && pre_internal_mouse_mode != MouseDraw) {
			/* we were drawing .. flip back to something sensible */
			set_mouse_mode (pre_internal_mouse_mode);
		}

		set_snap_to (pre_internal_snap_type);
		set_snap_mode (pre_internal_snap_mode);
	}

	reset_canvas_cursor ();
}

/** Update _join_object_range_state which indicate whether we are over the top
 *  or bottom half of a route view, used by the `join object/range' tool
 *  mode. Coordinates in canvas space.
 */
void
Editor::update_join_object_range_location (double y)
{
	if (_internal_editing || !get_smart_mode()) {
		_join_object_range_state = JOIN_OBJECT_RANGE_NONE;
		return;
	}

	JoinObjectRangeState const old = _join_object_range_state;

	if (mouse_mode == MouseObject) {
		_join_object_range_state = JOIN_OBJECT_RANGE_OBJECT;
	} else if (mouse_mode == MouseRange) {
		_join_object_range_state = JOIN_OBJECT_RANGE_RANGE;
	}

	if (entered_regionview) {

		ArdourCanvas::Duple const item_space = entered_regionview->get_canvas_group()->canvas_to_item (ArdourCanvas::Duple (0, y));
		double const c = item_space.y / entered_regionview->height();
			
		_join_object_range_state = c <= 0.5 ? JOIN_OBJECT_RANGE_RANGE : JOIN_OBJECT_RANGE_OBJECT;
		
		if (_join_object_range_state != old) {
			set_canvas_cursor (which_track_cursor ());
		}

	} else if (entered_track) {

		RouteTimeAxisView* entered_route_view = dynamic_cast<RouteTimeAxisView*> (entered_track);
		
		if (entered_route_view) {

			double cx = 0;
			double cy = y;

			entered_route_view->canvas_display()->canvas_to_item (cx, cy);

			double const c = cy / (entered_route_view->view()->child_height() );

			if (c <= 0.5) {
				_join_object_range_state = JOIN_OBJECT_RANGE_RANGE;
			} else {
				_join_object_range_state = JOIN_OBJECT_RANGE_OBJECT;
			}

		} else {
			/* Other kinds of tracks use object mode */
			_join_object_range_state = JOIN_OBJECT_RANGE_OBJECT;
		}

		if (_join_object_range_state != old) {
			set_canvas_cursor (which_track_cursor ());
		}
	}
}

Editing::MouseMode
Editor::effective_mouse_mode () const
{
	if (_join_object_range_state == JOIN_OBJECT_RANGE_OBJECT) {
		return MouseObject;
	} else if (_join_object_range_state == JOIN_OBJECT_RANGE_RANGE) {
		return MouseRange;
	}

	return mouse_mode;
}

void
Editor::remove_midi_note (ArdourCanvas::Item* item, GdkEvent *)
{
	NoteBase* e = reinterpret_cast<NoteBase*> (item->get_data ("notebase"));
	assert (e);

	e->region_view().delete_note (e->note ());
}

/** Obtain the pointer position in canvas coordinates */
void
Editor::get_pointer_position (double& x, double& y) const
{
	int px, py;
	_track_canvas->get_pointer (px, py);
	_track_canvas->window_to_canvas (px, py, x, y);
}
