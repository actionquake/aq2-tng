// Espionage Mode by darksaint
// File format inspired by a_dom.c by Raptor007 and a_ctf.c from TNG team
// Re-worked from scratch from the original AQDT, Black Monk and hal9000

#include "g_local.h"

ctfgame_t espgame;

cvar_t *esp = NULL;
cvar_t *esp_forcejoin = NULL;
cvar_t *esp_mode = NULL;
cvar_t *esp_dropflag = NULL;
cvar_t *esp_respawn = NULL;
cvar_t *esp_model = NULL;

cvar_t *dom = NULL;

unsigned int dom_team_effect[] = {
	EF_BLASTER | EF_TELEPORTER,
	EF_FLAG1,
	EF_FLAG2,
	EF_GREEN_LIGHT | EF_COLOR_SHELL
};

unsigned int dom_team_fx[] = {
	RF_GLOW,
	RF_FULLBRIGHT,
	RF_FULLBRIGHT,
	RF_SHELL_GREEN
};

int esp_marker_count = 0;
int esp_team_markers[ TEAM_TOP ] = {0};
int esp_winner = NOTEAM;
int esp_red_marker = 0;
int esp_pics[ TEAM_TOP ] = {0};
int esp_last_score = 0;


int DomFlagOwner( edict_t *marker )
{
	if( marker->s.effects == dom_team_effect[ TEAM1 ] )
		return TEAM1;
	if( marker->s.effects == dom_team_effect[ TEAM2 ] )
		return TEAM2;
	if( marker->s.effects == dom_team_effect[ TEAM3 ] )
		return TEAM3;
	return NOTEAM;
}


qboolean DomCheckRules( void )
{
	int max_score = dom_marker_count * ((teamCount == 3) ? 150 : 200);
	int winning_teams = 0;

	if( (int) level.time > dom_last_score )
	{
		dom_last_score = level.time;

		teams[ TEAM1 ].score += dom_team_markers[ TEAM1 ];
		teams[ TEAM2 ].score += dom_team_markers[ TEAM2 ];
		teams[ TEAM3 ].score += dom_team_markers[ TEAM3 ];
	}

	dom_winner = NOTEAM;

	if( max_score <= 0 )
		return true;

	if( teams[ TEAM1 ].score >= max_score )
	{
		dom_winner = TEAM1;
		winning_teams ++;
	}
	if( teams[ TEAM2 ].score >= max_score )
	{
		dom_winner = TEAM2;
		winning_teams ++;
	}
	if( teams[ TEAM3 ].score >= max_score )
	{
		dom_winner = TEAM3;
		winning_teams ++;
	}

	if( winning_teams == 1 )
	{
		// Winner: just show that they hit the score limit, not how far beyond they went.
		teams[ dom_winner ].score = max_score;
	}
	else if( winning_teams > 1 )
	{
		// Overtime: multiple teams hit the score limit.

		max_score = max(max( teams[ TEAM1 ].score, teams[ TEAM2 ].score ), teams[ TEAM3 ].score );
		winning_teams = 0;

		if( teams[ TEAM1 ].score == max_score )
		{
			dom_winner = TEAM1;
			winning_teams ++;
		}
		if( teams[ TEAM2 ].score == max_score )
		{
			dom_winner = TEAM2;
			winning_teams ++;
		}
		if( teams[ TEAM3 ].score == max_score )
		{
			dom_winner = TEAM3;
			winning_teams ++;
		}

		// Don't allow a tie.
		if( winning_teams > 1 )
			dom_winner = NOTEAM;
	}

	if( dom_winner != NOTEAM )
	{
		gi.bprintf( PRINT_HIGH, "%s team wins!\n", teams[ dom_winner ].name );
		return true;
	}

	return false;
}


