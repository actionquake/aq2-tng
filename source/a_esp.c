// Espionage Mode by darksaint
// File format inspired by a_dom.c by Raptor007 and a_ctf.c from TNG team
// Re-worked from scratch from the original AQDT, Black Monk and hal9000

#include "g_local.h"

espsettings_t espsettings;

cvar_t *esp_respawn = NULL;

unsigned int esp_team_effect[] = {
	EF_BLASTER | EF_TELEPORTER,
	EF_FLAG1,
	EF_FLAG2,
	EF_GREEN_LIGHT | EF_COLOR_SHELL
};

unsigned int esp_team_fx[] = {
	RF_GLOW,
	RF_FULLBRIGHT,
	RF_FULLBRIGHT,
	RF_SHELL_GREEN
};

int esp_marker_count = 0;
int esp_team_markers[ TEAM_TOP ] = {0};
int esp_winner = NOTEAM;
int esp_marker = 0;
int esp_pics[ TEAM_TOP ] = {0};
int esp_leader_pics[ TEAM_TOP ] = {0};
int esp_last_score = 0;

char *red_skin_name, *blue_skin_name, *green_skin_name;
char *red_team_name, *blue_team_name, *green_team_name;
char *red_leader_skin, *blue_leader_skin, *green_leader_skin;
char *red_leader_name, *blue_leader_name, *green_leader_name;

void EspMarkerThink( edict_t *marker )
{
	// If the marker was touched this frame, make it owned by that team.
	if( marker->owner && marker->owner->client && marker->owner->client->resp.team )
	{
		unsigned int effect = esp_team_effect[ marker->owner->client->resp.team ];
		if( marker->s.effects != effect )
		{
			char location[ 128 ] = "(";
			qboolean has_loc = false;
			edict_t *ent = NULL;

			marker->s.effects = effect;
			marker->s.renderfx = esp_team_fx[ marker->owner->client->resp.team ];
			esp_team_markers[ marker->owner->client->resp.team ] ++;

			if( marker->owner->client->resp.team == TEAM1 )
				marker->s.modelindex = esp_marker;

			// Get marker location if possible.
			has_loc = GetPlayerLocation( marker, location + 1 );
			if( has_loc )
				strcat( location, ") " );
			else
				location[0] = '\0';

			gi.bprintf( PRINT_HIGH, "%s has reached the %s for %s!\n",
				marker->owner->client->pers.netname,
				location,
				teams[ marker->owner->client->resp.team ].name );
				espsettings.capturestreak++;

			if( (esp_team_markers[ marker->owner->client->resp.team ] == esp_marker_count) && (esp_marker_count > 2) )
				gi.bprintf( PRINT_HIGH, "%s IS DOMINATING!\n",
				teams[ marker->owner->client->resp.team ].name );

			for( ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent ++ )
			{
				if( ! (ent->inuse && ent->client && ent->client->resp.team) )
					continue;
				else if( ent == marker->owner )
					unicastSound( ent, gi.soundindex("tng/markercap.wav"), 0.75 );
			}
		}
	}

	// Reset so the marker can be touched again.
	marker->owner = NULL;

	// Animate the marker waving.
	marker->s.frame = 173 + (((marker->s.frame - 173) + 1) % 16);

	marker->nextthink = level.framenum + FRAMEDIV;
}

void EspTouchMarker( edict_t *marker, edict_t *player, cplane_t *plane, csurface_t *surf )
{
	if( ! player->client )
		return;
	if( ! player->client->resp.team )
		return;
	if( (player->health < 1) || ! IS_ALIVE(player) )
		return;
	if( lights_camera_action || in_warmup )
		return;
	if( player->client->uvTime )
		return;
	// Player must be team leader on team 1 to activate the marker
	if (!IS_LEADER(player) && player->client->resp.team != TEAM1)
		return;

	// If the marker hasn't been touched this frame, the player will take it.
	if( ! marker->owner )
		marker->owner = player;
}


void EspMakeMarker( edict_t *marker )
{
	vec3_t dest = {0};
	trace_t tr = {0};

	VectorSet( marker->mins, -15, -15, -15 );
	VectorSet( marker->maxs,  15,  15,  15 );

	// Put the marker on the ground.
	VectorCopy( marker->s.origin, dest );
	dest[2] -= 128;
	tr = gi.trace( marker->s.origin, marker->mins, marker->maxs, dest, marker, MASK_SOLID );
	if( ! tr.startsolid )
		VectorCopy( tr.endpos, marker->s.origin );

	VectorCopy( marker->s.origin, marker->old_origin );

	marker->solid = SOLID_TRIGGER;
	marker->movetype = MOVETYPE_NONE;
	marker->s.modelindex = esp_marker;
	marker->s.skinnum = 0;
	marker->s.effects = esp_team_effect[ NOTEAM ];
	marker->s.renderfx = esp_team_fx[ NOTEAM ];
	marker->owner = NULL;
	marker->touch = EspTouchMarker;
	NEXT_KEYFRAME( marker, EspMarkerThink );
	marker->classname = "item_marker";
	marker->svflags &= ~SVF_NOCLIENT;
	gi.linkentity( marker );

	esp_marker_count ++;
}

