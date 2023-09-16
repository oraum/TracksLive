/*
  Copyright (C) 2002 Paul Davis

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

#include <limits.h>

#include "ardour/meter.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/barcontroller.h>
#include "pbd/fastlog.h"

#include "ardour_ui.h"
#include "global_signals.h"
#include "level_meter.h"
#include "utils.h"
#include "logmeter.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

LevelMeterBase::LevelMeterBase (Session* s, PBD::EventLoop::InvalidationRecord* ir, FastMeter::Orientation o)
	: parent_invalidator(ir)
	, _meter (0)
	, _meter_orientation(o)
	, regular_meter_width (6)
	, meter_length (0)
	, thin_meter_width(2)
        , max_peak (minus_infinity())
        , meter_type (MeterPeak)
        , io_configuration_changed (false)
        , color_changed (false)
{
	set_session (s);

    ARDOUR_UI::config()->ParameterChanged.connect_same_thread (_config_connection, boost::bind (&LevelMeterBase::ui_parameter_changed, this, _1) );
    
	ColorsChanged.connect (sigc::mem_fun (*this, &LevelMeterBase::color_handler));
}

LevelMeterBase::~LevelMeterBase ()
{
    _config_connection.disconnect ();
    
	_meter_configuration_connection.disconnect();
	_meter_type_connection.disconnect();
	_parameter_connection.disconnect();
    _meter_update_connection.disconnect();

    for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); i++) {
        delete (*i).meter;
    }
}

void
LevelMeterBase::set_meter (PeakMeter* meter)
{
    _config_connection.disconnect ();
    
	_meter_configuration_connection.disconnect();
	_meter_type_connection.disconnect();
    _meter_update_connection.disconnect();
    input_configuration.reset ();
    output_configuration.reset ();
                
	_meter = meter;
	color_changed = true;

	if (_meter) {
		_meter->ConfigurationChanged.connect (_meter_configuration_connection, parent_invalidator, boost::bind (&LevelMeterBase::configuration_changed, this, _1, _2), gui_context());
		_meter->TypeChanged.connect (_meter_type_connection, parent_invalidator, boost::bind (&LevelMeterBase::meter_type_changed, this, _1), gui_context());
        _meter->MetersUpdate.connect_same_thread (_meter_update_connection, boost::bind (&LevelMeterBase::update_meters, this));
	}
    
    ARDOUR_UI::config()->ParameterChanged.connect_same_thread (_config_connection, boost::bind (&LevelMeterBase::ui_parameter_changed, this, _1) );
}

static float meter_lineup_cfg(MeterLineUp lul, float offset) {
	switch (lul) {
		case MeteringLineUp24:
			return offset + 6.0;
		case MeteringLineUp20:
			return offset + 2.0;
		case MeteringLineUp18:
			return offset;
		case MeteringLineUp15:
			return offset - 3.0;
		default:
			break;
	}
	return offset;
}

static float meter_lineup(float offset) {
	return meter_lineup_cfg(ARDOUR_UI::config()->get_meter_line_up_level(), offset);
}

static float vu_standard() {
	// note - default meter config is +2dB (france)
	switch (ARDOUR_UI::config()->get_meter_vu_standard()) {
		default:
		case MeteringVUfrench:   // 0VU = -2dBu
			return 0;
		case MeteringVUamerican: // 0VU =  0dBu
			return -2;
		case MeteringVUstandard: // 0VU = +4dBu
			return -6;
		case MeteringVUeight:    // 0VU = +8dBu
			return -10;
	}
}

void
LevelMeterBase::update_meters ()
{
	if (!_meter) {
		return;
	}

	vector<MeterInfo>::iterator i;
	uint32_t n;

	uint32_t nmidi = _meter->input_streams().n_midi();

    bool update_max_peak = false;
    
	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
		if ((*i).packed) {
            
            // Update max peak indicator value first
			const float mpeak = _meter->meter_level(n, MeterMaxPeak);
			if (mpeak > (*i).max_peak) {
				(*i).max_peak = mpeak;
			}
            
			if (mpeak > max_peak) {
				max_peak = mpeak;
                // send the update of MAX value if it's changed
                update_max_peak = true;
			}

			if (n < nmidi) {
				(*i).meter->set (_meter->meter_level (n, MeterPeak));
			} else {
				const float peak = _meter->meter_level (n, meter_type);
				if (meter_type == MeterPeak) {
					(*i).meter->set (log_meter (peak));
				} else if (meter_type == MeterIEC1NOR) {
					(*i).meter->set (meter_deflect_nordic (peak + meter_lineup(0)));
				} else if (meter_type == MeterIEC1DIN) {
					(*i).meter->set (meter_deflect_din (peak + meter_lineup_cfg(ARDOUR_UI::config()->get_meter_line_up_din(), 3.0)));
				} else if (meter_type == MeterIEC2BBC || meter_type == MeterIEC2EBU) {
					(*i).meter->set (meter_deflect_ppm (peak + meter_lineup(0)));
				} else if (meter_type == MeterVU) {
					(*i).meter->set (meter_deflect_vu (peak + vu_standard() + meter_lineup(0)));
				} else if (meter_type == MeterK12) {
					(*i).meter->set (meter_deflect_k (peak, 12), meter_deflect_k(_meter->meter_level(n, MeterPeak), 12));
				} else if (meter_type == MeterK14) {
					(*i).meter->set (meter_deflect_k (peak, 14), meter_deflect_k(_meter->meter_level(n, MeterPeak), 14));
				} else if (meter_type == MeterK20) {
					(*i).meter->set (meter_deflect_k (peak, 20), meter_deflect_k(_meter->meter_level(n, MeterPeak), 20));
				} else { // RMS
					(*i).meter->set (log_meter (peak), log_meter(_meter->meter_level(n, MeterPeak)));
				}
			}
		}
	}
    
    if (update_max_peak) {
        MaxPeakUpdated (max_peak);
    }
}

void
LevelMeterBase::ui_parameter_changed (string p)
{
	ENSURE_GUI_THREAD (*this, &LevelMeterBase::parameter_changed, p)

	if (p == "meter-hold") {
		vector<MeterInfo>::iterator i;
		uint32_t n;

		for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
                (*i).meter->set_hold_count ((uint32_t) floor(ARDOUR_UI::config()->get_meter_hold()));
            }
		}
	else if (p == "meter-line-up-level") {
		color_changed = true;
		_setup_meters ();
	}
	else if (p == "meter-style-led") {
		color_changed = true;
		_setup_meters ();
	}
	else if (p == "meter-peak") {
		vector<MeterInfo>::iterator i;
		uint32_t n;

		for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
			(*i).max_peak = minus_infinity();
		}
	}
}

void
LevelMeterBase::configuration_changed (ChanCount in, ChanCount out)
{
        if (in != input_configuration) {
                input_configuration = in;
                io_configuration_changed = true;
        }
        if (out != output_configuration) {
                output_configuration = out;
                io_configuration_changed = true;
        }
        if (io_configuration_changed) {
                _setup_meters ();
        }
}

void
LevelMeterBase::meter_type_changed (MeterType t)
{
	meter_type = t;
	color_changed = true;
	_setup_meters ();
	MeterTypeChanged(t);
}

void
LevelMeterBase::hide_all_meters ()
{
	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); ++i) {
		if ((*i).packed) {
			mtr_remove (*((*i).meter));
			(*i).packed = false;
		}
	}
}

void
LevelMeterBase::_setup_meters ()
{
 	if (!_meter) {
 		return; /* do it later or never */
 	}

	int32_t nmidi = _meter->input_streams().n_midi();
    uint32_t nmeters = _meter->input_streams().n_total();
    
	guint16 width;

	if (nmeters == 0) {
		return;
	}

	if (nmeters < 2) {
		width = regular_meter_width;
	} else {
		width = thin_meter_width;
	}

    while (meters.size() > nmeters) {
        delete meters.back ().meter;
        meters.pop_back ();
    }
    
	while (meters.size() < nmeters) {
		meters.push_back (MeterInfo());
	}

	for (int32_t n = nmeters-1; nmeters && n >= 0 ; --n) {
		uint32_t c[10];
		uint32_t b[4];
		float stp[4];
		int styleflags = ARDOUR_UI::config()->get_meter_style_led() ? 3 : 1;

		b[0] = ARDOUR_UI::config()->get_canvasvar_MeterBackgroundBot();
		b[1] = ARDOUR_UI::config()->get_canvasvar_MeterBackgroundTop();
		b[2] = ARDOUR_UI::config()->get_canvasvar_MeterHighlightBackgroundBot(); // red highlight gradient Bot
		b[3] = ARDOUR_UI::config()->get_canvasvar_MeterHighlightBackgroundTop(); // red highlight gradient Top

		if (n < nmidi) {

                        /* MIDI meter colors */

                        c[0] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor0();
			c[1] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor1();
			c[2] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor2();
			c[3] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor3();
			c[4] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor4();
			c[5] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor5();
			c[6] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor6();
			c[7] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor7();
			c[8] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor8();
			c[9] = ARDOUR_UI::config()->get_canvasvar_MidiMeterColor9();
			stp[0] = 115.0 *  32.0 / 128.0;
			stp[1] = 115.0 *  64.0 / 128.0;
			stp[2] = 115.0 * 100.0 / 128.0;
			stp[3] = 115.0 * 112.0 / 128.0;
		} else {

                        /* Audio meter colors */


                        c[0] = ARDOUR_UI::config()->get_canvasvar_MeterColor0();
			c[1] = ARDOUR_UI::config()->get_canvasvar_MeterColor1();
			c[2] = ARDOUR_UI::config()->get_canvasvar_MeterColor2();
			c[3] = ARDOUR_UI::config()->get_canvasvar_MeterColor3();
			c[4] = ARDOUR_UI::config()->get_canvasvar_MeterColor4();
			c[5] = ARDOUR_UI::config()->get_canvasvar_MeterColor5();
			c[6] = ARDOUR_UI::config()->get_canvasvar_MeterColor6();
			c[7] = ARDOUR_UI::config()->get_canvasvar_MeterColor7();
			c[8] = ARDOUR_UI::config()->get_canvasvar_MeterColor8();
			c[9] = ARDOUR_UI::config()->get_canvasvar_MeterColor9();

			switch (meter_type) {
				case MeterK20:
					stp[0] = 115.0 * meter_deflect_k(-40, 20);  //-20
					stp[1] = 115.0 * meter_deflect_k(-20, 20);  //  0
					stp[2] = 115.0 * meter_deflect_k(-18, 20);  // +2
					stp[3] = 115.0 * meter_deflect_k(-16, 20);  // +4
					c[0] = c[1] = 0x008800ff;
					c[2] = c[3] = 0x00ff00ff;
					c[4] = c[5] = 0xffff00ff;
					c[6] = c[7] = 0xffff00ff;
					c[8] = c[9] = 0xff0000ff;
					break;
				case MeterK14:
					stp[0] = 115.0 * meter_deflect_k(-34, 14);  //-20
					stp[1] = 115.0 * meter_deflect_k(-14, 14);  //  0
					stp[2] = 115.0 * meter_deflect_k(-12, 14);  // +2
					stp[3] = 115.0 * meter_deflect_k(-10, 14);  // +4
					c[0] = c[1] = 0x008800ff;
					c[2] = c[3] = 0x00ff00ff;
					c[4] = c[5] = 0xffff00ff;
					c[6] = c[7] = 0xffff00ff;
					c[8] = c[9] = 0xff0000ff;
					break;
				case MeterK12:
					stp[0] = 115.0 * meter_deflect_k(-32, 12);  //-20
					stp[1] = 115.0 * meter_deflect_k(-12, 12);  //  0
					stp[2] = 115.0 * meter_deflect_k(-10, 12);  // +2
					stp[3] = 115.0 * meter_deflect_k( -8, 12);  // +4
					c[0] = c[1] = 0x008800ff;
					c[2] = c[3] = 0x00ff00ff;
					c[4] = c[5] = 0xffff00ff;
					c[6] = c[7] = 0xffff00ff;
					c[8] = c[9] = 0xff0000ff;
					break;
				case MeterIEC2BBC:
					c[0] = c[1] = c[2] = c[3] = c[4] = c[5] = c[6] = c[7] = c[8] = c[9] =
						ARDOUR_UI::config()->color_by_name ("meter color BBC");
					stp[0] = stp[1] = stp[2] = stp[3] = 115.0;
					break;
				case MeterIEC2EBU:
					stp[0] = 115.0 * meter_deflect_ppm(-24); // ignored
					stp[1] = 115.0 * meter_deflect_ppm(-18);
					stp[2] = 115.0 * meter_deflect_ppm( -9);
					stp[3] = 115.0 * meter_deflect_ppm(  0); // ignored
					c[3] = c[2] = c[1];
					c[6] = c[7] = c[8] = c[9];
					break;
				case MeterIEC1NOR:
					stp[0] = 115.0 * meter_deflect_nordic(-30); // ignored
					stp[1] = 115.0 * meter_deflect_nordic(-18);
					stp[2] = 115.0 * meter_deflect_nordic(-12);
					stp[3] = 115.0 * meter_deflect_nordic( -9); // ignored
					//c[2] = c[3] = c[1]; // dark-green
					c[0] = c[1] = c[2]; // bright-green
					c[6] = c[7] = c[8] = c[9];
					break;
				case MeterIEC1DIN:
					stp[0] = 115.0 * meter_deflect_din(-29); // ignored
					stp[1] = 115.0 * meter_deflect_din(-18);
					stp[2] = 115.0 * meter_deflect_din(-15); // ignored
					stp[3] = 115.0 * meter_deflect_din( -9);
					c[0] = c[1] = c[2] = c[3] = 0x00aa00ff;
					c[4] = c[6];
					c[5] = c[7];
					break;
				case MeterVU:
					stp[0] = 115.0 * meter_deflect_vu(-26); // -6
					stp[1] = 115.0 * meter_deflect_vu(-23); // -3
					stp[2] = 115.0 * meter_deflect_vu(-20); // 0
					stp[3] = 115.0 * meter_deflect_vu(-18); // +2
					c[0] = c[1] = c[2] = c[3] = c[4] = c[5] = 0x00aa00ff;
					c[6] = c[7] = c[8] = c[9] = 0xff8800ff;
					break;
				default: // PEAK, RMS
					stp[1] = 74.9;  // 115 * log_meter(-10)
					stp[2] = 87.49;  // 115 * log_meter(-3)
					stp[3] = 100.0; // 115 * log_meter(0)
				switch (ARDOUR_UI::config()->get_meter_line_up_level()) {
					case MeteringLineUp24:
						stp[0] = 42.0;
						break;
					case MeteringLineUp20:
						stp[0] = 50.0;
						break;
					default:
					case MeteringLineUp18:
						stp[0] = 55.0;
						break;
					case MeteringLineUp15:
						stp[0] = 62.5;
						break;
				}
			}
		}

		if (meters[n].width != width || meters[n].length != meter_length || io_configuration_changed || color_changed || meter_type != visible_meter_type) {
			bool hl = meters[n].meter ? meters[n].meter->get_highlight() : false;

                        /* Need a new meter because some property (width, length, IO configuration, color, meter type) has been 
                           changed. Unpack the old one and delete it
                        */

                        if (meters[n].meter && meters[n].meter->get_parent()) {
                                meters[n].meter->get_parent()->remove (*meters[n].meter);
                                meters[n].packed = false;
                        }
			delete meters[n].meter;

                        /* Create a new one */
                        
			meters[n].meter = new FastMeter ((uint32_t) floor (ARDOUR_UI::config()->get_meter_hold()), width, _meter_orientation, meter_length,
					c[0], c[1], c[2], c[3], c[4],
					c[5], c[6], c[7], c[8], c[9],
					b[0], b[1], b[2], b[3],
					stp[0], stp[1], stp[2], stp[3],
					styleflags
					);
			
			meters[n].meter->set_highlight(hl);
			meters[n].width = width;
			meters[n].length = meter_length;
			meters[n].meter->add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
			meters[n].meter->signal_button_press_event().connect (sigc::mem_fun (*this, &LevelMeterBase::meter_button_press));
			meters[n].meter->signal_button_release_event().connect (sigc::mem_fun (*this, &LevelMeterBase::meter_button_release));
                        
                        mtr_pack (*meters[n].meter);
                        meters[n].meter->show_all ();
                        meters[n].packed = true;
                }
	}

        io_configuration_changed = false;
	color_changed = false;
	visible_meter_type = meter_type;
}

