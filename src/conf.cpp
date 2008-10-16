
#include "conf.h"
#include "log.h"

#include <iostream>
#include <fstream>
#include <map>
using namespace std;

#include <string.h>

#define MAX_CONF_DEPTH 16 //avoid stack overflows

map<string, list<string> > data;


void config_set (const string&n, const string&v)
{
	Log_debug ("config: `%s' := `%s'", n.c_str(), v.c_str() );
	data[n].push_front (v);
}

bool config_is_set (const string&n)
{
	return data.count (n);
}

bool config_get (const string&n, string&res)
{
	if (!data.count (n) ) return false;
	res = data[n].front();
	return true;
}

void config_get_list (const string&n, list<string>&v)
{
	if (data.count (n) ) v = data[n];
	else v = list<string>();
}

bool config_is_true (const string&name)
{
	if (!data.count (name) ) return false;
	const string&v = data[name].front();
	if (!v.length() ) return false;
	return (v[0] == 'y') || (v[0] == 'Y') || (v[0] == '1');
}

static bool is_white (char c)
{
	return (c == ' ') || (c == '\t');
}

#define include_directive "@include"

bool parse_file (const string& name, int depth = 0)
{
	if (depth > MAX_CONF_DEPTH) return false;

	ifstream file (name.c_str() );

	if (!file.is_open() ) {
		Log_error ("config: opening file `%s' failed", name.c_str() );
		return false;
	} else Log_debug ("config: reading from file `%s'", name.c_str() );

	string l;
	while (getline (file, l) ) {
		int len = l.length();
		int value_start = 0;
		int name_end = 0;
		if (!len) continue; //empty line
		if (l[0] == '#') continue; //comment
		while ( (name_end < len) && (!is_white (l[name_end]) ) ) ++name_end;
		if ( (!name_end) || (name_end >= len) ) continue;
		value_start = name_end;
		while ( (value_start < len) && is_white (l[value_start]) ) ++value_start;
		if (value_start >= len) {
			Log_error ("config: value missing in file `%s'",
				   name.c_str() );
			return false;
		}

		string name (l, 0, name_end);
		string value (l, value_start, len);
		if (name == include_directive) {
			if (!parse_file (value, depth + 1) ) return false;
		} else config_set (name, value);
	}

	Log_debug ("config: file `%s' loaded OK", name.c_str() );

	return true;
}

bool config_parse (int argc, char**argv)
{
	//commandline syntax is -option_name value -another_option value
	++argv; //get rid of executable name

	while (*argv) {
		if (! (**argv) ) continue;
		if (*argv[0] == '-') {
			char* option_name = (*argv) + 1;
			++argv;
			if (! (*argv) ) {
				Log_error ("config: missing value for commandline option `%s'", option_name);
				return false;
			}
			if (!strcmp (option_name, include_directive) ) {
				if (!parse_file (*argv, 0) ) return false;
			} else config_set (option_name, *argv);
		} else {
			Log_error ("config: bad cmdline option at `%s'",
				   *argv);
			return false;
		}
		++argv;
	}

	Log_debug ("config: everything parsed OK");

	return true;
}