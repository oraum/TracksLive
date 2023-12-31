/*
    Copyright (C) 2013 Paul Davis
    Author: Robin Gareus

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

#ifndef __ardour_meter_strip__
#define __ardour_meter_strip__

#include <vector>

#include <cmath>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "route_ui.h"
#include "ardour_button.h"

#include "level_meter.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
	class Session;
}
namespace Gtk {
	class Window;
	class Style;
}

class MeterStrip :public RouteUI
{
  public:
	MeterStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	MeterStrip (int, ARDOUR::MeterType);
	~MeterStrip ();

	void set_session (ARDOUR::Session* s);
	void fast_update ();
	boost::shared_ptr<ARDOUR::Route> route() { return _route; }

	static PBD::Signal1<void,MeterStrip*> CatchDeletion;
	static PBD::Signal0<void> MetricChanged;
	static PBD::Signal0<void> ConfigurationChanged;

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	void set_meter_type_multi (int, ARDOUR::RouteGroup*, ARDOUR::MeterType);

	void set_metric_mode (int, ARDOUR::MeterType);
	int  get_metric_mode() { return _metricmode; }
//	void set_tick_bar (int);
	int  get_tick_bar() { return _tick_bar; }
	bool has_midi() { return _has_midi; }
	bool is_metric_display() { return _strip_type == 0; }
	ARDOUR::MeterType meter_type();

  protected:
	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList route_connections;
	PBD::ScopedConnectionList level_meter_connection;
	void self_delete ();

	//gint meter_metrics_expose (GdkEventExpose *);
	//gint meter_ticks1_expose (GdkEventExpose *);
	//gint meter_ticks2_expose (GdkEventExpose *);

	void on_theme_changed ();

	void on_size_allocate (Gtk::Allocation&);
	void on_size_request (Gtk::Requisition*);

	/* route UI */
	void update_rec_display ();
	std::string state_id() const;
	void set_button_names ();

  private:
//	Gtk::VBox mtr_vbox;
//	Gtk::VBox nfo_vbox;
//	Gtk::EventBox mtr_container;
//	Gtk::HSeparator mtr_hsep;
//	Gtk::HBox meterbox;
//	Gtk::HBox spacer;
//	Gtk::HBox namebx;
	WavesButton& name_label;
//	Gtk::DrawingArea meter_metric_area;
//	Gtk::DrawingArea meter_ticks1_area;
//	Gtk::DrawingArea meter_ticks2_area;

	WavesButton& peak_display;

	std::vector<ARDOUR::DataType> _types;
	ARDOUR::MeterType metric_type;

	float max_peak;
	bool _has_midi;
	int _tick_bar;
	int _strip_type;
	int _metricmode;

	Gtk::Box&       level_meter_home;
	LevelMeterHBox* level_meter;

	PBD::ScopedConnection _config_connection;
    PBD::ScopedConnectionList _global_meter_connections;
    
	void strip_property_changed (const PBD::PropertyChange&);
	void meter_configuration_changed (ARDOUR::ChanCount);
	void meter_type_changed (ARDOUR::MeterType);
	void update_background (ARDOUR::MeterType);

	bool peak_button_release (GdkEventButton*);

	void parameter_changed (std::string const & p);
//	void redraw_metrics ();
	void update_button_box ();
	void update_name_box ();

	bool _suspend_menu_callbacks;
	bool level_meter_button_release (GdkEventButton* ev);
	void popup_level_meter_menu (GdkEventButton* ev);
	void add_level_meter_type_item (Gtk::Menu_Helpers::MenuList&, Gtk::RadioMenuItem::Group&, std::string const &, ARDOUR::MeterType);

	bool name_label_button_release (GdkEventButton* ev);
	void popup_name_label_menu (GdkEventButton* ev);
	void add_label_height_item (Gtk::Menu_Helpers::MenuList&, Gtk::RadioMenuItem::Group&, std::string const &, uint32_t);

	void set_meter_type (ARDOUR::MeterType mode);
	void set_label_height (uint32_t);
};

#endif /* __ardour_mixer_strip__ */
