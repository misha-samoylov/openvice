#ifndef _RENDERWARE_H_
#define _RENDERWARE_H_

#ifdef _WIN32
  #include <windows.h>
#endif

#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>

#ifdef _DEBUG
	#define READ_HEADER(x)\
	header.read(rw);\
	if (header.type != (x)) {\
		ChunkNotFound((x), rw.tellg());\
	}
#else
	#define READ_HEADER(x)\
		header.read(rw);
#endif

namespace rw {

enum PLATFORM_ID {
	PLATFORM_OGL = 2,
	PLATFORM_PS2    = 4,
	PLATFORM_XBOX   = 5,
	PLATFORM_D3D8   = 8,
	PLATFORM_D3D9   = 9,
	PLATFORM_PS2FOURCC = 0x00325350 /* "PS2\0" */
};

enum CHUNK_TYPE {
    CHUNK_NAOBJECT        = 0x0,
    CHUNK_STRUCT          = 0x1,
    CHUNK_STRING          = 0x2,
    CHUNK_EXTENSION       = 0x3,
    CHUNK_CAMERA          = 0x5,
    CHUNK_TEXTURE         = 0x6,
    CHUNK_MATERIAL        = 0x7,
    CHUNK_MATLIST         = 0x8,
    CHUNK_ATOMICSECT      = 0x9,
    CHUNK_PLANESECT       = 0xA,
    CHUNK_WORLD           = 0xB,
    CHUNK_SPLINE          = 0xC,
    CHUNK_MATRIX          = 0xD,
    CHUNK_FRAMELIST       = 0xE,
    CHUNK_GEOMETRY        = 0xF,
    CHUNK_CLUMP           = 0x10,
    CHUNK_LIGHT           = 0x12,
    CHUNK_UNICODESTRING   = 0x13,
    CHUNK_ATOMIC          = 0x14,
    CHUNK_TEXTURENATIVE   = 0x15,
    CHUNK_TEXDICTIONARY   = 0x16,
    CHUNK_ANIMDATABASE    = 0x17,
    CHUNK_IMAGE           = 0x18,
    CHUNK_SKINANIMATION   = 0x19,
    CHUNK_GEOMETRYLIST    = 0x1A,
    CHUNK_ANIMANIMATION   = 0x1B,
    CHUNK_HANIMANIMATION  = 0x1B,
    CHUNK_TEAM            = 0x1C,
    CHUNK_CROWD           = 0x1D,
    CHUNK_RIGHTTORENDER   = 0x1F,
    CHUNK_MTEFFECTNATIVE  = 0x20,
    CHUNK_MTEFFECTDICT    = 0x21,
    CHUNK_TEAMDICTIONARY  = 0x22,
    CHUNK_PITEXDICTIONARY = 0x23,
    CHUNK_TOC             = 0x24,
    CHUNK_PRTSTDGLOBALDATA = 0x25,
    CHUNK_ALTPIPE         = 0x26,
    CHUNK_PIPEDS          = 0x27,
    CHUNK_PATCHMESH       = 0x28,
    CHUNK_CHUNKGROUPSTART = 0x29,
    CHUNK_CHUNKGROUPEND   = 0x2A,
    CHUNK_UVANIMDICT      = 0x2B,
    CHUNK_COLLTREE        = 0x2C,
    CHUNK_ENVIRONMENT     = 0x2D,
    CHUNK_COREPLUGINIDMAX = 0x2E,

	CHUNK_MORPH           = 0x105,
	CHUNK_SKYMIPMAP       = 0x110,
	CHUNK_SKIN            = 0x116,
	CHUNK_PARTICLES       = 0x118,
	CHUNK_HANIM           = 0x11E,
	CHUNK_MATERIALEFFECTS = 0x120,
	CHUNK_PDSPLG          = 0x131,
	CHUNK_ADCPLG          = 0x134,
	CHUNK_UVANIMPLG       = 0x135,
	CHUNK_BINMESH         = 0x50E,
	CHUNK_NATIVEDATA      = 0x510,
	CHUNK_VERTEXFORMAT    = 0x510,

	CHUNK_PIPELINESET      = 0x253F2F3,
	CHUNK_SPECULARMAT      = 0x253F2F6,
	CHUNK_2DFX             = 0x253F2F8,
	CHUNK_NIGHTVERTEXCOLOR = 0x253F2F9,
	CHUNK_COLLISIONMODEL   = 0x253F2FA,
	CHUNK_REFLECTIONMAT    = 0x253F2FC,
	CHUNK_MESHEXTENSION    = 0x253F2FD,
	CHUNK_FRAME            = 0x253F2FE
};

enum RASTER_TYPE {
	RASTER_DEFAULT = 0x0000,
	RASTER_1555 = 0x0100,
	RASTER_565 = 0x0200,
	RASTER_4444 = 0x0300,
	RASTER_LUM8 = 0x0400,
	RASTER_8888 = 0x0500,
	RASTER_888 = 0x0600,
	RASTER_16 = 0x0700,
	RASTER_24 = 0x0800,
	RASTER_32 = 0x0900,
	RASTER_555 = 0x0a00,