void EspSetMarker(int team, char *str)
{
	char *marker_name;
	edict_t *ent = NULL;
	vec3_t position;

	marker_name = "item_marker_team1";

	if (sscanf(str, "<%f %f %f>", &position[0], &position[1], &position[2]) != 3)
		return;

	/* find and remove existing marker(s) if any */
	while ((ent = G_Find(ent, FOFS(classname), marker_name)) != NULL) {
		G_FreeEdict (ent);
	}

	ent = G_Spawn ();

	ent->classname = ED_NewString (marker_name);
	ent->spawnflags &=
		~(SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD |
		SPAWNFLAG_NOT_COOP | SPAWNFLAG_NOT_DEATHMATCH);

	VectorCopy(position, ent->s.origin);
	VectorCopy(position, ent->old_origin);

	ED_CallSpawn (ent);

}

void EspSetTeamSpawns(int team, char *str)
{
	edict_t *spawn = NULL;
	char *next;
	vec3_t pos;
	float angle;

	char *team_spawn_name = "info_player_team1";
	if(team == TEAM2)
		team_spawn_name = "info_player_team2";
	if (teamCount == 3){
		if(team == TEAM3)
			team_spawn_name = "info_player_team3";
	}

	/* find and remove all team spawns for this team */
	while ((spawn = G_Find(spawn, FOFS(classname), team_spawn_name)) != NULL) {
		G_FreeEdict (spawn);
	}

	next = strtok(str, ",");
	do {
		if (sscanf(next, "<%f %f %f %f>", &pos[0], &pos[1], &pos[2], &angle) != 4) {
			gi.dprintf("EspSetTeamSpawns: invalid spawn point: %s, expected <x y z a>\n", next);
			continue;
		}

		spawn = G_Spawn ();
		VectorCopy(pos, spawn->s.origin);
		spawn->s.angles[YAW] = angle;
		spawn->classname = ED_NewString (team_spawn_name);
		ED_CallSpawn (spawn);

		next = strtok(NULL, ",");
	} while(next != NULL);
}

void EspEnforceDefaultSettings(char *defaulttype)
{
	qboolean default_team = (Q_stricmp(defaulttype,"team")==0) ? true : false;
	qboolean default_respawn = (Q_stricmp(defaulttype,"respawn")==0) ? true : false;
	qboolean default_author = (Q_stricmp(defaulttype,"author")==0) ? true : false;

	//gi.dprintf("I was called to set the default for %s\n", defaulttype);

	if(default_author) {
		espsettings.mode = ESPMODE_ATL;
		Q_strncpyz(espsettings.author, "AQ2World Team", sizeof(espsettings.author));
		Q_strncpyz(espsettings.name, "Time for Action!", sizeof(espsettings.name));

		gi.dprintf(" Author           : %s\n", espsettings.author);
		gi.dprintf(" Name		  : %s\n", espsettings.name);
		gi.dprintf(" Game type        : Assassinate the Leader\n");
	}

	if(default_respawn) {
		teams[TEAM1].respawn_timer = ESP_RESPAWN_TIME;
		teams[TEAM2].respawn_timer = ESP_RESPAWN_TIME;
		if (teamCount == 3){
			teams[TEAM3].respawn_timer = ESP_RESPAWN_TIME;
		}
		gi.dprintf("  Respawn Rate: %d seconds\n", ESP_RESPAWN_TIME);
	}

	if(default_team) {
		/// Default skin/team/names - red team
		Q_strncpyz(teams[TEAM1].name, ESP_RED_TEAM, sizeof(teams[TEAM1].name));
		Q_strncpyz(teams[TEAM1].skin, ESP_RED_SKIN, sizeof(teams[TEAM1].skin));
		Q_strncpyz(teams[TEAM1].leader_name, ESP_RED_LEADER_NAME, sizeof(teams[TEAM1].leader_name));
		Q_strncpyz(teams[TEAM1].leader_skin, ESP_RED_LEADER_SKIN, sizeof(teams[TEAM1].leader_skin));
		/// Default skin/team/names - blue team
		Q_strncpyz(teams[TEAM2].name, ESP_BLUE_TEAM, sizeof(teams[TEAM2].name));
		Q_strncpyz(teams[TEAM2].skin, ESP_BLUE_SKIN, sizeof(teams[TEAM2].skin));
		Q_strncpyz(teams[TEAM2].leader_name, ESP_BLUE_LEADER_NAME, sizeof(teams[TEAM2].leader_name));
		Q_strncpyz(teams[TEAM2].leader_skin, ESP_BLUE_LEADER_SKIN, sizeof(teams[TEAM2].leader_skin));
		if(teamCount == 3) {
			/// Default skin/team/names - green team
			Q_strncpyz(teams[TEAM3].name, ESP_GREEN_TEAM, sizeof(teams[TEAM3].name));
			Q_strncpyz(teams[TEAM3].skin, ESP_GREEN_SKIN, sizeof(teams[TEAM3].skin));
			Q_strncpyz(teams[TEAM3].leader_name, ESP_GREEN_LEADER_NAME, sizeof(teams[TEAM3].leader_name));
			Q_strncpyz(teams[TEAM3].leader_skin, ESP_GREEN_LEADER_SKIN, sizeof(teams[TEAM3].leader_skin));
		}
		gi.dprintf("  Red Team: %s -- Skin: %s\n", ESP_RED_TEAM, ESP_RED_SKIN);
		gi.dprintf("  Red Leader: %s -- Skin: %s\n", ESP_RED_LEADER_NAME, ESP_RED_LEADER_SKIN);
		gi.dprintf("  Blue Team: %s -- Skin: %s\n", ESP_BLUE_TEAM, ESP_BLUE_SKIN);
		gi.dprintf("  Blue Leader: %s -- Skin: %s\n", ESP_BLUE_LEADER_NAME, ESP_BLUE_LEADER_SKIN);
		if(teamCount == 3){
			gi.dprintf("  Green Team: %s -- Skin: %s\n", ESP_GREEN_TEAM, ESP_GREEN_SKIN);
			gi.dprintf("  Green Leader: %s -- Skin: %s\n", ESP_GREEN_LEADER_NAME, ESP_GREEN_LEADER_SKIN);
		}
	}
}

