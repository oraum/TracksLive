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

#ifndef __gtk2_waves_button_h__
#define __gtk2_waves_button_h__

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>

#include "pbd/signals.h"
#include "gtkmm2ext/binding_proxy.h"
#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"

class WavesButton : public CairoWidget , public Gtkmm2ext::Activatable
{
  public:
	
	enum TextAlignment {
		ALIGN_LEFT = 0,
		ALIGN_CENTER,
		ALIGN_RIGHT
	};
	
	WavesButton (const std::string& text = "");
	virtual ~WavesButton ();

	void set_toggleable (bool toggleable) { _toggleable = toggleable; }
	bool get_toggleable () { return _toggleable; }

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);
	bool get_act_on_release () const { return _act_on_release; }
	void set_act_on_release (bool act_on_release) { _act_on_release = act_on_release; }
	
	void set_corner_radius (float);
	void set_rounded_corner_mask (int);

	void set_text (const std::string&);
    const std::string& get_text() { return _text; }
	void set_angle (const double);
	void set_border_width(float, float, float, float);
	void set_border_width(const char*);
	void set_border_color(const char*);
	void set_text_alignment (TextAlignment);
	TextAlignment get_text_alignment () const { return _text_alignment; }
	void set_text_indent (int);
	int get_text_indent () const { return _text_indent; }
	
	Glib::RefPtr<Pango::Layout> layout() const { return _layout; }

	boost::shared_ptr<PBD::Controllable> get_controllable() { return _binding_proxy.get_controllable(); }
 	void set_controllable (boost::shared_ptr<PBD::Controllable> c);
	void watch ();

	void set_related_action (Glib::RefPtr<Gtk::Action>);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

	sigc::signal1<void, WavesButton*> signal_clicked;
	sigc::signal1<void, WavesButton*> signal_double_clicked;

	
  protected:
	struct RGBA {
		float red;
		double green;
		double blue;
		double alpha;
		RGBA() : red (0), green (0), blue (0), alpha (0) {}
	};

    void render (cairo_t *, cairo_rectangle_t*);
    void render_text (cairo_t *);

	void on_size_request (Gtk::Requisition* req);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
    void on_realize ();

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;
    
	void color_handler ();

	void action_toggled ();

	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();
	
	int   _text_width;
	int   _text_height;
	float _corner_radius;
	int   _corner_mask;
	float _left_border_width;
	float _top_border_width;
	float _right_border_width;
	float _bottom_border_width;
	double _angle;
	struct RGBA _border_color;
	bool _toggleable;
	bool _act_on_release;
	bool _hovering;
	bool _pushed;
	bool _pushed_active;
	Glib::RefPtr<Pango::Layout> _layout;
	std::string                 _text;
	int _text_indent;
	TextAlignment _text_alignment;
	
private:
		
	static void __prop_style_watcher(WavesButton *);
	void _prop_style_watcher();
	Gtk::Label* _find_label (Gtk::Container *child);
	Gtk::Entry* _find_entry (Gtk::Container *child);

	BindingProxy _binding_proxy;
};

#endif /* __gtk2_waves_button_h__ */
