// Espionage Mode by darksaint
// File format inspired by a_dom.c by Raptor007 and a_ctf.c from TNG team
// Re-worked from scratch from the original AQDT team, Black Monk, hal9000 and crew

#include "g_local.h"

espsettings_t espsettings;

int esp_potential_spawns;
int esp_last_chosen_spawn = 0;

cvar_t *esp_respawn = NULL;
edict_t *espflag = NULL;

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

int esp_flag_count = 0;
int esp_team_flags[ TEAM_TOP ] = {0};
int esp_winner = NOTEAM;
int esp_flag = 0;
int esp_pics[ TEAM_TOP ] = {0};
int esp_leader_pics[ TEAM_TOP ] = {0};
int esp_last_score = 0;


/*
EspModeCheck

Returns which mode we are running
*/
int EspModeCheck()
{
	if (!esp->value)
		return -1; // Espionage is disabled
	if (atl->value)
		return ESPMODE_ATL; // ATL mode
	else if (etv->value)
		return ESPMODE_ETV; // ETV mode
	else
		return -1;
}

/*
Toggles between the two game modes
*/
void EspForceEspionage(int espmode)
{
	gi.cvar_forceset("esp", "1");
	if (espmode == 0) {
		gi.cvar_forceset("atl", "1");
		gi.cvar_forceset("etv", "0");
	} else if (espmode == 1) {
		gi.cvar_forceset("etv", "1");
		gi.cvar_forceset("atl", "0");
	}
}

int EspCapturePointOwner( edict_t *flag )
{
	if( flag->s.effects == esp_team_effect[ TEAM1 ] )
		return TEAM1;
	return NOTEAM;
}

void EspCapturePointThink( edict_t *flag )
{
	// If the flag was touched this frame, make it owned by that team.
	if( flag->owner && flag->owner->client && flag->owner->client->resp.team ) {
		unsigned int effect = esp_team_effect[ flag->owner->client->resp.team ];
		if( flag->s.effects != effect )
		{
			edict_t *ent = NULL;
			int prev_owner = EspCapturePointOwner( flag );

			gi.dprintf("prev flag owner team is %d\n", prev_owner);

			if( prev_owner != NOTEAM )
				esp_team_flags[ prev_owner ] --;

			flag->s.effects = effect;
			flag->s.renderfx = esp_team_fx[ flag->owner->client->resp.team ];
			esp_team_flags[ flag->owner->client->resp.team ] ++;
			flag->s.modelindex = esp_flag;

			// Get flag location if possible.

			// Commented out but may still be useful at some point
			// This grabs the map location name
			    // qboolean has_loc = false;
				// char location[ 128 ] = "(";
				// has_loc = GetPlayerLocation( flag, location + 1 );
				// if( has_loc )
				// 	strcat( location, ") " );
				// else
				// 	location[0] = '\0';

			gi.bprintf( PRINT_HIGH, "%s has reached the %s for %s!\n",
				flag->owner->client->pers.netname,
				espsettings.target_name,
				teams[ flag->owner->client->resp.team ].name );

				espsettings.capturestreak++;

				// This is the game state, where the leader made it to the capture point
				espsettings.escortcap = true;

			// Escort point captured, end round and start again
			gi.sound( &g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD, gi.soundindex("tng/leader_win.wav"), 1.0, ATTN_NONE, 0.0 );
			espsettings.escortcap = flag->owner->client->resp.team;
			if (esp_punish->value)
				EspPunishment(OtherTeam(flag->owner->client->resp.team));

			if( (esp_team_flags[ flag->owner->client->resp.team ] == esp_flag_count) && (esp_flag_count > 2) )
				gi.bprintf( PRINT_HIGH, "%s IS UNSTOPPABLE!\n",
				teams[ flag->owner->client->resp.team ].name );

			for( ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent ++ )
			{
				if( ! (ent->inuse && ent->client && ent->client->resp.team) )
					continue;
				else if( ent == flag->owner )
					unicastSound( ent, gi.soundindex("tng/flagcap.wav"), 0.75 );
			}
		}
	}
	flag->owner = NULL;

	flag->nextthink = level.framenum + FRAMEDIV;
}

void EspTouchCapturePoint( edict_t *flag, edict_t *player, cplane_t *plane, csurface_t *surf )
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
	// Player must be team leader on team 1 to activate the flag
	if (!IS_LEADER(player) || player->client->resp.team != TEAM1)
		return;

	// If the flag hasn't been touched this frame, the player will take it.
	if( ! flag->owner )
		flag->owner = player;
}

void EspMakeCapturePoint( edict_t *flag)
{
	vec3_t dest = {0};
	trace_t tr = {0};

	VectorSet( flag->mins, -15, -15, -15 );
	VectorSet( flag->maxs,  15,  15,  15 );

	// Put the flag on the ground.
	VectorCopy( flag->s.origin, dest );
	dest[2] -= 128;
	tr = gi.trace( flag->s.origin, flag->mins, flag->maxs, dest, flag, MASK_SOLID );
	if( ! tr.startsolid )
		VectorCopy( tr.endpos, flag->s.origin );

	VectorCopy( flag->s.origin, flag->old_origin );

	flag->solid = SOLID_TRIGGER;
	flag->movetype = MOVETYPE_NONE;
	flag->s.modelindex = esp_flag;
	flag->s.skinnum = 0;
	flag->s.effects = esp_team_effect[ NOTEAM ];
	flag->s.renderfx = esp_team_fx[ NOTEAM ];
	flag->owner = NULL;
	flag->touch = EspTouchCapturePoint;
	NEXT_KEYFRAME( flag, EspCapturePointThink );
	flag->classname = "item_flag";
	flag->svflags &= ~SVF_NOCLIENT;
	gi.linkentity( flag );

	esp_flag_count ++;
}

// Function to find the sole capturepoint, remove it and recreate it exactly as it was
// This is used to reset the capturepoint after a round resets
void EspResetCapturePoint()
{
	edict_t *ent = NULL;
	edict_t *flag = NULL;
	vec3_t origin;
	vec3_t angles;

	// Find the flag
	while ((ent = G_Find(ent, FOFS(classname), "item_flag")) != NULL) {
		flag = ent;
	}

	if (flag == NULL){
		gi.dprintf("Warning: No flag found, aborting reset\n");
		return;
	}

	// Save the flag's origin and angles
	VectorCopy(flag->s.origin, origin);
	VectorCopy(flag->s.angles, angles);

	// Remove the flag
	G_FreeEdict(flag);

	// Create a new flag
	flag = G_Spawn();
	EspMakeCapturePoint(espflag);

	// Restore the flag's origin and angles
// 	VectorCopy(origin, flag->s.origin);
// 	VectorCopy(angles, flag->s.angles);
//
}