qboolean EspLoadConfig(const char *mapname)
{
	char buf[1024];
	char *ptr;
	qboolean no_file = false;
	FILE *fh;

	memset(&espsettings, 0, sizeof(espsettings));

	esp_marker = gi.modelindex("models/esp/marker.md2");

	gi.dprintf("Trying to load Espionage configuration file\n", mapname);

	sprintf (buf, "%s/tng/%s.esp", GAMEVERSION, mapname);
	fh = fopen (buf, "r");
	if (!fh) {
		//Default to ATL mode in this case
		gi.dprintf ("Warning: Espionage configuration file \" %s \" was not found.\n", buf);
		espsettings.mode = 0;
		sprintf (buf, "%s/tng/default.esp", GAMEVERSION);
		fh = fopen (buf, "r");
		if (!fh){
			gi.dprintf ("Warning: Default Espionage configuration file was not found.\n");
			gi.dprintf ("Using hard-coded Assassinate the Leader scenario settings.\n");
			no_file = true;
		} else {
			gi.dprintf("Found %s, attempting to load it...\n", buf);
		}
	}

	// Hard-coded scenario settings so things don't break
	if(no_file){
		espsettings.mode = 0;
		// TODO: A better GHUD method to display this?
		gi.dprintf("-------------------------------------\n");
		gi.dprintf("Hard-coded Espionage configuration loaded\n");

		// Set game type to ATL
		/// Default game settings
		EspEnforceDefaultSettings("author");
		EspEnforceDefaultSettings("respawn");
		EspEnforceDefaultSettings("team");

		// No custom spawns, use default for map
		espsettings.custom_spawns = false;
	} else {

		gi.dprintf("-------------------------------------\n");
		gi.dprintf("Espionage configuration found at %s\n", buf);
		ptr = INI_Find(fh, "esp", "author");
		if(ptr) {
			gi.dprintf("- Author    : %s\n", ptr);
			Q_strncpyz(espsettings.author, ptr, sizeof(espsettings.author));
		}
		ptr = INI_Find(fh, "esp", "name");
		if(ptr) {
			gi.dprintf("- Name      : %s\n", ptr);
			Q_strncpyz(espsettings.name, ptr, sizeof(espsettings.name));
		}

		ptr = INI_Find(fh, "esp", "type");
		char *gametypename = ESPMODE_ATL_NAME;
		if(!strcmp(ptr, ESPMODE_ATL_SNAME) && !strcmp(ptr, ESPMODE_ETV_SNAME)){
			gi.dprintf("Warning: Value for '[esp] type is not 'etv' or 'atl', forcing ATL mode\n");
		    gi.dprintf("- Game type : %s\n", ESPMODE_ATL_NAME);
		} else {
			if(ptr) {
				if(strcmp(ptr, ESPMODE_ETV_SNAME) == 0){
					espsettings.mode = ESPMODE_ETV;
					gametypename = ESPMODE_ETV_NAME;
				}
				if(strcmp(ptr, ESPMODE_ATL_SNAME) == 0){
					espsettings.mode = ESPMODE_ATL;
					gametypename = ESPMODE_ATL_NAME;
				}
			}
			if (use_3teams->value) {
				// Only ATL available in 3 team mode
				espsettings.mode = ESPMODE_ATL;
				gametypename = ESPMODE_ATL_NAME;
			}
			gi.dprintf("- Game type : %s\n", gametypename);
		}
		// Force ATL mode trying to get ETV mode going with 3teams
		if (espsettings.mode == ESPMODE_ETV && use_3teams->value){
			gi.dprintf("%s and use_3team are incompatible, defaulting to %s", ESPMODE_ETV_NAME, ESPMODE_ATL_NAME);
			espsettings.mode = ESPMODE_ATL;
		}

		gi.dprintf("- Respawn times\n");
		char *r_respawn_time, *b_respawn_time, *g_respawn_time;

		r_respawn_time = INI_Find(fh, "respawn", "red");
		b_respawn_time = INI_Find(fh, "respawn", "blue");
		if(teamCount == 3)
			g_respawn_time = INI_Find(fh, "respawn", "green");
		else
			g_respawn_time = NULL;

		if ((!r_respawn_time || !b_respawn_time) || (teamCount == 3 && !g_respawn_time)){
			gi.dprintf("Warning: Malformed or missing settings for respawn times\n");
			gi.dprintf("Enforcing defaults\n");
			EspEnforceDefaultSettings("respawn");
		} else {
			if(r_respawn_time) {
				gi.dprintf("    Red     : %s seconds\n", r_respawn_time);
				teams[TEAM1].respawn_timer = atoi(r_respawn_time);
			}
			if(b_respawn_time) {
				gi.dprintf("    Blue    : %s seconds\n", b_respawn_time);
				teams[TEAM2].respawn_timer = atoi(b_respawn_time);
			}
			if (teamCount == 3){
				if(g_respawn_time) {
					gi.dprintf("    Green   : %s seconds\n", g_respawn_time);
					teams[TEAM3].respawn_timer = atoi(g_respawn_time);
				}
			}
		}

		// Only set the marker if the scenario is ETV
		if(espsettings.mode == ESPMODE_ETV) {
			gi.dprintf("- Target\n");
			ptr = INI_Find(fh, "target", "escort");
			if(ptr) {
				EspSetMarker(TEAM1, ptr);
			}
			ptr = INI_Find(fh, "target", "name");
			size_t ptr_len = strlen(ptr);
			if(ptr) {
				if (ptr_len <= MAX_ESP_STRLEN) {
					gi.dprintf("    Area    : %s\n", ptr);
				} else {
					gi.dprintf("Warning: [target] name > 32 characters, setting to \"The Spot\"");
					ptr = "The Spot";
				}
				Q_strncpyz(espsettings.target_name, ptr, sizeof(espsettings.target_name));
			}
		}

		char *r_spawnlist, *b_spawnlist, *g_spawnlist;

		r_spawnlist = INI_Find(fh, "spawns", "red");
		b_spawnlist = INI_Find(fh, "spawns", "blue");
		if(teamCount == 3)
			g_spawnlist = INI_Find(fh, "spawns", "green");

		if ((!r_spawnlist || !b_spawnlist) || (teamCount == 3 && !g_spawnlist)){
			gi.dprintf("Warning: Malformed or missing settings for spawn locations\n");
			gi.dprintf("Enforcing normal spawnpoints\n");
			espsettings.custom_spawns = false;
		} else {
			espsettings.custom_spawns = true;
			EspSetTeamSpawns(TEAM1, r_spawnlist);
			memcpy(espsettings.red_spawns, r_spawnlist, sizeof(espsettings.red_spawns));

			EspSetTeamSpawns(TEAM2, b_spawnlist);
			memcpy(espsettings.blue_spawns, b_spawnlist, sizeof(espsettings.blue_spawns));
			if (teamCount == 3){
				EspSetTeamSpawns(TEAM3, g_spawnlist);
				memcpy(espsettings.green_spawns, g_spawnlist, sizeof(espsettings.green_spawns));
			}
		}
		
		gi.dprintf("- Teams\n");

		// char team_attrs[3][4][32];
		// char* team_names[] = {"red_team", "blue_team", "green_team"};
		// char* attr_names[] = {"name", "skin", "leader_name", "leader_skin"};

		// // Use teamCount here to ignore the third element in team_names if only 2 teams
		// for (int i = 0; i < teamCount; i++) {
		// 	for (int j = 0; j < sizeof(attr_names); j++) {
		// 		ptr = INI_Find(fh, team_names[i], attr_names[j]);
		// 		Q_strncpyz(teams[i][j], ptr, sizeof(team_attrs[i][j]));
		// 	}
		// }

		ptr = INI_Find(fh, "red_team", "name");
		Q_strncpyz(teams[TEAM1].name, ptr, sizeof(teams[TEAM1].name));
		ptr = INI_Find(fh, "red_team", "skin");
		Q_strncpyz(teams[TEAM1].skin, ptr, sizeof(teams[TEAM1].skin));
		ptr = INI_Find(fh, "red_team", "leader_name");
		Q_strncpyz(teams[TEAM1].leader_name, ptr, sizeof(teams[TEAM1].leader_name));
		ptr = INI_Find(fh, "red_team", "leader_skin");
		Q_strncpyz(teams[TEAM1].leader_skin, ptr, sizeof(teams[TEAM1].leader_skin));

		ptr = INI_Find(fh, "blue_team", "name");
		Q_strncpyz(teams[TEAM2].name, ptr, sizeof(teams[TEAM2].name));
		ptr = INI_Find(fh, "blue_team", "skin");
		Q_strncpyz(teams[TEAM2].skin, ptr, sizeof(teams[TEAM2].skin));
		ptr = INI_Find(fh, "blue_team", "leader_name");
		Q_strncpyz(teams[TEAM2].leader_name, ptr, sizeof(teams[TEAM2].leader_name));
		ptr = INI_Find(fh, "blue_team", "leader_skin");
		Q_strncpyz(teams[TEAM2].leader_skin, ptr, sizeof(teams[TEAM2].leader_skin));

		if (teamCount == 3){
			ptr = INI_Find(fh, "green_team", "name");
			Q_strncpyz(teams[TEAM3].name, ptr, sizeof(teams[TEAM3].name));
			ptr = INI_Find(fh, "green_team", "skin");
			Q_strncpyz(teams[TEAM3].skin, ptr, sizeof(teams[TEAM3].skin));
			ptr = INI_Find(fh, "green_team", "leader_name");
			Q_strncpyz(teams[TEAM3].leader_name, ptr, sizeof(teams[TEAM3].leader_name));
			ptr = INI_Find(fh, "green_team", "leader_skin");
			Q_strncpyz(teams[TEAM3].leader_skin, ptr, sizeof(teams[TEAM3].leader_skin));
		}

		if((!teams[TEAM1].skin || !teams[TEAM1].name || !teams[TEAM1].leader_name || !teams[TEAM1].leader_skin ||
		!teams[TEAM2].skin || !teams[TEAM2].name || !teams[TEAM2].leader_name || !teams[TEAM2].leader_skin) || 
		((teamCount == 3) && (!teams[TEAM3].skin || !teams[TEAM3].name || !teams[TEAM3].leader_name || !teams[TEAM3].leader_skin))){
			gi.dprintf("Warning: Could not read value for team skin, name, leader or leader_skin, review your file\n");
			gi.dprintf("Enforcing defaults\n");
			EspEnforceDefaultSettings("team");
			espsettings.custom_skins = false;
		} else {
			espsettings.custom_skins = true;

			gi.dprintf("    Red Team: %s, Skin: %s\n", 
			teams[TEAM1].name, teams[TEAM1].skin);
			gi.dprintf("    Red Leader: %s, Skin: %s\n\n", 
			teams[TEAM1].leader_name, teams[TEAM1].leader_skin);
			
			gi.dprintf("    Blue Team: %s, Skin: %s\n", 
			teams[TEAM2].name, teams[TEAM2].skin);
			gi.dprintf("    Blue Leader: %s, Skin: %s\n\n", 
			teams[TEAM2].leader_name, teams[TEAM2].leader_skin);

			if(teamCount == 3) {
				gi.dprintf("    Green Team: %s, Skin: %s\n", 
				teams[TEAM3].name, teams[TEAM3].skin);
				gi.dprintf("    Green Leader: %s, Skin: %s\n", 
				teams[TEAM3].leader_name, teams[TEAM3].leader_skin);
			}
		}
	}

	// automagically change spawns *only* when we do not have team spawns
	// if(!espgame.custom_spawns)
	// 	ChangePlayerSpawns();

	gi.dprintf("-------------------------------------\n");

	if (fh)
		fclose(fh);

	// Load skin indexes
	Com_sprintf(teams[TEAM1].skin_index, sizeof(teams[TEAM1].skin_index), "players/%s_i", teams[TEAM1].skin);
	Com_sprintf(teams[TEAM2].skin_index, sizeof(teams[TEAM2].skin_index), "players/%s_i", teams[TEAM2].skin);
	Com_sprintf(teams[TEAM3].skin_index, sizeof(teams[TEAM3].skin_index), "players/%s_i", teams[TEAM3].skin);


	gi.dprintf("Debugging:\n");
	gi.dprintf("Espionage mode: %d\n", espsettings.mode);
	gi.dprintf("Skin index: %s\n", teams[TEAM1].skin_index);

	if((espsettings.mode == ESPMODE_ETV) && teamCount == 3){
		gi.dprintf("Warning: ETV mode requested with use_3teams enabled, forcing ATL mode");
		espsettings.mode = ESPMODE_ATL;
	}

	return true;
}

