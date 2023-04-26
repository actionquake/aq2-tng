extern cvar_t *esp;

int EspFlagOwner( edict_t *flag );
qboolean EspCheckRules( void );
void EspRemember( const edict_t *ent, const gitem_t *item );
qboolean EspLoadConfig( const char *mapname );
void EspSetupStatusbar( void );
void SetEspStats( edict_t *ent );
