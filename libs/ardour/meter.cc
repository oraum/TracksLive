/*
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <algorithm>
#include <cmath>
#include <limits>

#include "pbd/compose.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/midi_buffer.h"
#include "ardour/session.h"
#include "ardour/rc_configuration.h"
#include "ardour/runtime_functions.h"

using namespace std;

using namespace ARDOUR;

PeakMeter::PeakMeter (Session& s, const std::string& name)
    : Processor (s, string_compose ("meter-%1", name))
{
	Kmeterdsp::init(s.nominal_frame_rate());
	Iec1ppmdsp::init(s.nominal_frame_rate());
	Iec2ppmdsp::init(s.nominal_frame_rate());
	Vumeterdsp::init(s.nominal_frame_rate());
	_pending_active = true;
	_meter_type = MeterPeak;
    
    g_atomic_int_set (&_reset_dpm, 1);
}

PeakMeter::~PeakMeter ()
{
	while (_kmeter.size() > 0) {
		delete (_kmeter.back());
		delete (_iec1meter.back());
		delete (_iec2meter.back());
		delete (_vumeter.back());
		_kmeter.pop_back();
		_iec1meter.pop_back();
		_iec2meter.pop_back();
		_vumeter.pop_back();
	}
}


/** Get peaks from @a bufs
 * Input acceptance is lenient - the first n buffers from @a bufs will
 * be metered, where n was set by the last call to setup(), excess meters will
 * be set to 0.
 *
 * (runs in jack realtime context)
 */
void
PeakMeter::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}
    
	const bool do_reset_dpm = (bool)g_atomic_int_get(&_reset_dpm);
	g_atomic_int_set (&_reset_dpm, 0);

	// cerr << "meter " << name() << " runs with " << bufs.available() << " inputs\n";

	const uint32_t n_audio = min (current_meters.n_audio(), bufs.count().n_audio());
	const uint32_t n_midi  = min (current_meters.n_midi(), bufs.count().n_midi());

	uint32_t n = 0;

    // calculating falloff here will provide MAX timing accuracy
	const float falloff_dB = Config->get_meter_falloff() * nframes / _session.nominal_frame_rate();

    // *** NEEDS REFACTORING ***
    // should be done the way it's done for AudioTracks

	// Meter MIDI in to the first n_midi peaks
	for (uint32_t i = 0; i < n_midi; ++i, ++n) {
		float val = 0.0f;
		const MidiBuffer& buf (bufs.get_midi(i));
		
		for (MidiBuffer::const_iterator e = buf.begin(); e != buf.end(); ++e) {
			const Evoral::MIDIEvent<framepos_t> ev(*e, false);
			if (ev.is_note_on()) {
				const float this_vel = ev.buffer()[2] / 127.0;
				if (this_vel > val) {
					val = this_vel;
				}
			} else {
				val += 1.0 / bufs.get_midi(n).capacity();
				if (val > 1.0) {
					val = 1.0;
				}
			}
		}
        
		if (_peak_power[n] < (1.0 / 512.0)) {
			_peak_power[n] = 0;
		} else {
			/* empirical algorithm WRT to audio falloff times */
			_peak_power[n] -= sqrtf (_peak_power[n]) * falloff_dB * 0.045f;
		}
		_peak_power[n] = max(_peak_power[n], val);
		_max_peak_power[n] = minus_infinity();
	}

    
	// Meter audio in to the rest of the peaks
    
    // Meter displaying thread must read current peak
    // which is stored in _peak_buffer[n]
    // and reset it right after it has been read
	for (uint32_t i = 0; i < n_audio; ++i, ++n) {
		if (bufs.get_audio(i).silent()) {
			;
		} else {
            // idealy acess to _peak_buffer should be ATOMIC
			_peak_buffer[n] = compute_peak (bufs.get_audio(i).data(), nframes, _peak_buffer[n]);
		}

		if (do_reset_dpm) {
            // idealy acess to _peak_buffer should be ATOMIC
			_peak_buffer[n] = 0;
            _falloff_dB[n] = 0;
		} else {
            // calculate falloff
            if (_falloff_dB[n] < infinity() - falloff_dB ) {
                _falloff_dB[n] += falloff_dB;
			} else {
                _falloff_dB[n] = 0;
			}
		}

		if (_meter_type & (MeterKrms | MeterK20 | MeterK14 | MeterK12)) {
			_kmeter[i]->process(bufs.get_audio(i).data(), nframes);
		}
		if (_meter_type & (MeterIEC1DIN | MeterIEC1NOR)) {
			_iec1meter[i]->process(bufs.get_audio(i).data(), nframes);
		}
		if (_meter_type & (MeterIEC2BBC | MeterIEC2EBU)) {
			_iec2meter[i]->process(bufs.get_audio(i).data(), nframes);
		}
		if (_meter_type & MeterVU) {
			_vumeter[i]->process(bufs.get_audio(i).data(), nframes);
		}
	}

	_active = _pending_active;
}

