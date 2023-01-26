
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_shape_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Tilemap data
 * \{ */

int shadow_tile_index(ivec2 tile)
{
  return tile.x + tile.y * SHADOW_TILEMAP_RES;
}

ivec2 shadow_tile_coord(int tile_index)
{
  return ivec2(tile_index % SHADOW_TILEMAP_RES, tile_index / SHADOW_TILEMAP_RES);
}

/* Return bottom left pixel position of the tilemap inside the tilemap atlas. */
ivec2 shadow_tilemap_start(int tilemap_index)
{
  return SHADOW_TILEMAP_RES *
         ivec2(tilemap_index % SHADOW_TILEMAP_PER_ROW, tilemap_index / SHADOW_TILEMAP_PER_ROW);
}

ivec2 shadow_tile_coord_in_atlas(ivec2 tile, int tilemap_index)
{
  return shadow_tilemap_start(tilemap_index) + tile;
}

/**
 * Return tile index inside `tiles_buf` for a given tile coordinate inside a specific LOD.
 * `tiles_index` should be `ShadowTileMapData.tiles_index`.
 */
int shadow_tile_offset(ivec2 tile, int tiles_index, int lod)
{
#if SHADOW_TILEMAP_LOD != 5
#  error This needs to be adjusted
#endif
  const int lod0_width = SHADOW_TILEMAP_RES / 1;
  const int lod1_width = SHADOW_TILEMAP_RES / 2;
  const int lod2_width = SHADOW_TILEMAP_RES / 4;
  const int lod3_width = SHADOW_TILEMAP_RES / 8;
  const int lod4_width = SHADOW_TILEMAP_RES / 16;
  const int lod5_width = SHADOW_TILEMAP_RES / 32;
  const int lod0_size = lod0_width * lod0_width;
  const int lod1_size = lod1_width * lod1_width;
  const int lod2_size = lod2_width * lod2_width;
  const int lod3_size = lod3_width * lod3_width;
  const int lod4_size = lod4_width * lod4_width;
  const int lod5_size = lod5_width * lod5_width;

  int offset = tiles_index;
  switch (lod) {
    case 5:
      offset += lod0_size + lod1_size + lod2_size + lod3_size + lod4_size;
      offset += tile.y * lod5_width;
      break;
    case 4:
      offset += lod0_size + lod1_size + lod2_size + lod3_size;
      offset += tile.y * lod4_width;
      break;
    case 3:
      offset += lod0_size + lod1_size + lod2_size;
      offset += tile.y * lod3_width;
      break;
    case 2:
      offset += lod0_size + lod1_size;
      offset += tile.y * lod2_width;
      break;
    case 1:
      offset += lod0_size;
      offset += tile.y * lod1_width;
      break;
    case 0:
    default:
      offset += tile.y * lod0_width;
      break;
  }
  offset += tile.x;
  return offset;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Load / Store functions.
 * \{ */

/** \note: Will clamp if out of bounds. */
ShadowTileData shadow_tile_load(usampler2D tilemaps_tx, ivec2 tile_co, int tilemap_index)
{
  /* NOTE(@fclem): This clamp can hide some small imprecision at clipmap transition.
   * Can be disabled to check if the clipmap is well centered. */
  tile_co = clamp(tile_co, ivec2(0), ivec2(SHADOW_TILEMAP_RES - 1));
  uint tile_data =
      texelFetch(tilemaps_tx, shadow_tile_coord_in_atlas(tile_co, tilemap_index), 0).x;
  return shadow_tile_unpack(tile_data);
}

/* This function should be the inverse of ShadowDirectional::coverage_get(). */
int shadow_directional_level(LightData light, vec3 lP)
{
  /* We need to hide one tile worth of data to hide the moving transition. */
  const float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 1.0001);
  float distance_from_shadow_origin = length(lP) * narrowing;
  if (light.type == LIGHT_SUN) {
    distance_from_shadow_origin = log2(distance_from_shadow_origin);
    /* Since the distance is centered around the camera (and thus by extension the tilemap),
     * we need to multiply by 2 to get the lod level which covers the following range:
     * [-coverage_get(lod)/2..coverage_get(lod)/2] */
    distance_from_shadow_origin += 1.0;
  }
  else {
    /* Since we want half of the size, bias the level by -1. */
    distance_from_shadow_origin /= exp2(float(light.clipmap_lod_min - 1));
  }
  int clipmap_lod = int(ceil(distance_from_shadow_origin + light._clipmap_lod_bias));
  return clamp(clipmap_lod, light.clipmap_lod_min, light.clipmap_lod_max);
}

struct ShadowCoordinates {
  /* Index of the tilemap to containing the tile. */
  int tilemap_index;
  /* LOD of the tile to load relative to the min level. Always positive. */
  int lod_relative;
  /* Tile coordinate inside the tilemap. */
  ivec2 tile_coord;
  /* UV coordinates in [0..SHADOW_TILEMAP_RES) range. */
  vec2 uv;
};

/* Retain sign bit and avoid costly int division. */
ivec2 shadow_decompress_grid_offset(eLightType light_type, ivec2 ofs, int level_relative)
{
  if (light_type == LIGHT_SUN_ORTHO) {
    /* Should match cascade_tilemaps_distribution(). */
    return -ivec2(floor(intBitsToFloat(ofs) * float(level_relative)));
  }
  else {
    return ((ofs & 0xFFFF) >> level_relative) - ((ofs >> 16) >> level_relative);
  }
}

/**
 * \a lP shading point position in light space (world unit) and translated to camera position
 * snapped to smallest clipmap level.
 */
ShadowCoordinates shadow_directional_coordinates(LightData light, vec3 lP)
{
  ShadowCoordinates ret;

  int level = shadow_directional_level(light, lP - light._position);
  /* This difference needs to be less than 32 for the later shift to be valid.
   * This is ensured by ShadowDirectional::clipmap_level_range(). */
  int level_relative = level - light.clipmap_lod_min;

  ret.tilemap_index = light.tilemap_index + level_relative;

  ret.lod_relative = (light.type == LIGHT_SUN_ORTHO) ? light.clipmap_lod_min : level;

  /* Compute offset in tile. */
  ivec2 clipmap_offset = shadow_decompress_grid_offset(
      light.type, light.clipmap_base_offset, level_relative);

  ret.uv = lP.xy - vec2(light._clipmap_origin_x, light._clipmap_origin_y);
  ret.uv /= exp2(float(ret.lod_relative));
  ret.uv = ret.uv * float(SHADOW_TILEMAP_RES) + float(SHADOW_TILEMAP_RES / 2);
  ret.uv -= vec2(clipmap_offset);
  /* Clamp to avoid out of tilemap access. */
  ret.tile_coord = clamp(ivec2(ret.uv), ivec2(0.0), ivec2(SHADOW_TILEMAP_RES - 1));
  return ret;
}

/* Transform vector to face local coordinate. */
vec3 shadow_punctual_local_position_to_face_local(int face_id, vec3 lL)
{
  switch (face_id) {
    case 1:
      return vec3(-lL.y, lL.z, -lL.x);
    case 2:
      return vec3(lL.y, lL.z, lL.x);
    case 3:
      return vec3(lL.x, lL.z, -lL.y);
    case 4:
      return vec3(-lL.x, lL.z, lL.y);
    case 5:
      return vec3(lL.x, -lL.y, -lL.z);
    default:
      return lL;
  }
}

/**
 * \a lP shading point position in face local space (world unit).
 * \a face_id is the one used to rotate lP using shadow_punctual_local_position_to_face_local().
 */
ShadowCoordinates shadow_punctual_coordinates(LightData light, vec3 lP, int face_id)
{
  ShadowCoordinates ret;
  ret.tilemap_index = light.tilemap_index + face_id;
  /* UVs in [-1..+1] range. */
  ret.uv = lP.xy / abs(lP.z);
  /* UVs in [0..SHADOW_TILEMAP_RES] range. */
  ret.uv = ret.uv * float(SHADOW_TILEMAP_RES / 2) + float(SHADOW_TILEMAP_RES / 2);
  /* Clamp to avoid out of tilemap access. */
  ret.tile_coord = clamp(ivec2(ret.uv), ivec2(0), ivec2(SHADOW_TILEMAP_RES - 1));
  return ret;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Frustum shapes.
 * \{ */

vec3 shadow_tile_corner_persp(ShadowTileMapData tilemap, ivec2 tile)
{
  return tilemap.corners[1].xyz + tilemap.corners[2].xyz * float(tile.x) +
         tilemap.corners[3].xyz * float(tile.y);
}

Pyramid shadow_tilemap_cubeface_bounds(ShadowTileMapData tilemap,
                                       ivec2 tile_start,
                                       const ivec2 extent)
{
  Pyramid shape;
  shape.corners[0] = tilemap.corners[0].xyz;
  shape.corners[1] = shadow_tile_corner_persp(tilemap, tile_start + ivec2(0, 0));
  shape.corners[2] = shadow_tile_corner_persp(tilemap, tile_start + ivec2(extent.x, 0));
  shape.corners[3] = shadow_tile_corner_persp(tilemap, tile_start + extent);
  shape.corners[4] = shadow_tile_corner_persp(tilemap, tile_start + ivec2(0, extent.y));
  return shape;
}

/** \} */
