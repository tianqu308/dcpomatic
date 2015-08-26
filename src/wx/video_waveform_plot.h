/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/signals2.hpp>

namespace dcp {
	class OpenJPEGImage;
}

class PlayerVideo;
class Image;
class FilmViewer;

class VideoWaveformPlot : public wxPanel
{
public:
	VideoWaveformPlot (wxWindow* parent, FilmViewer* viewer);

	void set_enabled (bool e);
	void set_component (int c);
	void set_contrast (int b);

private:
	void paint ();
	void sized (wxSizeEvent &);
	void create_waveform ();
	void set_image (boost::weak_ptr<PlayerVideo>);

	boost::shared_ptr<dcp::OpenJPEGImage> _image;
	boost::shared_ptr<const Image> _waveform;
	bool _dirty;
	bool _enabled;
	int _component;
	int _contrast;

	static int const _vertical_margin;

	boost::signals2::connection _viewer_connection;
};