void EspSetTeamSpawns(int team, char *str)
{
	espsettings_t *es = &espsettings;
	edict_t *spawn = NULL;
	char *next;
	vec3_t pos;
	float angle;
	int i = 0;

	char *team_spawn_name = "info_player_team1";
	if(team == TEAM2)
		team_spawn_name = "info_player_team2";
	if (teamCount == 3 && team == TEAM3)
		team_spawn_name = "info_player_team3";

	/* find and remove all team spawns for this team */
	while ((spawn = G_Find(spawn, FOFS(classname), team_spawn_name)) != NULL) {
		G_FreeEdict (spawn);
	}
	for (i = 0; i < MAX_SPAWNS; i++) {
		es->custom_spawns[team][i] = NULL;
	}
	/* End Cleanup phase */

	/* Generate spawn points based on file contents */
	next = strtok(str, ",");
	do {
		if (sscanf(next, "<%f %f %f %f>", &pos[0], &pos[1], &pos[2], &angle) != 4) {
			gi.dprintf("EspSetTeamSpawns: invalid spawn point: %s, expected <x y z a>\n", next);
			continue;
		}

		spawn = G_Spawn();
		VectorCopy(pos, spawn->s.origin);
		spawn->s.angles[YAW] = angle;
		spawn->classname = ED_NewString (team_spawn_name);

		es->custom_spawns[team][esp_potential_spawns] = spawn;
		esp_potential_spawns++;
		if (esp_potential_spawns >= MAX_SPAWNS)
		{
			gi.dprintf ("Warning: MAX_SPAWNS exceeded\n");
			break;
		}
		
		// spawn = G_Spawn();
		// VectorCopy(pos, spawn->s.origin);
		// spawn->s.angles[YAW] = angle;
		// spawn->classname = ED_NewString (team_spawn_name);
		// ED_CallSpawn(spawn);

		next = strtok(NULL, ",");
	} while(next != NULL);
	
	// spot = NULL;
	// while ((spot = G_Find (spot, FOFS (classname), "info_player_deathmatch")) != NULL)
	// {
	// 	potential_spawns[num_potential_spawns] = spot;
	// 	num_potential_spawns++;
	// 	if (num_potential_spawns >= MAX_SPAWNS)
	// 	{
	// 		gi.dprintf ("Warning: MAX_SPAWNS exceeded\n");
	// 		break;
	// 	}
	// }
}

