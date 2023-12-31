/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __gtk_ardour_panner_interface_h__
#define __gtk_ardour_panner_interface_h__

#include <boost/shared_ptr.hpp>
#include <gtkmm/label.h>
#include "pbd/destructible.h"
#include "gtkmm2ext/cairo_widget.h"

#include "waves_persistent_tooltip.h"
namespace ARDOUR {
	class Panner;
}

class PannerEditor;

class PannerPersistentTooltip : public Gtkmm2ext::WavesPersistentTooltip
{
public:
	PannerPersistentTooltip (Gtk::Widget* w);

	void target_start_drag ();
	void target_stop_drag ();

	bool dragging () const;

private:
	bool _dragging;
};
	

/** Parent class for some panner UI classes that contains some common code */
class PannerInterface : public CairoWidget, public PBD::Destructible
{
public:
	PannerInterface (boost::shared_ptr<ARDOUR::Panner>);
	virtual ~PannerInterface ();

	boost::shared_ptr<ARDOUR::Panner> panner () {
		return _panner;
	}

	void edit ();

protected:
	virtual void set_tooltip () = 0;

	void value_change ();
	
    bool on_enter_notify_event (GdkEventCrossing *);
    bool on_leave_notify_event (GdkEventCrossing *);
	bool on_key_release_event  (GdkEventKey *);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

	boost::shared_ptr<ARDOUR::Panner> _panner;
	PannerPersistentTooltip _tooltip;

	static Glib::RefPtr<Gdk::Pixbuf> load_pixbuf (const std::string& name);
	static Glib::RefPtr<Gdk::Pixbuf> _knob_image[101];
	static const char* _knob_image_files[101];

private:
	virtual PannerEditor* editor () = 0;
	PannerEditor* _editor;
};

#endif
