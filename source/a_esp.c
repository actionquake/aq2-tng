// Espionage Mode by darksaint
// File format inspired by a_dom.c by Raptor007 and a_ctf.c from TNG team
// Re-worked from scratch from the original AQDT, Black Monk and hal9000

#include "g_local.h"

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

int dom_flag_count = 0;
int dom_team_flags[ TEAM_TOP ] = {0};
int dom_winner = NOTEAM;
int dom_red_flag = 0, dom_blue_flag = 0;
int esp_pics[ TEAM_TOP ] = {0};
int dom_last_score = 0;


int DomFlagOwner( edict_t *flag )
{
	if( flag->s.effects == dom_team_effect[ TEAM1 ] )
		return TEAM1;
	if( flag->s.effects == dom_team_effect[ TEAM2 ] )
		return TEAM2;
	if( flag->s.effects == dom_team_effect[ TEAM3 ] )
		return TEAM3;
	return NOTEAM;
}


qboolean DomCheckRules( void )
{
	int max_score = dom_flag_count * ((teamCount == 3) ? 150 : 200);
	int winning_teams = 0;

	if( (int) level.time > dom_last_score )
	{
		dom_last_score = level.time;

		teams[ TEAM1 ].score += dom_team_flags[ TEAM1 ];
		teams[ TEAM2 ].score += dom_team_flags[ TEAM2 ];
		teams[ TEAM3 ].score += dom_team_flags[ TEAM3 ];
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


void DomFlagThink( edict_t *flag )
{
	int prev = flag->s.frame;

	// If the flag was touched this frame, make it owned by that team.
	if( flag->owner && flag->owner->client && flag->owner->client->resp.team )
	{
		unsigned int effect = dom_team_effect[ flag->owner->client->resp.team ];
		if( flag->s.effects != effect )
		{
			char location[ 128 ] = "(";
			qboolean has_loc = false;
			edict_t *ent = NULL;
			int prev_owner = DomFlagOwner( flag );

			if( prev_owner != NOTEAM )
				dom_team_flags[ prev_owner ] --;

			flag->s.effects = effect;
			flag->s.renderfx = dom_team_fx[ flag->owner->client->resp.team ];
			dom_team_flags[ flag->owner->client->resp.team ] ++;

			if( flag->owner->client->resp.team == TEAM1 )
				flag->s.modelindex = dom_red_flag;
			else
				flag->s.modelindex = dom_blue_flag;

			// Get flag location if possible.
			has_loc = GetPlayerLocation( flag, location + 1 );
			if( has_loc )
				strcat( location, ") " );
			else
				location[0] = '\0';

			gi.bprintf( PRINT_HIGH, "%s secured %s flag %sfor %s!\n",
				flag->owner->client->pers.netname,
				(dom_flag_count == 1) ? "the" : "a",
				location,
				teams[ flag->owner->client->resp.team ].name );

			if( (dom_team_flags[ flag->owner->client->resp.team ] == dom_flag_count) && (dom_flag_count > 1) )
				gi.bprintf( PRINT_HIGH, "%s TEAM IS DOMINATING!\n",
				teams[ flag->owner->client->resp.team ].name );

			gi.sound( flag, CHAN_ITEM, gi.soundindex("tng/flagret.wav"), 0.75, 0.125, 0 );

			for( ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent ++ )
			{
				if( ! (ent->inuse && ent->client && ent->client->resp.team) )
					continue;
				else if( ent == flag->owner )
					unicastSound( ent, gi.soundindex("tng/flagcap.wav"), 0.75 );
				else if( ent->client->resp.team != flag->owner->client->resp.team )
					unicastSound( ent, gi.soundindex("tng/flagtk.wav"), 0.75 );
			}
		}
	}

	// Reset so the flag can be touched again.
	flag->owner = NULL;

	// Animate the flag waving.
	flag->s.frame = 173 + (((flag->s.frame - 173) + 1) % 16);

	// Blink between red and blue if it's unclaimed.
	if( (flag->s.frame < prev) && (flag->s.effects == dom_team_effect[ NOTEAM ]) )
		flag->s.modelindex = (flag->s.modelindex == dom_blue_flag) ? dom_red_flag : dom_blue_flag;

	flag->nextthink = level.framenum + FRAMEDIV;
}


void EspTouchMarker( edict_t *flag, edict_t *player, cplane_t *plane, csurface_t *surf )
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

	// If the flag hasn't been touched this frame, the player will take it.
	if( ! flag->owner )
		flag->owner = player;
	// If somebody on another team also touched the flag this frame, nobody takes it.
	else if( flag->owner->client && (flag->owner->client->resp.team != player->client->resp.team) )
		flag->owner = flag;
}


void EspMakeMarker( edict_t *flag )
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
	flag->s.modelindex = dom_blue_flag;
	flag->s.skinnum = 0;
	flag->s.effects = dom_team_effect[ NOTEAM ];
	flag->s.renderfx = dom_team_fx[ NOTEAM ];
	flag->owner = NULL;
	flag->touch = DomTouchFlag;
	NEXT_KEYFRAME( flag, DomFlagThink );
	flag->classname = "item_flag";
	flag->svflags &= ~SVF_NOCLIENT;
	gi.linkentity( flag );

	dom_flag_count ++;
}

