#pragma once

#include "../renderware.h"

struct Texture {
	uint32_t filterFlags;
	std::string name;
	std::string maskName;

	/* Extensions */

	/* sky mipmap */
	bool hasSkyMipmap;

	/* functions */
	void read(char* bytes, size_t* offset);
	void readExtension(char* bytes, size_t* offset);
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
	MatFx* matFx;

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
	void read(char* bytes, size_t* offset);
	void readExtension(char* bytes, size_t* offset);

	void dump(uint32_t index, std::string ind = "");

	Material(void);
	Material(const Material& orig);
	Material& operator=(const Material& that);
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
	float *vertices;
	float *normals;

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
	MeshExtension* meshExtension;

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
	void read(char* bytes, size_t* offset);
	void readExtension(char* bytes, size_t* offset);
	void readMeshExtension(char* bytes, size_t* offset);

	void dump(uint32_t index, std::string ind = "", bool detailed = false);

	Geometry(void);
	Geometry(const Geometry& orig);
	Geometry& operator= (const Geometry& other);
	~Geometry(void);
private:
	void readNativeSkinMatrices(char* bytes, size_t* offset);
	bool isDegenerateFace(uint32_t i, uint32_t j, uint32_t k);
	void generateFaces(void);
};