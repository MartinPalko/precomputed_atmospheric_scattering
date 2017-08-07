#include <windows.h>

#include <GL/glew.h>
#include <GL/glut.h>
#include "tiffio.h"

#include "TextureSaver.h"

//#include "ImfHeader.h"
//#include "ImfOutputFile.h"
//#include "ImfFrameBuffer.h"
//#include "ImfChannelList.h"
//#include "ImfPixelType.h"

namespace SaveFormat
{
	enum Type
	{
		TIF,
		EXR
	};
}

void SaveTexture(const char* path, unsigned int texture, int textureType, bool alpha, int width, int height, int depth, SaveFormat::Type format)
{
	const bool halfPrecision = false; // Hard coded switch between half and full precision floats.

	if (textureType != GL_TEXTURE_3D)
		depth = 1;

	int sampleperpixel = alpha ? 4 : 3; // RGBA
	int channelSize = halfPrecision ? 2 : 4; // 16 or 32 bit floating point
	int pixelSize = channelSize * sampleperpixel;
	int panelSize = height * width * pixelSize;
	int bufferSize = panelSize * depth;

	if (textureType == GL_TEXTURE_CUBE_MAP)
		bufferSize *= 6;

	byte* pixelbuffer = new byte[bufferSize];
	byte* tempbuffer = new byte[bufferSize];

	glBindTexture(textureType, texture);

	GLenum glFormat = alpha ? GL_RGBA : GL_RGB;
	GLenum glPixelDepth = halfPrecision ? GL_HALF_FLOAT : GL_FLOAT;

	if (textureType == GL_TEXTURE_CUBE_MAP)
	{
		for (int i = 0; i < 6; i++)
			glGetTexImage(GL_TEXTURE_BINDING_CUBE_MAP + 1 + i, 0, glFormat, glPixelDepth, pixelbuffer + panelSize * i);
	}
	else
	{
		glGetTexImage(textureType, 0, glFormat, glPixelDepth, pixelbuffer);
	}

	// Reorder buffer to save out 3d texture to tif (from tiling vertically, to tiling horizontally)
	if (textureType == GL_TEXTURE_3D)
	{
		memcpy(tempbuffer, pixelbuffer, bufferSize);

		int srcLineSize = width * pixelSize;
		int destLineSize = width * depth * pixelSize;
		int panelWidth = width * pixelSize;

		for (int z = 0; z < depth; z++)
		{
			for (int y = 0; y < height; y++)
			{
				int srcLineStart = (z * panelSize) + (y * srcLineSize);
				int destLineStart = (z * panelWidth) + (y * destLineSize);

				memcpy(pixelbuffer + destLineStart, tempbuffer + srcLineStart, srcLineSize);
			}
		}
	}

	int outputWidth = width * depth;
	int outputHeight = height;

	// Make tiff big enough to lay out cube faces properly
	if (textureType == GL_TEXTURE_CUBE_MAP)
	{
		outputWidth *= 3;
		outputHeight *= 4;
	}

	if (format == SaveFormat::TIF)
	{
		TIFF *out = TIFFOpen(path, "w");
		TIFFSetField(out, TIFFTAG_IMAGEWIDTH, outputWidth);  // set the width of the image
		TIFFSetField(out, TIFFTAG_IMAGELENGTH, outputHeight);    // set the height of the image
		TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, sampleperpixel);   // set number of channels per pixel
		TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, channelSize * 8);    // set the size of the channels
		TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);    // set the origin of the image.
		TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
		TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP); // Floating point
		TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);

		tsize_t linebytes = pixelSize * outputWidth; // length in memory of one row of pixel in the image.
		byte* buf = NULL; // buffer used to store the row of pixel information for writing to file

		// Allocating memory to store the pixels of current row
		if (TIFFScanlineSize(out) < linebytes)
			buf = (byte*)_TIFFmalloc(linebytes);
		else
			buf = (byte*)_TIFFmalloc(TIFFScanlineSize(out));

		// We set the strip size of the file to be size of one row of pixels
		TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, width * sampleperpixel));

		//Now writing image to the file one strip at a time
		if (textureType == GL_TEXTURE_CUBE_MAP)
		{
			tsize_t thirdBytes = linebytes / 3;
			byte* zerobuf = (byte*)_TIFFmalloc(thirdBytes);
			for (int i = 0; i < thirdBytes; i++)
			{
				//zerobuf[i] = 0;
			}
			void* dest1 = buf;
			void* dest2 = buf + thirdBytes;
			void* dest3 = buf + (thirdBytes * 2);

			// Lay out the cube faces in a cross pattern as we write
			for (uint32 row = 0; row < outputHeight; row++)
			{
				int faceRow = row % height;

				// TODO: Do 4 seperate loops, instead of branching on each iteration of one loop.
				switch (row / height)
				{
				case 0: // Blank, +Y, Blank
					memcpy(dest1, zerobuf, thirdBytes);
					memcpy(dest2, &pixelbuffer[((GL_TEXTURE_CUBE_MAP_POSITIVE_Y - GL_TEXTURE_BINDING_CUBE_MAP - 1) * panelSize) + (faceRow * thirdBytes)], thirdBytes);
					memcpy(dest3, zerobuf, thirdBytes);
					break;
				case 1: // -X, +Z, +X
					memcpy(dest1, &pixelbuffer[((GL_TEXTURE_CUBE_MAP_NEGATIVE_X - GL_TEXTURE_BINDING_CUBE_MAP - 1) * panelSize) + (faceRow * thirdBytes)], thirdBytes);
					memcpy(dest2, &pixelbuffer[((GL_TEXTURE_CUBE_MAP_POSITIVE_Z - GL_TEXTURE_BINDING_CUBE_MAP - 1) * panelSize) + (faceRow * thirdBytes)], thirdBytes);
					memcpy(dest3, &pixelbuffer[((GL_TEXTURE_CUBE_MAP_POSITIVE_X - GL_TEXTURE_BINDING_CUBE_MAP - 1) * panelSize) + (faceRow * thirdBytes)], thirdBytes);
					break;
				case 2: // Blank, -Y, Blank
					memcpy(dest1, zerobuf, thirdBytes);
					memcpy(dest2, &pixelbuffer[((GL_TEXTURE_CUBE_MAP_NEGATIVE_Y - GL_TEXTURE_BINDING_CUBE_MAP - 1) * panelSize) + (faceRow * thirdBytes)], thirdBytes);
					memcpy(dest3, zerobuf, thirdBytes);
					break;
				case 3: // Blank, -Z, Blank
				default:
					memcpy(dest1, zerobuf, thirdBytes);
					memcpy(dest2, &pixelbuffer[((GL_TEXTURE_CUBE_MAP_NEGATIVE_Z - GL_TEXTURE_BINDING_CUBE_MAP - 1) * panelSize) + (faceRow * thirdBytes)], thirdBytes);
					memcpy(dest3, zerobuf, thirdBytes);
					break;
				}

				if (TIFFWriteScanline(out, buf, row, 0) < 0)
					break;
			}
		}
		else
		{
			// Standard write
			for (uint32 row = 0; row < outputHeight; row++)
			{
				memcpy(buf, &pixelbuffer[row * linebytes], linebytes);
				if (TIFFWriteScanline(out, buf, row, 0) < 0)
					break;
			}
		}

		(void)TIFFClose(out);
		if (buf)
			_TIFFfree(buf);
	}
	else if (format == SaveFormat::EXR)
	{
		// WIP
		//Imf::Header header(width, height);
		//header.channels().insert("G", Imf::Channel(Imf::HALF));
		//header.channels().insert("Z", Imf::Channel(Imf::FLOAT));

		//Imf::OutputFile file(path, header);
		//Imf::FrameBuffer frameBuffer;
		//frameBuffer.insert("G", // name
		//	Imf::Slice(Imf::HALF, // type
		//	(char*)&pixelbuffer, // base
		//	sizeof(pixelbuffer) * 1, // xStride
		//	sizeof(pixelbuffer) * width)); // yStride
		//frameBuffer.insert("Z", // name
		//	Imf::Slice(Imf::FLOAT, // type
		//	(char*)&pixelbuffer, // base
		//	sizeof(pixelbuffer) * 1, // xStride
		//	sizeof(pixelbuffer) * width)); // yStride
		//file.setFrameBuffer(frameBuffer); //
		//file.writePixels(height);
	}

	delete pixelbuffer;
	delete tempbuffer;
}

void SaveTextureToTiff(const char* path, unsigned int texture, int textureType, bool alpha, int width, int height, int depth /*= 1*/)
{
	SaveTexture(path, texture, textureType, alpha, width, height, depth, SaveFormat::TIF);
}

//void SaveTextureToEXR(const char* path, unsigned int texture, int textureType, int width, int height, int depth /*= 1*/)
//{
//	SaveTexture(path, texture, textureType, width, height, depth, SaveFormat::EXR);
//}