#include <cstring>
#include <cstdlib>
#include <fstream>

#include "renderware.h"

void TextureDictionary::read(istream &rw)
{
	HeaderInfo header;

	header.read(rw);
	if (header.type != CHUNK_TEXDICTIONARY)
		return;

	READ_HEADER(CHUNK_STRUCT);
	uint32_t textureCount = readUInt16(rw);
	rw.seekg(2, ios::cur);
	texList.resize(textureCount);

	for (uint32_t i = 0; i < textureCount; i++) {
		READ_HEADER(CHUNK_TEXTURENATIVE);
		rw.seekg(0x0c, ios::cur);
		texList[i].platform = readUInt32(rw);
		rw.seekg(-0x10, ios::cur);

		if (texList[i].platform == PLATFORM_D3D8 ||
		           texList[i].platform == PLATFORM_D3D9) {
			texList[i].readD3d(rw);
		} else {
			printf("Cannot identify platform\n");
		}

		READ_HEADER(CHUNK_EXTENSION);
		uint32_t end = header.length;
		end += rw.tellg();
		while (rw.tellg() < end) {
			header.read(rw);
			switch (header.type) {
			case CHUNK_SKYMIPMAP:
				rw.seekg(4, ios::cur);
				break;
			default:
				rw.seekg(header.length, ios::cur);
				break;
			}
		}
	}
}

void TextureDictionary::clear(void)
{
	texList.clear();
}

TextureDictionary::~TextureDictionary(void)
{
	texList.clear();
}

/*
 * Native Texture
 */

void NativeTexture::readD3d(istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_STRUCT);
	uint32_t end = rw.tellg();
	end += header.length;

	uint32_t platform = readUInt32(rw);
	// improve error handling
	if (platform != PLATFORM_D3D8 && platform != PLATFORM_D3D9)
		return;

	filterFlags = readUInt32(rw);

	char buffer[32];
	rw.read(buffer, 32);
	name = buffer;
	rw.read(buffer, 32);
	maskName = buffer;

	rasterFormat = readUInt32(rw);
//cout << hex << rasterFormat << " ";

	hasAlpha = false;
	char fourcc[5];
	fourcc[4] = 0;
	if (platform == PLATFORM_D3D9) {
		rw.read(fourcc, 4*sizeof(char));
	} else {
		hasAlpha = readUInt32(rw);
//cout << hasAlpha << " ";
	}

	width.push_back(readUInt16(rw));
	height.push_back(readUInt16(rw));
	depth = readUInt8(rw);
	mipmapCount = readUInt8(rw);
//cout << dec << mipmapCount << " ";
	rw.seekg(sizeof(int8_t), ios::cur); // raster type (always 4)
	dxtCompression = readUInt8(rw);
//cout << dxtCompression << " ";

	if (platform == PLATFORM_D3D9) {
		hasAlpha = dxtCompression & 0x1;
		if (dxtCompression & 0x8)
			dxtCompression = fourcc[3] - '0';
		else
			dxtCompression = 0;
//cout << fourcc << " ";
	}
//	cout << hasAlpha << " " << maskName << " " << name << endl;


	if (rasterFormat & RASTER_PAL8 || rasterFormat & RASTER_PAL4) {
		paletteSize = (rasterFormat & RASTER_PAL8) ? 0x100 : 0x10;
		palette = new uint8_t[paletteSize*4*sizeof(uint8_t)];
		rw.read(reinterpret_cast <char *> (palette),
			paletteSize*4*sizeof(uint8_t));
	}

	for (uint32_t i = 0; i < mipmapCount; i++) {
		if (i > 0) {
			width.push_back(width[i-1]/2);
			height.push_back(height[i-1]/2);
			// DXT compression works on 4x4 blocks,
			// no smaller values allowed
			if (dxtCompression) {
				if (width[i] < 4 && width[i] != 0)
					width[i] = 4;
				if (height[i] < 4 && height[i] != 0)
					height[i] = 4;
			}
		}

		uint32_t dataSize = readUInt32(rw);

		// There is no way to predict, when the size is going to be zero
		if (dataSize == 0)
			width[i] = height[i] = 0;

		dataSizes.push_back(dataSize);
		texels.push_back(new uint8_t[dataSize]);
		rw.read(reinterpret_cast <char *> (&texels[i][0]),
		        dataSize*sizeof(uint8_t));
	}
