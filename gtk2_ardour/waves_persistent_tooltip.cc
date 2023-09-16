/*
    Copyright (C) 2015 Waves Audio Ltd.
 
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
#include "waves_persistent_tooltip.h"

#include "gtkmm2ext/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

/** @param target The widget to provide the tooltip for */
WavesPersistentTooltip::WavesPersistentTooltip (Gtk::Widget* target, std::string xml_file, bool draggable)
: Window (WINDOW_POPUP)
, WavesUI (xml_file, *this)
, _target (target)
, _label (get_label ("tooltip_text"))
, _draggable (draggable)
, _maybe_dragging (false)
, _align_to_center (true)
{
    init ();
}

void
WavesPersistentTooltip::init ()
{
    _target->signal_enter_notify_event().connect (sigc::mem_fun (*this, &WavesPersistentTooltip::enter), false);
    _target->signal_leave_notify_event().connect (sigc::mem_fun (*this, &WavesPersistentTooltip::leave), false);
    _target->signal_button_press_event().connect (sigc::mem_fun (*this, &WavesPersistentTooltip::press), false);
    _target->signal_button_release_event().connect (sigc::mem_fun (*this, &WavesPersistentTooltip::release), false);

	set_decorated (false);
    set_type_hint (Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
	Gtk::Window* tlw = dynamic_cast<Gtk::Window*> (_target->get_toplevel ());
	if (tlw) {
		set_transient_for (*tlw);
	}	
}


WavesPersistentTooltip::~WavesPersistentTooltip ()
{
}

bool
WavesPersistentTooltip::enter (GdkEventCrossing *)
{
    if (_timeout.connected()) {
        leave (NULL);
    }
    _timeout = Glib::signal_timeout ().connect (sigc::mem_fun (*this, &WavesPersistentTooltip::timeout), 500);
    return false;
}

bool
WavesPersistentTooltip::timeout ()
{
    show_tooltip ();
    return false;
}

bool
WavesPersistentTooltip::leave (GdkEventCrossing *)
{
    _timeout.disconnect ();
    if (!dragging ()) {
        hide ();
    }
    
    return false;
}

bool
WavesPersistentTooltip::press (GdkEventButton* ev)
{
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
        _maybe_dragging = true;
    }
    
    return false;
}

bool
WavesPersistentTooltip::release (GdkEventButton* ev)
{
    if (ev->type == GDK_BUTTON_RELEASE && ev->button == 1) {
        _maybe_dragging = false;
    }
    
    return false;
}

bool
WavesPersistentTooltip::dragging () const
{
    return _maybe_dragging && _draggable;
}

void
WavesPersistentTooltip::show_tooltip ()
{
    if (!is_visible ()) {
        int rx, ry, sw;
        sw = gdk_screen_width ();
        _target->get_window ()->get_origin (rx, ry);
        
        if (_align_to_center) {
            move (rx + (_target->get_width () - this->get_width ()) / 2, ry + _target->get_height ());
            present ();
            move (rx + (_target->get_width () - this->get_width ()) / 2, ry + _target->get_height () + 10);
        } else {
            move (rx, ry + _target->get_height () + 10);
            present ();
        }
        
        /* the window needs to be realized first
         * for _window->get_width() to be correct.
         */
        if (sw < rx + this->get_width ()) {
            rx = sw - this->get_width ();
            move (rx, ry + _target->get_height ());
        }
    }
}

void
WavesPersistentTooltip::set_tip (string t)
{
    _tip = t;
    _label.set_text (t);
}

void
WavesPersistentTooltip::set_font (Pango::FontDescription font)
{
    _font = font;
}

void
WavesPersistentTooltip::set_center_alignment (bool align_to_center)
{
    _align_to_center = align_to_center;
}
