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

#ifndef __gtk2_waves_icon_h__
#define __gtk2_waves_icon_h__

#include <gtkmm/image.h>

class WavesIcon : public Gtk::Image
{
  public:
	WavesIcon ();

	void set_image (const Glib::RefPtr<Gdk::Pixbuf>& img, Gtk::StateType state);
	void set_image_width (int image_width) { _image_width = image_width; }
	void set_image_height (int image_height) { _image_height = image_height; }
	int image_width () const { return _image_width; }
	int image_height () const { return _image_height; }

  protected:
	virtual bool on_expose_event (GdkEventExpose *evt);

  private:
	Glib::RefPtr<Gdk::Pixbuf>   _normal_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _active_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _insensitive_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _selected_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _prelight_pixbuf;
	int _image_width;
	int _image_height;
};

#endif /* __gtk2_waves_icon_h__ */