void EspEnforceDefaultSettings(char *defaulttype)
{
	int i = 0;
	qboolean default_team = (Q_stricmp(defaulttype,"team")==0) ? true : false;
	qboolean default_respawn = (Q_stricmp(defaulttype,"respawn")==0) ? true : false;
	qboolean default_author = (Q_stricmp(defaulttype,"author")==0) ? true : false;

	//gi.dprintf("I was called to set the default for %s\n", defaulttype);

	if(default_author) {
		EspForceEspionage(ESPMODE_ATL);
		Q_strncpyz(espsettings.author, "AQ2World Team", sizeof(espsettings.author));
		Q_strncpyz(espsettings.name, "Time for Action!", sizeof(espsettings.name));

		gi.dprintf(" Author           : %s\n", espsettings.author);
		gi.dprintf(" Name		  : %s\n", espsettings.name);
		gi.dprintf(" Game type        : Assassinate the Leader\n");
	}

	if(default_respawn) {
		for (i = TEAM1; i < teamCount; i++) {
			teams[i].respawn_timer = ESP_RESPAWN_TIME;
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
		EspForceEspionage(ESPMODE_ATL);
	}
}

qboolean EspLoadConfig(const char *mapname)
{
	char buf[1024];
	char *ptr;
	qboolean no_file = false;
	FILE *fh;
	int i = 0;

	memset(&espsettings, 0, sizeof(espsettings));

	esp_flag = gi.modelindex("models/items/bcase/g_bc1.md2");

	gi.dprintf("Trying to load Espionage configuration file\n", mapname);

	sprintf (buf, "%s/tng/%s.esp", GAMEVERSION, mapname);
	fh = fopen (buf, "r");
	if (!fh) {
		//Default to ATL mode in this case
		gi.dprintf ("Warning: Espionage configuration file \" %s \" was not found.\n", buf);
		EspForceEspionage(ESPMODE_ATL);
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
		EspForceEspionage(ESPMODE_ATL);
		// TODO: A better GHUD method to display this?
		gi.dprintf("-------------------------------------\n");
		gi.dprintf("Hard-coded Espionage configuration loaded\n");

		// Set game type to ATL
		/// Default game settings
		EspEnforceDefaultSettings("author");
		EspEnforceDefaultSettings("respawn");
		EspEnforceDefaultSettings("team");
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
			EspForceEspionage(ESPMODE_ATL);
			gametypename = ESPMODE_ATL_NAME;
		} else {
			if(ptr) {
				if(strcmp(ptr, ESPMODE_ETV_SNAME) == 0){
					EspForceEspionage(ESPMODE_ETV);
					gametypename = ESPMODE_ETV_NAME;
				}
				if(strcmp(ptr, ESPMODE_ATL_SNAME) == 0){
					EspForceEspionage(ESPMODE_ATL);
					gametypename = ESPMODE_ATL_NAME;
				}
			}
			if (use_3teams->value) {
				// Only ATL available in 3 team mode
				EspForceEspionage(ESPMODE_ATL);
				gametypename = ESPMODE_ATL_NAME;
			}
			gi.dprintf("- Game type : %s\n", gametypename);
		}
		// Force ATL mode trying to get ETV mode going with 3teams
		if (etv->value && use_3teams->value){
			gi.dprintf("%s and use_3team are incompatible, defaulting to %s", ESPMODE_ETV_NAME, ESPMODE_ATL_NAME);
			EspForceEspionage(ESPMODE_ATL);
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

		// Only set the flag if the scenario is ETV
		if(etv->value) {
			gi.dprintf("- Target\n");
			ptr = INI_Find(fh, "target", "escort");
			if(ptr) {
				ptr = strchr( ptr, '<' );
				while( ptr )
				{
					edict_t *flag = G_Spawn();

					char *space = NULL, *end = strchr( ptr + 1, '>' );
					if( end )
						*end = '\0';

					flag->s.origin[0] = atof( ptr + 1 );
					space = strchr( ptr + 1, ' ' );
					if( space )
					{
						flag->s.origin[1] = atof( space );
						space = strchr( space + 1, ' ' );
						if( space )
						{
							flag->s.origin[2] = atof( space );
							space = strchr( space + 1, ' ' );
							if( space )
								flag->s.angles[YAW] = atof( space );
						}
					}

					EspMakeCapturePoint( flag );
					// Set flag info as global
					espflag = flag;
					ptr = strchr( (end ? end : ptr) + 1, '<' );
				}

				if( esp_flag_count )
					gi.dprintf( "Espionage ETV mode: %i target generated.\n", esp_flag_count );
				else {
					gi.dprintf( "Warning: Espionage needs escort target in: tng/%s.esp\n", mapname );
					EspForceEspionage(ESPMODE_ATL);
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
			} else {
				gi.dprintf( "Warning: Escort target coordinates not found in tng/%s.esp, setting to ATL mode\n", mapname );
				EspForceEspionage(ESPMODE_ATL);
			}
		}

		ptr = INI_Find(fh, "spawns", "red");
		if(ptr) {
			gi.dprintf("Team 1 spawns: %s\n", ptr);
			EspSetTeamSpawns(TEAM1, ptr);
		}
		ptr = INI_Find(fh, "spawns", "blue");
		if(ptr) {
			gi.dprintf("Team 2 spawns: %s\n", ptr);
			EspSetTeamSpawns(TEAM2, ptr);
		}
		if (teamCount == 3){
			ptr = INI_Find(fh, "spawns", "green");
			if(ptr) {
				gi.dprintf("Team 3 spawns: %s\n", ptr);
				EspSetTeamSpawns(TEAM3, ptr);
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

		// Cleaned up?
		// ptr = INI_Find(fh, "red_team", "name");
		// Q_strncpyz(teams[TEAM1].name, ptr, sizeof(teams[TEAM1].name));
		// ptr = INI_Find(fh, "red_team", "skin");
		// Q_strncpyz(teams[TEAM1].skin, ptr, sizeof(teams[TEAM1].skin));
		// ptr = INI_Find(fh, "red_team", "leader_name");
		// Q_strncpyz(teams[TEAM1].leader_name, ptr, sizeof(teams[TEAM1].leader_name));
		// ptr = INI_Find(fh, "red_team", "leader_skin");
		// Q_strncpyz(teams[TEAM1].leader_skin, ptr, sizeof(teams[TEAM1].leader_skin));

		// ptr = INI_Find(fh, "blue_team", "name");
		// Q_strncpyz(teams[TEAM2].name, ptr, sizeof(teams[TEAM2].name));
		// ptr = INI_Find(fh, "blue_team", "skin");
		// Q_strncpyz(teams[TEAM2].skin, ptr, sizeof(teams[TEAM2].skin));
		// ptr = INI_Find(fh, "blue_team", "leader_name");
		// Q_strncpyz(teams[TEAM2].leader_name, ptr, sizeof(teams[TEAM2].leader_name));
		// ptr = INI_Find(fh, "blue_team", "leader_skin");
		// Q_strncpyz(teams[TEAM2].leader_skin, ptr, sizeof(teams[TEAM2].leader_skin));

		// if (teamCount == 3){
		// 	ptr = INI_Find(fh, "green_team", "name");
		// 	Q_strncpyz(teams[TEAM3].name, ptr, sizeof(teams[TEAM3].name));
		// 	ptr = INI_Find(fh, "green_team", "skin");
		// 	Q_strncpyz(teams[TEAM3].skin, ptr, sizeof(teams[TEAM3].skin));
		// 	ptr = INI_Find(fh, "green_team", "leader_name");
		// 	Q_strncpyz(teams[TEAM3].leader_name, ptr, sizeof(teams[TEAM3].leader_name));
		// 	ptr = INI_Find(fh, "green_team", "leader_skin");
		// 	Q_strncpyz(teams[TEAM3].leader_skin, ptr, sizeof(teams[TEAM3].leader_skin));
		// }

		for (i = TEAM1; i <= teamCount; i++) {
			const char *team_color;
			switch (i) {
				case TEAM1:
					team_color = "red";
					break;
				case TEAM2:
					team_color = "blue";
					break;
				case TEAM3:
					team_color = "green";
					break;
				default:
					continue;
			}

			ptr = INI_Find(fh, team_color, "name");
			if (ptr) {
				Q_strncpyz(teams[i].name, ptr, sizeof(teams[i].name));
			}

			ptr = INI_Find(fh, team_color, "skin");
			if (ptr) {
				Q_strncpyz(teams[i].skin, ptr, sizeof(teams[i].skin));
			}

			ptr = INI_Find(fh, team_color, "leader_name");
			if (ptr) {
				Q_strncpyz(teams[i].leader_name, ptr, sizeof(teams[i].leader_name));
			}

			ptr = INI_Find(fh, team_color, "leader_skin");
			if (ptr) {
				Q_strncpyz(teams[i].leader_skin, ptr, sizeof(teams[i].leader_skin));
			}
		}

		// TODO: This will always resolve to true, maybe need to validate in another way
		// if((!teams[TEAM1].skin || !teams[TEAM1].name || !teams[TEAM1].leader_name || !teams[TEAM1].leader_skin ||
		// !teams[TEAM2].skin || !teams[TEAM2].name || !teams[TEAM2].leader_name || !teams[TEAM2].leader_skin) || 
		// ((teamCount == 3) && (!teams[TEAM3].skin || !teams[TEAM3].name || !teams[TEAM3].leader_name || !teams[TEAM3].leader_skin))){
		// 	gi.dprintf("Warning: Could not read value for team skin, name, leader or leader_skin, review your file\n");
		// 	gi.dprintf("Enforcing defaults\n");
		// 	EspEnforceDefaultSettings("team");
		// 	espsettings.custom_skins = false;
		// } else {
		// 	espsettings.custom_skins = true;

			qboolean missing_property = false;
			for (i = TEAM1; i <= teamCount; i++) {
				if (strlen(teams[i].skin) == 0 || 
				strlen(teams[i].name) == 0 || 
				strlen(teams[i].leader_name) == 0 || 
				strlen(teams[i].leader_skin) == 0) {
					missing_property = true;
					break;
				}
			}
			if (missing_property) {
				gi.dprintf("Warning: Could not read value for team skin, name, leader or leader_skin; review your file\n");
				gi.dprintf("Enforcing defaults\n");
				EspEnforceDefaultSettings("team");
				espsettings.custom_skins = false;
			} else {
				espsettings.custom_skins = true;
			}

			for (i = TEAM1; i <= teamCount; i++) {
				const char *team_color;
				switch (i) {
					case TEAM1:
						team_color = "Red";
						break;
					case TEAM2:
						team_color = "Blue";
						break;
					case TEAM3:
						team_color = "Green";
						break;
					default:
						continue;
				}

				gi.dprintf("    %s Team: %s, Skin: %s\n", team_color, teams[i].name, teams[i].skin);
				gi.dprintf("    %s Leader: %s, Skin: %s\n\n", team_color, teams[i].leader_name, teams[i].leader_skin);
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

	if((etv->value) && teamCount == 3){
		gi.dprintf("Warning: ETV mode requested with use_3teams enabled, forcing ATL mode");
		EspForceEspionage(ESPMODE_ATL);
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

	if (!IS_LEADER(ent)) {
		gi.cprintf(ent, PRINT_HIGH, "You will respawn in %d seconds\n", spawntime);
	}
	return spawntime;
}

// edict_t *getLeaderCoordinates (edict_t *leader)
// {
//         edict_t *leadercoords;
//         float   bestdistance, bestplayerdistance;
//         edict_t *spot;

//         spot = NULL;
//         leadercoords = NULL;
//         bestdistance = -1;


//         while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
//         {
//                 bestplayerdistance = PlayersRangeFromSpot (spot);
//                 if ((bestplayerdistance < bestdistance) || (bestdistance < 0))
//                 {
//                         leadercoords = spot;
//                         bestdistance = bestplayerdistance;
//                 }
//         }
//         return leadercoords;
// }


// static void SelectEspRespawnPoint(edict_t *ent)
// {
// 	vec3_t		respawn_coords, angles;
// 	edict_t 	*spot = NULL;
// 	int i;

// 	// Gets the leader entity
// 	spot = teams[ent->client->resp.team].leader->old_origin;

// 	// Copies the entity's coordinates
// 	VectorCopy (spot->s.origin, respawn_coords);
// 	respawn_coords[2] += 9;
// 	VectorCopy (spot->s.angles, angles);
// 	ent->client->jumping = 0;
// 	ent->movetype = MOVETYPE_NOCLIP;
// 	gi.unlinkentity (ent);

// 	VectorCopy (respawn_coords, ent->s.origin);
// 	VectorCopy (respawn_coords, ent->s.old_origin);

// 	// clear the velocity and hold them in place briefly
// 	VectorClear (ent->velocity);

// 	ent->client->ps.pmove.pm_time = 160>>3;		// hold time
// 	//ent->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
	
// 	VectorClear (ent->s.angles);
// 	VectorClear (ent->client->ps.viewangles);
// 	VectorClear (ent->client->v_angle);

// 	VectorCopy(angles,ent->s.angles);
// 	VectorCopy(ent->s.angles,ent->client->v_angle);

// 	for (i=0;i<2;i++)
// 		ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->v_angle[i] - ent->client->resp.cmd_angles[i]);
	
// 	if (ent->client->pers.spectator)
// 		ent->solid = SOLID_BBOX;
// 	else
// 		ent->solid = SOLID_TRIGGER;
	
// 	ent->deadflag = DEAD_NO;
// 	gi.linkentity (ent);
// 	ent->movetype = MOVETYPE_WALK;
// }

void EspRespawnPlayer(edict_t *ent)
{
	// Leaders do not respawn
	if (IS_LEADER(ent))
		return;

	// Don't respawn until the current framenum is more than the respawn timer's framenum
	if (level.framenum > ent->client->respawn_framenum) {
		//gi.dprintf("Level framenum is %d, respawn timer was %d for %s\n", level.framenum, ent->client->respawn_framenum, ent->client->pers.netname);
		// If your leader is alive, you can respawn
		if (teams[ent->client->resp.team].leader != NULL) {
			if (atl->value && IS_ALIVE(teams[ent->client->resp.team].leader)) {
				respawn(ent);
			}
		// If TEAM1's leader is alive, you can respawn
		} else if (teams[TEAM1].leader != NULL) { // NULL check
			if (etv->value && IS_ALIVE(teams[TEAM1].leader)) {
				respawn(ent);
			}
		}
	}
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

/*
Internally used only, spot check if leader is alive
depending on the game mode
Please run a NULL check on the leader before calling this
*/
qboolean _EspLeaderAliveCheck(edict_t *ent, edict_t *leader, int espmode)
{
	if (espmode < 0) {
		gi.dprintf("Warning: Invalid espmode returned from EspModeCheck()\n");
		return false;
	}
	if (!leader) {
		gi.dprintf("Warning: Leader was NULL\n");
		return false;
	}
	if (espmode == ESPMODE_ATL) {
		if (teams[leader->client->resp.team].leader)
			return true;
		else
			return false;
	}

	if (espmode == ESPMODE_ETV) {
		if (ent->client->resp.team == TEAM1 &&
		IS_ALIVE(teams[TEAM1].leader))
			return true;
		else
			return false;
	}

	// Catch-all safety
	return false;
}

edict_t *SelectEspCustomSpawnPoint(edict_t * ent)
{
	espsettings_t *es = &espsettings;
    int teamNum = ent->client->resp.team;
    srand(time(NULL)); // Random seed
	int random_index = 0;
	int count = 0;
	int i = 0;

    for (i = 0; i < game.maxclients; i++) {
        if (es->custom_spawns[teamNum][i]) {
            count++;
        }
    }

	if (count > 0) {
        do {
            random_index = rand() % count; // Generate a random index between 0 and the number of spawns
        } while (count > 1 && random_index == esp_last_chosen_spawn); // Keep generating a new index until it is different from the last one, unless there is only one spawn point
    }
	// Keep track of which spawn was last chosen
	esp_last_chosen_spawn = random_index;

	return es->custom_spawns[teamNum][random_index];
}

	// if (espionage_spawns[teamNum][] < 1) {
	// 	gi.dprintf("New Spawncode: gone through all spawns, re-reading spawns\n");
	// 	NS_GetSpawnPoints ();
	// 	NS_SetupTeamSpawnPoints ();
	// 	return false;
	// }

	// spawn_point = newrand (esp_potential_spawns[teamNum]);
	// esp_potential_spawns[teamNum]--;

	// teamplay_spawns[teamNum] = NS_potential_spawns[teamNum][spawn_point];
	// teams_assigned[teamNum] = true;

	// for (z = spawn_point;z < esp_potential_spawns[teamNum];z++) {
	// 	NS_potential_spawns[teamNum][z] = NS_potential_spawns[teamNum][z + 1];
	// }

	//return true;


edict_t *SelectEspSpawnPoint(edict_t * ent)
{
	edict_t 	*spot, *spot1, *spot2;
	edict_t		*teamLeader, *spawn;
	int 		count = 0;
	int 		selection;
	float 		range, range1, range2;
	char 		*cname;
	vec3_t 		respawn_coords;

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

	/*
	Check if this is a respawn or a round start spawn
	Respawn Logic: 
	1. It's ETV mode
	2. You are on TEAM1
	3. TEAM1's leader is alive
	4. Then spawn at the leader's location
	(if you are on TEAM2, you respawn at your original spawnpoint)
		or
	1. It's ATL mode
	2. Your leader is alive
	3. Then spawn at your leader's location

	Also run a NULL check on the leader entity, if it doesn't exist then safely respawn
	at a map spawnpoint
	*/

	// if (team_round_going &&
	// // ETV Check
    // ((etv->value && ent->client->resp.team == TEAM1 && 
	// IS_ALIVE(teams[TEAM1].leader) && 
	// teams[TEAM1].leader) || 
	// //End ETV Check
	// // ATL Check
    // (atl->value && 
	// teams[ent->client->resp.team].leader && 
	// IS_ALIVE(teams[ent->client->resp.team].leader)))) {

	if (team_round_going && _EspLeaderAliveCheck(ent, teams[ent->client->resp.team].leader, EspModeCheck())) {
		// Get the leader's coordinates
		teamLeader = teams[ent->client->resp.team].leader;
		VectorCopy(teamLeader->s.origin, respawn_coords);

		spawn = G_Spawn();
		respawn_coords[2] += 9; // So they don't spawn in the floor
		VectorCopy(respawn_coords, spawn->s.origin);
		spawn->s.angles[YAW] = teamLeader->s.angles[YAW]; // Facing the same direction as the leader on respawn
		spawn->classname = ED_NewString(cname);
		spawn->think = G_FreeEdict;
		spawn->nextthink = level.framenum + 1;
		//ED_CallSpawn(spawn);

		//gi.dprintf("Coordinates are %f %f %f %f\n", teamLeader->s.origin[0], teamLeader->s.origin[1], teamLeader->s.origin[2], teamLeader->s.angles[YAW]);

		return spawn;
		// Copies the entity's coordinates
		// VectorCopy (spot->s.origin, teamLeader->s.origin);
		// respawn_coords[2] += 9;
		// VectorCopy (spot->s.angles, angles);
		// return spot;
	} else {
		if (!espsettings.custom_spawns[ent->client->resp.team][0])
			return SelectTeamplaySpawnPoint(ent);
		else
			return SelectEspCustomSpawnPoint(ent);
	// // Print all of the teamplay_spawns
	// 	int spawn_point, z;

	// 	if (esp_potential_spawns[team] < 1) {
	// 		gi.dprintf("New Spawncode: gone through all spawns, re-reading spawns\n");
	// 		NS_GetSpawnPoints ();
	// 		NS_SetupTeamSpawnPoints ();
	// 		return false;
	// 	}

	// 	spawn_point = newrand (NS_num_potential_spawns[team]);
	// 	NS_num_potential_spawns[team]--;

	// 	teamplay_spawns[team] = NS_potential_spawns[team][spawn_point];
	// 	teams_assigned[team] = true;

	// 	for (z = spawn_point;z < NS_num_potential_spawns[team];z++) {
	// 		NS_potential_spawns[team][z] = NS_potential_spawns[team][z + 1];
	// 	}

		// while ((spot = G_Find(spot, FOFS(classname), cname)) != NULL) {
		// 	//gi.dprintf("Spawn coordinates are: %f %f %f\n", spot->s.origin[0], spot->s.origin[1], spot->s.origin[2], spot->s.angles[YAW]);
		// 	count++;
		// 	range = PlayersRangeFromSpot(spot);
		// 	if (range < range1) {
		// 		if (range1 < range2) {
		// 			range2 = range1;
		// 			spot2 = spot1;
		// 		}
		// 		range1 = range;
		// 		spot1 = spot;
		// 	} else if (range < range2) {
		// 		range2 = range;
		// 		spot2 = spot;
		// 	}
		// }

		// //gi.dprintf("*********Found %i spawn points\n", count);

		// if (!count)
		// 	return SelectRandomDeathmatchSpawnPoint();

		// if (count <= 2) {
		// 	spot1 = spot2 = NULL;
		// } else
		// 	count -= 2;

		// selection = rand() % count;

		// spot = NULL;
		// do {
		// 	spot = G_Find(spot, FOFS(classname), cname);
		// 	if (spot == spot1 || spot == spot2)
		// 		selection++;
		// }
		// while (selection--);

		// return spot;
	}
}

void EspScoreBonuses(edict_t * targ, edict_t * inflictor, edict_t * attacker)
{
	int i, enemyteam;
	edict_t *ent, *flag, *leader;
	vec3_t v1, v2;

	if (IS_LEADER(targ))
		leader = targ;

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
		attacker->client->resp.score += ESP_LEADER_FRAG_BONUS;
		gi.cprintf(attacker, PRINT_MEDIUM,
			   "BONUS: %d points for killing the enemy leader!\n", ESP_LEADER_FRAG_BONUS);

		for (i = 1; i <= game.maxclients; i++) {
			ent = g_edicts + i;
			if (ent->inuse && ent->client->resp.team == enemyteam)
				ent->client->resp.esp_lasthurtleader = 0;
		}
		return;
	}

	if (targ->client->resp.esp_lasthurtleader &&
	    level.framenum - targ->client->resp.esp_lasthurtleader <
	    ESP_LEADER_DANGER_PROTECT_TIMEOUT * HZ) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		attacker->client->resp.score += ESP_LEADER_DANGER_PROTECT_BONUS;
		gi.bprintf(PRINT_MEDIUM,
			   "%s defends %s's leader against an aggressive enemy\n",
			   attacker->client->pers.netname, teams[attacker->client->resp.team].name);
		IRC_printf(IRC_T_GAME,
			   "%n defends %n's leader against an aggressive enemy\n",
			   attacker->client->pers.netname,
			   teams[attacker->client->resp.team].name);
		return;
	}
	// flag and flag carrier area defense bonuses
	// we have to find the flag and carrier entities
	// find the flag
	flag = NULL;
	// while ((flag = G_Find(flag, FOFS(classname), flag_item->classname)) != NULL) {
	// 	if (!(flag->spawnflags & DROPPED_ITEM))
	// 		break;
	// }

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

	if (VectorLength(v1) < ESP_ATTACKER_PROTECT_RADIUS || VectorLength(v2) < ESP_ATTACKER_PROTECT_RADIUS
		|| visible(flag, targ, MASK_SOLID) || visible(flag, attacker, MASK_SOLID)) {
		// we defended the base flag
		attacker->client->resp.score += ESP_FLAG_DEFENSE_BONUS;
		if (flag->solid == SOLID_NOT) {
			gi.bprintf(PRINT_MEDIUM, "%s defends the %s.\n",
				   attacker->client->pers.netname, espsettings.target_name);
			IRC_printf(IRC_T_GAME, "%n defends the %n.\n",
				   attacker->client->pers.netname,
				   espsettings.target_name);
		}
		return;
	}

	if (leader && leader != attacker) {
		VectorSubtract(targ->s.origin, leader->s.origin, v1);
		VectorSubtract(attacker->s.origin, leader->s.origin, v1);

		if (VectorLength(v1) < ESP_LEADER_DANGER_PROTECT_BONUS ||
		    VectorLength(v2) < ESP_LEADER_DANGER_PROTECT_BONUS ||
			visible(leader, targ, MASK_SOLID) || visible(leader, attacker, MASK_SOLID)) {
			attacker->client->resp.score += ESP_LEADER_DANGER_PROTECT_BONUS;
			gi.bprintf(PRINT_MEDIUM, "%s thwarts an assassination attempt on %s\n",
				   attacker->client->pers.netname, teams[attacker->client->resp.team].leader_name);
			IRC_printf(IRC_T_GAME, "%n thwarts an assassination attempt on %n\n",
				   attacker->client->pers.netname,
				   teams[attacker->client->resp.team].leader_name);
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
	// ent->client->ps.stats[ STAT_TEAM1_LEADERPIC ] = esp_leader_pics[ TEAM1 ];
	// ent->client->ps.stats[ STAT_TEAM2_LEADERPIC ] = esp_leader_pics[ TEAM2 ];

	if (teamCount == 3) {
		ent->client->ps.stats[ STAT_TEAM3_SCORE ] = teams[ TEAM3 ].score;
		ent->client->ps.stats[ STAT_TEAM3_PIC ] = esp_pics[ TEAM3 ];
		// ent->client->ps.stats[ STAT_TEAM3_LEADERPIC ] = esp_leader_pics[ TEAM3 ];
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

/*
This is only used when there are 2 teams

Commenting out for now in favor of using OtherTeam()
*/
// int EspOtherTeam(int team)
// {
// 	if(teamCount == 3)
// 		return -1;

// 	switch (team) {
// 	case TEAM1:
// 		return TEAM2;
// 	case TEAM2:
// 		return TEAM1;
// 	case NOTEAM:
// 		return NOTEAM; /* there is no other team for NOTEAM, but I want it back! */
// 	}
// 	return -1;		// invalid value
// }

void EspSwapTeams()
{
	edict_t *ent;
	int i;

	for (i = 0; i < game.maxclients; i++) {
		ent = &g_edicts[1 + i];
		if (ent->inuse && ent->client->resp.team) {
			ent->client->resp.team = OtherTeam(ent->client->resp.team);
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
	if(etv->value) {
		if( capturelimit->value && (teams[TEAM1].score >= capturelimit->value || teams[TEAM2].score >= capturelimit->value) )
		{
			gi.bprintf(PRINT_HIGH, "Capturelimit hit.\n");
			IRC_printf(IRC_T_GAME, "Capturelimit hit.\n");
			return true;
		}
	}

	if( timelimit->value > 0 && etv->value )
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
		else if( espsettings.halftime < 3 && level.matchTime >= (timelimit->value * 60) / 2 + 1 && esp_etv_halftime->value && etv->value == 1)
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


int _findVolunteerIndex(edict_t *ent)
{
	espsettings_t *es = &espsettings;
	int teamNum = ent->client->resp.team;
    int i = 0;
    for (i = 0; i < game.maxclients; i++) {
        if (es->volunteers[teamNum][i] == ent) {
			gi.dprintf("Found volunteer %d at index %d\n", ent->client->resp.team, i);
            return i;
        }
    }

	gi.dprintf("Player not found?\n");
    return -1; // Player not found
}
/*
This clears a single team of volunteers and leaders
Tends to only be used in Matchmode
*/

void EspClearVolunteer(int teamNum)
{
	espsettings_t *es = &espsettings;
	int i, j = 0;
	// Clear the struct array
	for (i = 0; i < MAX_CLIENTS; i++) {
		es->volunteers[teamNum][i] = NULL;
	}

	// Clear the player resp values
	edict_t *ent;
	for (j = 0; j < game.maxclients; j++)
	{
		ent = &g_edicts[1 + i];
		if (ent->client->resp.team == teamNum)
			ent->client->resp.is_volunteer = false;
	}
}

/*
This clears the Espionage volunteer table and all volunteers
*/
void EspClearVolunteers(void)
{
	int i = TEAM1;
	for (i = TEAM1; i <= teamCount; i++)
		EspClearVolunteer(i);
}

/*
Returns count of players in the volunteer queue
*/
int EspGetVolunteerCount(int teamNum, espsettings_t *es)
{
    int count = 0;
	int i = 0;
    for (i = 0; i < game.maxclients; i++) {
        if (es->volunteers[teamNum][i]) {
            count++;
        }
    }
    return count;
}

/*
This is called when a leader dies, and we need to shift the leader queue
*/
void EspShiftLeaderQueue(int teamNum)
{
    espsettings_t *es = &espsettings;
    int volunteerCount = EspGetVolunteerCount(teamNum, es);

    if (volunteerCount > 1) {
        edict_t *temp = es->volunteers[teamNum][0];
        for (int i = 0; i < volunteerCount - 1; i++) {
            es->volunteers[teamNum][i] = es->volunteers[teamNum][i + 1];
        }
        es->volunteers[teamNum][volunteerCount - 1] = temp;
    }

    // A new leader is born
    EspSetLeader();
}

/*
Keep it simple:  
If matchmode is on, only allow 1 volunteer per team, and if they aren't the captain, deny them
*/
qboolean EspMMLeaderMgr(edict_t *ent, int volunteer_count, int teamNum, int playerNum, espsettings_t *es)
{
	if (volunteer_count > 1) {
			gi.dprintf("Error: Detected more than 1 volunteer in queue for team %d\nThis can lead to unexpected behavior!", teamNum);
			return false;
		}

	if (!IS_CAPTAIN(ent)) {
		// Someone tried to volunteer without being Captain
		gi.cprintf(ent, PRINT_HIGH, "To be a team Leader you must be the team Captain.\n");
		ent->client->resp.is_volunteer = false;
		return false;
	} else {
		// Automatically become Leader if Captain
		es->volunteers[teamNum][0] = ent;
		ent->client->resp.is_volunteer = true;
		return true;
	}
}

qboolean EspLeaderToggle(edict_t *ent, int teamNum, int playerNum, espsettings_t *es)
{
	edict_t *existingLeader = teams[teamNum].leader;

	// Toggles volunteer status for player
	gi.dprintf("%s is a volunteer A: %d\n", ent->client->pers.netname, ent->client->resp.is_volunteer);
	if (ent->client->resp.is_volunteer == true) {
		// Player is already a volunteer, remove them from the volunteer list
		gi.dprintf("Removing volunteer %d from team %d\n", playerNum, teamNum);
		ent->client->resp.is_volunteer = false;
		gi.dprintf("%s is a volunteer B: %d\n", ent->client->pers.netname, ent->client->resp.is_volunteer);
		es->volunteers[teamNum][_findVolunteerIndex(ent)] = NULL;
		if (ent->inuse && !ent->is_bot) { // Don't sent messages to potentially disappearing clients or bots
			gi.cprintf (ent, PRINT_HIGH, "You are no longer volunteering for duty\n");
			if (IS_LEADER(ent))
				// This clears the leader, if the entity was the leader
				// This has to be done here because EspSetLeader has no context
				teams[teamNum].leader = NULL;
		}
		return false;
	} else {
		// Player is not already a volunteer, add them to the volunteer list
		ent->client->resp.is_volunteer = true;
		gi.dprintf("teamNum: %d, playerNum: %d\n", teamNum, playerNum);
		es->volunteers[teamNum][EspGetVolunteerCount(teamNum, es)] = ent;
		gi.dprintf("Volunteer index was %d for team %i\n", _findVolunteerIndex(ent), teamNum);
		if (!existingLeader) {
			gi.cprintf (ent, PRINT_HIGH, "You are now volunteering to be a team leader\n");
		} else {
			gi.cprintf(ent, PRINT_HIGH, "Your team already has a leader (%s)\nAdding you to the volunteer queue\n",
				teams[teamNum].leader->client->pers.netname );
		}
		return true;
	}
}

/*
This manages Espionage leader queues, responsible for
adding and removing volunteers from the queue

Returns true if player successfully added to queue

EspSetLeader() is called on round end, to notify the next leader if they are queued
*/
qboolean EspLeaderQueueMgr(edict_t *ent)
{
	espsettings_t *es = &espsettings;
	int teamNum = ent->client->resp.team;
	int playerNum = ent - g_edicts - 1;
	int volunteer_count = EspGetVolunteerCount(teamNum, es);
	int i = 0;

	// No leader for TEAM2 in ETV mode
	if (etv->value && teamNum != TEAM1) {
		gi.centerprintf(ent, "Only the Red team (team 1) has a leader in ETV mode\n");
		return false;
	}

	if (matchmode->value) {
		EspMMLeaderMgr(ent, volunteer_count, teamNum, playerNum, es);
	} 
	else // Not matchmode
	{
		gi.dprintf("EspLeaderQueueMgr: teamNum: %d, playerNum: %d\n", teamNum, playerNum);
		EspLeaderToggle(ent, teamNum, playerNum, es);
	}
}

/*
This is called if the leader of a team disconnects/leaves
*/
int ChooseRandomLeader(int teamNum)
{
	int players[TEAM_TOP] = { 0 }, i;
	edict_t *ent;

	// Not sure if we'd ever get to this state...?
	if (matchmode->value && !TeamsReady())
		return 0;

	// Get all eligible players
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
			// Subs can't be elected leaders
			return 0;
		else {
			// Congrats, you're the new leader
			EspLeaderQueueMgr(ent);
			return 1;
		}
	} else {
		// No one is eligible, halt the match?
		return 0;
	}
}

void LeaderChange(edict_t *newLeader)
{
	gi.dprintf("team_round_going: %d, gameSettings: %d\n", team_round_going, gameSettings);

	// Remove the old leader first
	if (teams[newLeader->client->resp.team].leader)
		teams[newLeader->client->resp.team].leader = NULL;

	gi.sound( &g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD, gi.soundindex( "misc/comp_up.wav" ), 1.0, ATTN_NONE, 0.0 );
	AssignSkin(newLeader, teams[newLeader->client->resp.team].leader_skin, false);

	gi.dprintf("New leader elected!\n");
	newLeader->client->resp.is_volunteer = true;
	gi.dprintf("Assigning leader: %s\n", newLeader->client->pers.netname);
	teams[newLeader->client->resp.team].leader = newLeader;

	char temp[128];
	Com_sprintf(temp, sizeof(temp), "%s is now %s's leader\n", newLeader->client->pers.netname, teams[newLeader->client->resp.team].name );
	CenterPrintAll(temp);
	gi.cprintf(newLeader, PRINT_CHAT, "You are the leader of '%s'\n", teams[newLeader->client->resp.team].name );
	gi.sound(newLeader, CHAN_VOICE, gi.soundindex("aqdt/leader.wav"), 1, ATTN_STATIC, 0);
}

void EspSetLeaderLive(void)
{
	espsettings_t *es = &espsettings;
	edict_t *newLeader = NULL;
	int i = 0;

	for (i = 0; i < MAX_TEAMS; i++) {
		// Choose the first member of the array as the new leader for each team
		// This will always results in a new leader somehow, unless there are no
		// more players at all
		if (es->volunteers[i][0]) {
			newLeader = es->volunteers[i][0];
			LeaderChange(newLeader);
		} else {
			// If there are no volunteers, select a random player
			if (!ChooseRandomLeader(i)) {
				// Halt the game immediately, there can be no winner
				gi.dprintf("Error: No eligible players for team %d\n", i);
				// The other team immediately wins, and the next round should not
				// automatically begin because AllTeamsHaveLeaders() should fail
				if (teamCount == 2) {
					gi.bprintf(PRINT_HIGH, "%s has no eligible leaders, %s wins!", teams[i].name, teams[OtherTeam(i)].name);
					WonGame(OtherTeam(i));
				} else if (teamCount == 3) {
					// We have no choice but to kill everyone on the team with no leader
					CenterPrintTeam(i, "Your leader has abandoned you, you have been eliminated!");
					KillEveryone(i);
				}
			}
		}
	}
}

void EspSetLeaderBetweenRounds(void)
{
	espsettings_t *es = &espsettings;
	edict_t *newLeader = NULL;
	int i, j = 0;

	for (i = 0; i < MAX_TEAMS; i++) {
		//gi.dprintf("Scanning team %i\n", i);
		for (j = 0; j < game.maxclients; j++) {
			//gi.dprintf("Scanning players %i\n", j);
			if (es->volunteers[i][j]) {
				gi.dprintf("Team %d volunteer: %s\n", i, es->volunteers[i][j]->client->pers.netname);
				if (es->volunteers[i][0] != teams[i].leader) {
					gi.dprintf("Volunteer found: %s\n", es->volunteers[i][j]->client->pers.netname);
					newLeader = es->volunteers[i][0];
					LeaderChange(newLeader);
				}
			}
		}
	}
}
/*
This sets the leader of a team, and assigns a skin to the leader
It depends on the volunteer queue in the espsettings_t struct
*/
void EspSetLeader(void) 
{
	espsettings_t *es = &espsettings;
	edict_t *newLeader = NULL;
	qboolean leaderChange = false;
	int i, j = 0;
	//edict_t *oldLeader = teams[ent->client->resp.team].leader;

	// A mid-round change, we have to select the next player to be the leader!
	if (team_round_going){
		EspSetLeaderLive();
	} else { // Between rounds or during pauses
		EspSetLeaderBetweenRounds();
	}

	if (teams[TEAM1].leader)
		gi.dprintf("leaders: %s\n", teams[TEAM1].leader->client->pers.netname);
	if (teams[TEAM2].leader)
		gi.dprintf("leaders: %s\n", teams[TEAM2].leader->client->pers.netname);



	// If there's a leaderchange and NULL check on newLeader
	gi.dprintf("leaderchange: %d, newLeader: %s\n", leaderChange, newLeader ? newLeader->client->pers.netname : "NULL");

	
	
}

// Old, deprecate:
// void EspSetLeader( int teamNum, edict_t *ent )
// {
// 	edict_t *oldLeader = teams[teamNum].leader;
// 	char temp[128];

// 	//ChooseRandomPlayer(teamNum, true);

// 	if (teamNum == NOTEAM)
// 		ent = NULL;

// 	if (etv->value && teamNum != TEAM1) {
// 		gi.centerprintf(ent, "Only the Red team (team 1) has a leader in ETV mode\n");
// 		return;
// 	}

// 	teams[teamNum].leader = ent;
// 	if(ent) // Only assign a skin to an ent
// 		AssignSkin(ent, teams[teamNum].leader_skin, false);

// 	if (!ent) {
// 		if (!team_round_going || (gameSettings & GS_ROUNDBASED)) {
// 			if (teams[teamNum].ready) {
// 				Com_sprintf( temp, sizeof( temp ), "%s has lost their leader and is no longer ready to play!", teams[teamNum].name );
// 				CenterPrintAll( temp );
// 			}
// 			teams[teamNum].ready = 0;
// 		}
// 		if (oldLeader) {
// 			Com_sprintf(temp, sizeof(temp), "%s is no longer %s's leader\n", oldLeader->client->pers.netname, teams[teamNum].name );
// 			CenterPrintAll(temp);
// 			//if(!esp_mustvolunteer->value) {
// 			SelectNextLeader(teamNum);
// 			//} else {
// 			//gi.bprintf( PRINT_HIGH, "%s needs a new leader!  Enter 'volunteer' to apply for duty\n", teams[teamNum].name );
// 			//}
// 		}
// 		teams[teamNum].locked = 0;
// 		return;
// 	}

// 	if (ent != oldLeader) {
// 		Com_sprintf(temp, sizeof(temp), "%s is now %s's leader\n", ent->client->pers.netname, teams[teamNum].name );
// 		CenterPrintAll(temp);
// 		gi.cprintf( ent, PRINT_CHAT, "You are the leader of '%s'\n", teams[teamNum].name );
// 		gi.sound( &g_edicts[0], CHAN_VOICE | CHAN_NO_PHS_ADD, gi.soundindex( "misc/comp_up.wav" ), 1.0, ATTN_NONE, 0.0 );
// 		AssignSkin(ent, teams[teamNum].leader_skin, false);
// 	}
// }

void EspLeaderLeftTeam(edict_t *ent)
{
	int teamNum = ent->client->resp.team;

	// If client wasn't a leader, don't do anything
	if (!IS_LEADER(ent)){
		return;
	} else {
		// Clears current leader
		EspLeaderQueueMgr(ent);
		ent->client->resp.subteam = 0;
		EspShiftLeaderQueue(teamNum);
	}
}

/*
This is called from player_die, and only called
if the player was a leader
*/
int EspReportLeaderDeath(edict_t *ent)
{
	// Get the team the leader was on
	int dead_leader_team = ent->client->resp.team;
	int winner = 0;

	// Set the dead leader val
	teams[dead_leader_team].leader_dead = true;

	// This checks if leader was on TEAM 1 in ETV mode
	if (etv->value) {
		if (dead_leader_team == TEAM1) {
			winner = TEAM2;
		}
	}

	// ATL mode - 2 team winner checks
	if (atl->value) {
		if (teamCount == 2) {
			if (dead_leader_team == TEAM1)
				winner = TEAM2;
			else if (dead_leader_team == TEAM2)
				winner = TEAM1;
		// 3 team winner checks
		} else {
			if (dead_leader_team == TEAM1) {
				if (IS_ALIVE(teams[TEAM2].leader) && !IS_ALIVE(teams[TEAM3].leader))
					winner = TEAM2;
				else if (!IS_ALIVE(teams[TEAM2].leader) && IS_ALIVE(teams[TEAM3].leader))
					winner = TEAM3;
			}
			else if (dead_leader_team == TEAM2) {
				if (IS_ALIVE(teams[TEAM1].leader) && !IS_ALIVE(teams[TEAM3].leader))
					winner = TEAM1;
				else if (!IS_ALIVE(teams[TEAM1].leader) && IS_ALIVE(teams[TEAM3].leader))
					winner = TEAM3;
			}
			else if (dead_leader_team == TEAM3) {
				if (IS_ALIVE(teams[TEAM1].leader) && !IS_ALIVE(teams[TEAM2].leader))
					winner = TEAM1;
				else if (!IS_ALIVE(teams[TEAM1].leader) && IS_ALIVE(teams[TEAM2].leader))
					winner = TEAM2;
			}
		}
	}
	// Leader shifts to the next volunteer in non-matchmode games
	if (!matchmode->value)
		EspShiftLeaderQueue(dead_leader_team);
	return winner;
}

void EspPunishment(int teamNum)
{
	// Only perform team punishments if there's only 2 teams
	if (esp->value && teamCount == 2){
		if(esp_punish->value == 1){
			// Immediately kill all losing members of the remaining team
			KillEveryone(teamNum);
		} else if (esp_punish->value == 2){
			// Grant uv shield to winning team
			int uvtime = 60;
			MakeTeamInvulnerable(OtherTeam(teamNum), uvtime);
		}
	}
}

/*
This adds a medkit to the player's inventory, up to the medkit_max value
Passing a true parameter instantly provides this medkit
Passing a false parameter assumes you want to generate one at a time interval set by medkit_time
*/
void EspGenerateMedKit(qboolean instant)
{
	int i = 0;
	edict_t *ent;
	int roundseconds = current_round_length / 10;
	int interval = (int)medkit_time->value;
	int max_kits = (int)medkit_max->value;

	// Loop through all players and generate a medkit for each leader
	for (i = 0; i < game.maxclients; i++) {
		ent = &g_edicts[1 + i];
		if (IS_LEADER(ent)) {
			// Do nothing if the ent already has max medkits
			if (ent->client->medkit >= max_kits)
				return;
			else if (roundseconds - ent->client->resp.medkit_award_time >= interval) {
				ent->client->resp.medkit_award_time = roundseconds;
				ent->client->medkit++;

				// gi.dprintf("I generated a medkit for %s, %d seconds since the round started, %d frames in\n", ent->client->pers.netname, roundseconds, roundseconds);
				// gi.dprintf("%s has %d medkits now\n", ent->client->pers.netname, ent->client->medkit);
			}
			//gi.dprintf("Current seconds in round is %d\n", roundseconds);
		}
	}
}

void EspSetupStatusbar( void )
{
	Q_strncatz(level.statusbar, 
		// Red Team
		"yb -172 " "if 24 xr -24 pic 24 endif " "xr -92 num 4 26 "
		// Blue Team
		"yb -148 " "if 25 xr -24 pic 25 endif " "xr -92 num 4 27 ",
		sizeof(level.statusbar) );
	
	if( teamCount >= 3 )
	{
		Q_strncatz(level.statusbar, 
			// Green Team
			"yb -124 " "if 30 xr -24 pic 30 endif " "xr -92 num 4 31 ",
			sizeof(level.statusbar) );
	}
}

void EspAnnounceDetails( void )
{
	int i;
	edict_t *ent;

	for (i = 0; i < game.maxclients; i++){
		ent = g_edicts + 1 + i;
		if (!ent->inuse)
			continue;
		if (IS_LEADER(ent)){
			gi.sound(ent, CHAN_VOICE, gi.soundindex("aqdt/leader.wav"), 1, ATTN_STATIC, 0);
			gi.cprintf(ent, PRINT_HIGH, "Take cover, you're the leader!\n");
		}
		if (!IS_LEADER(ent)) {
			if (atl->value){
				gi.cprintf(ent, PRINT_HIGH, "Defend your leader and attack the other one to win!\n");
			} else if (etv->value){
				if (ent->client->resp.team == TEAM1)
					gi.cprintf(ent, PRINT_HIGH, "Escort your leader to the briefcase!\n");
				else
					gi.cprintf(ent, PRINT_HIGH, "Kill the enemy leader to win!\n");
			}
		}
	}
}