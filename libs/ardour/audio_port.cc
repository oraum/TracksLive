/*
    Copyright (C) 2006 Paul Davis

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

#include <cassert>

#include "pbd/stacktrace.h"

#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/audio_port.h"
#include "ardour/data_type.h"
#include "ardour/port_engine.h"

using namespace ARDOUR;
using namespace std;

#define port_engine AudioEngine::instance()->port_engine()

AudioPort::AudioPort (const std::string& name, PortFlags flags)
	: Port (name, DataType::AUDIO, flags)
	, _buffer (new AudioBuffer (0))
{
	assert (name.find_first_of (':') == string::npos);
}

AudioPort::~AudioPort ()
{
	delete _buffer;
}

void
AudioPort::cycle_start (pframes_t nframes)
{
	/* caller must hold process lock */

    Port::cycle_start (nframes);

	if (sends_output() ) {
        get_audio_buffer (nframes);
        _buffer->prepare ();
	}
}

void
AudioPort::cycle_end (pframes_t nframes)
{
    if (sends_output() && !_buffer->written()) {
		if (_buffer->capacity() >= nframes) {
            _buffer->silence (nframes);
        }
    }
}

void
AudioPort::cycle_split ()
{
}

AudioBuffer&
AudioPort::get_audio_buffer (pframes_t nframes)
{
	/* caller must hold process lock */
	_buffer->set_data ((Sample *) port_engine.get_buffer (_port_handle, _cycle_nframes) +
			   _global_port_buffer_offset + _port_buffer_offset, nframes);
	return *_buffer;
}

Sample* 
AudioPort::engine_get_whole_audio_buffer ()
{
	/* caller must hold process lock */
	return (Sample *) port_engine.get_buffer (_port_handle, _cycle_nframes);
}




