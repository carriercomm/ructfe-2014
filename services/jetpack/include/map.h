#ifndef JETPACK_MAP_MAP_H
#define JETPACK_MAP_MAP_H

#include "types.h"

#define MAP_HEIGHT 3000
#define MAP_WIDTH 3000
#define MAP_REGION_HEIGHT 300
#define MAP_REGION_WIDTH 300
#define MAX_ALLOWED_DISTANCE 100
#define MAX_ALLOWED_WIDTH 15

#define MAX_BASE_DIR_LENGTH 256

int jp_init_map(char *new_map_base_dir);

int jp_clear_path(point *path, int length);

int jp_build_path(point source, point destination, point *path, point *region_buffer);

int jp_get_bytes(point *path, byte *buffer, int offset, int length);

#endif