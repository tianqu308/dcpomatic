/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "timeline_view.h"

class wxWindow;

class TimelineLabelsView : public TimelineView
{
public:
	TimelineLabelsView (Timeline& tl);

	dcpomatic::Rect<int> bbox () const;

	void set_3d (bool s);
	void set_subtitle (bool s);
	void set_atmos (bool s);

private:
	void do_paint (wxGraphicsContext* gc, std::list<dcpomatic::Rect<int> > overlaps);

	int _width;
	bool _threed;
	bool _subtitle;
	bool _atmos;
};
