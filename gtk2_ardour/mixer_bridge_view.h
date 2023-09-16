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
#ifndef __ardour_mixer_bridge_view_h__
#define __ardour_mixer_bridge_view_h__

#include <glibmm/thread.h>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "gtkmm2ext/visibility_tracker.h"

#include "waves_ui.h"
#include "mixer_strip.h"
#include "mixer_actor.h"

class MixerBridgeView :
	public Gtk::EventBox,
	public WavesUI,
	public PBD::ScopedConnectionList,
	public ARDOUR::SessionHandlePtr,
	public MixerActor
{
  public:
	MixerBridgeView (const std::string& mixer_bridge_script_name, const std::string& mixer_strip_script_name = 0);
	~MixerBridgeView();
	void set_session (ARDOUR::Session *);
    void set_max_name_size(size_t size) {_max_name_size = size;}
	void track_editor_selection ();
    void all_gain_sliders_set_visible (bool);
    MixerStrip* get_mixer_strip (boost::shared_ptr<ARDOUR::Route>);

  protected:
	void set_route_targets_for_operation ();
	void toggle_midi_input_active (bool flip_others);
	void delete_processors ();
	void select_none ();
    virtual void on_size_allocate (Gtk::Allocation& alloc);

  private:
	Gtk::Container& _mixer_strips_home;
    Gtk::ScrolledWindow& _scroll;
    bool _following_editor_selection;
	std::string _mixer_strip_script_name;
    
	gint start_updating ();
	gint stop_updating ();
    
	sigc::connection fast_screen_update_connection;
	void fast_update ();

	void add_strips (ARDOUR::RouteList&);
	void remove_strip (MixerStrip *);

	void session_going_away ();
	void sync_order_keys ();
	void follow_editor_selection ();
    void ensure_first_selected_track_is_visible ();
	bool strip_button_release_event (GdkEventButton*, MixerStrip*);
	void parent_on_size_allocate (Gtk::Allocation&);
    void begin_strip_name_edit (MixerStrip::TabToStrip, const MixerStrip*);
    void ensure_strip_is_visible (const MixerStrip*);
    int get_number_of_strip (const MixerStrip*) const;
    int get_line_of_strip (const MixerStrip*) const;

	MixerStrip* strip_by_route (boost::shared_ptr<ARDOUR::Route> route);
	MixerStrip* strip_under_pointer ();

	std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*> _strips;
	mutable Glib::Threads::Mutex _resync_mutex;
    
    size_t _max_name_size;
};

#endif //__ardour_mixer_bridge_view_h__