void EspSetMarker(int team, char *str)
{
	char *flag_name;
	edict_t *ent = NULL;
	vec3_t position;

	if(team == TEAM1)
		flag_name = "item_flag_team1";
	else if(team == TEAM2)
		flag_name = "item_flag_team2";
	else
		return;

	if (sscanf(str, "<%f %f %f>", &position[0], &position[1], &position[2]) != 3)
		return;

	/* find and remove existing flag(s) if any */
	while ((ent = G_Find(ent, FOFS(classname), flag_name)) != NULL) {
		G_FreeEdict (ent);
	}

	ent = G_Spawn ();

	ent->classname = ED_NewString (flag_name);
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
		gi.dprintf("-------------------------------------\n");
		gi.dprintf("Hard-coded Espionage configuration loaded\n");
		gi.dprintf(" Game type  : %s\n", "Assassinate the Leader");
		gi.dprintf(" Respawn times: 10 seconds\n");
		gi.dprintf(" Skins\n");
		gi.dprintf("  Red Member: %s\n", "male/ctf_r");
		gi.dprintf("  Red Leader: %s\n", "male/babarracuda");
		gi.dprintf("  Blue Member: %s\n", "male/ctf_b");
		gi.dprintf("  Blue Leader: %s\n", "male/blues");


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
			if(strcmp(ptr, "etv") == 0)
				espgame.type = 1;
				gametypename = "Escort the VIP";	
			if(strcmp(ptr, "atl") == 0)
				espgame.type = 0;
				gametypename = "Assassinate the Leader";
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
			espgame.custom_spawns = true;
		}
		ptr = INI_Find(fh, "spawns", "blue");
		if(ptr) {
			gi.dprintf("  Blue     : %s\n", ptr);
			EspSetTeamSpawns(TEAM2, ptr);
			espgame.custom_spawns = true;
		}
		
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
			gi.dprintf("Warning: No skin set for blue_leader, defaulting to male/adidas\n");
			gi.dprintf("  Blue Leader: %s\n", "male/adidas");
		}
	}

	// automagically change spawns *only* when we do not have team spawns
	if(!espgame.custom_spawns)
		ChangePlayerSpawns();

	gi.dprintf("-------------------------------------\n");

	fclose(fh);

	return true;
}


// qboolean EspLoadConfig( const char *mapname )
// {
// 	char buf[1024] = "";
// 	char *ptr = NULL;
// 	FILE *fh = NULL;
// 	size_t i = 0;

// 	gi.dprintf("-------------------------------------\n");

// 	esp_marker_count = 0;
// 	memset( &dom_team_flags, 0, sizeof(dom_team_flags) );
// 	esp_winner = NOTEAM;
// 	teams[ TEAM1 ].score = 0;
// 	teams[ TEAM2 ].score = 0;
// 	teams[ TEAM3 ].score = 0;
// 	esp_last_score = 0;
// 	target_marker = gi.modelindex("models/espionage/marker.md2");
// 	esp_pics[ TEAM1 ] = gi.imageindex(teams[ TEAM1 ].skin_index);
// 	esp_pics[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].skin_index);
// 	esp_pics[ TEAM3 ] = gi.imageindex(teams[ TEAM3 ].skin_index);
// 	esp_pics_leader[ TEAM1 ] = gi.imageindex(teams[ TEAM1 ].leader_skin_index);
// 	esp_pics_leader[ TEAM2 ] = gi.imageindex(teams[ TEAM2 ].leader_skin_index);
// 	esp_pics_leader[ TEAM3 ] = gi.imageindex(teams[ TEAM3 ].leader_skin_index);

// 	Com_sprintf( buf, sizeof(buf), "%s/tng/%s.esp", GAMEVERSION, mapname );
// 	fh = fopen( buf, "rt" );
// 	if( fh )
// 	{
// 		// Found an Espionage config file for this map.

// 		gi.dprintf( "%s\n", buf );

