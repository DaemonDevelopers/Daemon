/*
===========================================================================

Daemon BSD Source Code
Copyright (c) 2013-2016 Daemon Developers
All rights reserved.

This file is part of the Daemon BSD Source Code (Daemon Source Code).

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Daemon developers nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL DAEMON DEVELOPERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===========================================================================
*/
// InternalImage.cpp
#include "tr_local.h"

// Comments may include short quotes from other authors.

/*
===============
GetInternalImageSize

Returns internal size in GPU memory in bytes.
===============
*/
int R_GetInternalImageSize( const image_t *image )
{

	constexpr int s8 = 8;
	constexpr int s16 = 16;
	constexpr int i8 = 8;
	constexpr int i16 = 16;
	constexpr int i32 = 32;
	constexpr int ui2= 2;
	constexpr int ui8 = 8;
	constexpr int ui10 = 10;
	constexpr int ui16 = 16;
	constexpr int ui32 = 32;
	constexpr int f10 = 10;
	constexpr int f11 = 11;
	constexpr int f16 = 16;
	constexpr int f32 = 32;

	int texelPerBlock = 1;
	int texelDivisor = 1;

	int texelSize;
	int imageSize;

	int texelCount = image->uploadWidth * image->uploadHeight;

	switch ( image->internalFormat )
	{
		/* Sized formats

		Values described as:
			red + green + blue + alpha + shared
		borrowed from:
			https://www.khronos.org/opengl/wiki/GLAPI/glTexStorage3D */

		case GL_R8:
			texelSize = 8;
			break;
		case GL_R8_SNORM:
			texelSize = s8;
			break;
		case GL_R16:
			texelSize = 16;
			break;
		case GL_R16_SNORM:
			texelSize = s16;
			break;
		case GL_RG8:
			texelSize = 8 + 8;
			break;
		case GL_RG8_SNORM:
			texelSize = s8 + s8;
			break;
		case GL_RG16:
			texelSize = 16 + 16;
			break;
		case GL_RG16_SNORM:
			texelSize = s16 + s16;
			break;
		case GL_R3_G3_B2:
			texelSize = 3 + 3 + 2;
			break;
		case GL_RGB4:
			texelSize = 4 + 4 + 4;
			break;
		case GL_RGB5:
			texelSize = 5 + 5 + 5;
			break;
		case GL_RGB8:
			texelSize = 8 + 8 + 8;
			break;
		case GL_RGB8_SNORM:
			texelSize = s8 + s8 + s8;
			break;
		case GL_RGB10:
			texelSize = 10 + 10 + 10;
			break;
		case GL_RGB12:
			texelSize = 12 + 12 + 12;
			break;
		case GL_RGB16_SNORM:
			texelSize = 16 + 16 + 16;
			break;
		case GL_RGBA2:
			texelSize = 2 + 2 + 2 + 2;
			break;
		case GL_RGBA4:
			texelSize = 4 + 4 + 4 + 4;
			break;
		case GL_RGB5_A1:
			texelSize = 5 + 5 + 5 + 1;
			break;
		case GL_RGBA8:
			texelSize = 8 + 8 + 8 + 8;
			break;
		case GL_RGBA8_SNORM:
			texelSize = s8 + s8 + s8 + s8;
			break;
		case GL_RGB10_A2:
			texelSize = 10 + 10 + 10 + 2;
			break;
		case GL_RGB10_A2UI:
			texelSize = ui10 + ui10 + ui10 + ui2;
			break;
		case GL_RGBA12:
			texelSize = 12 + 12 + 12 + 12;
			break;
		case GL_RGBA16:
			texelSize = 16 + 16 + 16 + 16;
			break;
		case GL_SRGB8:
			texelSize = 8 + 8 + 8;
			break;
		case GL_SRGB8_ALPHA8:
			texelSize = 8 + 8 + 8 + 8;
			break;
		case GL_R16F:
			texelSize = f16;
			break;
		case GL_RG16F:
			texelSize = f16 + f16;
			break;
		case GL_RGB16F:
			texelSize = f16 + f16 + f16;
			break;
		case GL_RGBA16F:
			texelSize = f16 + f16 + f16 + f16;
			break;
		case GL_R32F:
			texelSize = f32;
			break;
		case GL_RG32F:
			texelSize = f32 + f32;
			break;
		case GL_RGB32F:
			texelSize = f32 + f32 + f32;
			break;
		case GL_RGBA32F:
			texelSize = f32 + f32 + f32 + f32;
			break;
		case GL_R11F_G11F_B10F:
			texelSize = f11 + f11 + f10;
			break;
		case GL_RGB9_E5:
			texelSize = 9 + 9 + 9 + 0 + 5;
			break;
		case GL_R8I:
			texelSize = i8;
			break;
		case GL_R8UI:
			texelSize = ui8;
			break;
		case GL_R16I:
			texelSize = i16;
			break;
		case GL_R16UI:
			texelSize = ui16;
			break;
		case GL_R32I:
			texelSize = i32;
			break;
		case GL_R32UI:
			texelSize = ui32;
			break;
		case GL_RG8I:
			texelSize = i8 + i8;
			break;
		case GL_RG8UI:
			texelSize = ui8 + ui8;
			break;
		case GL_RG16I:
			texelSize = i16 + i16;
			break;
		case GL_RG16UI:
			texelSize = ui16 + ui16;
			break;
		case GL_RG32I:
			texelSize = i32 + i32;
			break;
		case GL_RG32UI:
			texelSize = ui32 + ui32;
			break;
		case GL_RGB8I:
			texelSize = i8 + i8 + i8;
			break;
		case GL_RGB8UI:
			texelSize = ui8 + ui8 + ui8;
			break;
		case GL_RGB16I:
			texelSize = i16 + i16 + i16;
			break;
		case GL_RGB16UI:
			texelSize = ui16 + ui16 + ui16;
			break;
		case GL_RGB32I:
			texelSize = i32 + i32 + i32;
			break;
		case GL_RGB32UI:
			texelSize = ui32 + ui32 + ui32;
			break;
		case GL_RGBA8I:
			texelSize = i8 + i8 + i8 + i8;
			break;
		case GL_RGBA8UI:
			texelSize = ui8 + ui8 + ui8 + ui8;
			break;
		case GL_RGBA16I:
			texelSize = i16 + i16 + i16 + i16;
			break;
		case GL_RGBA16UI:
			texelSize = ui16 + ui16 + ui16 + ui16;
			break;
		case GL_RGBA32I:
			texelSize = i32 + i32 + i32 + i32;
			break;
		case GL_RGBA32UI:
			texelSize = ui32 + ui32 + ui32 + ui32;
			break;

		/* Sized depth and stencil formats

		Values described as:
			depth + stencil
		borrowed from:
			https://www.khronos.org/opengl/wiki/GLAPI/glTexStorage3D */

		case GL_DEPTH_COMPONENT16:
			texelSize = 16;
			break;
		case GL_DEPTH_COMPONENT24:
			texelSize = 24;
			break;
		case GL_DEPTH_COMPONENT32:
			texelSize = 32;
			break;
		case GL_DEPTH_COMPONENT32F:
			texelSize = f32;
			break;
		case GL_DEPTH24_STENCIL8:
			texelSize = 24 + 8;
			break;
		case GL_DEPTH32F_STENCIL8:
			texelSize = f32 + 8;
			break;
		case GL_STENCIL_INDEX8:
			texelSize = 0 + 8;
			break;

		/* ARB texture float formats

		Values described as:
			red + green + blue + alpha + luminance + intensity
		borrowed from:
			https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_float.txt

		Some are already defined without _ARB suffix:

		case GL_RGB16F_ARB:
			texelSize = f16 + f16 + f16;
			break;
		case GL_RGBA16F_ARB:
			texelSize = f16 + f16 + f16 + f16;
			break;
		case GL_RGB32F_ARB:
			texelSize = f32 + f32 + f32;
			break;
		case GL_RGBA32F_ARB:
			texelSize = f32 + f32 + f32 + f32;
			break;

		But not the following ones. */

		case GL_ALPHA16F_ARB:
			texelSize = 0 + 0 + 0 + f16;
			break;
		case GL_LUMINANCE16F_ARB:
			texelSize = 0 + 0 + 0 + 0 + f16;
			break;
		case GL_LUMINANCE_ALPHA16F_ARB:
			texelSize = 0 + 0 + 0 + f16 + f16;
			break;
		case GL_INTENSITY16F_ARB:
			texelSize = 0 + 0 + 0 + 0 + 0 + f16;
			break;
		case GL_ALPHA32F_ARB:
			texelSize = 0 + 0 + 0 + f32;
			break;
		case GL_LUMINANCE32F_ARB:
			texelSize = 0 + 0 + 0 + 0 + f32;
			break;
		case GL_LUMINANCE_ALPHA32F_ARB:
			texelSize = 0 + 0 + 0 + f32 + f32;
			break;
		case GL_INTENSITY32F_ARB:
			texelSize = 0 + 0 + 0 + 0 + 0 + f32;
			break;

		/* S3TC DXT1 RGB formats

		> A DXT1-compressed image is an RGB image format.
		> As such, the alpha of any color is assumed to be 1.
		> Each 4x4 block takes up 64-bits of data, so compared
		> to a 24-bit RGB format, it provides 6:1 compression.

		> Each 4x4 block stores color data as follows. There
		> are 2 16-bit color values, color0 followed by color1.
		> Following this is a 32-bit unsigned integer containing
		> values that describe how the two colors are combined to
		> determine the color for a given texel.
		-- https://www.khronos.org/opengl/wiki/S3_Texture_Compression

		> Most compression formats have a fixed size of blocks. S3TC, BPTC,
		> and RGTC all use 4x4 texel blocks. The byte sizes can be different
		> based on different variations of the format. DXT1 in S3TC uses
		> 8-byte blocks, while DXT3/5 use 16-byte blocks.
		-- https://www.khronos.org/opengl/wiki/ASTC_Texture_Compression

		So we can either do:
			24 / 6 
		or do:
			( 16 + 16 + 32 ) / ( 4 * 4 )
		or do:
			( 8 * 8 ) / ( 4 * 4 )
		which gives:
			4

		Also, texels are stored in 4×4 blocks:

		The sRGB variant only uses different values to code colors.

		Both DXT1, DXT3 and DXT5 are little-endian formats. */

		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			texelPerBlock = 16;
			texelSize = 4;
			break;
		case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
			texelPerBlock = 16;
			texelSize = 4;
			break;

		/* S3TC DXT1 RGBA compressed formats

		> The format of the data is identical to the above case,
		> which is why this is still DXT1 compression.
		> The interpretation differs slightly.
		-- https://www.khronos.org/opengl/wiki/S3_Texture_Compression

		The sRGB variant only uses different values to code colors. */

		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			texelPerBlock = 16;
			texelSize = 4;
			break;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
			texelPerBlock = 16;
			texelSize = 4;
			break;

		/* S3TC DXT3 compressed formats

		> The DXT3 format is an RGBA format. Each 4x4 block takes up
		> 128 bits of data. Thus, compared to a 32-bit RGBA texture,
		> it offers 4:1 compression.
		> Each block of 128 bits is broken into 2 64-bit chunks.
		> The second chunk contains the color information, compressed
		> almost as in the DXT1 case;
		> The alpha 64-bit chunk is stored as a little-endian 64-bit
		> unsigned integer.
		-- https://www.khronos.org/opengl/wiki/S3_Texture_Compression

		So we can either do:
			32 / 4
		or do:
			( 64 + 64 ) / ( 4 * 4 )
		or do:
			( 16 * 8 ) / ( 4 * 4 )
		which gives:
			8

		The sRGB variant only uses different values to code colors. */

		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			texelPerBlock = 16;
			texelSize = 8;
			break;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
			texelPerBlock = 16;
			texelSize = 8;
			break;

		/* S3TC DXT5 compressed formats

		> The DXT5 format is an alternate RGBA format. As in the DXT3 case,
		> each 4x4 block takes up 128 bits. So it provides the same 4:1
		> compression as in the DXT3 case.
		> The alpha data is stored as 2 8-bit alpha values, alpha0 and alpha1,
		> followed by a 48-bit unsigned integer that describes how to combine
		> these two reference alpha values to achieve the final alpha value.

		So we can either do:
			32 / 4
		or do:
			( 64 + ( 8 + 8 ) + 48 ) / ( 4 * 4 )
		or do:
			( 16 * 8 ) / ( 4 * 4 )
		which gives:
			8

		The sRGB variant only uses different values to code colors. */

		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			texelPerBlock = 16;
			texelSize = 8;
			break;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
			texelPerBlock = 16;
			texelSize = 8;
			break;

		/* Red Green compressed formats

		> All of these basically work the same. They use compression identical
		> to the alpha form of DXT5. So for red only formats, you have 1 64-bit
		> block of the format used for DXT5's alpha; this represents the red color.
		> For red/green formats you have 2 64-bit blocks, so that red and green
		> can vary independently of one another.
		> The signed formats are almost identical. The two control colors are
		> simply considered to be two's compliment 8-bit signed integers rather
		> than unsigned integers, and all arithmetic is signed rather than unsigned.
		-- https://www.khronos.org/opengl/wiki/Red_Green_Texture_Compression

		So for RED we can do:
			64 / ( 4 * 4 )
		which gives:
			4
		And for RG we can do:
			( 64 + 64 ) / ( 4 * 4 )
		which gives:
			8

		The signed variants have the same size. */

		case GL_COMPRESSED_RED_RGTC1:
			texelPerBlock = 16;
			texelSize = 4;
			break;
		case GL_COMPRESSED_SIGNED_RED_RGTC1:
			texelPerBlock = 16;
			texelSize = 4;
			break;
		case GL_COMPRESSED_RG_RGTC2:
			texelPerBlock = 16;
			texelSize = 8;
			break;
		case GL_COMPRESSED_SIGNED_RG_RGTC2:
			texelPerBlock = 16;
			texelSize = 8;
			break;

		/* BPTC compression formats

		> Both formats use 4x4 texel blocks, and each block in both compression
		> format is 128-bits in size. Unlike S3 Texture Compression, the blocks
		> are taken as byte streams, and thus they are endian-independent. 
		-- https://www.khronos.org/opengl/wiki/BPTC_Texture_Compression

		So we can do:
			128 / ( 4 * 4 )
		which gives:
			8

		The sRGB variants only use different values to code colors. */

		case GL_COMPRESSED_RGBA_BPTC_UNORM:
			texelPerBlock = 16;
			texelSize = 8;
			break;
		case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
			texelPerBlock = 16;
			texelSize = 8;
			break;
		case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
			texelPerBlock = 16;
			texelSize = 8;
			break;
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
			texelPerBlock = 16;
			texelSize = 8;
			break;

		/* ASTC compression formats

		With ASTC, bits per texel is a float. To avoid float computation
		texel size is premultiplied by 100, then once texel size is multiplied
		with texel count, the result is divised by 100.

		See https://www.khronos.org/opengl/wiki/ASTC_Texture_Compression */

		case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
			texelPerBlock = 4 * 4;
			texelDivisor = 100;
			texelSize = 800;
			break;

		case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
			texelPerBlock = 5 * 4;
			texelDivisor = 100;
			texelSize = 640;
			break;

		case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
			texelPerBlock = 5 * 5;
			texelDivisor = 100;
			texelSize = 512;
			break;

		case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
			texelPerBlock = 6 * 5;
			texelDivisor = 100;
			texelSize = 427;
			break;

		case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
			texelPerBlock = 6 * 6;
			texelDivisor = 100;
			texelSize = 356;
			break;

		case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
			texelPerBlock = 8 * 5;
			texelDivisor = 100;
			texelSize = 320;
			break;

		case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
			texelPerBlock = 8 * 6;
			texelDivisor = 100;
			texelSize = 267;
			break;

		case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
			texelPerBlock = 10 * 5;
			texelDivisor = 100;
			texelSize = 256;
			break;

		case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
			texelPerBlock = 10 * 6;
			texelDivisor = 100;
			texelSize = 213;
			break;

		case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
			texelPerBlock = 8 * 8;
			texelDivisor = 100;
			texelSize = 200;
			break;

		case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
			texelPerBlock = 10 * 8;
			texelDivisor = 100;
			texelSize = 160;
			break;

		case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
			texelPerBlock = 10 * 10;
			texelDivisor = 100;
			texelSize = 128;
			break;

		case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
			texelPerBlock = 12 * 10;
			texelDivisor = 100;
			texelSize = 107;
			break;

		case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
			texelPerBlock = 12 * 12;
			texelDivisor = 100;
			texelSize = 89;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
			texelPerBlock = 4 * 4;
			texelDivisor = 100;
			texelSize = 800;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
			texelPerBlock = 5 * 4;
			texelDivisor = 100;
			texelSize = 640;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
			texelPerBlock = 5 * 5;
			texelDivisor = 100;
			texelSize = 512;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
			texelPerBlock = 6 * 5;
			texelDivisor = 100;
			texelSize = 427;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
			texelPerBlock = 6 * 6;
			texelDivisor = 100;
			texelSize = 356;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
			texelPerBlock = 8 * 5;
			texelDivisor = 100;
			texelSize = 320;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
			texelPerBlock = 8 * 6;
			texelDivisor = 100;
			texelSize = 267;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
			texelPerBlock = 10 * 5;
			texelDivisor = 100;
			texelSize = 256;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
			texelPerBlock = 10 * 6;
			texelDivisor = 100;
			texelSize = 213;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
			texelPerBlock = 8 * 8;
			texelDivisor = 100;
			texelSize = 200;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
			texelPerBlock = 10 * 8;
			texelDivisor = 100;
			texelSize = 160;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
			texelPerBlock = 10 * 10;
			texelDivisor = 100;
			texelSize = 128;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
			texelPerBlock = 12 * 10;
			texelDivisor = 100;
			texelSize = 107;
			break;

		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
			texelPerBlock = 12 * 12;
			texelDivisor = 100;
			texelSize = 89;
			break;

		/* Compressed formats

		> Generic formats don't have any particular internal representation.
		> OpenGL implementations are free to do whatever it wants to the data,
		> including using a regular uncompressed format if it so desires.
		> These formats rely on the driver to compress the data for you.
		> Because of this uncertainty, it is suggested that you avoid these
		> in favor of compressed formats with a specific compression format.
		-- https://www.khronos.org/opengl/wiki/Image_Format

		> GL_TEXTURE_COMPRESSED_IMAGE_SIZE
		> params returns a single integer value, the number of unsigned
		> bytes of the compressed texture image that would be returned
		> from glGetCompressedTexImage.
		-- https://www.khronos.org/opengl/wiki/GLAPI/glGetTexLevelParameter

		The sRGB variants only use different values to code colors. */

		case GL_COMPRESSED_RED:
		case GL_COMPRESSED_RG:
		case GL_COMPRESSED_RGB:
		case GL_COMPRESSED_RGBA:
		case GL_COMPRESSED_SRGB:
		case GL_COMPRESSED_SRGB_ALPHA:
			glGetIntegeri_v( GL_TEXTURE_COMPRESSED_IMAGE_SIZE, image->texnum, &imageSize );
			return imageSize;

		/* Others

		Assume RGBA8 because 8 bit per channel is pretty common
		and GL is known to sample RED, RG and RGB to RGBA */

		default:
			Log::Debug( "Unknown texel size for undocumented internal format %i for image %s", image->internalFormat, image->name );
			texelSize = 8 + 8 + 8 + 8;
			break;
	}

	if ( texelCount < texelPerBlock )
	{
		imageSize = texelPerBlock * texelSize / texelDivisor / 8;
	}
	else
	{
		imageSize = texelCount * texelSize / texelDivisor / 8;
	}

	// Assume the minimal size for any data is a byte.
	return 8 > imageSize ? 8 : imageSize;
}

