#pragma once

/* Compile-time game / engine configuration.
 * Change paths here for your GTA Vice City install. */

#define PROJECT_NAME "openvice"

/* RT map demo: full VC map (IPL interior 0/13), RayQuery, free camera, no player. */
#define ENABLE_SINGLE_OBJECT_RT_DEMO 1

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#if ENABLE_SINGLE_OBJECT_RT_DEMO
#define WINDOW_TITLE L"openvice — full map RT demo"
#else
#define WINDOW_TITLE L"openvice"
#endif

#if ENABLE_SINGLE_OBJECT_RT_DEMO
#define CAMERA_FAR_PLANE 800.0f
#define DRAW_DISTANCE 700.0f
#define FOG_START_FACTOR 0.70f
#define FOG_END_FACTOR 0.95f
#define ENABLE_OCCLUSION_CULLING 0
#define ENABLE_CSM_SHADOWS 0
#define ENABLE_SSAO 0
#define ENABLE_RT_SHADOWS 1
#define ENABLE_RT_INLINE_PS 0
#define ENABLE_RT_BOUNCE_PASS 0
#define ENABLE_RT_FULL_SCENE 1 /* primary rays + shade + sun shadow */
#define ENABLE_RT_FULL_HALF_RES 1 /* half-res ray pass — avoids TDR on full map */
#else
#define CAMERA_FAR_PLANE 300.0f
#define DRAW_DISTANCE 300.0f
#define FOG_START_FACTOR 0.55f
#define FOG_END_FACTOR 0.92f
#define ENABLE_OCCLUSION_CULLING 1
#define ENABLE_CSM_SHADOWS 0
#define ENABLE_SSAO 0
#define ENABLE_RT_SHADOWS 1
#define ENABLE_RT_INLINE_PS 0
#define ENABLE_RT_BOUNCE_PASS 1
#define ENABLE_RT_FULL_SCENE 0
#define ENABLE_RT_FULL_HALF_RES 0
#endif

/* Daytime hour for tobj visibility (re3 CTimeModelInfo). Night lights are off. */
#define WORLD_HOUR 12

/* Interior IDs from IPL (gtamods.com/wiki/Interior). Scene renders exterior + listed IDs. */
#define INTERIOR_EXTERIOR 0
#define INTERIOR_EVERYWHERE 13 /* VIS_EVERYWHERE — stream in any interior */
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

/* Set 1 to load only listed IPL basenames; 0 = all maps (interior 0/13 still filtered in Scene). */
#define GTA_VC_IPL_FILTER_ENABLED 0