//cout << endl;
}

void NativeTexture::convertTo32Bit(void)
{
	// depth is always 8 (even if the palette is 4 bit)
	if (rasterFormat & RASTER_PAL8 || rasterFormat & RASTER_PAL4) {
		for (uint32_t j = 0; j < mipmapCount; j++) {
			uint32_t dataSize = width[j]*height[j]*4;
			uint8_t *newtexels = new uint8_t[dataSize];
			for (uint32_t i = 0; i < width[j]*height[j]; i++) {
				// swap r and b
				newtexels[i*4+2] = palette[texels[j][i]*4+0];
				newtexels[i*4+1] = palette[texels[j][i]*4+1];
				newtexels[i*4+0] = palette[texels[j][i]*4+2];
				newtexels[i*4+3] = palette[texels[j][i]*4+3];
			}
			delete[] texels[j];
			texels[j] = newtexels;
			dataSizes[j] = dataSize;
		}
		delete[] palette;
		palette = 0;
		rasterFormat &= ~(RASTER_PAL4 | RASTER_PAL8);
		depth = 0x20;
	} else if ((rasterFormat & RASTER_MASK) ==  RASTER_1555) {
		for (uint32_t j = 0; j < mipmapCount; j++) {
			uint32_t dataSize = width[j]*height[j]*4;
			uint8_t *newtexels = new uint8_t[dataSize];
			for (uint32_t i = 0; i < width[j]*height[j]; i++) {
				uint32_t col = *((uint16_t *) &texels[j][i*2]);
				newtexels[i*4+0] =((col&0x001F)>>0x0)*0xFF/0x1F;
				newtexels[i*4+1] =((col&0x03E0)>>0x5)*0xFF/0x1F;
				newtexels[i*4+2] =((col&0x7C00)>>0xa)*0xFF/0x1F;
				newtexels[i*4+3] =((col&0x8000)>>0xf)*0xFF;
			}
			delete[] texels[j];
			texels[j] = newtexels;
			dataSizes[j] = dataSize;
		}
		rasterFormat = RASTER_8888;
		depth = 0x20;
	} else if ((rasterFormat & RASTER_MASK) ==  RASTER_565) {
		for (uint32_t j = 0; j < mipmapCount; j++) {
			uint32_t dataSize = width[j]*height[j]*4;
			uint8_t *newtexels = new uint8_t[dataSize];
			for (uint32_t i = 0; i < width[j]*height[j]; i++) {
				uint32_t col = *((uint16_t *) &texels[j][i*2]);
				newtexels[i*4+0] =((col&0x001F)>>0x0)*0xFF/0x1F;
				newtexels[i*4+1] =((col&0x07E0)>>0x5)*0xFF/0x3F;
				newtexels[i*4+2] =((col&0xF800)>>0xb)*0xFF/0x1F;
				newtexels[i*4+3] = 255;
			}
			delete[] texels[j];
			texels[j] = newtexels;
			dataSizes[j] = dataSize;
		}
		rasterFormat = RASTER_888;
		depth = 0x20;
	} else if ((rasterFormat & RASTER_MASK) ==  RASTER_4444) {
		for (uint32_t j = 0; j < mipmapCount; j++) {
			uint32_t dataSize = width[j]*height[j]*4;
			uint8_t *newtexels = new uint8_t[dataSize];
			for (uint32_t i = 0; i < width[j]*height[j]; i++) {
				uint32_t col = *((uint16_t *) &texels[j][i*2]);
				// swap r and b
				newtexels[i*4+0] =((col&0x000F)>>0x0)*0xFF/0xF;
				newtexels[i*4+1] =((col&0x00F0)>>0x4)*0xFF/0xF;
				newtexels[i*4+2] =((col&0x0F00)>>0x8)*0xFF/0xF;
				newtexels[i*4+3] =((col&0xF000)>>0xc)*0xFF/0xF;
			}
			delete[] texels[j];
			texels[j] = newtexels;
			dataSizes[j] = dataSize;
		}
		rasterFormat = RASTER_8888;
		depth = 0x20;
	}
	// no support for other raster formats yet
}

