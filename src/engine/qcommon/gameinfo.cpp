/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 2017, Daemon Developers

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

#include <algorithm>
#include <string>

#include "gameinfo.h"
#include "common/FileSystem.h"


Gameinfo& Gameinfo::getInstance()
{
	static Gameinfo instance;
	return instance;
}

void Gameinfo::parse(std::string fname)
{
	if (!FS::RawPath::FileExists(fname))
	{
		Sys::Error("GAMEINFO: No gameinfo.conf found in libpath");
	}
	FS::File f = FS::RawPath::OpenRead(fname);
	std::string content = f.ReadAll();
	f.Close();
	Cmd::Args a(content);
	if (a.Argc() & 1)
	{
		Sys::Error("GAMEINFO: Invalid gaminfo.conf");
	}
	std::vector<std::string> argvec = a.ArgVector();
	for (int i = 0; i < argvec.size(); i += 2)
	{
		if(argvec[i] == "NAME")
		{
			_name = argvec[i+1];
		}
		else if(argvec[i] == "VERSION")
		{
			_version = argvec[i+1];
		}
		else if(argvec[i] == "DEFAULT_BASEPAK")
		{
			_default_basepak = argvec[i+1];
		}
		else if(argvec[i] == "MASTERSERVER1")
		{
			_masterserver1 = argvec[i+1];
		}
		else if(argvec[i] == "MASTERSERVER2")
		{
			_masterserver2 = argvec[i+1];
		}
		else if(argvec[i] == "MASTERSERVER3")
		{
			_masterserver3 = argvec[i+1];
		}
		else if(argvec[i] == "MASTERSERVER4")
		{
			_masterserver4 = argvec[i+1];
		}
		else if(argvec[i] == "MASTERSERVER5")
		{
			_masterserver5 = argvec[i+1];
		}
		else if(argvec[i] == "WWW_BASEURL")
		{
			_www_baseurl = argvec[i+1];
		}
		else if(argvec[i] == "GAMENAME_STRING")
		{
			_gamename_string = argvec[i+1];
		}
		else
		{
			Log::Warn("GAMEINFO: Unrecognized field '%s' in gameinfo.conf", argvec[i]);
		}
	}
}

std::string Gameinfo::name() const
{
	return _name;
}

std::string Gameinfo::name_lower() const
{
	std::string temp = _name;
	std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
	return temp;
}

std::string Gameinfo::name_upper() const
{
	std::string temp = _name;
	std::transform(temp.begin(), temp.end(), temp.begin(), ::toupper);
	return temp;
}

std::string Gameinfo::version() const
{
	return _version;
}

std::string Gameinfo::default_basepak() const
{
	return _default_basepak;
}

std::string Gameinfo::masterserver1() const
{
	return _masterserver1;
}

std::string Gameinfo::masterserver2() const
{
	return _masterserver2;
}

std::string Gameinfo::masterserver3() const
{
	return _masterserver3;
}

std::string Gameinfo::masterserver4() const
{
	return _masterserver4;
}

std::string Gameinfo::masterserver5() const
{
	return _masterserver5;
}

std::string Gameinfo::www_baseurl() const
{
	return _www_baseurl;
}

std::string Gameinfo::unnamed_server() const
{
	return _name + " " + _version + " Server";
}

std::string Gameinfo::gamename_string() const
{
	return _gamename_string;
}
