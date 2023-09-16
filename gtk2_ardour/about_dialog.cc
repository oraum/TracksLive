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

#include "pbd/file_utils.h"
#include "ardour/ardour.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"
#include "about_dialog.h"
#include "license_dialog.h"
#include "public_editor.h"
#include "utils.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

About::About ()
	: WavesDialog (_("about_dialog.xml"), true, false)
    , _image_home ( get_v_box("image_home") )
    , _about_button ( get_waves_button ("about_button") )
    , _credits ( get_label("credits") )
{
	set_modal (true);
	set_resizable (false);
    
	string image_path;
    
	if (find_file (ardour_data_search_path(), "splash.png", image_path)) {
		Gtk::Image* image;
		if ((image = manage (new Gtk::Image (image_path))) != 0) {
			_image_home.pack_start (*image, false, false);
		}
    }
   
    init_credits ();
    _about_button.signal_clicked.connect (sigc::mem_fun (*this, &About::about_button_pressed));
    
	show_all ();
}

void
About::init_credits ()
{
    ARDOUR::FPU_Optimization fpu_optimization = ARDOUR::get_fpu_optimization ();
    std::string optimization;
    switch (fpu_optimization) {
		case ARDOUR::SSE:
		optimization = "SSE";
    break;
    case ARDOUR::AVX:
		optimization = "AVX";
	break;
	case ARDOUR::APPLE_VECLIB:
		optimization = "Apple VecLib";
	break;
    case ARDOUR::NONE:
    default:
    break;
    }
    
    string text = "Version : " + ARDOUR_UI_UTILS::get_tracks_revision () + "\n" + optimization;

    _credits.set_text (text);
}

void
About::on_esc_pressed ()
{
    hide();
}

void
About::about_button_pressed (WavesButton*)
{
    ActionDisabler m;
    LicenseDialog license_dialog;
    license_dialog.set_position (WIN_POS_CENTER);
    license_dialog.run ();
}

void 
About::on_realize ()
{
	WavesDialog::on_realize();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_ALL));
}

About::~About ()
{
}