void
PeakMeter::reset ()
{
	if (_active || _pending_active) {
        g_atomic_int_set (&_reset_dpm, 1);
	} else {
        // MAX calculation is not active for this meter
        for (size_t i = 0; i < _peak_buffer.size(); ++i) {
            _peak_buffer[i] = 0;
        }
    }
    
    // _peak_power[n] is calculated by displaying thread, so just reset these values
		for (size_t i = 0; i < _peak_power.size(); ++i) {
			_peak_power[i] = -std::numeric_limits<float>::infinity();
	}

	// these are handled async just fine.
	for (size_t n = 0; n < _kmeter.size(); ++n) {
		_kmeter[n]->reset();
		_iec1meter[n]->reset();
		_iec2meter[n]->reset();
		_vumeter[n]->reset();
	}
}

void
PeakMeter::reset_max ()
{
	if (!_active && !_pending_active) {
        // MAX calculation is not active for this meter
        for (size_t i = 0; i < _peak_buffer.size(); ++i) {
            _peak_buffer[i] = 0;
        }
    }
    
    for (size_t i = 0; i < _max_peak_power.size(); ++i) {
        _max_peak_power[i] = -std::numeric_limits<float>::infinity();
    }
}

bool
PeakMeter::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
PeakMeter::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}

	current_meters = in;

	set_max_channels (in);

	return Processor::configure_io (in, out);
}

void
PeakMeter::reflect_inputs (const ChanCount& in)
{
	reset();
	current_meters = in;
	reset_max();
	// ConfigurationChanged() postponed
}

void
PeakMeter::emit_configuration_changed () {
	ConfigurationChanged (current_meters, current_meters); /* EMIT SIGNAL */
}

void
PeakMeter::set_max_channels (const ChanCount& chn)
{
	uint32_t const limit = chn.n_total();
	const size_t n_audio = chn.n_audio();

	while (_peak_power.size() > limit) {
		_peak_buffer.pop_back();
		_peak_power.pop_back();
		_max_peak_power.pop_back();
        _falloff_dB.pop_back();
	}

	while (_peak_power.size() < limit) {
		_peak_buffer.push_back(0);
		_peak_power.push_back(-std::numeric_limits<float>::infinity());
		_max_peak_power.push_back(-std::numeric_limits<float>::infinity());
        _falloff_dB.push_back(0);
	}

	assert(_peak_buffer.size() == limit);
	assert(_peak_power.size() == limit);
	assert(_max_peak_power.size() == limit);
    assert(_falloff_dB.size() == limit);

	/* alloc/free other audio-only meter types. */
	while (_kmeter.size() > n_audio) {
		delete (_kmeter.back());
		delete (_iec1meter.back());
		delete (_iec2meter.back());
		delete (_vumeter.back());
		_kmeter.pop_back();
		_iec1meter.pop_back();
		_iec2meter.pop_back();
		_vumeter.pop_back();
	}
	while (_kmeter.size() < n_audio) {
		_kmeter.push_back(new Kmeterdsp());
		_iec1meter.push_back(new Iec1ppmdsp());
		_iec2meter.push_back(new Iec2ppmdsp());
		_vumeter.push_back(new Vumeterdsp());
	}
	assert(_kmeter.size() == n_audio);
	assert(_iec1meter.size() == n_audio);
	assert(_iec2meter.size() == n_audio);
	assert(_vumeter.size() == n_audio);

	reset();
	reset_max();
}

/** To be driven by the Meter signal from IO.
 * Caller MUST hold its own processor_lock to prevent reconfiguration
 * of meter size during this call.
 */

