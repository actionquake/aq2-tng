// Common definitions for precise trace collision routines.

typedef byte g_partid_t;

typedef struct g_collision_data_s g_collision_data_t;

qboolean Col_SetModel(edict_t *ent, const char *model, const char *collision_model);

typedef struct {
	trace_t		tr;
	g_partid_t	part;
} trace_precise_t;

trace_precise_t Col_PreciseTrace(vec3_t start, vec3_t end, edict_t *passent, int contentmask);

#define COLLISION_PART_NONE 0

#define COLLISION_PART_CHEST 15
#define COLLISION_PART_HEAD 242
#define COLLISION_PART_STOMACH 243
#define COLLISION_PART_LEG 208

typedef struct {
	const char *model_name;
	g_collision_data_t *model;
} entity_collision_t;

#define Col_EncodeDamage(d, p) \
	((d) | ((p) << 24))

#define Col_DecodeDamage(d, p) \
	p = ((d) >> 24) & 0xFF, d = d & 0xFFFFFF

void Col_RemoveUnusedModels(void);

void Col_ClearModelLinks(void);