	RASTER_AUTOMIPMAP = 0x1000,
	RASTER_PAL8 = 0x2000,
	RASTER_PAL4 = 0x4000,
	RASTER_MIPMAP = 0x8000,

	RASTER_MASK = 0x0F00
};

enum FLAGS_TYPE {
    FLAGS_TRISTRIP   = 0x01, 
    FLAGS_POSITIONS  = 0x02, 
    FLAGS_TEXTURED   = 0x04, 
    FLAGS_PRELIT     = 0x08, 
    FLAGS_NORMALS    = 0x10, 
    FLAGS_LIGHT      = 0x20, 
	FLAGS_MODULATEMATERIALCOLOR  = 0x40, 
    FLAGS_TEXTURED2  = 0x80
};

enum MAT_FX {
	MATFX_BUMPMAP = 0x1,
	MATFX_ENVMAP = 0x2,
	MATFX_BUMPENVMAP = 0x3,
	MATFX_DUAL = 0x4,
	MATFX_UVTRANSFORM = 0x5,
	MATFX_DUALUVTRANSFORM = 0x6
};

enum FACE_TYPE {
	FACETYPE_LIST = 0x0,
	FACETYPE_STRIP = 0x1
};

/* gta3 ps2: 302, 304, 310 
 * gta3 pc: 304, 310, 401ffff, 800ffff, c02ffff
 * gtavc ps2: c02ffff
 * gtavc pc: c02ffff, 800ffff, 1003ffff
 * gtasa: 1803ffff */

enum GTA_VERSION {
    GTA3_1 = 0x00000302,
    GTA3_2 = 0x00000304,
    GTA3_3 = 0x00000310,
    GTA3_4 = 0x0800FFFF,
    VCPS2  = 0x0C02FFFF,
    VCPC   = 0x1003FFFF,
    SA     = 0x1803FFFF
};

struct HeaderInfo {
	uint32_t type;
	uint32_t length;
	uint32_t build;
	uint32_t version;
	bool read(std::istream &rw);
	bool peek(std::istream &rw);
	bool findChunk(std::istream &rw, uint32_t type);
};

void ChunkNotFound(CHUNK_TYPE chunk, uint32_t address);
int8_t readInt8(std::istream &rw);
uint8_t readUInt8(std::istream &rw);
int16_t readInt16(std::istream &rw);
uint16_t readUInt16(std::istream &rw);
int32_t readInt32(std::istream &rw);
uint32_t readUInt32(std::istream &rw);
float readFloat32(std::istream &rw);


struct Frame {
	float rotationMatrix[9];
	float position[3];
	int32_t parent;

	/* Extensions */

	/* node name */
	std::string name;

	/* hanim */
	bool hasHAnim;
	uint32_t hAnimUnknown1;
	int32_t hAnimBoneId;
	uint32_t hAnimBoneCount;
	uint32_t hAnimUnknown2;
	uint32_t hAnimUnknown3;
	std::vector<int32_t> hAnimBoneIds;
	std::vector<uint32_t> hAnimBoneNumbers;
	std::vector<uint32_t> hAnimBoneTypes;

	/* functions */
	void readStruct(std::istream &dff);
	void readExtension(std::istream &dff);

	void dump(uint32_t index, std::string ind = "");

	Frame(void);
};

struct Atomic {
	int32_t frameIndex;
	int32_t geometryIndex;

	/* Extensions */

	/* right to render */
	bool hasRightToRender;
	uint32_t rightToRenderVal1;
	uint32_t rightToRenderVal2;

	/* particles */
	bool hasParticles;
	uint32_t particlesVal;

	/* pipelineset */
	bool hasPipelineSet;
	uint32_t pipelineSetVal;

	/* material fx */
	bool hasMaterialFx;
	uint32_t materialFxVal;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	void dump(uint32_t index, std::string ind = "");

	Atomic(void);
};

struct Texture {
	uint32_t filterFlags;
	std::string name;
	std::string maskName;

	/* Extensions */

	/* sky mipmap */
	bool hasSkyMipmap;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	void dump(std::string ind = "");

	Texture(void);
};

struct MatFx {
	uint32_t type;

	float bumpCoefficient;
	float envCoefficient;
	float srcBlend;
	float destBlend;

	bool hasTex1;
	Texture tex1;
	bool hasTex2;
	Texture tex2;
	bool hasDualPassMap;
	Texture dualPassMap;

	void dump(std::string ind = "");

	MatFx(void);
};

struct Material {
	uint32_t flags;
	uint8_t color[4];
	uint32_t unknown;
	bool hasTex;
	float surfaceProps[3]; /* ambient, specular, diffuse */

	Texture texture;

	/* Extensions */

	/* right to render */
	bool hasRightToRender;
	uint32_t rightToRenderVal1;
	uint32_t rightToRenderVal2;

	/* matfx */
	bool hasMatFx;
	MatFx *matFx;

	/* reflection mat */
	bool hasReflectionMat;
	float reflectionChannelAmount[4];
	float reflectionIntensity;