void NativeTexture::decompressDxt4(void)
{
	for (uint32_t i = 0; i < mipmapCount; i++) {
		/* j loops through old texels
		 * x and y loop through new texels */
		uint32_t x = 0, y = 0;
		uint32_t dataSize = width[i]*height[i]*4;
		uint8_t *newtexels = new uint8_t[dataSize];
		for (uint32_t j = 0; j < width[i]*height[i]; j += 16) {
			/* calculate colors */
			uint32_t col0 = *((uint16_t *) &texels[i][j+8]);
			uint32_t col1 = *((uint16_t *) &texels[i][j+10]);
			uint32_t c[4][4];
			// swap r and b
			c[0][0] = (col0 & 0x1F)*0xFF/0x1F;
			c[0][1] = ((col0 & 0x7E0) >> 5)*0xFF/0x3F;
			c[0][2] = ((col0 & 0xF800) >> 11)*0xFF/0x1F;

			c[1][0] = (col1 & 0x1F)*0xFF/0x1F;
			c[1][1] = ((col1 & 0x7E0) >> 5)*0xFF/0x3F;
			c[1][2] = ((col1 & 0xF800) >> 11)*0xFF/0x1F;

			c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
			c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
			c[2][2] = (2*c[0][2] + 1*c[1][2])/3;

			c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
			c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
			c[3][2] = (1*c[0][2] + 2*c[1][2])/3;

			uint32_t a[8];
			a[0] = texels[i][j+0];
			a[1] = texels[i][j+1];
			if (a[0] > a[1]) {
				a[2] = (6*a[0] + 1*a[1])/7;
				a[3] = (5*a[0] + 2*a[1])/7;
				a[4] = (4*a[0] + 3*a[1])/7;
				a[5] = (3*a[0] + 4*a[1])/7;
				a[6] = (2*a[0] + 5*a[1])/7;
				a[7] = (1*a[0] + 6*a[1])/7;
			} else {
				a[2] = (4*a[0] + 1*a[1])/5;
				a[3] = (3*a[0] + 2*a[1])/5;
				a[4] = (2*a[0] + 3*a[1])/5;
				a[5] = (1*a[0] + 4*a[1])/5;
				a[6] = 0;
				a[7] = 0xFF;
			}

			/* make index list */
			uint32_t indicesint = *((uint32_t *) &texels[i][j+12]);
			uint8_t indices[16];
			for (int32_t k = 0; k < 16; k++) {
				indices[k] = indicesint & 0x3;
				indicesint >>= 2;
			}
			// actually 6 bytes
			uint64_t alphasint = *((uint64_t *) &texels[i][j+2]);
			uint8_t alphas[16];
			for (int32_t k = 0; k < 16; k++) {
				alphas[k] = alphasint & 0x7;
				alphasint >>= 3;
			}

			/* write bytes */
			for (uint32_t k = 0; k < 4; k++)
				for (uint32_t l = 0; l < 4; l++) {
	// wtf?
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 0] = c[indices[l*4+k]][0];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 1] = c[indices[l*4+k]][1];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 2] = c[indices[l*4+k]][2];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 3] = a[alphas[l*4+k]];
				}
			x += 4;
			if (x >= width[i]) {
				y += 4;
				x = 0;
			}
		}
		delete[] texels[i];
		texels[i] = newtexels;
		dataSizes[i] = dataSize;
	}
	depth = 0x20;
	rasterFormat = RASTER_8888;
	dxtCompression = 0;
}