void
LevelMeterBase::set_type(MeterType t)
{
	meter_type = t;
	_meter->set_type(t);
}

bool
LevelMeterBase::meter_button_press (GdkEventButton* ev)
{
	return ButtonPress (ev); /* EMIT SIGNAL */
}

bool
LevelMeterBase::meter_button_release (GdkEventButton* ev)
{
	if (ev->button == 1) {
		clear_meters (false);
	}
	ButtonRelease(ev);

	return true;
}


void LevelMeterBase::clear_meters (bool reset_highlight)
{
	for (vector<MeterInfo>::iterator i = meters.begin(); i < meters.end(); i++) {
		(*i).meter->clear();
		(*i).max_peak = minus_infinity();
		if (reset_highlight)
			(*i).meter->set_highlight(false);
	}
    max_peak = minus_infinity();
}

void LevelMeterBase::hide_meters ()
{
	hide_all_meters();
}

void
LevelMeterBase::color_handler ()
{
        cerr << this << " colors changes color\n";
	color_changed = true;
	_setup_meters ();
}

LevelMeterHBox::LevelMeterHBox(Session* s)
	: LevelMeterBase(s, invalidator(*this))
{
	set_spacing(1);
	show();
}

LevelMeterHBox::~LevelMeterHBox() {}

void
LevelMeterHBox::setup_meters (int width /* =3 */, int thin /* = 2 */)
{
	Gtk::Requisition sz;
	size_request (sz);
	meter_length = sz.height;
        if (meter_length > 2) {
                meter_length -= 2;
        }
	regular_meter_width = width;
	thin_meter_width = thin;
	_setup_meters ();
}

void
LevelMeterHBox::mtr_pack(Gtk::Widget &w) {
	pack_end (w, false, false);
}

void
LevelMeterHBox::mtr_remove(Gtk::Widget &w) {
	remove (w);
}


LevelMeterVBox::LevelMeterVBox(Session* s)
	: LevelMeterBase(s, invalidator(*this), FastMeter::Horizontal)
{
	set_spacing(1);
	show();
}
LevelMeterVBox::~LevelMeterVBox() {}

void
LevelMeterVBox::setup_meters (int width /* =3 */, int thin /* = 2 */)
{
	Gtk::Requisition sz;
	size_request (sz);
	meter_length = sz.width - 2;
	regular_meter_width = width;
	thin_meter_width = thin;
	_setup_meters ();
}

void
LevelMeterVBox::mtr_pack(Gtk::Widget &w) {
	pack_end (w, false, false);
}

void
LevelMeterVBox::mtr_remove(Gtk::Widget &w) {
	remove (w);
}
