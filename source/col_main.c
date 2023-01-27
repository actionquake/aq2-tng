#include "g_local.h"
#include <inttypes.h>
#include <float.h>

typedef vec_t vec2_t[2];

typedef struct
{
	uint16_t	width, height;
	g_partid_t	*map;
} g_collision_part_t;

typedef struct
{
	struct
	{
		uint16_t	xyz;
		uint16_t	st;
	} verts[3];
} g_collision_tri_t;

typedef struct
{
	vec3_t *verts;
	vec3_t	mins, maxs;
} g_collision_frame_t;

typedef struct g_collision_data_s
{
	uint16_t	num_tris;
	uint16_t	num_frames;
	uint16_t	num_maps;

	g_collision_tri_t *tris;
	vec2_t		*st;
	g_collision_frame_t *frames;
	g_collision_part_t *parts;

	const char *name;
	g_collision_data_t *next;
	qboolean inuse;
} g_collision_data_t;

typedef struct
{
	int16_t    s;
	int16_t    t;
} dmd2stvert_t;

typedef struct
{
	uint16_t    index_xyz[3];
	uint16_t    index_st[3];
} dmd2triangle_t;

typedef struct
{
	uint8_t    v[3];            // scaled byte to fit in frame mins/maxs
	uint8_t    lightnormalindex;
} dmd2trivertx_t;

typedef struct
{
	float           scale[3];       // multiply byte verts by this
	float           translate[3];   // then add this
	char            name[16];       // frame name from grabbing
	dmd2trivertx_t  verts[0];       // variable sized
} dmd2frame_t;

typedef struct
{
	uint32_t        ident;
	uint32_t        version;

	uint32_t        skinwidth;
	uint32_t        skinheight;
	uint32_t        framesize;      // byte size of each frame

	uint32_t        num_skins;
	uint32_t        num_xyz;
	uint32_t        num_st;         // greater than num_xyz for seams
	uint32_t        num_tris;
	uint32_t        num_glcmds;     // dwords in strip/fan command list
	uint32_t        num_frames;

	uint32_t        ofs_skins;      // each skin is a MAX_SKINNAME string
	uint32_t        ofs_st;         // byte offset from start for stverts
	uint32_t        ofs_tris;       // offset for dtriangles
	uint32_t        ofs_frames;     // offset for first frame
	uint32_t        ofs_glcmds;
	uint32_t        ofs_end;        // end of file
} dmd2header_t;

typedef struct
{
	uint8_t     manufacturer;
	uint8_t     version;
	uint8_t     encoding;
	uint8_t     bits_per_pixel;
	uint16_t    xmin, ymin, xmax, ymax;
	uint16_t    hres, vres;
	uint8_t     palette[48];
	uint8_t     reserved;
	uint8_t     color_planes;
	uint16_t    bytes_per_line;
	uint16_t    palette_type;
	uint8_t     filler[58];
	uint8_t     data[0];            // unbounded
} dpcx_t;

static g_collision_data_t *collision_table;

static qboolean Col_ReadSkin(const char *filename, g_collision_part_t *part)
{
	FILE *skin = fopen(filename, "rb");

	if (!skin)
		return qfalse;

	dpcx_t pcx;

	fread(&pcx, sizeof(pcx), 1, skin);

	part->width = (pcx.xmax - pcx.xmin) + 1;
	part->height = (pcx.ymax - pcx.ymin) + 1;

	part->map = gi.TagMalloc(part->width * part->height, TAG_GAME);

	byte *pixels = part->map;

	for (int y = 0; y < part->height; y++, pixels += part->width)
	{
		for (int x = 0; x < pcx.bytes_per_line;)
		{
			byte dataByte;
			uint32_t runLength;

			fread(&dataByte, sizeof(byte), 1, skin);

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				fread(&dataByte, sizeof(byte), 1, skin);
			}
			else
				runLength = 1;

			while (runLength--)
			{
				if (x < part->width)
					pixels[x] = dataByte;

				x++;
			}
		}
	}

	fclose(skin);
	return qtrue;
}

