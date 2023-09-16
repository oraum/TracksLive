/*
    Copyright (C) 2010 Paul Davis

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
#include "waves_icon.h"
#include "gtkmm/container.h"
#include <iostream>

#define REFLECTION_HEIGHT 2

WavesIcon::WavesIcon ()
	: _image_width (0)
	, _image_height (0)
{
}

bool
WavesIcon::on_expose_event (GdkEventExpose *evt)
{
	int origx(0), origy(0);
	
	for (Gtk::Widget *parent = get_parent (); parent; parent = parent->get_parent ()) {
		if (parent->get_has_window()) {
			translate_coordinates (*parent, 0, 0, origx, origy);
			break;
		}
	}

	Glib::RefPtr<Gdk::Pixbuf> pixbuf; 

	switch (get_state ()) {
	case Gtk::STATE_ACTIVE:
		pixbuf = _active_pixbuf;
		break;
	case Gtk::STATE_PRELIGHT:
		pixbuf = _prelight_pixbuf;
		break;
	case Gtk::STATE_SELECTED:
		pixbuf = _selected_pixbuf;
		break;
	case Gtk::STATE_INSENSITIVE:
		pixbuf = _insensitive_pixbuf;
		break;
	case Gtk::STATE_NORMAL:
	default: 
		pixbuf = _normal_pixbuf;
		break;
	}

	if (!pixbuf) {
		pixbuf = _normal_pixbuf;
	}

	// pixbuf, if any
	if (pixbuf) {
		Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context ();

		cr->rectangle (origx, origy, get_width (), get_height ());
		cr->clip ();

		int pbwidth = pixbuf->get_width ();
		int pbheight = pixbuf->get_height ();
		int width = (_image_width < 0 ? get_width () :_image_width);
		int height = (_image_height < 0 ? get_height () :_image_height);
		double xscale = 1.0;
		double yscale = 1.0;
		
		if ((width != pbwidth) || (height != pbheight)) {
			xscale = double (width) / double (pbwidth);
			yscale = double (height) / double (pbheight);
			cr->scale (xscale, yscale);
		}

		gdk_cairo_set_source_pixbuf (cr->cobj (), pixbuf->gobj(), origx/xscale, origy/yscale);
		cr->paint ();
	}
	return true;
}

void
WavesIcon::set_image (const Glib::RefPtr<Gdk::Pixbuf>& img, Gtk::StateType state)
{
	switch (state) {
	case Gtk::STATE_ACTIVE:
		_active_pixbuf = img;
		break;
	case Gtk::STATE_PRELIGHT:
		_prelight_pixbuf = img;
		break;
	case Gtk::STATE_SELECTED:
		_selected_pixbuf = img;
		break;
	case Gtk::STATE_INSENSITIVE:
		_insensitive_pixbuf = img;
		break;
	case Gtk::STATE_NORMAL:
	default: 
		_normal_pixbuf = img;
		break;
	}

	if (get_state () == state) {
		queue_draw ();
	}
}
