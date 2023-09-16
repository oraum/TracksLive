/*
    Copyright (C) 2012 Waves Audio Ltd.  

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

#include "device_connection_control.h"
#include "pbd/convert.h"

const char * DeviceConnectionControl::id_name = "_id_name";

DeviceConnectionControl::DeviceConnectionControl (const std::string& device_capture_name, bool active, uint16_t capture_number, const std::string& track_name)

	: Gtk::Layout(),
	WavesUI("device_capture_control.xml", *this)
    , _active(false)
	, _active_on_button (NULL)
	, _active_off_button (NULL)
	, _name_label (NULL)
	, _number_label (NULL)
	, _track_name_label (NULL)
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());

	_active_on_button = &get_waves_button ("capture_on_button");
	_active_off_button = &get_waves_button ("capture_off_button");
	_name_label = &get_label ("capture_name_label");
	_number_label = &get_label ("capture_number_label");
	_track_name_label  = &get_label ("track_name_label");

	init(device_capture_name, active, capture_number, track_name);
}

DeviceConnectionControl::DeviceConnectionControl (const std::string& device_playback_name, bool active, uint16_t playback_number)

	: Gtk::Layout()
	, WavesUI("device_playback_control.xml", *this)
    , _active(false)
	, _active_on_button (NULL)
	, _active_off_button (NULL)
	, _name_label (NULL)
	, _number_label (NULL)
	, _track_name_label (NULL)
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());

	_active_on_button = &get_waves_button ("playback_on_button");
	_active_off_button = &get_waves_button ("playback_off_button");
	_name_label = &get_label ("playback_name_label");
	_number_label = &get_label ("playback_number_label");
	
	init(device_playback_name, active, playback_number);
}


void DeviceConnectionControl::init(const std::string& name, bool active, uint16_t number, const std::string& track_name)
{
	_active_on_button->signal_clicked.connect (sigc::mem_fun (*this, &DeviceConnectionControl::on_active_on));
	_active_off_button->signal_clicked.connect (sigc::mem_fun (*this, &DeviceConnectionControl::on_active_off));

	if (_name_label != NULL) {
		_name_label->set_text (name);
		_name_label->set_tooltip_text(name);
	}

    set_number (number);
    set_track_name (track_name);
	set_active (active);
}


void
DeviceConnectionControl::set_number (uint16_t number)
{
	if (_number_label != NULL) {
		if (number == NoNumber) {
			_number_label->get_parent()->hide ();
		} else {
			_number_label->get_parent()->show ();
			_number_label->set_text(PBD::to_string (number, std::dec));
		}
	}
}

void
DeviceConnectionControl::set_active (bool active)
{
	_active_on_button->set_active (active);
	_active_off_button->set_active (!active);
    _active = active;
}


void
DeviceConnectionControl::set_track_name (const std::string& new_track_name)
{
    if (_track_name_label != NULL ) {
		_track_name_label->set_text (new_track_name);
		_track_name_label->set_tooltip_text(new_track_name);
        if (new_track_name.empty() ) {
            _track_name_label->get_parent()->hide();
        } else {
            _track_name_label->get_parent()->show();
        }
	}
}


std::string
DeviceConnectionControl::get_port_name ()
{
    std::string name;
    if (_name_label != NULL) {
        name = _name_label->get_text();
    }
    
    return name;
}


void
DeviceConnectionControl::on_active_on(WavesButton*)
{
    if (_active) {
        return;
    }
        
	set_active (true);
	signal_active_changed(this, true);
}

void
DeviceConnectionControl::on_active_off(WavesButton*)
{
    if (!_active) {
        return;
    }
        
	set_active (false);
	signal_active_changed(this, false);
}
