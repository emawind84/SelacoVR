#pragma once

enum area_t
{
	area_normal,
	area_below,
	area_above,
	area_default
};


// Global functions. Make them members of GLRenderer later?
bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void gl_ClearFakeFlat();
sector_t * gl_FakeFlat(sector_t * sec, area_t in_area, bool back, sector_t *localcopy = nullptr);
area_t gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);

