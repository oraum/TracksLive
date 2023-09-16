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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <map>
#include <sigc++/bind.h>

#include <gtkmm/accelmap.h>

#include <glibmm/threads.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "compact_meter_bridge.h"

#include "keyboard.h"
#include "monitor_section.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "global_signals.h"
#include "meter_patterns.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

using PBD::atoi;

struct SignalOrderRouteSorter {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		if (a->is_master() || a->is_monitor() || !boost::dynamic_pointer_cast<Track>(a)) {
			/* "a" is a special route (master, monitor, etc), and comes
			 * last in the mixer ordering
			 */
			return false;
		} else if (b->is_master() || b->is_monitor() || !boost::dynamic_pointer_cast<Track>(b)) {
			/* everything comes before b */
			return true;
		}
		return a->order_key () < b->order_key ();
	}
};

CompactMeterbridge::CompactMeterbridge ()
	: Gtk::EventBox()
	, WavesUI ("compact_meter_bridge.xml", *this)
	, _compact_meter_strips_home (get_box ("compact_meter_strips_home"))
    , _scroll (get_scrolled_window ("scroller"))
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	Route::SyncOrderKeys.connect (*this, invalidator (*this), boost::bind (&CompactMeterbridge::sync_order_keys, this), gui_context());
	CompactMeterStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&CompactMeterbridge::remove_strip, this, _1), gui_context());
}

CompactMeterbridge::~CompactMeterbridge ()
{
}

void
CompactMeterbridge::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	boost::shared_ptr<RouteList> routes = _session->get_routes();

	add_strips(*routes);

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&CompactMeterbridge::add_strips, this, _1), gui_context());

    start_updating ();
}

void
CompactMeterbridge::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &CompactMeterbridge::session_going_away);

	for (std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		delete (*i).second;
	}

	_strips.clear ();
	stop_updating ();

	SessionHandlePtr::session_going_away ();
	_session = 0;
}

gint
CompactMeterbridge::start_updating ()
{
	return 0;
}

gint
CompactMeterbridge::stop_updating ()
{
	return 0;
}

void
CompactMeterbridge::fast_update ()
{
	if (!is_mapped () || !_session) {
		return;
	}
}

void
CompactMeterbridge::add_strips (RouteList& routes)
{

	// WARNING: This is a rapid implementation. It must be OPTIMIZED in order to
	// eliminate the code duplicating (see sync_reorder_keys())

	// First detach all the prviously added strips from the ui tree.
	for (std::map<boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		_compact_meter_strips_home.remove (*(*i).second); // we suppose _compact_meter_strips_home is
		                                                  // the parnet. 
	}

	// Now create the strips for newly added routes
	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master()
			|| !boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}

		CompactMeterStrip* strip = new CompactMeterStrip (_session, route);
        strip->signal_button_press_event().connect (sigc::bind (sigc::mem_fun(*this, &CompactMeterbridge::strip_button_press_event), strip));
		_strips [route] = strip;
		strip->show();
	}

	// Now sort the session's routes and pack the strips accordingly
	SignalOrderRouteSorter sorter;
	RouteList copy(*_session->get_routes());
	copy.sort(sorter);

	size_t serial_number = 0;
	for (RouteList::iterator x = copy.begin(); x != copy.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track>(route)) {
			continue;
		}
		std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
		if (i != _strips.end ()) {
			_compact_meter_strips_home.pack_start (*(*i).second, false, false);
			(*i).second->set_serial_number (++serial_number);
            (*i).second->update_tooltip ();
            (*i).second->update_track_number ();
		}
	}
}

void
CompactMeterbridge::remove_strip (CompactMeterStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	boost::shared_ptr<ARDOUR::Route> route = strip->route ();
	std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
	if (i != _strips.end ()) {
		_strips.erase (i);
	}
}

void
CompactMeterbridge::sync_order_keys ()
{
	Glib::Threads::Mutex::Lock lm (_resync_mutex);

	if (!_session) {
		return;
	}

	// First detach all the prviously added strips from the ui tree.
	for (std::map<boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		_compact_meter_strips_home.remove (*(*i).second); // we suppose _compact_meter_strips_home is
		                                                  // the parnet. 
	}

	// Now sort the session's routes and pack the strips accordingly
	SignalOrderRouteSorter sorter;	
	RouteList copy(*_session->get_routes());
	copy.sort(sorter);

	size_t serial_number = 0;
	for (RouteList::iterator x = copy.begin(); x != copy.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}
		std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
		if (i != _strips.end ()) {
			_compact_meter_strips_home.pack_start (*(*i).second, false, false);
			(*i).second->set_serial_number (++serial_number);
            (*i).second->update_tooltip ();
            (*i).second->update_track_number ();
		}
	}
}

void
CompactMeterbridge::track_editor_selection ()
{
    PublicEditor::instance().get_selection().TracksChanged.connect (sigc::mem_fun (*this, &CompactMeterbridge::follow_editor_selection));
}

void
CompactMeterbridge::follow_editor_selection ()
{
    if (!_session) {
        return ;
    }
    
    clear_selection ();
    
    TrackSelection& s (PublicEditor::instance().get_selection().tracks);
    
    for (TrackViewList::iterator i = s.begin(); i != s.end(); ++i) {
        RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
        if (rtav) {
            CompactMeterStrip* cms = strip_by_route (rtav->route());
            if (cms) {
                cms->set_selected (true);
            }
        }
    }

    // make sure first selected track is visible in compact meter bridge
    // get ORDERED list of routes from the session
    // to acoomplish this - sort the list of routes as they are displayed
    // the same sorter is used to pack strips for mixer and meter
    SignalOrderRouteSorter sorter;
    RouteList sorted_routes (*_session->get_routes ());
    sorted_routes.sort (sorter);
        
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();
       
    for (RouteList::iterator i = sorted_routes.begin (); i != sorted_routes.end (); ++i) {
        
        CompactMeterStrip* compact_meter_strip = strip_by_route (*i);
            
        if (!compact_meter_strip) {
            continue;
        }
            
        TimeAxisView* tav = ARDOUR_UI::instance ()->the_editor ().axis_view_from_route (compact_meter_strip->route ());
            
        if (selection.selected (tav) ) {
            ensure_strip_is_visible (compact_meter_strip);
            break;
        }
    }
}