static g_collision_data_t *Col_Load(const char *filename)
{
	char	name[MAX_OSPATH];
	cvar_t *game;

	game = gi.cvar("game", "", 0);
	sprintf (name, "%s/%s", !*game->string ? GAMEVERSION : game->string, filename);

	FILE *fp = fopen(name, "rb");

	if (!fp) {
		gi.dprintf("Disabling true_hitbox - File not found: %s.\n", filename);
		gi.cvar_forceset("true_hitbox", "0");
		return NULL;
	}

	dmd2header_t header;
	fread(&header, sizeof(header), 1, fp);

	g_collision_data_t *collision = gi.TagMalloc(sizeof(g_collision_data_t), TAG_GAME);

	collision->num_frames = header.num_frames;
	collision->num_tris = header.num_tris;

	collision->tris = gi.TagMalloc(sizeof(*collision->tris) * collision->num_tris, TAG_GAME);

	fseek(fp, header.ofs_tris, SEEK_SET);

	uint16_t *xyzst = (uint16_t *) collision->tris;

	for (int i = 0; i < header.num_tris; i++)
	{
		dmd2triangle_t tri;

		fread(&tri, sizeof(tri), 1, fp);

		for (int x = 0; x < 3; x++)
		{
			*xyzst++ = tri.index_xyz[x];
			*xyzst++ = tri.index_st[x];
		}
	}

	collision->st = gi.TagMalloc(sizeof(*collision->st) * header.num_st, TAG_GAME);

	fseek(fp, header.ofs_st, SEEK_SET);

	vec_t *stout = (vec_t *) collision->st;

	for (int i = 0; i < header.num_st; i++)
	{
		dmd2stvert_t st;

		fread(&st, sizeof(st), 1, fp);

		stout[0] = st.s / (float)header.skinwidth;
		stout[1] = st.t / (float)header.skinheight;

		stout += 2;
	}

	collision->frames = gi.TagMalloc(sizeof(*collision->frames) * header.num_frames, TAG_GAME);

	fseek(fp, header.ofs_frames, SEEK_SET);

	for (int i = 0; i < header.num_frames; i++)
	{
		vec_t *fxyz = (vec_t *) (collision->frames[i].verts = gi.TagMalloc(sizeof(vec3_t) * header.num_xyz, TAG_GAME));

		dmd2frame_t frame;

		fread(&frame, sizeof(frame), 1, fp);

		ClearBounds(collision->frames[i].mins, collision->frames[i].maxs);

		for (int x = 0; x < header.num_xyz; x++)
		{
			uint8_t ixyz[4];

			fread(&ixyz, sizeof(uint8_t), 4, fp);

			for (int y = 0; y < 3; y++)
			{
				*fxyz = ixyz[y] * frame.scale[y] + frame.translate[y];
				fxyz++;
			}

			AddPointToBounds(fxyz - 3, collision->frames[i].mins, collision->frames[i].maxs);
		}
	}

	// see if we have collision override maps
	char skin_name[MAX_QPATH];

	sprintf (skin_name, "%s/%s.c0.pcx", !*game->string ? GAMEVERSION : game->string, filename);
	FILE *f = fopen(skin_name, "rb");

	if (f)
	{
		// yes, so enumerate and load
		collision->num_maps = 1;

		fclose(f);

		for (int i = 1; i < 32; i++)
		{
			sprintf (skin_name, "%s/%s.c%i.pcx", !*game->string ? GAMEVERSION : game->string, filename, i);

			f = fopen(skin_name, "rb");

			if (f)
			{
				collision->num_maps++;
				fclose(f);
				continue;
			}

			break;
		}

		collision->parts = gi.TagMalloc(sizeof(*collision->parts) * collision->num_maps, TAG_GAME);

		g_collision_part_t *part = collision->parts;

		for (int i = 0; i < collision->num_maps; i++)
		{
			if (i != 0)
				*part = *(part - 1);
			else
			{
				part->width = 0;
				part->height = 0;
				part->map = NULL;
			}

			sprintf (skin_name, "%s/%s.c%i.pcx", !*game->string ? GAMEVERSION : game->string, filename, i);

			Col_ReadSkin(skin_name, part);

			part++;
		}
	}
	else
	{
		// no, so use the skins in the header
		collision->num_maps = header.num_skins;

		collision->parts = gi.TagMalloc(sizeof(*collision->parts) * collision->num_maps, TAG_GAME);

		g_collision_part_t *part = collision->parts;

		for (int i = 0; i < collision->num_maps; i++)
		{
			if (i != 0)
				*part = *(part - 1);
			else
			{
				part->width = 0;
				part->height = 0;
				part->map = NULL;
			}

			fseek(fp, header.ofs_skins + (i * 64), SEEK_SET);
			memset(name, 0, sizeof(name));
			fread(name, 1, 64, fp);

			sprintf (skin_name, "%s/%s", !*game->string ? GAMEVERSION : game->string, name);

			Col_ReadSkin(skin_name, part);

			part++;
		}
	}

	fclose(fp);

	collision->name = filename;

	collision->next = collision_table;
	collision_table = collision;
	collision->inuse = true;

	return collision;
}

