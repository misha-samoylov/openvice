#pragma once

/* Compile-time game / engine configuration.
 * Change paths here for your GTA Vice City install. */

#define PROJECT_NAME "openvice"

#define WINDOW_WIDTH 3840
#define WINDOW_HEIGHT 2160
#define WINDOW_TITLE L"openvice"

#define CAMERA_FAR_PLANE 1500.0f
/* Fog must reach full sky before the far clip, or a hard pop-in line shows. */
#define FOG_START_FACTOR 0.40f
#define FOG_END_FACTOR 0.82f

/* Daytime hour for tobj visibility (re3 CTimeModelInfo). Night lights are off. */
#define WORLD_HOUR 12

/* VC stadium interior IDs (gtamods.com/wiki/Interior). */
#define INTERIOR_DIRTRING 14
#define INTERIOR_BLOODRING 15
#define INTERIOR_HOTRING 16

/* Matches DXRender clear color — fog fades geometry into the sky. */
#define SKY_COLOR_R 0.49804f
#define SKY_COLOR_G 0.78431f
#define SKY_COLOR_B 0.94510f
#define SKY_COLOR_A 1.0f

/* ---- GTA Vice City install paths ---- */
#define GTA_VC_ROOT "C:/Games/Grand Theft Auto Vice City"

#define GTA_VC_IMG_PATH L"C:/Games/Grand Theft Auto Vice City/models/gta3.img"
#define GTA_VC_DIR_PATH L"C:/Games/Grand Theft Auto Vice City/models/gta3.dir"

#define GTA_VC_MAPS_DIR "C:/Games/Grand Theft Auto Vice City/data/maps/"
#define GTA_VC_GENERIC_IDE "C:/Games/Grand Theft Auto Vice City/data/maps/generic.ide"
#define GTA_VC_WATERPRO "C:/Games/Grand Theft Auto Vice City/data/waterpro.dat"
#define GTA_VC_PARTICLE_TXD "C:/Games/Grand Theft Auto Vice City/models/particle.txd"
#define GTA_VC_PED_IFP "C:/Games/Grand Theft Auto Vice City/anim/ped.ifp"

/* Map IDE/IPL basenames under data/maps/<name>/<name>.ide|.ipl */
#define GTA_VC_MAP_COUNT 31
