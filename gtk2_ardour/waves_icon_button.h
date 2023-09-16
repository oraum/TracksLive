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

#ifndef __gtk2_waves_icon_button_h__
#define __gtk2_waves_icon_button_h__

#include "waves_button.h"

class WavesIconButton : public WavesButton
{
  public:
	WavesIconButton (const std::string& title = "");
	virtual ~WavesIconButton ();

	void set_normal_image (const Glib::RefPtr<Gdk::Pixbuf>& img);
	void set_active_image (const Glib::RefPtr<Gdk::Pixbuf>& img);
	void set_implicit_active_image (const Glib::RefPtr<Gdk::Pixbuf>& img);
	void set_inactive_image (const Glib::RefPtr<Gdk::Pixbuf>& img);	
	void set_prelight_image (const Glib::RefPtr<Gdk::Pixbuf>& img);
	void set_image_width (int image_width) { _image_width = image_width; }
	void set_image_height (int image_height) { _image_height = image_height; }
    void set_render_text (bool render_text);
	int image_width () const { return _image_width; }
	int image_height () const { return _image_height; }
    
    const bool get_render_text () const { return _render_text; }
    const Glib::RefPtr<Gdk::Pixbuf> get_normal_pixbuf () const { return _normal_pixbuf; }
    const Glib::RefPtr<Gdk::Pixbuf> get_active_pixbuf () const { return _active_pixbuf; }
    const Glib::RefPtr<Gdk::Pixbuf> get_implicit_active_pixbuf () const { return _implicit_active_pixbuf; }
    const Glib::RefPtr<Gdk::Pixbuf> get_inactive_pixbuf () const { return _inactive_pixbuf; }
    const Glib::RefPtr<Gdk::Pixbuf> get_prelight_pixbuf () const { return _prelight_pixbuf; }

  protected:
	void render (cairo_t *, cairo_rectangle_t*);

  private:
	Glib::RefPtr<Gdk::Pixbuf>   _normal_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _active_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _implicit_active_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _inactive_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _prelight_pixbuf;
	int _image_width;
	int _image_height;
    bool _render_text;
};

#endif /* __gtk2_waves_icon_button_h__ */