qboolean Col_SetModel(edict_t *ent, const char *model, const char *collision_model)
{
	if (model)
	{
		if (collision_model == NULL)
			collision_model = model;

		gi.setmodel(ent, (char *) model);
	}

	for (g_collision_data_t *m = collision_table; m; m = m->next)
		if (Q_stricmp(collision_model, m->name) == 0)
		{
			ent->collision.model_name = m->name;
			ent->collision.model = m;
			m->inuse = true;
			return qtrue;
		}

	ent->collision.model = Col_Load(collision_model);

	if (ent->collision.model)
	{
		ent->collision.model_name = ent->collision.model->name;
		return qtrue;
	}

	return qfalse;
}

void Col_ClearModelLinks(void)
{
	for (g_collision_data_t *col = collision_table; col; col = col->next)
		col->inuse = qfalse;
}

void Col_RemoveUnusedModels(void)
{
	g_collision_data_t **ptr = &collision_table;

	while (*ptr)
	{
		g_collision_data_t *self = (*ptr);
		
		// freeing self
		if (!self->inuse)
		{
			g_collision_data_t *real_next = self->next;

			gi.TagFree(self->tris);

			for (int i = 0; i < self->num_frames; i++)
				gi.TagFree(self->frames[i].verts);

			gi.TagFree(self->frames);

			gi.TagFree(self->st);

			for (int i = 0; i < self->num_maps; i++)
			{
				if (!self->parts[i].map)
					continue;

				gi.TagFree(self->parts[i].map);

				for (int x = 0; x < self->num_maps; x++)
					if (self->parts[x].map == self->parts[i].map)
						self->parts[x].map = NULL;

				self->parts[i].map = NULL;
			}

			gi.TagFree(self->parts);

			gi.TagFree(self);

			(*ptr) = real_next;
		}
		else
			ptr++;
	}
}

// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
static void VectorBarycentric(const vec3_t p, const vec3_t a, const vec3_t b, const vec3_t c, vec3_t uvw)
{
	vec3_t v0, v1, v2;
	VectorSubtract(b, a, v0);
	VectorSubtract(c, a, v1);
	VectorSubtract(p, a, v2);
	float d00 = DotProduct(v0, v0);
	float d01 = DotProduct(v0, v1);
	float d11 = DotProduct(v1, v1);
	float d20 = DotProduct(v2, v0);
	float d21 = DotProduct(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	uvw[1] = (d11 * d20 - d01 * d21) / denom;
	uvw[2] = (d00 * d21 - d01 * d20) / denom;
	uvw[0] = 1.0f - uvw[1] - uvw[2];
}

static qboolean intersectTriangle(vec3_t pt, vec3_t dir, float *t, vec3_t v0, vec3_t v1, vec3_t v2)
{
	const float EPSILON = 0.0000001;
	vec3_t edge1, edge2, h, s, q;
	float a, f, u, v;
	VectorSubtract(v1, v0, edge1);
	VectorSubtract(v2, v0, edge2);
	CrossProduct(dir, edge2, h);
	a = DotProduct(edge1, h);
	if (a > 0 || (a > -EPSILON && a < EPSILON))
		return false;    // This ray is parallel to this triangle.
	f = 1.0 / a;
	VectorSubtract(pt, v0, s);
	u = f * DotProduct(s, h);
	if (u < 0.0 || u > 1.0)
		return false;
	CrossProduct(s, edge1, q);
	v = f * DotProduct(dir, q);
	if (v < 0.0 || u + v > 1.0)
		return false;
	// At this stage we can compute t to find out where the intersection point is on the line.
	*t = f * DotProduct(edge2, q);
	if (*t > EPSILON) // ray intersection
		return true;
	
	// This means that there is a line intersection but not a ray intersection.
	return false;
}

typedef vec_t mat4x4_t[16];

static void MatIdentity(mat4x4_t mat)
{
	memset(mat, 0, sizeof(mat4x4_t));
	mat[0] = mat[5] = mat[10] = mat[15] = 1;
}

static void MatTranslate(mat4x4_t mat, float x, float y, float z)
{
	mat[12] = mat[0] * x + mat[4] * y + mat[8] * z + mat[12];
	mat[13] = mat[1] * x + mat[5] * y + mat[9] * z + mat[13];
	mat[14] = mat[2] * x + mat[6] * y + mat[10] * z + mat[14];
	mat[15] = mat[3] * x + mat[7] * y + mat[11] * z + mat[15];
}

static float hypot_array(float *args, int num_args)
{
	int y = 0, i = num_args;
	while (i--)
		y += args[i] * args[i];
	return sqrt(y);
}

static void MatRotate(mat4x4_t mat, float rad, vec3_t axis)
{
	float x = axis[0], y = axis[1], z = axis[2];
	float len = hypot_array((float[]) { x, y, z }, 3);

	if (len < FLT_EPSILON)
		return;

	len = 1 / len;
	x *= len;
	y *= len;
	z *= len;

	float s = sin(rad);
	float c = cos(rad);
	float t = 1 - c;

	float a00 = mat[0]; float a01 = mat[1]; float a02 = mat[2]; float a03 = mat[3];
	float a10 = mat[4]; float a11 = mat[5]; float a12 = mat[6]; float a13 = mat[7];
	float a20 = mat[8]; float a21 = mat[9]; float a22 = mat[10]; float a23 = mat[11];

	// Construct the elements of the rotation matrix
	float b00 = x * x * t + c; float b01 = y * x * t + z * s; float b02 = z * x * t - y * s;
	float b10 = x * y * t - z * s; float b11 = y * y * t + c; float b12 = z * y * t + x * s;
	float b20 = x * z * t + y * s; float b21 = y * z * t - x * s; float b22 = z * z * t + c;

	// Perform rotation-specific matrix multiplication
	mat[0] = a00 * b00 + a10 * b01 + a20 * b02;
	mat[1] = a01 * b00 + a11 * b01 + a21 * b02;
	mat[2] = a02 * b00 + a12 * b01 + a22 * b02;
	mat[3] = a03 * b00 + a13 * b01 + a23 * b02;
	mat[4] = a00 * b10 + a10 * b11 + a20 * b12;
	mat[5] = a01 * b10 + a11 * b11 + a21 * b12;
	mat[6] = a02 * b10 + a12 * b11 + a22 * b12;
	mat[7] = a03 * b10 + a13 * b11 + a23 * b12;
	mat[8] = a00 * b20 + a10 * b21 + a20 * b22;
	mat[9] = a01 * b20 + a11 * b21 + a21 * b22;
	mat[10] = a02 * b20 + a12 * b21 + a22 * b22;
	mat[11] = a03 * b20 + a13 * b21 + a23 * b22;
}

static void VectorTransform(vec3_t a, mat4x4_t m, vec3_t out)
{
	float x = a[0], y = a[1], z = a[2];
	float w = m[3] * x + m[7] * y + m[11] * z + m[15];
	if (!w)
		w = 1.f;
	out[0] = (m[0] * x + m[4] * y + m[8] * z + m[12]) / w;
	out[1] = (m[1] * x + m[5] * y + m[9] * z + m[13]) / w;
	out[2] = (m[2] * x + m[6] * y + m[10] * z + m[14]) / w;
}

static void VectorLerp(vec3_t start, vec3_t end, float t, vec3_t out)
{
	for (int i = 0; i < 3; i++)
		out[i] = start[i] + t * (end[i] - start[i]);
}

#ifndef DEG2RAD
#define DEG2RAD(x)		(x) * (M_PI / 180.0f)
#endif

typedef struct {
	vec3_t mins, maxs;
	int solid;
} edict_backup_t;

trace_precise_t Col_PreciseTrace(vec3_t start, vec3_t end, edict_t *passent, int contentmask)
{
	trace_precise_t tr;

	tr.part = COLLISION_PART_NONE;

	edict_t *entity_list[MAX_EDICTS];

	// calculate where the "real" end would be if we didn't hit any enemies.
	// this is because most traces are extremely long, and we'd get like every
	// enemy in the map in the BoxEdicts check, so we're gonna speed it up
	// by doing the initial clip ourselves.
	trace_t real_end = gi.trace(start, NULL, NULL, end, NULL, MASK_SOLID);
	
	vec3_t mins, maxs;
	ClearBounds(mins, maxs);
	AddPointToBounds(start, mins, maxs);
	AddPointToBounds(real_end.endpos, mins, maxs);

	int num_entities = gi.BoxEdicts(mins, maxs, entity_list, MAX_EDICTS, AREA_SOLID);

	// no entities, so just pass through.
	if (!num_entities)
	{
		tr.tr = gi.trace(start, NULL, NULL, end, passent, contentmask);
		return tr;
	}
	
	edict_t *entity_collidables[64];
	edict_backup_t entity_backup[64];
	int num_collidables = 0;

	// we only care about modifying entities with collision data,
	// remove the entities from the list that aren't this
	for (int i = 0; i < num_entities && num_collidables < 64; i++)
		if (entity_list[i]->solid == SOLID_BBOX &&
			!(entity_list[i]->svflags & SVF_DEADMONSTER) &&
			entity_list[i]->collision.model)
			entity_collidables[num_collidables++] = entity_list[i];

	// no collidables, so just pass through.
	if (!num_collidables)
	{
		tr.tr = gi.trace(start, NULL, NULL, end, passent, contentmask);
		return tr;
	}

	// we have collidables! back up their data and calculate their new bbox (if appropriate)
	for (int i = 0; i < num_collidables; i++)
	{
		edict_t *e = entity_collidables[i];
		g_collision_data_t *cd = e->collision.model;

		VectorCopy(e->mins, entity_backup[i].mins);
		VectorCopy(e->maxs, entity_backup[i].maxs);
		entity_backup[i].solid = e->solid;

		VectorCopy(cd->frames[e->s.frame % cd->num_frames].mins, e->mins);
		VectorCopy(cd->frames[e->s.frame % cd->num_frames].maxs, e->maxs);

		// if we have angles, rotate and calculate new bbox
		if (e->s.angles[0] || e->s.angles[1] || e->s.angles[2])
		{
			mat4x4_t m;
			MatIdentity(m);
			MatRotate(m, DEG2RAD(e->s.angles[2]), (vec3_t) { 1, 0, 0 });
			MatRotate(m, -DEG2RAD(e->s.angles[0]), (vec3_t) { 0, 1, 0 });
			MatRotate(m, -DEG2RAD(e->s.angles[1]), (vec3_t) { 0, 0, 1 });

			vec3_t points[8];
			
			VectorSet(points[0], e->mins[0], e->mins[1], e->mins[2]);
			VectorSet(points[1], e->maxs[0], e->mins[1], e->mins[2]);
			VectorSet(points[2], e->mins[0], e->maxs[1], e->mins[2]);
			VectorSet(points[3], e->maxs[0], e->maxs[1], e->mins[2]);
			VectorSet(points[4], e->mins[0], e->mins[1], e->maxs[2]);
			VectorSet(points[5], e->maxs[0], e->mins[1], e->maxs[2]);
			VectorSet(points[6], e->mins[0], e->maxs[1], e->maxs[2]);
			VectorSet(points[7], e->maxs[0], e->maxs[1], e->maxs[2]);

			ClearBounds(e->mins, e->maxs);

			for (int p = 0; p < 8; p++)
			{
				VectorTransform(points[p], m, points[p]);
				AddPointToBounds(points[p], e->mins, e->maxs);
			}
		}

		gi.linkentity(e);
	}

	// loop through, hitting collidables along the way
	while (true)
	{
		// do trace as normal
		tr.tr = gi.trace(start, NULL, NULL, end, passent, contentmask);

		// not a collidable, so we're done here
		if (!tr.tr.ent->collision.model)
			break;

		// found collidable,
		edict_t *ent = tr.tr.ent;
		const g_collision_data_t *collide = ent->collision.model;

		// make the ray relative to the entity
		vec3_t relative_start, relative_end;
		mat4x4_t ray_matrix;

		MatIdentity(ray_matrix);
		MatRotate(ray_matrix, DEG2RAD(ent->s.angles[2]), (vec3_t) { 1, 0, 0 });
		MatRotate(ray_matrix, -DEG2RAD(ent->s.angles[0]), (vec3_t) { 0, 1, 0 });
		MatRotate(ray_matrix, -DEG2RAD(ent->s.angles[1]), (vec3_t) { 0, 0, 1 });
		MatTranslate(ray_matrix, -ent->s.origin[0], -ent->s.origin[1], -ent->s.origin[2]);

		VectorTransform(start, ray_matrix, relative_start);
		VectorTransform(end, ray_matrix, relative_end);

		vec3_t ray_dir;

		VectorSubtract(relative_end, relative_start, ray_dir);

		// start checking triangles!
		const g_collision_frame_t *frame = &collide->frames[ent->s.frame % collide->num_frames];

		for (int i = 0; i < collide->num_tris; i++)
		{
			const g_collision_tri_t *tri = &collide->tris[i];

			vec_t *v0 = frame->verts[tri->verts[0].xyz];
			vec_t *v1 = frame->verts[tri->verts[1].xyz];
			vec_t *v2 = frame->verts[tri->verts[2].xyz];

			float t;

			if (intersectTriangle(relative_start, ray_dir, &t, v0, v1, v2))
			{
				// setup end pos, return us
				tr.tr.fraction = t;
				VectorLerp(start, end, t, tr.tr.endpos);

				int map_num = ent->s.skinnum % collide->num_maps;

				// let's find texture coordinates
				if (collide->parts[map_num].map)
				{
					vec3_t relative_endpos;
					VectorLerp(relative_start, relative_end, t, relative_endpos);

					vec3_t uvw;
					VectorBarycentric(relative_endpos, v0, v1, v2, uvw);

					vec_t *st0 = collide->st[tri->verts[0].st];
					vec_t *st1 = collide->st[tri->verts[1].st];
					vec_t *st2 = collide->st[tri->verts[2].st];

					vec_t uv_s = uvw[0] * st0[0] + uvw[1] * st1[0] + (1 - uvw[0] - uvw[1]) * st2[0];
					vec_t uv_t = uvw[0] * st0[1] + uvw[1] * st1[1] + (1 - uvw[0] - uvw[1]) * st2[1];

					uv_s *= collide->parts[map_num].width;
					uv_t *= collide->parts[map_num].height;

					int s_int = floor(uv_s);
					int t_int = floor(uv_t);
					int b = (t_int * collide->parts[map_num].width) + s_int;

					tr.part = collide->parts[map_num].map[b];
				}

				vec3_t normal, u, v;

				VectorSubtract(v2, v0, u);
				VectorSubtract(v1, v0, v);
				CrossProduct(u, v, normal);
				VectorNormalize(normal);

				MatIdentity(ray_matrix);
				MatRotate(ray_matrix, DEG2RAD(ent->s.angles[1]), (vec3_t) { 0, 0, 1 });
				MatRotate(ray_matrix, DEG2RAD(ent->s.angles[0]), (vec3_t) { 0, 1, 0 });
				MatRotate(ray_matrix, -DEG2RAD(ent->s.angles[2]), (vec3_t) { 1, 0, 0 });

				VectorTransform(normal, ray_matrix, tr.tr.plane.normal);

				goto finish;
			}
		}

		// didn't hit, so mark us non-solid and keep going
		ent->solid = SOLID_NOT;
		gi.linkentity(ent);
	}

finish:
	// return entities back to their usual boxes
	for (int i = 0; i < num_collidables; i++)
	{
		VectorCopy(entity_backup[i].mins, entity_collidables[i]->mins);
		VectorCopy(entity_backup[i].maxs, entity_collidables[i]->maxs);

		entity_collidables[i]->solid = entity_backup[i].solid;

		gi.linkentity(entity_collidables[i]);
	}

	return tr;
}