int EspGetRespawnTime(edict_t *ent)
{
	int spawntime = 0;
	if(ent->client->resp.team == TEAM1 && teams[TEAM1].respawn_timer > -1)
		spawntime = teams[TEAM1].respawn_timer;
	else if(ent->client->resp.team == TEAM2 && teams[TEAM2].respawn_timer > -1)
		spawntime = teams[TEAM2].respawn_timer;
	else if((teamCount == 3) && ent->client->resp.team == TEAM3 && teams[TEAM3].respawn_timer > -1)
		spawntime = teams[TEAM3].respawn_timer;

	gi.cprintf(ent, PRINT_HIGH, "You will respawn in %d seconds\n", spawntime);

	return spawntime;
}

void EspAssignTeam(gclient_t * who)
{
	edict_t *player;
	int i, team1count = 0, team2count = 0, team3count = 0;

	who->resp.esp_state = ESP_STATE_START;

	if (!DMFLAGS(DF_ESP_FORCEJOIN)) {
		who->resp.team = NOTEAM;
		return;
	}

	for (i = 1; i <= game.maxclients; i++) {
		player = &g_edicts[i];
		if (!player->inuse || player->client == who)
			continue;
		switch (player->client->resp.team) {
		case TEAM1:
			team1count++;
			break;
		case TEAM2:
			team2count++;
			break;
		case TEAM3:
			team3count++;
		}
	}
	if (team1count < team2count)
		who->resp.team = TEAM1;
	else if (team2count < team1count)
		who->resp.team = TEAM2;
	else if (team3count < team1count && team3count < team2count)
		who->resp.team = TEAM3;
	else if (rand() & 1)
		who->resp.team = TEAM1;
	else
		who->resp.team = TEAM2;

	teams_changed = true;
}