void NativeTexture::decompressDxt3(void)
{
	for (uint32_t i = 0; i < mipmapCount; i++) {
		/* j loops through old texels
		 * x and y loop through new texels */
		uint32_t x = 0, y = 0;
		uint32_t dataSize = width[i]*height[i]*4;
		uint8_t *newtexels = new uint8_t[dataSize];
		for (uint32_t j = 0; j < width[i]*height[i]; j += 16) {
			/* calculate colors */
			uint32_t col0 = *((uint16_t *) &texels[i][j+8]);
			uint32_t col1 = *((uint16_t *) &texels[i][j+10]);
			uint32_t c[4][4];
			// swap r and b
			c[0][0] = (col0 & 0x1F)*0xFF/0x1F;
			c[0][1] = ((col0 & 0x7E0) >> 5)*0xFF/0x3F;
			c[0][2] = ((col0 & 0xF800) >> 11)*0xFF/0x1F;

			c[1][0] = (col1 & 0x1F)*0xFF/0x1F;
			c[1][1] = ((col1 & 0x7E0) >> 5)*0xFF/0x3F;
			c[1][2] = ((col1 & 0xF800) >> 11)*0xFF/0x1F;

			c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
			c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
			c[2][2] = (2*c[0][2] + 1*c[1][2])/3;

			c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
			c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
			c[3][2] = (1*c[0][2] + 2*c[1][2])/3;

			/* make index list */
			uint32_t indicesint = *((uint32_t *) &texels[i][j+12]);
			uint8_t indices[16];
			for (int32_t k = 0; k < 16; k++) {
				indices[k] = indicesint & 0x3;
				indicesint >>= 2;
			}
			uint64_t alphasint = *((uint64_t *) &texels[i][j+0]);
			uint8_t alphas[16];
			for (int32_t k = 0; k < 16; k++) {
				alphas[k] = (alphasint & 0xF)*17;
				alphasint >>= 4;
			}

			/* write bytes */
			for (uint32_t k = 0; k < 4; k++)
				for (uint32_t l = 0; l < 4; l++) {
	// wtf?
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 0] = c[indices[l*4+k]][0];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 1] = c[indices[l*4+k]][1];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 2] = c[indices[l*4+k]][2];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 3] = alphas[l*4+k];
				}
			x += 4;
			if (x >= width[i]) {
				y += 4;
				x = 0;
			}
		}
		delete[] texels[i];
		texels[i] = newtexels;
		dataSizes[i] = dataSize;
	}
	depth = 0x20;
	rasterFormat += 0x0200;
	dxtCompression = 0;
}

void NativeTexture::decompressDxt1(void)
{
	for (uint32_t i = 0; i < mipmapCount; i++) {
		/* j loops through old texels
		 * x and y loop through new texels */
		uint32_t x = 0, y = 0;
		uint32_t dataSize = width[i]*height[i]*4;
		uint8_t *newtexels = new uint8_t[dataSize];
		for (uint32_t j = 0; j < width[i]*height[i]/2; j += 8) {
			/* calculate colors */
			uint32_t col0 = *((uint16_t *) &texels[i][j+0]);
			uint32_t col1 = *((uint16_t *) &texels[i][j+2]);
			uint32_t c[4][4];
			// swap r and b
			c[0][0] = (col0 & 0x1F)*0xFF/0x1F;
			c[0][1] = ((col0 & 0x7E0) >> 5)*0xFF/0x3F;
			c[0][2] = ((col0 & 0xF800) >> 11)*0xFF/0x1F;
			c[0][3] = 0xFF;

			c[1][0] = (col1 & 0x1F)*0xFF/0x1F;
			c[1][1] = ((col1 & 0x7E0) >> 5)*0xFF/0x3F;
			c[1][2] = ((col1 & 0xF800) >> 11)*0xFF/0x1F;
			c[1][3] = 0xFF;
			if (col0 > col1) {
				c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
				c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
				c[2][2] = (2*c[0][2] + 1*c[1][2])/3;
				c[2][3] = 0xFF;

				c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
				c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
				c[3][2] = (1*c[0][2] + 2*c[1][2])/3;
				c[3][3] = 0xFF;
			} else {
				c[2][0] = (c[0][0] + c[1][0])/2;
				c[2][1] = (c[0][1] + c[1][1])/2;
				c[2][2] = (c[0][2] + c[1][2])/2;
				c[2][3] = 0xFF;

				c[3][0] = 0x00;
				c[3][1] = 0x00;
				c[3][2] = 0x00;
				if (rasterFormat & 0x0200)
					c[3][3] = 0xFF;
				else // 0x0100
					c[3][3] = 0x00;
			}

			/* make index list */
			uint32_t indicesint = *((uint32_t *) &texels[i][j+4]);
			uint8_t indices[16];
			for (int32_t k = 0; k < 16; k++) {
				indices[k] = indicesint & 0x3;
				indicesint >>= 2;
			}

			/* write bytes */
			for (uint32_t k = 0; k < 4; k++)
				for (uint32_t l = 0; l < 4; l++) {
	// wtf?
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 0] = c[indices[l*4+k]][0];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 1] = c[indices[l*4+k]][1];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 2] = c[indices[l*4+k]][2];
	newtexels[(y+l)*width[i]*4 + (x+k)*4 + 3] = c[indices[l*4+k]][3];
				}
			x += 4;
			if (x >= width[i]) {
				y += 4;
				x = 0;
			}
		}
		delete[] texels[i];
		texels[i] = newtexels;
		dataSizes[i] = dataSize;
	}
	depth = 0x20;
	rasterFormat += 0x0400;
	dxtCompression = 0;
}