// 		ptr = INI_Find( fh, "esp", "target" );
// 		if( ptr )
// 			ptr = strchr( ptr, '<' );
// 		while( ptr )
// 		{
// 			edict_t *marker = G_Spawn();

// 			char *space = NULL, *end = strchr( ptr + 1, '>' );
// 			if( end )
// 				*end = '\0';

// 			marker->s.origin[0] = atof( ptr + 1 );
// 			space = strchr( ptr + 1, ' ' );
// 			if( space )
// 			{
// 				marker->s.origin[1] = atof( space );
// 				space = strchr( space + 1, ' ' );
// 				if( space )
// 				{
// 					marker->s.origin[2] = atof( space );
// 					space = strchr( space + 1, ' ' );
// 					if( space )
// 						marker->s.angles[YAW] = atof( space );
// 				}
// 			}

// 			EspMakeMarker( marker );
// 			ptr = strchr( (end ? end : ptr) + 1, '<' );
// 		}

// 		fclose( fh );
// 		fh = NULL;
// 	}

// 	if( esp_marker_count )
// 		gi.dprintf( "Espionage Escort the VIP mode: %i marker loaded.\n", esp_marker_count );
// 	else
// 	{
// 		// Try to generate markers for ETV mode

// 		edict_t *spawns[ 32 ] = {0}, *spot = NULL;
// 		int spawn_count = 0, need = 3;

// 		if(( spot = G_Find( NULL, FOFS(classname), "item_flag_team1" )) != NULL)
// 		{
// 			EspMakeMarker( spot );
// 			need --;
// 		}

// 		spot = NULL;
// 		while( ((spot = G_Find( spot, FOFS(classname), "info_player_deathmatch" )) != NULL) && (spawn_count < 32) )
// 		{
// 			spawns[ spawn_count ] = spot;
// 			spawn_count ++;
// 		}

// 		// If we have flags, don't convert scarce player spawns.
// 		if( dom_flag_count && (spawn_count <= 3) )
// 			need = 0;
// 		// Can't convert more spawns than we have.
// 		else if( need > spawn_count )
// 			need = spawn_count;
// 		else if( need < spawn_count )
// 		{
// 			// If we have plenty of choices, randomize which spawns we convert.
// 			for( i = 0; i < need; i ++ )
// 			{
// 				edict_t *swap = spawns[ i ];
// 				int index = rand() % spawn_count;
// 				spawns[ i ] = spawns[ index ];
// 				spawns[ index ] = swap;
// 			}
// 		}

// 		for( i = 0; i < need; i ++ )
// 		{
// 			if( spawn_count > 3 )
// 			{
// 				// Turn a spawn location into a flag.
// 				DomMakeFlag( spawns[ i ] );
// 				spawn_count --;
// 			}
// 			else if( ! dom_flag_count )
// 			{
// 				// We're desperate, so make a copy of a player spawn as a flag location.
// 				edict_t *flag = G_Spawn();
// 				VectorCopy( spawns[ i ]->s.origin, flag->s.origin );
// 				VectorCopy( spawns[ i ]->s.angles, flag->s.angles );
// 				DomMakeFlag( flag );
// 			}
// 		}

// 		if( dom_flag_count )
// 			gi.dprintf( "Domination mode: %i flags generated.\n", dom_flag_count );
// 		else
// 			gi.dprintf( "Warning: Domination needs flags in: tng/%s.dom\n", mapname );
// 	}

// 	gi.dprintf("-------------------------------------\n");

// 	return (dom_flag_count > 0);
// }


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
	ent->client->ps.stats[ STAT_TEAM3_SCORE ] = teams[ TEAM3 ].score;

	// Team icons for the score display and HUD.
	ent->client->ps.stats[ STAT_TEAM1_PIC ] = dom_pics[ TEAM1 ];
	ent->client->ps.stats[ STAT_TEAM2_PIC ] = dom_pics[ TEAM2 ];
	ent->client->ps.stats[ STAT_TEAM3_PIC ] = dom_pics[ TEAM3 ];

	// During intermission, blink the team icon of the winning team.
	if( level.intermission_framenum && ((level.realFramenum / FRAMEDIV) & 8) )
	{
		if (dom_winner == TEAM1)
			ent->client->ps.stats[ STAT_TEAM1_PIC ] = 0;
		else if (dom_winner == TEAM2)
			ent->client->ps.stats[ STAT_TEAM2_PIC ] = 0;
		else if (dom_winner == TEAM3)
			ent->client->ps.stats[ STAT_TEAM3_PIC ] = 0;
	}
}
