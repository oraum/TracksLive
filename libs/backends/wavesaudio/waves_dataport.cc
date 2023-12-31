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

#include "pbd/error.h"
#include <iostream>
#include "waves_dataport.h"

using namespace ARDOUR;

WavesDataPort::WavesDataPort (const std::string& inport_name, PortFlags inflags)
    : _name (inport_name)
    , _flags (inflags)
{
    _capture_latency_range.min = 
    _capture_latency_range.max = 
    _playback_latency_range.min = 
    _playback_latency_range.max = 0;
}


WavesDataPort::~WavesDataPort ()
{
    _disconnect_all ();
}


int WavesDataPort::connect (WavesDataPort *port)
{
    if (!port) {
        PBD::error << "WavesDataPort::connect (): invalid (null) port to connect to!" << endmsg;
        return -1;
    }

    if (type () != port->type ())    {
        PBD::error << "WavesDataPort::connect (): wrong type of the port to connect to!" << endmsg;
        return -1;
    }

    if (is_output () && port->is_output ()) {
        PBD::error << "WavesDataPort::connect (): attempt to connect output port to output port!" << endmsg;
        return -1;
    }

    if (is_input () && port->is_input ()) {
        PBD::error << "WavesDataPort::connect (): attempt to connect input port to input port!" << endmsg;
        return -1;
    }

    if (this == port) {
        PBD::error << "WavesDataPort::connect (): attempt to connect port to itself!" << endmsg;
        return -1; 
    }

    if (is_connected (port)) {
        // PBD::error << "WavesDataPort::connect (): the ports are already connected!" << std::endl;
        return -1;
    }

    _connect (port, true);
    return 0;
}


void WavesDataPort::_connect (WavesDataPort *port, bool api_call)
{
    _connections.push_back (port);
    if (api_call) {
        port->_connect (this, false);
    }
}


int WavesDataPort::disconnect (WavesDataPort *port)
{
    if (port == NULL) {
        PBD::error << "WavesDataPort::disconnect (): invalid (null) port to disconnect from!" << endmsg;
        return -1;
    }

    if (!is_connected (port)) {
        PBD::error << "WavesDataPort::disconnect (): the ports are not connected!" << endmsg;
        return -1;
    }

    _disconnect (port, true);

    return 0;
}


void WavesDataPort::_disconnect (WavesDataPort *port, bool api_call)
{
    std::vector<WavesDataPort*>::iterator it = std::find (_connections.begin (), _connections.end (), port);
    
    if (it != _connections.end ()) { // actually, it's supposed to be always true.
        _connections.erase (it);
    }

    if (api_call) {
        port->_disconnect (this, false);
    }

	if (is_input() && _connections.empty())	{
		_wipe_buffer();
	}
}


void WavesDataPort::disconnect_all ()
{
    _disconnect_all ();
    
	if (is_input())	{
		_wipe_buffer();
	}
}

void WavesDataPort::_disconnect_all ()
{
    while (!_connections.empty ()) {
        _connections.back ()->_disconnect (this, false);
        _connections.pop_back ();
    }
}

bool WavesDataPort::is_physically_connected () const
{
    for (std::vector<WavesDataPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
        if ((*it)->is_physical ()) {
            return true;
        }
    }

    return false;
}