void DomFlagThink( edict_t *marker )
{
	int prev = marker->s.frame;

	// If the marker was touched this frame, make it owned by that team.
	if( marker->owner && marker->owner->client && marker->owner->client->resp.team )
	{
		unsigned int effect = dom_team_effect[ marker->owner->client->resp.team ];
		if( marker->s.effects != effect )
		{
			char location[ 128 ] = "(";
			qboolean has_loc = false;
			edict_t *ent = NULL;
			int prev_owner = DomFlagOwner( marker );

			if( prev_owner != NOTEAM )
				dom_team_markers[ prev_owner ] --;

			marker->s.effects = effect;
			marker->s.renderfx = dom_team_fx[ marker->owner->client->resp.team ];
			dom_team_markers[ marker->owner->client->resp.team ] ++;

			if( marker->owner->client->resp.team == TEAM1 )
				marker->s.modelindex = dom_red_marker;
			else
				marker->s.modelindex = dom_blue_marker;

			// Get marker location if possible.
			has_loc = GetPlayerLocation( marker, location + 1 );
			if( has_loc )
				strcat( location, ") " );
			else
				location[0] = '\0';

			gi.bprintf( PRINT_HIGH, "%s secured %s marker %sfor %s!\n",
				marker->owner->client->pers.netname,
				(dom_marker_count == 1) ? "the" : "a",
				location,
				teams[ marker->owner->client->resp.team ].name );

			if( (dom_team_markers[ marker->owner->client->resp.team ] == dom_marker_count) && (dom_marker_count > 1) )
				gi.bprintf( PRINT_HIGH, "%s TEAM IS DOMINATING!\n",
				teams[ marker->owner->client->resp.team ].name );

			gi.sound( marker, CHAN_ITEM, gi.soundindex("tng/markerret.wav"), 0.75, 0.125, 0 );

			for( ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent ++ )
			{
				if( ! (ent->inuse && ent->client && ent->client->resp.team) )
					continue;
				else if( ent == marker->owner )
					unicastSound( ent, gi.soundindex("tng/markercap.wav"), 0.75 );
				else if( ent->client->resp.team != marker->owner->client->resp.team )
					unicastSound( ent, gi.soundindex("tng/markertk.wav"), 0.75 );
			}
		}
	}

	// Reset so the marker can be touched again.
	marker->owner = NULL;

	// Animate the marker waving.
	marker->s.frame = 173 + (((marker->s.frame - 173) + 1) % 16);

	// Blink between red and blue if it's unclaimed.
	if( (marker->s.frame < prev) && (marker->s.effects == dom_team_effect[ NOTEAM ]) )
		marker->s.modelindex = (marker->s.modelindex == dom_blue_marker) ? dom_red_marker : dom_blue_marker;

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

	// If the marker hasn't been touched this frame, the player will take it.
	if( ! marker->owner )
		marker->owner = player;
	// If somebody on another team also touched the marker this frame, nobody takes it.
	else if( marker->owner->client && (marker->owner->client->resp.team != player->client->resp.team) )
		marker->owner = marker;
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
	marker->s.modelindex = dom_blue_marker;
	marker->s.skinnum = 0;
	marker->s.effects = dom_team_effect[ NOTEAM ];
	marker->s.renderfx = dom_team_fx[ NOTEAM ];
	marker->owner = NULL;
	marker->touch = EspTouchFlag;
	NEXT_KEYFRAME( marker, DomFlagThink );
	marker->classname = "item_marker";
	marker->svmarkers &= ~SVF_NOCLIENT;
	gi.linkentity( marker );

	dom_marker_count ++;
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

qboolean EspLoadConfig(char *mapname)
{
	char buf[1024];
	char *ptr, *ptr_team;
	qboolean no_file = false;
	FILE *fh;

	memset(&espgame, 0, sizeof(espgame));

	gi.dprintf("Trying to load Espionage configuration file\n", mapname);

	/* zero is perfectly acceptable respawn time, but we want to know if it came from the config or not */
	espgame.spawn_red = -1;
	espgame.spawn_blue = -1;
	if(use_3teams->value)
		espgame.spawn_green = -1;

	sprintf (buf, "%s/tng/%s.esp", GAMEVERSION, mapname);
	fh = fopen (buf, "r");
	if (!fh) {
		//Default to ATL mode in this case
		gi.dprintf ("Warning: Espionage configuration file \" %s \" was not found.\n", buf);
		gi.dprintf ("Using default Assassinate the Leader scenario settings.\n");
		espgame.type = 0;
		sprintf (buf, "%s/tng/default.esp", GAMEVERSION);
		fh = fopen (buf, "r");
		if (!fh){
			gi.dprintf ("Warning: Default Espionage configuration file was not found.\n");
			gi.dprintf ("Using hard-coded Assassinate the Leader scenario settings.\n");
			no_file = true;
		}
	}

	// Hard-coded scenario settings so things don't break
	if(no_file){
		// TODO: A better GHUD method to display this?
		gi.dprintf("-------------------------------------\n");
		gi.dprintf("Hard-coded Espionage configuration loaded\n");
		gi.dprintf(" Game type  : %s\n", "Assassinate the Leader");
		gi.dprintf(" Respawn times: 10 seconds\n");
		gi.dprintf(" Skins\n");
		gi.dprintf("  Red Member: %s\n", "male/ctf_r");
		gi.dprintf("  Red Leader: %s\n", "male/babarracuda");
		gi.dprintf("  Blue Member: %s\n", "male/ctf_b");
		gi.dprintf("  Blue Leader: %s\n", "male/blues");
		if(use_3teams->value){
			gi.dprintf("  Green Member: %s\n", "male/ctf_g");
			gi.dprintf("  Green Leader: %s\n", "male/hulk2");

			espgame.spawn_green = 10;
			esp_pics[ TEAM3 ] = gi.imageindex(teams[ TEAM3 ].skin_index);
			esp_pics[ TEAM3 ] = gi.imageindex(teams[ TEAM3 ].leader_skin_index);
		}

		espgame.spawn_red = 10;
		espgame.spawn_blue = 10;
		// Skin and Team Names
		esp_pics[ TEAM1 ] = gi.imageindex(teams[ TEAM1 ].skin_index);
		esp_pics[ TEAM1 ] = gi.imageindex(teams[ TEAM1 ].leader_skin_index);
		esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].skin_index);
		esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].leader_skin_index);

		// No custom spawns, use default for map
		espgame.custom_spawns = false;
	} else {

		gi.dprintf("-------------------------------------\n");
		gi.dprintf("Espionage configuration loaded from %s\n", buf);
		ptr = INI_Find(fh, "esp", "author");
		if(ptr) {
			gi.dprintf(" Author    : %s\n", ptr);
			Q_strncpyz(espgame.author, ptr, sizeof(espgame.author));
		}
		ptr = INI_Find(fh, "esp", "comment");
		if(ptr) {
			gi.dprintf(" Comment   : %s\n", ptr);
			Q_strncpyz(espgame.comment, ptr, sizeof(espgame.comment));
		}

		ptr = INI_Find(fh, "esp", "type");
		char *gametypename;
		if(ptr) {
			if (espionage->value == 1) {
				if(strcmp(ptr, "etv") == 0)
					espgame.type = 1;
					gametypename = "Escort the VIP";	
				if(strcmp(ptr, "atl") == 0)
					espgame.type = 0;
					gametypename = "Assassinate the Leader";
			} else if (espionage->value == 2) {
				espgame.type = 0;
					gametypename = "Assassinate the Leader";
			}
			gi.dprintf(" Game type : %s\n", gametypename);
		}

		gi.dprintf(" Respawn times\n");
		ptr = INI_Find(fh, "respawn", "red");
		if(ptr) {
			gi.dprintf("  Red      : %s\n", ptr);
			espgame.spawn_red = atoi(ptr);
		}
		ptr = INI_Find(fh, "respawn", "blue");
		if(ptr) {
			gi.dprintf("  Blue     : %s\n", ptr);
			espgame.spawn_blue = atoi(ptr);
		}
		if (use_3teams->value){
			ptr = INI_Find(fh, "respawn", "green");
			if(ptr) {
				gi.dprintf("  Green     : %s\n", ptr);
				espgame.spawn_green = atoi(ptr);
			}
		}

		// Only set the marker if the scenario is ETV
		if(espgame.type = 1) {
			gi.dprintf(" Target\n");
			ptr = INI_Find(fh, "target", "escort");
			if(ptr) {
				gi.dprintf("  Target      : %s\n", ptr);
				EspSetMarker(TEAM1, ptr);
			}
		}

		gi.dprintf(" Spawns\n");
		ptr = INI_Find(fh, "spawns", "red");
		if(ptr) {
			gi.dprintf("  Red      : %s\n", ptr);
			EspSetTeamSpawns(TEAM1, ptr);
		}
		ptr = INI_Find(fh, "spawns", "blue");
		if(ptr) {
			gi.dprintf("  Blue     : %s\n", ptr);
			EspSetTeamSpawns(TEAM2, ptr);
		}
		if (use_3teams->value){
			ptr = INI_Find(fh, "spawns", "green");
			if(ptr) {
				gi.dprintf("  Green     : %s\n", ptr);
				EspSetTeamSpawns(TEAM3, ptr);
			}
		}
		espgame.custom_spawns = true;
		
		gi.dprintf(" Skins\n");
		ptr = INI_Find(fh, "skins", "red_member");
		ptr_team = INI_Find(fh, "teams", "red");
		if(ptr) {
			if(ptr_team) {
				gi.dprintf("  %s: %s\n", ptr_team, ptr);
			} else {
				gi.dprintf("  Red Member: %s\n", ptr);
				esp_pics[ TEAM1 ] = gi.imageindex(teams[ TEAM1 ].skin_index);
				//EspSetTeamSpawns(TEAM1, ptr);
				espgame.custom_skins = true;
			}
		} else {
			gi.dprintf("Warning: No skin set for red_member, defaulting to male/ctf_r\n");
			gi.dprintf("  Red Member: %s\n", "male/ctf_r");
		}
		ptr = INI_Find(fh, "skins", "blue_member");
		ptr_team = INI_Find(fh, "teams", "blue");
		if(ptr) {
			if(ptr_team) {
				gi.dprintf("  %s: %s\n", ptr_team, ptr);
			} else {
				gi.dprintf("  Blue Member: %s\n", ptr);
				esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].skin_index);
				//EspSetTeamSpawns(TEAM1, ptr);
				espgame.custom_skins = true;
			}
		} else {
			gi.dprintf("Warning: No skin set for blue_member, defaulting to male/ctf_b\n");
			gi.dprintf("  Blue Member: %s\n", "male/ctf_b");
		}
		if(use_3teams->value) {
			ptr = INI_Find(fh, "skins", "green_member");
			ptr_team = INI_Find(fh, "teams", "green");
			if(ptr) {
				if(ptr_team) {
					gi.dprintf("  %s: %s\n", ptr_team, ptr);
				} else {
					gi.dprintf("  Green Member: %s\n", ptr);
					esp_pics[ TEAM3 ] = gi.imageindex(teams[ TEAM3 ].skin_index);
					//EspSetTeamSpawns(TEAM1, ptr);
					espgame.custom_skins = true;
				}
			} else {
				gi.dprintf("Warning: No skin set for green_member, defaulting to male/ctf_g\n");
				gi.dprintf("  Green Member: %s\n", "male/ctf_g");
			}
		}
		// Leader Skins
		ptr = INI_Find(fh, "skins", "red_leader");
		ptr_team = INI_Find(fh, "teams", "red_leader");
		if(ptr) {
			if(ptr_team) {
				gi.dprintf("  %s: %s\n", ptr_team, ptr);
			} else {
				gi.dprintf("  Red Leader: %s\n", ptr);
				esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].skin_index);
				//EspSetTeamSpawns(TEAM1, ptr);
				espgame.custom_skins = true;
			}
		} else {
			gi.dprintf("Warning: No skin set for red_leader, defaulting to male/resdog\n");
			gi.dprintf("  Red Leader: %s\n", "male/resdog");
		}
		ptr = INI_Find(fh, "skins", "blue_leader");
		ptr_team = INI_Find(fh, "teams", "blue_leader");
		if(ptr) {
			if(ptr_team) {
				gi.dprintf("  %s: %s\n", ptr_team, ptr);
			} else {
				gi.dprintf("  Blue Leader: %s\n", ptr);
				esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].skin_index);
				//EspSetTeamSpawns(TEAM1, ptr);
				espgame.custom_skins = true;
			}
		} else {
			gi.dprintf("Warning: No skin set for blue_leader, defaulting to male/blues\n");
			gi.dprintf("  Blue Leader: %s\n", "male/blues");
		}
		if(use_3teams->value){
			ptr = INI_Find(fh, "skins", "green_leader");
			ptr_team = INI_Find(fh, "teams", "green_leader");
			if(ptr) {
				if(ptr_team) {
					gi.dprintf("  %s: %s\n", ptr_team, ptr);
				} else {
					gi.dprintf("  Green Leader: %s\n", ptr);
					esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].skin_index);
					//EspSetTeamSpawns(TEAM1, ptr);
					espgame.custom_skins = true;
				}
			} else {
				gi.dprintf("Warning: No skin set for green_leader, defaulting to male/hulk2\n");
				gi.dprintf("  Green Leader: %s\n", "male/hulk2");
			}
		}
	}

	// automagically change spawns *only* when we do not have team spawns
	if(!espgame.custom_spawns)
		ChangePlayerSpawns();

	gi.dprintf("-------------------------------------\n");

	fclose(fh);

	return true;
}


void DomSetupStatusbar( void )
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


void SetDomStats( edict_t *ent )
{
	// Team scores for the score display and HUD.
	ent->client->ps.stats[ STAT_TEAM1_SCORE ] = teams[ TEAM1 ].score;
	ent->client->ps.stats[ STAT_TEAM2_SCORE ] = teams[ TEAM2 ].score;

	// Team icons for the score display and HUD.
	ent->client->ps.stats[ STAT_TEAM1_PIC ] = esp_pics[ TEAM1 ];
	ent->client->ps.stats[ STAT_TEAM2_PIC ] = esp_pics[ TEAM2 ];

	// During intermission, blink the team icon of the winning team.
	if( level.intermission_framenum && ((level.realFramenum / FRAMEDIV) & 8) )
	{
		if (dom_winner == TEAM1)
			ent->client->ps.stats[ STAT_TEAM1_PIC ] = 0;
		else if (dom_winner == TEAM2)
			ent->client->ps.stats[ STAT_TEAM2_PIC ] = 0;
	}
}