	/* specular mat */
	bool hasSpecularMat;
	float specularLevel;
	std::string specularName;

	/* uv anim */
	bool hasUVAnim;
	uint32_t uvVal;
	std::string uvName;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);

	void dump(uint32_t index, std::string ind = "");

	Material(void);
	Material(const Material &orig);
	Material &operator=(const Material &that);
	~Material(void);
};

struct MeshExtension {
	uint32_t unknown;

	std::vector<float> vertices;
	std::vector<float> texCoords;
	std::vector<uint8_t> vertexColors;
	std::vector<uint16_t> faces;
	std::vector<uint16_t> assignment;

	std::vector<std::string> textureName;
	std::vector<std::string> maskName;
	std::vector<float> unknowns;
};

struct Split {
	uint32_t matIndex;
	std::vector<uint32_t> indices;
};

struct Geometry {
	uint32_t flags;
	uint32_t numUVs;
	bool hasNativeGeometry;

	uint32_t vertexCount;
	std::vector<uint16_t> faces;
	std::vector<uint8_t> vertexColors;
	std::vector<float> texCoords[8];

	/* morph target (only one) */
	float boundingSphere[4];
	uint32_t hasPositions;
	uint32_t hasNormals;
	std::vector<float> vertices;
	std::vector<float> normals;

	std::vector<Material> materialList;

	/* Extensions */

	/* bin mesh */
	uint32_t faceType;
	uint32_t numIndices;
	std::vector<Split> splits;

	/* skin */
	bool hasSkin;
	uint32_t boneCount;
	uint32_t specialIndexCount;
	uint32_t unknown1;
	uint32_t unknown2;
	std::vector<uint8_t> specialIndices;
	std::vector<uint32_t> vertexBoneIndices;
	std::vector<float> vertexBoneWeights;
	std::vector<float> inverseMatrices;

	/* mesh extension */
	bool hasMeshExtension;
	MeshExtension *meshExtension;

	/* night vertex colors */
	bool hasNightColors;
	uint32_t nightColorsUnknown;
	std::vector<uint8_t> nightColors;

	/* 2dfx */
	bool has2dfx;
	std::vector<uint8_t> twodfxData;

	/* morph (only flag) */
	bool hasMorph;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	void readMeshExtension(std::istream &dff);

	void cleanUp(void);

	void dump(uint32_t index, std::string ind = "", bool detailed = false);

	Geometry(void);
	Geometry(const Geometry &orig);
	Geometry &operator= (const Geometry &other);
	~Geometry(void);
private:
	void readNativeSkinMatrices(std::istream &dff);
	bool isDegenerateFace(uint32_t i, uint32_t j, uint32_t k);
	void generateFaces(void);

	uint32_t addTempVertexIfNew(uint32_t index);
};

struct Light {
	int32_t frameIndex;
	float radius;
	float color[3];
	float minusCosAngle;
	uint32_t type;
	uint32_t flags;

	void read(std::istream &dff);
};

struct Clump {
	std::vector<Atomic> atomicList;
	std::vector<Frame> frameList;
	std::vector<Geometry> geometryList;
	std::vector<Light> lightList;

	/* Extensions */
	/* collision file */
	bool hasCollision;
	std::vector<uint8_t> colData;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	void dump(bool detailed = false);
	void clear(void);
};

/*
 * TXDs
 */

struct NativeTexture {
	uint32_t platform;
	std::string name;
	std::string maskName;
	uint32_t filterFlags;
	uint32_t rasterFormat;
	std::vector<uint32_t> width;	// store width & height
	std::vector<uint32_t> height;	// for each mipmap
	uint32_t depth;
	std::vector<uint32_t> dataSizes;
	std::vector<uint8_t*> texels;	// holds either indices or color values
					// (also per mipmap)
	uint8_t *palette;
	uint32_t paletteSize;

	bool hasAlpha;
	uint32_t mipmapCount;

	// PS2
	std::vector<uint32_t> swizzleWidth;
	std::vector<uint32_t> swizzleHeight;
	// bit 0: alpha values above (or equal to) the threshold
	// bit 1: alpha values below the threshold
	// both 0: no info
	uint32_t alphaDistribution;

	// PC
	uint32_t dxtCompression;

	/* functions */
	void readD3d(std::istream &txd);

	void decompressDxt(void);
	void decompressDxt1(void);
	void decompressDxt3(void);
	void decompressDxt4(void);
	void convertTo32Bit(void);

	NativeTexture(void);
	NativeTexture(const NativeTexture &orig);
	NativeTexture &operator=(const NativeTexture &that);
	~NativeTexture(void);
};

struct TextureDictionary {
	std::vector<NativeTexture> texList;

	/* functions */
	void read(std::istream &txd);
	void clear(void);
	~TextureDictionary(void);
};

struct UVAnimation {
	std::vector<uint8_t> data;

	void read(std::istream &dff);
};

struct UVAnimDict {
	std::vector<UVAnimation> animList;

	void read(std::istream &dff);
	void clear(void);
	~UVAnimDict(void);
};

}

#endif