#define CHECKSIZE(MTR) (n < MTR.size() + n_midi && n >= n_midi)

float
PeakMeter::get_peak_power_and_drop_peak (uint32_t n) {
    
    if (n < _peak_power.size()) {
        
        if (_peak_power[n] >  -318.8f && Config->get_meter_falloff() != 0) {
            _peak_power[n] -= _falloff_dB[n];
        } else {
            _peak_power[n] = -std::numeric_limits<float>::infinity();
        }
        
        // determines meter level and peak
        // idealy acess to _peak_buffer should be ATOMIC
        _peak_power[n] = max(_peak_power[n], accurate_coefficient_to_dB(_peak_buffer[n]));
        
        // determines max peak indicator value
        _max_peak_power[n] = max(_max_peak_power[n], _peak_power[n]);
        
        // drop current peak
        _falloff_dB[n] = 0;
        
        // idealy acess to _peak_buffer should be ATOMIC
        _peak_buffer[n] = 0;
        
        return _peak_power[n];
    }
    
    return minus_infinity();
}

void
PeakMeter::publish_meters() {
    
    // must be called by UI thread only
    
    // update all peak_power and max_peak_power records
    for (uint32_t i = 0; i < _peak_power.size(); ++i) {
        get_peak_power_and_drop_peak(i);
    }
    
    // proparate an update request to all related meters
    MetersUpdate();
}

float
PeakMeter::meter_level(uint32_t n, MeterType type) {
	switch (type) {
		case MeterKrms:
		case MeterK20:
		case MeterK14:
		case MeterK12:
			{
				const uint32_t n_midi = current_meters.n_midi();
				if (CHECKSIZE(_kmeter)) {
					return accurate_coefficient_to_dB (_kmeter[n - n_midi]->read());
				}
			}
			break;
		case MeterIEC1DIN:
		case MeterIEC1NOR:
			{
				const uint32_t n_midi = current_meters.n_midi();
				if (CHECKSIZE(_iec1meter)) {
					return accurate_coefficient_to_dB (_iec1meter[n - n_midi]->read());
				}
			}
			break;
		case MeterIEC2BBC:
		case MeterIEC2EBU:
			{
				const uint32_t n_midi = current_meters.n_midi();
				if (CHECKSIZE(_iec2meter)) {
					return accurate_coefficient_to_dB (_iec2meter[n - n_midi]->read());
				}
			}
			break;
		case MeterVU:
			{
				const uint32_t n_midi = current_meters.n_midi();
				if (CHECKSIZE(_vumeter)) {
					return accurate_coefficient_to_dB (_vumeter[n - n_midi]->read());
				}
			}
			break;
		case MeterPeak:
		case MeterPeak0dB:
			if (n < _peak_power.size()) {
				return _peak_power[n];
			}
			break;
		case MeterMaxSignal:
			assert(0);
			break;
		default:
		case MeterMaxPeak:
			if (n < _max_peak_power.size()) {
				return _max_peak_power[n];
			}
			break;
	}
	return minus_infinity();
}

void
PeakMeter::set_type(MeterType t)
{
	if (t == _meter_type) {
		return;
	}

	_meter_type = t;

	if (t & (MeterKrms | MeterK20 | MeterK14 | MeterK12)) {
		const size_t n_audio = current_meters.n_audio();
		for (size_t n = 0; n < n_audio; ++n) {
			_kmeter[n]->reset();
		}
	}
	if (t & (MeterIEC1DIN | MeterIEC1NOR)) {
		const size_t n_audio = current_meters.n_audio();
		for (size_t n = 0; n < n_audio; ++n) {
			_iec1meter[n]->reset();
		}
	}
	if (t & (MeterIEC2BBC | MeterIEC2EBU)) {
		const size_t n_audio = current_meters.n_audio();
		for (size_t n = 0; n < n_audio; ++n) {
			_iec2meter[n]->reset();
		}
	}
	if (t & MeterVU) {
		const size_t n_audio = current_meters.n_audio();
		for (size_t n = 0; n < n_audio; ++n) {
			_vumeter[n]->reset();
		}
	}

	TypeChanged(t);
}

XMLNode&
PeakMeter::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "meter");
	return node;
}