void
CompactMeterbridge::clear_selection ()
{
    for (std::map<boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
       (*i).second->set_selected (false);
    }
}

void
CompactMeterbridge::ensure_strip_is_visible (const CompactMeterStrip* cur_strip)
{
    
    Gtk::Adjustment* horizontal_adjustment = _scroll.get_hadjustment ();
    const int STRIP_WIDTH = cur_strip->get_width () + _compact_meter_strips_home.get_spacing ();
    
    double current_view_min_pos = horizontal_adjustment->get_value ();
    double current_view_max_pos = current_view_min_pos + horizontal_adjustment->get_page_size ();
    
    double strip_min_pos = get_number_of_strip (cur_strip) * STRIP_WIDTH;

    double strip_max_pos = strip_min_pos + STRIP_WIDTH;
    
    if ( strip_min_pos >= current_view_min_pos &&
        strip_max_pos < current_view_max_pos ) {
        // already visible
        return;
    }
    
    double new_value = 0.0;
    if (strip_min_pos < current_view_min_pos) {
        // Strip is left the current view
        new_value = strip_min_pos;
    } else {
        // Strip is right the current view
        new_value = strip_max_pos - horizontal_adjustment->get_page_size ();
    }
    horizontal_adjustment->set_value (new_value);
}

int
CompactMeterbridge::get_number_of_strip (const CompactMeterStrip* cur_strip) const
{
    boost::shared_ptr<ARDOUR::Route> route = const_cast <CompactMeterStrip*> (cur_strip) ->route ();
    if (route) {
        if (Config->get_output_auto_connect() & AutoConnectMaster) {
            // in Stereo Out mode
            // Master Bus has order_key = 0
            // isn't visible in compact bridge
            return route-> order_key () - 1;
        } else {
            return route-> order_key ();
        }
    }
    else {
        return -1;
    }
}

CompactMeterStrip*
CompactMeterbridge::strip_by_route (boost::shared_ptr<Route> route)
{
    std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
    if (i != _strips.end ()) {
        return (*i).second;
    }
    return 0;
}

bool
CompactMeterbridge::strip_button_press_event (GdkEventButton *ev, CompactMeterStrip *strip)
{
    if (!_session) {
        return false;
    }
    
    Selection& selection = ARDOUR_UI::instance ()->the_editor ().get_selection ();

    if (ev->button == 1) {
        
        // primary modifier usecase
        TimeAxisView* tav = ARDOUR_UI::instance ()->the_editor ().axis_view_from_route (strip->route ());
        if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier) ) {
            if (selection.selected (tav) ){
                selection.remove (tav);
            } else {
                selection.add (tav);
            }
            
            return true;
        }
        // secondary modifier usecase (multi-selection)
        if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier) )  {
            if (!selection.selected (tav)) {
                
                /* extend selection */
                vector<CompactMeterStrip*> tmp;
                tmp.push_back (strip);
                
                // get ORDERED list of routes from the session
                // to acoomplish this - sort the list of routes as they are displayed
                // the same sorter is used to pack strips for mixer and meter
                SignalOrderRouteSorter sorter;
                RouteList sorted_routes (*_session->get_routes ());
                sorted_routes.sort (sorter);
                
                bool accumulate = false;
                bool passed_target = false;
                for (RouteList::iterator i = sorted_routes.begin (); i != sorted_routes.end (); ++i) {
                    
                    CompactMeterStrip* compact_meter_strip = strip_by_route (*i);
                    
                    if (!compact_meter_strip) {
                        // we do not create CompactMeterStrip for master bus
                        // it appears to be the last
                        // in the right case we won't hit the end
                        // because multi selection always happens between selected and slicked
                        continue;
                    }
                    
                    if (compact_meter_strip == strip) {
                        /* hit clicked strip, start accumulating till we hit the first
                         selected strip
                         */
                        if (accumulate) {
                            /* done */
                            break;
                        } else {
                            accumulate = true;
                            passed_target = true;
                        }
                        
                        // error here
                    } else {
                        TimeAxisView* selected_tav = ARDOUR_UI::instance ()->the_editor ().axis_view_from_route (compact_meter_strip->route ());
                        if (selection.selected (selected_tav) ) {
                            /* hit selected strip. if currently accumulating others,
                             we're done. if not accumulating others, start doing so.
                             */
                            if (accumulate) {
                            
                                if (passed_target)
                                    break;
                            
                            } else {
                                accumulate = true;
                            }
                        } else {
                            if (accumulate) {
                                tmp.push_back (compact_meter_strip);
                            }
                        }
                    }
                }
                selection.block_tracks_changed (true);
                for (vector<CompactMeterStrip*>::iterator i = tmp.begin(); i != tmp.end(); ++i) {
                    TimeAxisView* tav = ARDOUR_UI::instance ()->the_editor ().axis_view_from_route ((*i)->route ());
                    selection.add (tav);
                }
                selection.block_tracks_changed (false);
                selection.TracksChanged ();
            }
            
            return true;
        }
        
        // other cases
        selection.set (tav);
        return true;
    }
    
    return false;

}
