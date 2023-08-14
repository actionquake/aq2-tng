/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//
// tng_net.c - CURL integration for various uses
//

#include "g_local.h"

cvar_t* logfile_name;
void Write_MsgToLog(int logType, const char* msg, ...)
{
	va_list	argptr;
	char	msg_print[1024];
	char	msg_cpy[1024];
	char	logpath[MAX_QPATH];
	char	*logsuffix;
	char	*JSONname;
	FILE* 	f;
	logfile_name = gi.cvar("logfile_name", "", CVAR_NOSET);
	int eventtime = (int)time(NULL);

	// Do nothing if logfile_msgs is 0
	if(!logfile_msgs->value)
		return;

	// Take va args and format into a single char
	va_start(argptr, msg);
	vsprintf(msg_cpy, msg, argptr);
	va_end(argptr);

	switch(logType) {
		case 0:
			Q_strncpyz(logsuffix, "stats", sizeof(logsuffix));
			Q_strncpyz(msg_print, msg_cpy, sizeof(msg_print));
			break;
		case 1:
			Q_strncpyz(JSONname, "chat", sizeof(JSONname));
			break;
		case 2:
			Q_strncpyz(JSONname, "server", sizeof(JSONname));
			break;
		case 3:
			Q_strncpyz(JSONname, "vote", sizeof(JSONname));
			break;
	}

	if (logType > 0) {
		Q_strncpyz(logsuffix, "srv", sizeof(logsuffix));

		// JSONify if not stats (already JSON-ified)
		Com_sprintf(
			msg_print, sizeof(msg_print),
			"{\"%s\":{\"sid\":\"%s\",\"time\":\"%d\"\"msg\":\"%s\"}}\n",
			JSONname,
			server_id->string,
			eventtime,
			msg_cpy
		);
	}

	// Log to the named file
	sprintf(logpath, "action/logs/%s.%s", logfile_name->string, logsuffix);

	if ((f = fopen(logpath, "a")) != NULL)
	{
		fprintf(f, "%s", msg_print);
		fclose(f);
	}
	else
		gi.dprintf("Write_MsgToLog: Error writing to %s.%s\n", logfile_name->string, logsuffix);

}