edict_t *SelectEspSpawnPoint(edict_t * ent)
{
	edict_t *spot, *spot1, *spot2;
	int count = 0;
	int selection;
	float range, range1, range2;
	char *cname;

	ent->client->resp.esp_state = ESP_STATE_PLAYING;

	switch (ent->client->resp.team) {
	case TEAM1:
		cname = "info_player_team1";
		break;
	case TEAM2:
		cname = "info_player_team2";
		break;
	case TEAM3:
		cname = "info_player_team3";
		break;
	default:
		/* FIXME: might return NULL when dm spawns are converted to team ones */
		return SelectRandomDeathmatchSpawnPoint();
	}

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	while ((spot = G_Find(spot, FOFS(classname), cname)) != NULL) {
		count++;
		range = PlayersRangeFromSpot(spot);
		if (range < range1) {
			if (range1 < range2) {
				range2 = range1;
				spot2 = spot1;
			}
			range1 = range;
			spot1 = spot;
		} else if (range < range2) {
			range2 = range;
			spot2 = spot;
		}
	}

	if (!count)
		return SelectRandomDeathmatchSpawnPoint();

	if (count <= 2) {
		spot1 = spot2 = NULL;
	} else
		count -= 2;

	selection = rand() % count;

	spot = NULL;
	do {
		spot = G_Find(spot, FOFS(classname), cname);
		if (spot == spot1 || spot == spot2)
			selection++;
	}
	while (selection--);

	return spot;
}