int R_GetBlockSize( const image_t *image )
{
	switch ( image->internalFormat )
	{
		// S3TC formats

		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
			return 8;

		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
			return 16;

		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
			return 16;

		// RGTC formats

		case GL_COMPRESSED_RED_RGTC1:
			return 8;
		case GL_COMPRESSED_RG_RGTC2:
			return 16;

		/* Others

		Assume 16 so it may be large enough. */

		default:
			Log::Debug( "Unknown block size for undocumented internal format %i for image %s", image->internalFormat, image->name );
			return 16;
	}
}

int R_GetMipSize( int imageWidth, int imageHeight, int blockSize )
{
	return ( ( imageWidth + 3 ) >> 2 ) * ( ( imageHeight + 3 ) >> 2 ) * blockSize;
}

int R_GetMipSize( const image_t *image )
{
	int imageWidth = image->uploadWidth;
	int imageHeight = image->uploadHeight;
	int blockSize = R_GetBlockSize( image );

	return R_GetMipSize( imageWidth, imageHeight, blockSize );
}

int R_GetImageHardwareScalingStep( image_t *image, GLenum format )
{
	/* From https://www.khronos.org/opengl/wiki/GLAPI/glTexImage3D

	> If target is GL_PROXY_TEXTURE_3D, no data is read from data,
	> but all of the texture image state is recalculated, checked
	> for consistency, and checked against the implementation's
	> capabilities. If the implementation cannot handle a texture
	> of the requested texture size, it sets all of the image state
	> to 0, but does not generate an error (see glGetError).
	> To query for an entire mipmap array, use an image array level
	> greater than or equal to 1.

	From https://www.khronos.org/opengl/wiki/GLAPI/glTexImage3D

	> To then query this state, call glGetTexLevelParameter. */

	void ( *functionCompressed3D ) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
	void ( *function3D ) (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
	void ( *functionCompressed2D ) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
	void ( *function2D ) (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);

	GLenum target;

	constexpr GLint border = 0;
	constexpr GLenum type = GL_UNSIGNED_BYTE;

	bool texture3D = false;
	bool textureCompressed = format == GL_NONE;

	int mipSize = R_GetMipSize( image );

	switch ( image->type )
	{
		case GL_TEXTURE_3D:
			texture3D = true;
			functionCompressed3D = glCompressedTexImage3D;
			function3D = glTexImage3D;
			target = GL_PROXY_TEXTURE_3D;
			break;

		case GL_TEXTURE_2D:
			functionCompressed2D = glCompressedTexImage2D;
			function2D = glTexImage2D;
			target = GL_PROXY_TEXTURE_2D;
			break;

		case GL_TEXTURE_CUBE_MAP:
			functionCompressed2D = glCompressedTexImage2D;
			function2D = glTexImage2D;
			target = GL_PROXY_TEXTURE_CUBE_MAP;
			break;

		default:
			ASSERT_UNREACHABLE();
			return 0;
	}

	if ( texture3D )
	{
		if ( textureCompressed )
		{
			functionCompressed3D( target, 0, image->internalFormat, image->uploadWidth, image->uploadHeight, 0, border, mipSize, nullptr );

			GL_CheckErrors();
		}
		else
		{
			function3D( target, 0, image->internalFormat, image->uploadWidth, image->uploadHeight, 0, border, format, type, nullptr );

			GL_CheckErrors();
		}
	}
	else
	{
		if ( textureCompressed )
		{
			functionCompressed2D( target, 0, image->internalFormat, image->uploadWidth, image->uploadHeight, border, mipSize, nullptr );

			GL_CheckErrors();
		}
		else
		{
			function2D( target, 0, image->internalFormat, image->uploadWidth, image->uploadHeight, border, format, type, nullptr );

			GL_CheckErrors();
		}
	}

	GLint finalWidth;
	glGetTexLevelParameteriv( target, 0, GL_TEXTURE_WIDTH, &finalWidth );

	if ( finalWidth == 0 )
	{
		image->uploadWidth >>= 1;
		image->uploadHeight >>= 1;

		if ( image->uploadWidth <= 1 || image->uploadHeight <= 1 )
		{
			return 1;
		}

		return 1 + R_GetImageHardwareScalingStep( image, format );
	}

	return 0;
}

int R_GetImageCustomScalingStep( const image_t *image, imageParams_t *imageParams )
{
	int scalingStep = 0;

	// Perform optional picmip operation.
	if ( !( image->bits & IF_NOPICMIP ) )
	{
		int imageMinDimension = std::max( -1, r_imageMinDimension->integer );
		int imageMaxDimension = std::max( 0, r_imageMaxDimension->integer );
		int shaderMinDimension = 0;
		int shaderMaxDimension = 0;

		if ( imageParams != nullptr )
		{
			shaderMinDimension = std::max( 0, imageParams->minDimension );
			shaderMaxDimension = std::max( 0, imageParams->maxDimension );
		}

		// If shader has imageMaxDimension value set, enforce downscaling using this value.
		if ( shaderMaxDimension != 0 )
		{
			// Except if r_imageMaxDimension is set and smaller.
			if ( imageMaxDimension == 0 || imageMaxDimension >= shaderMaxDimension )
			{
				imageMaxDimension = shaderMaxDimension;
			}
		}

		// If r_imageMinDimension == -1 and shader has imageMinDimension value set, keep original size.
		if ( imageMinDimension == -1 && shaderMinDimension != 0 )
		{
			imageMaxDimension = 0;
		}

		if ( imageMaxDimension != 0 )
		{
			// If image has imageMinDimension value set and r_imageMinDimension > 0,
			// use greater value between imageMinDimension and imageMaxDimension.
			if ( shaderMinDimension > 0 && imageMinDimension > 0 )
			{
				imageMaxDimension = std::max( imageMinDimension, imageMaxDimension );
			}

			// Do not downscale this image below imageMinDimension size.
			if ( imageMaxDimension < shaderMinDimension )
			{
				imageMaxDimension = shaderMinDimension;
			}

			// Downscale image to imageMaxDimension size.
			int scaledWidth = image->width;
			int scaledHeight = image->height;

			while ( imageMaxDimension < scaledWidth || imageMaxDimension < scaledHeight )
			{
				scaledWidth >>= 1;
				scaledHeight >>= 1;
				scalingStep++;
			}
		}

		scalingStep = std::max( scalingStep, std::max( 0, r_picmip->integer ) );
	}

	return scalingStep;
}

void R_DownscaleImageDimensions( int scalingStep, int *scaledWidth, int *scaledHeight, const byte ***dataArray, int numLayers, int *numMips )
{
	if ( scalingStep > 0 )
	{
		*scaledWidth >>= scalingStep;
		*scaledHeight >>= scalingStep;

		if( *dataArray && *numMips > scalingStep ) {
			*dataArray += numLayers * scalingStep;
			*numMips -= scalingStep;
		}
	}

	// Clamp to minimum size.
	if ( *scaledWidth < 1 )
	{
		*scaledWidth = 1;
	}

	if ( *scaledHeight < 1 )
	{
		*scaledHeight = 1;
	}
}
