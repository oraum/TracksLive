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

#ifndef __ardour_meter_h__
#define __ardour_meter_h__

#include <vector>
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/processor.h"
#include "pbd/fastlog.h"

#include "ardour/kmeterdsp.h"
#include "ardour/iec1ppmdsp.h"
#include "ardour/iec2ppmdsp.h"
#include "ardour/vumeterdsp.h"

namespace ARDOUR {

class BufferSet;
class ChanCount;
class Session;

/** Meters peaks on the input and stores them for access.
 */
class LIBARDOUR_API PeakMeter : public Processor {
public:
        PeakMeter(Session& s, const std::string& name);
        ~PeakMeter();

	void reset ();
	void reset_max ();

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	/* special method for meter, to ensure that it can always handle the maximum
	   number of streams in the route, no matter where we put it.
	*/

	void set_max_channels (const ChanCount&);

	/* tell the meter than no matter how many channels it can handle,
	   `in' is the number it is actually going be handling from
	   now on.
	*/

	void reflect_inputs (const ChanCount& in);
	void emit_configuration_changed ();

	/** Compute peaks */
	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);

	void activate ()   { }
	void deactivate () { }

	ChanCount input_streams () const { return current_meters; }
	ChanCount output_streams () const { return current_meters; }

    void publish_meters();
	float meter_level (uint32_t n, MeterType type);

	void set_type(MeterType t);
	MeterType get_type() { return _meter_type; }

	XMLNode& state (bool full);

	PBD::Signal1<void, MeterType> TypeChanged;
    PBD::Signal0<void> MetersUpdate;

private:
    float get_peak_power_and_drop_peak (uint32_t n);
    
	friend class IO;

	/** The number of meters that we are currently handling;
	 *  may be different to _configured_input and _configured_output
	 *  as it can be altered outside a ::configure_io by ::reflect_inputs.
	 */
	ChanCount current_meters;

	gint               _reset_dpm;

    std::vector<float> _falloff_dB; // falloff
	std::vector<float> _peak_buffer; // keeps current peaks
	std::vector<float> _peak_power;  // internal dB calculation is done on demand by UI
	std::vector<float> _max_peak_power; // internal dB calculation of maximum peak

	std::vector<Kmeterdsp *> _kmeter;
	std::vector<Iec1ppmdsp *> _iec1meter;
	std::vector<Iec2ppmdsp *> _iec2meter;
	std::vector<Vumeterdsp *> _vumeter;

	MeterType _meter_type;
};

} // namespace ARDOUR

#endif // __ardour_meter_h__