void EspScoreBonuses(edict_t * targ, edict_t * inflictor, edict_t * attacker)
{
	int i, enemyteam;
	gitem_t *flag_item, *enemy_flag_item;
	edict_t *ent, *flag, *leader;
	vec3_t v1, v2;

	leader = IS_LEADER(targ);

	// no bonus for fragging yourself
	if (!targ->client || !attacker->client || targ == attacker)
		return;

	enemyteam = (targ->client->resp.team != attacker->client->resp.team);
	if (!enemyteam)
		return;		// whoever died isn't on a team

	// // same team, if the flag at base, check to he has the enemy flag
	// flag_item = team_flag[targ->client->resp.team];
	// enemy_flag_item = team_flag[otherteam];

	// did the attacker frag the flag carrier?
	if (IS_LEADER(targ)){
		attacker->client->resp.esp_lasthurtleader = level.framenum;
		attacker->client->resp.score += ESP_LEADER_HARASS_BONUS;
		gi.cprintf(attacker, PRINT_MEDIUM,
			   "BONUS: %d points for killing the enemy leader!\n", ESP_LEADER_HARASS_BONUS);

		for (i = 1; i <= game.maxclients; i++) {
			ent = g_edicts + i;
			if (ent->inuse && ent->client->resp.team == enemyteam)
				ent->client->resp.esp_lasthurtleader = 0;
		}
		return;
	}

	if (targ->client->resp.ctf_lasthurtcarrier &&
	    level.framenum - targ->client->resp.ctf_lasthurtcarrier <
	    CTF_CARRIER_DANGER_PROTECT_TIMEOUT * HZ && !attacker->client->inventory[ITEM_INDEX(flag_item)]) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		attacker->client->resp.score += CTF_CARRIER_DANGER_PROTECT_BONUS;
		gi.bprintf(PRINT_MEDIUM,
			   "%s defends %s's flag carrier against an agressive enemy\n",
			   attacker->client->pers.netname, CTFTeamName(attacker->client->resp.team));
		IRC_printf(IRC_T_GAME,
			   "%n defends %n's flag carrier against an agressive enemy\n",
			   attacker->client->pers.netname,
			   CTFTeamName(attacker->client->resp.team));
		return;
	}
	// flag and flag carrier area defense bonuses
	// we have to find the flag and carrier entities
	// find the flag
	flag = NULL;
	while ((flag = G_Find(flag, FOFS(classname), flag_item->classname)) != NULL) {
		if (!(flag->spawnflags & DROPPED_ITEM))
			break;
	}

	if (!flag)
		return;		// can't find attacker's flag

	// find attacker's team's flag carrier
	// for (i = 1; i <= game.maxclients; i++) {
	// 	leader = g_edicts + i;
	// 	if (carrier->inuse && carrier->client->inventory[ITEM_INDEX(flag_item)])
	// 		break;
	// 	leader = NULL;
	// }

	// ok we have the attackers flag and a pointer to the carrier
	// check to see if we are defending the base's flag
	VectorSubtract(targ->s.origin, flag->s.origin, v1);
	VectorSubtract(attacker->s.origin, flag->s.origin, v2);

	if (VectorLength(v1) < CTF_TARGET_PROTECT_RADIUS || VectorLength(v2) < CTF_TARGET_PROTECT_RADIUS
		|| visible(flag, targ, MASK_SOLID) || visible(flag, attacker, MASK_SOLID)) {
		// we defended the base flag
		attacker->client->resp.score += CTF_FLAG_DEFENSE_BONUS;
		if (flag->solid == SOLID_NOT) {
			gi.bprintf(PRINT_MEDIUM, "%s defends the %s base.\n",
				   attacker->client->pers.netname, CTFTeamName(attacker->client->resp.team));
			IRC_printf(IRC_T_GAME, "%n defends the %n base.\n",
				   attacker->client->pers.netname,
				   CTFTeamName(attacker->client->resp.team));
		} else {
			gi.bprintf(PRINT_MEDIUM, "%s defends the %s flag.\n",
				   attacker->client->pers.netname, CTFTeamName(attacker->client->resp.team));
			IRC_printf(IRC_T_GAME, "%n defends the %n flag.\n",
				   attacker->client->pers.netname,
				   CTFTeamName(attacker->client->resp.team));
		}
		return;
	}

	if (leader && leader != attacker) {
		VectorSubtract(targ->s.origin, leader->s.origin, v1);
		VectorSubtract(attacker->s.origin, leader->s.origin, v1);

		if (VectorLength(v1) < CTF_ATTACKER_PROTECT_RADIUS ||
		    VectorLength(v2) < CTF_ATTACKER_PROTECT_RADIUS ||
			visible(leader, targ, MASK_SOLID) || visible(leader, attacker, MASK_SOLID)) {
			attacker->client->resp.score += CTF_CARRIER_PROTECT_BONUS;
			gi.bprintf(PRINT_MEDIUM, "%s defends the %s's flag carrier.\n",
				   attacker->client->pers.netname, CTFTeamName(attacker->client->resp.team));
			IRC_printf(IRC_T_GAME, "%n defends the %n's flag carrier.\n",
				   attacker->client->pers.netname,
				   CTFTeamName(attacker->client->resp.team));
			return;
		}
	}
}

