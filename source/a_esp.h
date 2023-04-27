extern cvar_t *esp;

#define IS_LEADER(ent) (teams[(ent)->client->resp.team].leader == (ent))
#define	HAVE_LEADER(teamNum) (teams[(teamNum)].leader)

int EspFlagOwner( edict_t *flag );
void EspRemember( const edict_t *ent, const gitem_t *item );
qboolean EspLoadConfig( const char *mapname );
void EspSetupStatusbar( void );
void SetEspStats( edict_t *ent );

typedef enum
{
  ESP_STATE_START,
  ESP_STATE_PLAYING
}
espstate_t;

typedef struct espgame_s {
	int team1, team2, team3;
	int total1, total2;	// these are only set when going into intermission!
	int last_flag_capture;
	int last_capture_team;
	int halftime;

	/* ESP configuration from .esp */
	int type;		// 0 = atl, 1 = etv
	/* team spawn times in seconds */
	int spawn_red;
	int spawn_blue;
    int spawn_green;
	qboolean custom_spawns;
	qboolean custom_skins;
	char author[64];
	char name[128];
} espgame_t;

extern espgame_t espgame;

extern gitem_t *team_flag[TEAM_TOP];

#define ESP_TEAM1_SKIN "ctf_r"
#define ESP_TEAM2_SKIN "ctf_b"
#define ESP_TEAM3_SKIN "ctf_g"
#define ESP_TEAMLEADER1_SKIN "babarracuda"
#define ESP_TEAMLEADER2_SKIN "blues"
#define ESP_TEAMLEADER3_SKIN "hulk2"


#define DF_ESP_FORCEJOIN	131072

// Team score bonuses
#define TS_TEAM_BONUS                      1   // this is the bonus point teams get for fragging enemy leader

// Individual score bonuses
#define ESP_LEADER_FRAG_BONUS   	        10	// points player receives for fragging enemy leader
#define ESP_LEADER_ESCORT_BONUS             10  // points player receives if they are leader and they successfully touch escort marker

#define ESP_LEADER_DANGER_PROTECT_BONUS 	2	// bonus for fragging someone who has recently hurt your leader
#define ESP_LEADER_PROTECT_BONUS    		1	// bonus for fragging someone while either you or your target are near your leader
#define ESP_MARKER_DEFENSE_BONUS    		1	// bonus for fragging someone while either you or your target are near your flag

#define ESP_LEADER_HARASS_BONUS             2   // points for attacking defenders of the leader

#define ESP_TARGET_PROTECT_RADIUS   		400	// the radius around an object being defended where a target will be worth extra frags
#define ESP_ATTACKER_PROTECT_RADIUS 		400	// the radius around an object being defended where an attacker will get extra frags when making kills

#define ESP_LEADER_DANGER_PROTECT_TIMEOUT	8   // time in seconds until player is eligible for the bonus points after receiving them
#define ESP_LEADER_HARASS_TIMEOUT       	8
#define ESP_FRAG_LEADER_ASSIST_TIMEOUT		10

void ESPInit (void);
void ESPSetFlag(int, char *);
void ESPSetTeamSpawns(int, char *);
int ESPGetRespawnTime(edict_t *);

qboolean AllTeamsHaveLeaders(void);

void SP_info_player_team1 (edict_t * self);
void SP_info_player_team2 (edict_t * self);

char *ESPTeamName (int team);
char *ESPOtherTeamName (int team);
void ESPAssignTeam (gclient_t * who);
edict_t *SelectESPSpawnPoint (edict_t * ent);

void ESPResetFlags(void);

qboolean ESPPickup_Flag (edict_t * ent, edict_t * other);
void ESPDrop_Flag (edict_t * ent, gitem_t * item);
void ESPEffects (edict_t * player);
void ESPCalcScores (void);
void SetESPStats (edict_t * ent);
void ESPDeadDropFlag (edict_t * self);
void ESPFlagSetup (edict_t * ent);
void ESPScoreBonuses (edict_t * targ, edict_t * inflictor, edict_t * attacker);
void ESPCheckHurtCarrier (edict_t * targ, edict_t * attacker);

void ESPOpenJoinMenu (edict_t * ent);

qboolean EspCheckRules (void);
qboolean HasFlag (edict_t * ent);

void SP_misc_ctf_banner (edict_t * ent);
void SP_misc_ctf_small_banner (edict_t * ent);

void SP_trigger_teleport (edict_t * ent);
void SP_info_teleport_destination (edict_t * ent);

void ResetPlayers ();
void GetESPScores(int *t1score, int *t2score);
void ESPCapReward(edict_t *);
