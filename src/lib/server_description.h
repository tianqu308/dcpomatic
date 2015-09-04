/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SERVER_DESCRIPTION_H
#define DCPOMATIC_SERVER_DESCRIPTION_H

/** @class ServerDescription
 *  @brief Class to describe a server to which we can send encoding work.
 */
class ServerDescription
{
public:
	ServerDescription ()
		: _host_name ("")
		, _threads (1)
	{}

	/** @param h Server host name or IP address in string form.
	 *  @param t Number of threads to use on the server.
	 */
	ServerDescription (std::string h, int t)
		: _host_name (h)
		, _threads (t)
	{}

	/* Default copy constructor is fine */

	/** @return server's host name or IP address in string form */
	std::string host_name () const {
		return _host_name;
	}

	/** @return number of threads to use on the server */
	int threads () const {
		return _threads;
	}

	void set_host_name (std::string n) {
		_host_name = n;
	}

	void set_threads (int t) {
		_threads = t;
	}

private:
	/** server's host name */
	std::string _host_name;
	/** number of threads to use on the server */
	int _threads;
};

#endif