void EspCheckHurtLeader(edict_t * targ, edict_t * attacker)
{
	int enemyteam;

	if (!targ->client || !attacker->client)
		return;

	// Enemy team is any team that is not the attacker's (supports >2 teams)
	enemyteam = (targ->client->resp.team != attacker->client->resp.team);
	if (!enemyteam)
		return;

	if (targ->client->resp.team != attacker->client->resp.team)
		attacker->client->resp.esp_lasthurtleader = level.framenum;
}

void SetEspStats( edict_t *ent )
{
	ent->client->ps.stats[STAT_TEAM1_HEADER] = level.pic_esp_teamtag[TEAM1];
	ent->client->ps.stats[STAT_TEAM2_HEADER] = level.pic_esp_teamtag[TEAM2];

	// Team scores for the score display and HUD.
	ent->client->ps.stats[ STAT_TEAM1_SCORE ] = teams[ TEAM1 ].score;
	ent->client->ps.stats[ STAT_TEAM2_SCORE ] = teams[ TEAM2 ].score;

	// Team icons for the score display and HUD.
	ent->client->ps.stats[ STAT_TEAM1_PIC ] = esp_pics[ TEAM1 ];
	ent->client->ps.stats[ STAT_TEAM2_PIC ] = esp_pics[ TEAM2 ];
	ent->client->ps.stats[ STAT_TEAM1_LEADERPIC ] = esp_leader_pics[ TEAM1 ];
	ent->client->ps.stats[ STAT_TEAM2_LEADERPIC ] = esp_leader_pics[ TEAM2 ];

	if (teamCount == 3) {
		ent->client->ps.stats[ STAT_TEAM3_SCORE ] = teams[ TEAM3 ].score;
		ent->client->ps.stats[ STAT_TEAM3_PIC ] = esp_pics[ TEAM3 ];
		ent->client->ps.stats[ STAT_TEAM3_LEADERPIC ] = esp_leader_pics[ TEAM3 ];
	}

	// During intermission, blink the team icon of the winning team.
	if( level.intermission_framenum && ((level.realFramenum / FRAMEDIV) & 8) )
	{
		if (esp_winner == TEAM1)
			ent->client->ps.stats[ STAT_TEAM1_PIC ] = 0;
		else if (esp_winner == TEAM2)
			ent->client->ps.stats[ STAT_TEAM2_PIC ] = 0;
		else if (teamCount == 3 && esp_winner == TEAM3)
			ent->client->ps.stats[ STAT_TEAM3_PIC ] = 0;
	}

	ent->client->ps.stats[STAT_ID_VIEW] = 0;
	if (!ent->client->pers.id)
		SetIDView(ent);
}

int EspOtherTeam(int team)
{
	// This is only used when there are 2 teams
	if(teamCount == 3)
		return -1;

	switch (team) {
	case TEAM1:
		return TEAM2;
	case TEAM2:
		return TEAM1;
	case NOTEAM:
		return NOTEAM; /* there is no other team for NOTEAM, but I want it back! */
	}
	return -1;		// invalid value
}

void EspSwapTeams()
{
	edict_t *ent;
	int i;

	for (i = 0; i < game.maxclients; i++) {
		ent = &g_edicts[1 + i];
		if (ent->inuse && ent->client->resp.team) {
			ent->client->resp.team = EspOtherTeam(ent->client->resp.team);
			AssignSkin(ent, teams[ent->client->resp.team].skin, false);
		}
	}

	/* swap scores too! */
	i = teams[TEAM1].score;
	teams[TEAM1].score = teams[TEAM2].score;
	teams[TEAM2].score = i;

	// Swap matchmode team leaders.
	ent = teams[TEAM1].leader;
	teams[TEAM1].leader = teams[TEAM2].leader;
	teams[TEAM2].leader = ent;

	teams_changed = true;
}

qboolean EspCheckRules(void)
{
	// Espionage ETV uses the same capturelimit cvars as CTF
	if(espsettings.mode == ESPMODE_ETV) {
		if( capturelimit->value && (teams[TEAM1].score >= capturelimit->value || teams[TEAM2].score >= capturelimit->value) )
		{
			gi.bprintf(PRINT_HIGH, "Capturelimit hit.\n");
			IRC_printf(IRC_T_GAME, "Capturelimit hit.\n");
			return true;
		}
	}

	if( timelimit->value > 0 && espsettings.mode > 0 )
	{
		if( espsettings.halftime == 0 && level.matchTime >= (timelimit->value * 60) / 2 - 60 )
		{
			if( use_warnings->value )
			{
				CenterPrintAll( "1 MINUTE LEFT..." );
				gi.sound( &g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD, gi.soundindex("tng/1_minute.wav"), 1.0, ATTN_NONE, 0.0 );
			}
			espsettings.halftime = 1;
		}
		else if( espsettings.halftime == 1 && level.matchTime >= (timelimit->value * 60) / 2 - 10 )
		{
			if( use_warnings->value )
				gi.sound( &g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD, gi.soundindex("world/10_0.wav"), 1.0, ATTN_NONE, 0.0 );
			espsettings.halftime = 2;
		}
		else if( espsettings.halftime < 3 && level.matchTime >= (timelimit->value * 60) / 2 + 1 && esp_etv_halftime->value && esp_mode->value == 1)
		{
			team_round_going = team_round_countdown = team_game_going = 0;
			MakeAllLivePlayersObservers ();
			EspSwapTeams();
			CenterPrintAll("The teams have been switched!");
			gi.sound(&g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD,
				 gi.soundindex("misc/secret.wav"), 1.0, ATTN_NONE, 0.0);
			espsettings.halftime = 3;
		}
	}
	return false;
}

