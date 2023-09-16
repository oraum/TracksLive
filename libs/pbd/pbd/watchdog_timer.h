/*
    Copyright (C) 2014 Tim Mayberry

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

#ifndef __libpbd_timer_h__
#define __libpbd_timer_h__

#include <pbd/signals.h>
#include "pbd/libpbd_visibility.h"

namespace PBD {

/**
 * The Timer class is a wrapper around Glib TimeoutSources
 * The Timer will start automatically when the first connection
 * is made and stop when the last callback is disconnected.
 */
class LIBPBD_API WatchdogTimer
{
public:
    typedef boost::function<void ()> WatchdogHandler;

	WatchdogTimer ();
    
    // set the timer with time interval in microsec
    void set (unsigned int new_interval_usec, const WatchdogHandler& new_handler);
    
    // reset timer - stop the timer and start it again with initial timeout
    // does nothing if the timer is currently stopped (canceled)
    void reset ();
    
    // stop (cancel) the timer
    void cancel ();

    // check if the timer is active
    bool active () { return Running == (States)g_atomic_int_get(&_timer_state); }
    
    ~WatchdogTimer();
    
private:

    enum States {
        Stopped = 0,
        Running = 1
    };
    
	WatchdogTimer(const WatchdogTimer&);
	WatchdogTimer& operator= (const WatchdogTimer&);

	void timer_thread ();

    Glib::Threads::Thread*                 _timer_thread;
    Glib::Threads::Cond                    _timer_condition;
    Glib::Threads::Mutex                   _timer_lock;
    gint                                   _timer_state;
    gint                                   _terminate_timer;
    
    PBD::ScopedConnection                  _timeout_connection;
	PBD::Signal0<void>                     _timeout_signal;
	unsigned int                           _timeout_interval_usec;
};

} //PBD

#endif