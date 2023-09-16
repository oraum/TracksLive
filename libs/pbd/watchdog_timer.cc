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

#include "pbd/watchdog_timer.h"

namespace PBD {

WatchdogTimer::WatchdogTimer ()
	: _timer_thread (0)
    , _timer_state (Stopped)
	, _terminate_timer (0)
	, _timeout_interval_usec (0)
{
    g_atomic_int_set(&_terminate_timer, 0);
    _timer_thread = Glib::Threads::Thread::create (boost::bind (&WatchdogTimer::timer_thread, this));
}

WatchdogTimer::~WatchdogTimer()
{
    _timeout_connection.disconnect ();
    
    {   // stop the timer thread
        // _timer_lock must be held
        // to change _terminate_timer variable and signal condition
        // this is a SPECIFIC OF POSIX CONDITION VARIABLE
        Glib::Threads::Mutex::Lock guard (_timer_lock);
        g_atomic_int_set (&_terminate_timer, 1);
        _timer_condition.signal ();
    }
    
    _timer_thread->join ();
    _timer_thread = 0;
}

void
WatchdogTimer::set(unsigned int new_interval_usec, const WatchdogHandler& new_handler)
{
    // stop timer
    cancel ();
    
    // _timer_lock must be held
    // to change state variable and signal condition
    // this is a SPECIFIC OF POSIX CONDITION VARIABLE
    Glib::Threads::Mutex::Lock guard (_timer_lock);
    
    // set new interval and callback
    g_atomic_int_set (&_timeout_interval_usec, new_interval_usec);
    
    _timeout_connection.disconnect ();
    _timeout_signal.connect_same_thread (_timeout_connection, new_handler);
    
    // start the timer thread
    g_atomic_int_set(&_timer_state, (gint)Running);
    _timer_condition.signal ();
}

void
WatchdogTimer::reset()
{
    // _timer_lock must be held
    // to change state variable and signal condition
    // this is a SPECIFIC OF POSIX CONDITION VARIABLE
    Glib::Threads::Mutex::Lock guard (_timer_lock);
    // reset: if timer is active - restart it
    const States state = (States)g_atomic_int_get(&_timer_state);
    
    if (state == Running) {
        _timer_condition.signal ();
    }
}
    
void
WatchdogTimer::cancel ()
{
    // _timer_lock must be held
    // to change state variable and signal condition
    // this is a SPECIFIC OF POSIX CONDITION VARIABLE
    Glib::Threads::Mutex::Lock guard (_timer_lock);
    const States state = (States)g_atomic_int_get(&_timer_state);
    
    if (state == Running) {
        g_atomic_int_set(&_timer_state, (gint)Stopped);
        _timer_condition.signal ();
    }
}

void
WatchdogTimer::timer_thread ()
{
    // timer LOOP, hold mutex to use _timer_condition
    // this is a SPECIFIC OF POSIX CONDITION VARIABLE
	Glib::Threads::Mutex::Lock guard (_timer_lock);

    while (!g_atomic_int_get(&_terminate_timer) ) {
        
        const States state = (States)g_atomic_int_get(&_timer_state);
        
        switch (state) {
            case Running:
            {
                // recalculate timeout into absolute time to wake up
                const unsigned int timeout_usec = g_atomic_int_get(&_timeout_interval_usec);
                const gint64 end_time_usec = g_get_monotonic_time() + timeout_usec;
                
                // wait with timeout,
                // calling thread releases the mutex before it is suspended
                // and takes the mutex again when it's woken up
                bool result = _timer_condition.wait_until (_timer_lock, end_time_usec);
                
                if (result != true /*TIME IS OUT*/) {
                    
                    // stop the timer
                    g_atomic_int_set(&_timer_state, (gint)Stopped);
                    
                    _timer_lock.unlock(); // do not hold a lock while executing user's callback
                    _timeout_signal(); // execute timeout callback
                    _timer_lock.lock(); // acquire the lock back to check the state
                    
                }

                break;
            }
            
            case Stopped:
            {
                // just fall asleep and wait for actions
                // calling thread releases the mutex before it is suspended
                // and takes the mutex again when it's woken up
                _timer_condition.wait (_timer_lock);
                
                break;
            }
                
            default:
                // must not be reachable
                assert(0);
                break;
        }
    }
}
    
} // namespace PBD