qboolean AllTeamsHaveLeaders(void)
{
	int teamsWithLeaders = 0;

	//AQ2:TNG Slicer Matchmode
	if (matchmode->value && !TeamsReady())
		return false;
	//AQ2:TNG END

	for (int i = TEAM1; i <= teamCount; i++)
	{
		if (HAVE_LEADER(i)) {
			teamsWithLeaders++;
		}
	}

	// Only Team 1 needs a leader in ETV mode
	if((espsettings.mode == ESPMODE_ETV) && HAVE_LEADER(TEAM1))
		return true;

	if(teamsWithLeaders == teamCount){
		return true;
	}

	gi.dprintf("Leadercount: %d\n", teamsWithLeaders);
	return false;
}

void EspSetLeader( int teamNum, edict_t *ent )
{
	edict_t *oldLeader = teams[teamNum].leader;
	char temp[128];

	if (teamNum == NOTEAM)
		ent = NULL;

	// If Espionage is enabled, in ATL mode, also set captain as leader
	// If ETV mode is enabled, and the entity asking to become captain is on Team 1 (Red)
	if((esp_mode->value) == 0 || (esp_mode->value == 1 && teamNum == TEAM1)){
		teams[teamNum].leader = ent;
		if(ent) // Only assign a skin to an ent
			AssignSkin(ent, teams[teamNum].leader_skin, false);
	} else {
		// Do not set leader attribute to team 2 in ETV
		teams[teamNum].leader = NULL;
		gi.centerprintf(ent, "Only the Red team has a leader in this mode\n");
	}
	
	if (!ent) {
		if (!team_round_going || (gameSettings & GS_ROUNDBASED)) {
			if (teams[teamNum].ready) {
				Com_sprintf( temp, sizeof( temp ), "%s has lost their leader and is no longer ready to play!", teams[teamNum].name );
				CenterPrintAll( temp );
			}
			teams[teamNum].ready = 0;
		}
		if (oldLeader) {
			Com_sprintf(temp, sizeof(temp), "%s is no longer %s's leader\n", oldLeader->client->pers.netname, teams[teamNum].name );
			CenterPrintAll(temp);
			gi.bprintf( PRINT_HIGH, "%s needs a new leader!  Enter 'volunteer' to apply for duty\n", teams[teamNum].name );
		}
		teams[teamNum].locked = 0;
		return;
	}

	if (ent != oldLeader) {
		Com_sprintf(temp, sizeof(temp), "%s is now %s's leader\n", ent->client->pers.netname, teams[teamNum].name );
		CenterPrintAll(temp);
		gi.cprintf( ent, PRINT_CHAT, "You are the leader of '%s'\n", teams[teamNum].name );
		//gi.dprintf("%s became leader for team %d !  Clientnum check: %d %d!\n", ent->client->pers.netname, ent->client->resp.team, ent->client->clientNum, teams[ent->client->resp.team].leader->client->clientNum);
		gi.sound( &g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD, gi.soundindex( "misc/comp_up.wav" ), 1.0, ATTN_NONE, 0.0 );
		AssignSkin(ent, teams[teamNum].leader_skin, false);
	}
}

void ChooseRandomLeader(int teamNum)
{
	// Call this if esp_mustvolunteer is 0
	// or if a the leader of a team disconnects/leaves

	int players[TEAM_TOP] = { 0 }, i;
	edict_t *ent;

	if (matchmode->value && !TeamsReady())
		return;

	for (i = 0; i < game.maxclients; i++)
	{
		ent = &g_edicts[1 + i];
		if (!ent->inuse || game.clients[i].resp.team == NOTEAM)
			continue;
		if (!game.clients[i].resp.subteam)
			players[game.clients[i].resp.team]++;
	}

	if (ent->client->resp.team == teamNum) {
		if (matchmode->value && ent->client->resp.subteam == teamNum)
			;
		else
			// Congrats, you're the new leader
			EspSetLeader(teamNum, ent);
	}

}

void EspLeaderLeftTeam( edict_t *ent )
{
	int teamNum = ent->client->resp.team;

	if (teams[teamNum].leader == ent) {
		EspSetLeader( teamNum, NULL );
	}
	ent->client->resp.subteam = 0;

	// esp_mustvolunteer is off, anyone can get picked, except a bot
	if (!teams[teamNum].leader && !ent->is_bot) {
		if (!esp_mustvolunteer->value) {
			ChooseRandomLeader(teamNum);
		}
	}
}