void NativeTexture::decompressDxt(void)
{
	if (dxtCompression == 0)
		return;

	if (dxtCompression == 1)
		decompressDxt1();
	else if (dxtCompression == 3)
		decompressDxt3();
	else if (dxtCompression == 4) {
		decompressDxt4();
	} else
		cout << "dxt" << dxtCompression << " not supported\n";
}

NativeTexture::NativeTexture(void)
: platform(0), name(""), maskName(""), filterFlags(0), rasterFormat(0),
  depth(0), palette(0), paletteSize(0), hasAlpha(false), mipmapCount(0),
  alphaDistribution(0), dxtCompression(0)
{
}

NativeTexture::NativeTexture(const NativeTexture &orig)
: platform(orig.platform),
  name(orig.name),
  maskName(orig.maskName),
  filterFlags(orig.filterFlags),
  rasterFormat(orig.rasterFormat),
  width(orig.width),
  height(orig.height),
  depth(orig.depth),
  dataSizes(orig.dataSizes),
  paletteSize(orig.paletteSize),
  hasAlpha(orig.hasAlpha),
  mipmapCount(orig.mipmapCount),
  swizzleWidth(orig.swizzleWidth),
  swizzleHeight(orig.swizzleHeight),
  alphaDistribution(orig.alphaDistribution),
  dxtCompression(orig.dxtCompression)
{
	if (orig.palette) {
		palette = new uint8_t[paletteSize*4*sizeof(uint8_t)];
		memcpy(palette, orig.palette, paletteSize*4*sizeof(uint8_t));
	} else {
		palette = 0;
	}

	for (uint32_t i = 0; i < orig.texels.size(); i++) {
		uint32_t dataSize = dataSizes[i];
		uint8_t *newtexels = new uint8_t[dataSize];
		memcpy(newtexels, &orig.texels[i][0], dataSize);
		texels.push_back(newtexels);
	}
}

NativeTexture &NativeTexture::operator=(const NativeTexture &that)
{
	if (this != &that) {
		platform = that.platform;
		name = that.name;
		maskName = that.maskName;
		filterFlags = that.filterFlags;
		rasterFormat = that.rasterFormat;
		width = that.width;
		height = that.height;
		depth = that.depth;
		dataSizes = that.dataSizes;

		paletteSize = that.paletteSize;
		hasAlpha = that.hasAlpha;
		mipmapCount = that.mipmapCount;
		swizzleWidth = that.swizzleWidth;
		swizzleHeight = that.swizzleHeight;
		dxtCompression = that.dxtCompression;


		delete[] palette;
		palette = 0;
		if (that.palette) {
			palette = new uint8_t[that.paletteSize*4];
			memcpy(&palette[0], &that.palette[0],
			       that.paletteSize*4*sizeof(uint8_t));
		}

		for (uint32_t i = 0; i < texels.size(); i++) {
			delete[] texels[i];
			texels[i] = 0;
			if (that.texels[i]) {
				texels[i] = new uint8_t[that.dataSizes[i]];
				memcpy(&texels[i][0], &that.texels[i][0],
				       that.dataSizes[i]*sizeof(uint8_t));
			}
		}
	}
	return *this;
}

NativeTexture::~NativeTexture(void)
{
	delete[] palette;
	palette = 0;
	for (uint32_t i = 0; i < texels.size(); i++) {
		delete[] texels[i];
		texels[i] = 0;
	}
}
