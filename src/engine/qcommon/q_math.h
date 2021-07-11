/*
 * ===========================================================================
 *
 * Daemon GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * This file is part of the Daemon GPL Source Code (Daemon Source Code).
 *
 * Daemon Source Code is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Daemon Source Code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, the Daemon Source Code is also subject to certain additional terms.
 * You should have received a copy of these additional terms immediately following the
 * terms and conditions of the GNU General Public License which accompanied the Daemon
 * Source Code.  If not, please request a copy in writing from id Software at the address
 * below.
 *
 * If you have questions concerning this license or the applicable additional terms, you
 * may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
 * Maryland 20850 USA.
 *
 * ===========================================================================
 */

// q_math.h -- stateless support routines that are included in each code module

#ifndef Q_MATH_H_
#define Q_MATH_H_

/*
==============================================================

MATHLIB

==============================================================
*/

// math.h/cmath uses _USE_MATH_DEFINES to decide if to define M_PI etc or not.
// So define _USE_MATH_DEFINES early before including math.h/cmath
// and before including any other header in case they bring in math.h/cmath indirectly.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

// C standard library headers
#include <float.h>
#include <math.h>

// C++ standard library headers
#include <algorithm>

#include "common/Platform.h"
#include "common/Compiler.h"
#include "common/Endian.h"

/* FIXME: such macro would better live in a header
common to q_shared.h and q_math.h, it currently lives
there because it's the best way to make it available
to q_math.h and q_shared.h at the same time. */
#define Q_UNUSED(x) (void)(x)

using vec_t = float;
using vec2_t = vec_t[2];

using vec3_t = vec_t[3];
using vec4_t = vec_t[4];

using axis_t = vec3_t[3];
using matrix3x3_t = vec_t[3 * 3];
using matrix_t = vec_t[4 * 4];
using quat_t = vec_t[4];

//=============================================================

using byte = uint8_t;
using uint = unsigned int;

union floatint_t
{
	float f;
	int i;
	uint ui;
};

//=============================================================

// A transform_t represents a product of basic
// transformations, which are a rotation about an arbitrary
// axis, a uniform scale or a translation. Any a product can
// alway be brought into the form rotate, then scale, then
// translate. So the whole transform_t can be stored in 8
// floats (quat: 4, scale: 1, translation: 3), which is very
// convenient for SSE and GLSL, which operate on 4-dimensional
// float vectors.
#if idx86_sse
	// Here we have a union of scalar struct and sse struct, transform_u and the
	// scalar struct must match transform_t so we have to use anonymous structs.
	// We disable compiler warnings when using -Wpedantic for this specific case.
	#ifdef __GNUC__
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wpedantic"
	#endif
	ALIGNED(16, union transform_t {
		struct {
			quat_t rot;
			vec3_t trans;
			vec_t  scale;
		};
		struct {
			__m128 sseRot;
			__m128 sseTransScale;
		};
	});
	#ifdef __GNUC__
		#pragma GCC diagnostic pop
	#endif
#else // !idx86_sse
	ALIGNED(16, struct transform_t {
		quat_t rot;
		vec3_t trans;
		vec_t  scale;
	});
#endif // !idx86_sse

using fixed4_t = int;
using fixed8_t = int;
using fixed16_t = int;

#ifndef M_PI
	#define M_PI 3.14159265358979323846f // matches value in gcc v2 math.h
#endif

#ifndef M_SQRT2
	#define M_SQRT2 1.414213562f
#endif

#ifndef M_ROOT3
	#define M_ROOT3 1.732050808f
#endif

#ifndef LINE_DISTANCE_EPSILON
#define LINE_DISTANCE_EPSILON 1e-05f
#endif

#define ARRAY_LEN(x) ( sizeof( x ) / sizeof( *( x ) ) )

#define DEG2RAD( a ) \
	( ( ( a ) * M_PI ) / 180.0f )
#define RAD2DEG( a ) \
	( ( ( a ) * 180.0f ) / M_PI )

#define ANGLE2SHORT( x ) ( (int)( ( x ) * 65536 / 360 ) & 65535 )
#define SHORT2ANGLE( x ) ( ( x ) * ( 360.0 / 65536 ) )

#define Square( x ) ( ( x ) * ( x ) )

// MSVC does not have roundf
#ifdef _MSC_VER
	#define roundf( f ) ( floor( (f) + 0.5 ) )
#endif

// angle indexes
#define PITCH 0 // up / down
#define YAW   1 // left / right
#define ROLL  2 // fall over

#define NUMVERTEXNORMALS 162
extern vec3_t bytedirs[ NUMVERTEXNORMALS ];

extern vec3_t vec3_origin;
extern vec3_t axisDefault[ 3 ];
extern matrix_t matrixIdentity;
extern quat_t quatIdentity;

#define nanmask ( 255 << 23 )

#define IS_NAN( x ) ( ( ( *(int *)&( x ) ) & nanmask ) == nanmask )

#define Q_ftol(x) ((long)(x))

inline unsigned int Q_floatBitsToUint( float number )
{
	floatint_t t;

	t.f = number;
	return t.ui;
}

inline float Q_uintBitsToFloat( unsigned int number )
{
	floatint_t t;

	t.ui = number;
	return t.f;
}

inline float Q_rsqrt( float number )
{
	float x = 0.5f * number;
	float y;

	Q_UNUSED(x);

	// compute approximate inverse square root
	#if defined( idx86_sse )
		_mm_store_ss( &y, _mm_rsqrt_ss( _mm_load_ss( &number ) ) );
	#elif idppc
		#ifdef __GNUC__
			asm( "frsqrte %0, %1" : "=f"( y ) : "f"( number ) );
		#else
			y = __frsqrte( number );
		#endif
	#else
		y = Q_uintBitsToFloat( 0x5f3759df - (Q_floatBitsToUint( number ) >> 1) );
		y *= ( 1.5f - ( x * y * y ) ); // initial iteration
	#endif

	y *= ( 1.5f - ( x * y * y ) ); // second iteration for higher precision
	return y;
}

inline float Q_fabs( float x )
{
	return fabsf( x );
}

//==============================================================

#define random() \
	( ( rand() & 0x7fff ) / ( (float)0x7fff ) )

#define crandom() \
	( 2.0 * ( random() - 0.5 ) )

inline int Q_rand( int *seed )
{
	*seed = ( 69069 * *seed + 1 );
	return *seed;
}

// Range of [0,1]
inline float Q_random( int *seed )
{
	return ( Q_rand( seed ) & 0xffff ) / ( float ) 0x10000;
}

// Range of [-1,1]
inline float Q_crandom( int *seed )
{
	return 2.0 * ( Q_random( seed ) - 0.5 );
}

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

// FIXME: this may better live in its own .h/.cpp files

// plane sides
enum class planeSide_t : int
{
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2,
	SIDE_CROSS = 3
};

// plane types are used to speed some tests
// 0-2 are axial planes
#define PLANE_X          0
#define PLANE_Y          1
#define PLANE_Z          2
#define PLANE_NON_AXIAL  3
#define PLANE_NON_PLANAR 4

enum class traceType_t
{
	TT_NONE,

	TT_AABB,
	TT_CAPSULE,
	TT_BISPHERE,

	TT_NUM_TRACE_TYPES
};

// plane_t structure
struct cplane_t
{
	vec3_t normal;
	float dist;
	byte type; // for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte signbits; // signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte pad[ 2 ];
};

// a trace is returned when a box is swept through the world
struct trace_t
{
	bool allsolid; // if true, plane is not valid
	bool startsolid; // if true, the initial point was in a solid area
	float fraction; // time completed, 1.0 = didn't hit anything
	vec3_t endpos; // final position
	cplane_t plane; // surface normal at impact, transformed to world space
	int surfaceFlags; // surface hit
	int contents; // contents on other side of surface hit
	int entityNum; // entity the contacted surface is a part of
	float lateralFraction; // fraction of collision tangetially to the trace direction
};

// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD

// markfragments are returned by CM_MarkFragments()
struct markFragment_t
{
	int firstPoint;
	int numPoints;
};

struct orientation_t
{
	vec3_t origin;
	vec3_t axis[ 3 ];
};

#define PlaneTypeForNormal( x ) ( x[ 0 ] == 1.0 ? PLANE_X : ( x[ 1 ] == 1.0 ? PLANE_Y : ( x[ 2 ] == 1.0 ? PLANE_Z : ( x[ 0 ] == 0.f && x[ 1 ] == 0.f && x[ 2 ] == 0.f ? PLANE_NON_PLANAR : PLANE_NON_AXIAL ) ) ) )

void SetPlaneSignbits( cplane_t *out );
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const struct cplane_t *plane );

//=====================================================================

inline byte ClampByte( int i )
{
	if ( i < 0 )
	{
		return 0;
	}

	if ( i > 255 )
	{
		return 255;
	}

	return i;
}

inline signed char ClampChar( int i )
{
	if ( i < -128 )
	{
		return -128;
	}

	if ( i > 127 )
	{
		return 127;
	}

	return i;
}

#define LinearRemap(x, an, ap, bn, bp) \
	( ( (x)-(an)) / ( (ap) - (an) ) *( (bp)-(bn) ) + (bn) )

// MA Stands for MultiplyAdd: adding a vector "b" scaled by "s" to "v" and writing it to "o"

// 2-component vector

#define Vector2Set( v, x, y ) \
	( ( v )[ 0 ] = ( x ), ( v )[ 1 ] = ( y ) )

#define Vector2Copy( a, b ) \
	( ( b )[ 0 ] = ( a )[ 0 ], ( b )[ 1 ] = ( a )[ 1 ] )

#define Vector2Subtract( a, b, c ) \
	( ( c )[ 0 ] = ( a )[ 0 ] - ( b )[ 0 ], ( c )[ 1 ] = ( a )[ 1 ] - ( b )[ 1 ] )

// 3-component vector

#define VectorSet( v, x, y, z ) \
	( ( v )[ 0 ] = ( x ), ( v )[ 1 ] = ( y ), ( v )[ 2 ] = ( z ) )

#define VectorCopy( a, b ) \
	( ( b )[ 0 ] = ( a )[ 0 ], ( b )[ 1 ] = ( a )[ 1 ], ( b )[ 2 ] = ( a )[ 2 ] )

#define VectorSubtract( a, b, c ) \
	( ( c )[ 0 ] = ( a )[ 0 ] - ( b )[ 0 ], ( c )[ 1 ] = ( a )[ 1 ] - ( b )[ 1 ], ( c )[ 2 ] = ( a )[ 2 ] - ( b )[ 2 ] )

#define VectorAdd( a, b, c ) \
	( ( c )[ 0 ] = ( a )[ 0 ] + ( b )[ 0 ], ( c )[ 1 ] = ( a )[ 1 ] + ( b )[ 1 ], ( c )[ 2 ] = ( a )[ 2 ] + ( b )[ 2 ] )

#define VectorScale( v, s, o ) \
	( ( o )[ 0 ] = ( v )[ 0 ] * ( s ), ( o )[ 1 ] = ( v )[ 1 ] * ( s ), ( o )[ 2 ] = ( v )[ 2 ] * ( s ) )

#define VectorMA( v, s, b, o ) \
	( ( o )[ 0 ] = ( v )[ 0 ] + ( b )[ 0 ] * ( s ), ( o )[ 1 ] = ( v )[ 1 ] + ( b )[ 1 ] * ( s ), ( o )[ 2 ] = ( v )[ 2 ] + ( b )[ 2 ] * ( s ) )

#define VectorLerpTrem( f, s, e, r ) \
	( ( r )[ 0 ] = ( s )[ 0 ] + ( f ) * (( e )[ 0 ] - ( s )[ 0 ] ), \
	( r )[ 1 ] = ( s )[ 1 ] + ( f ) * (( e )[ 1 ] - ( s )[ 1 ] ), \
	( r )[ 2 ] = ( s )[ 2 ] + ( f ) * (( e )[ 2 ] - ( s )[ 2 ] ) )

#define VectorClear( a ) \
	( ( a )[ 0 ] = ( a )[ 1 ] = ( a )[ 2 ] = 0 )

#define VectorNegate( a, b ) \
	( ( b )[ 0 ] = -( a )[ 0 ], ( b )[ 1 ] = -( a )[ 1 ], ( b )[ 2 ] = -( a )[ 2 ] )

#define DotProduct( x,y ) \
	( ( x )[ 0 ] * ( y )[ 0 ] + ( x )[ 1 ] * ( y )[ 1 ] + ( x )[ 2 ] * ( y )[ 2 ] )

#define SnapVector( v ) \
	do { ( v )[ 0 ] = ( floor( ( v )[ 0 ] + 0.5f ) ); ( v )[ 1 ] = ( floor( ( v )[ 1 ] + 0.5f ) ); ( v )[ 2 ] = ( floor( ( v )[ 2 ] + 0.5f ) ); } while ( 0 )

// 4-component vector

#define Vector4Set( v, x, y, z, n ) \
	( ( v )[ 0 ] = ( x ), ( v )[ 1 ] = ( y ), ( v )[ 2 ] = ( z ), ( v )[ 3 ] = ( n ) )

#define Vector4Copy( a, b ) \
	( ( b )[ 0 ] = ( a )[ 0 ], ( b )[ 1 ] = ( a )[ 1 ], ( b )[ 2 ] = ( a )[ 2 ], ( b )[ 3 ] = ( a )[ 3 ] )

#define Vector4MA( v, s, b, o ) \
	( ( o )[ 0 ] = ( v )[ 0 ] + ( b )[ 0 ] * ( s ), ( o )[ 1 ] = ( v )[ 1 ] + ( b )[ 1 ] * ( s ), ( o )[ 2 ] = ( v )[ 2 ] + ( b )[ 2 ] * ( s ), ( o )[ 3 ] = ( v )[ 3 ] + ( b )[ 3 ] * ( s ) )

#define Vector4Average( v, b, s, o ) \
	( ( o )[ 0 ] = ( ( v )[ 0 ] * ( 1 - ( s ) ) ) + ( ( b )[ 0 ] * ( s ) ), ( o )[ 1 ] = ( ( v )[ 1 ] * ( 1 - ( s ) ) ) + ( ( b )[ 1 ] * ( s ) ), ( o )[ 2 ] = ( ( v )[ 2 ] * ( 1 - ( s ) ) ) + ( ( b )[ 2 ] * ( s ) ), ( o )[ 3 ] = ( ( v )[ 3 ] * ( 1 - ( s ) ) ) + ( ( b )[ 3 ] * ( s ) ) )

#define DotProduct4( x, y ) \
	( ( x )[ 0 ] * ( y )[ 0 ] + ( x )[ 1 ] * ( y )[ 1 ] + ( x )[ 2 ] * ( y )[ 2 ] + ( x )[ 3 ] * ( y )[ 3 ] )

int DirToByte( vec3_t dir );

inline void ByteToDir( int b, vec3_t dir )
{
	if ( b < 0 || b >= NUMVERTEXNORMALS )
	{
		VectorCopy( vec3_origin, dir );
		return;
	}

	VectorCopy( bytedirs[ b ], dir );
}

inline vec_t PlaneNormalize( vec4_t plane )
{
	vec_t length2, ilength;

	length2 = DotProduct( plane, plane );

	if ( length2 == 0.0f )
	{
		VectorClear( plane );
		return 0.0f;
	}

	ilength = Q_rsqrt( length2 );

	plane[ 0 ] = plane[ 0 ] * ilength;
	plane[ 1 ] = plane[ 1 ] * ilength;
	plane[ 2 ] = plane[ 2 ] * ilength;
	plane[ 3 ] = plane[ 3 ] * ilength;

	return length2 * ilength;
}

inline vec_t VectorNormalize( vec3_t v );
inline vec_t VectorNormalize2( const vec3_t v, vec3_t out );
inline vec_t VectorLength( const vec3_t v );
inline void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );

/*
 * =====================
 * PlaneFromPoints
 *
 * Returns false if the triangle is degenerate.
 * The normal will point out of the clock for clockwise ordered points
 * =====================
 */
inline bool PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c )
{
	vec3_t d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );

	if ( VectorNormalize( plane ) == 0 )
	{
		return false;
	}

	plane[ 3 ] = DotProduct( a, plane );
	return true;
}

/*
 * =====================
 * PlaneFromPoints
 *
 * Returns false if the triangle is degenerate.
 * =====================
 */
inline bool PlaneFromPointsOrder( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c, bool cw )
{
	vec3_t d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );

	if ( cw )
	{
		CrossProduct( d2, d1, plane );
	}

	else
	{
		CrossProduct( d1, d2, plane );
	}

	if ( VectorNormalize( plane ) == 0 )
	{
		return false;
	}

	plane[ 3 ] = DotProduct( a, plane );
	return true;
}

/* greebo: This calculates the intersection point of three planes.
 * Returns <0,0,0> if no intersection point could be found, otherwise returns the coordinates of the intersection point
 * (this may also be 0,0,0) */

inline bool PlanesGetIntersectionPoint( const vec4_t plane1, const vec4_t plane2, const vec4_t plane3, vec3_t out )
{
	// http://www.cgafaq.info/wiki/Intersection_of_three_planes

	vec3_t n1, n2, n3;
	vec3_t n1n2, n2n3, n3n1;
	vec_t denom;

	VectorNormalize2( plane1, n1 );
	VectorNormalize2( plane2, n2 );
	VectorNormalize2( plane3, n3 );

	CrossProduct( n1, n2, n1n2 );
	CrossProduct( n2, n3, n2n3 );
	CrossProduct( n3, n1, n3n1 );

	denom = DotProduct( n1, n2n3 );

	// check if the denominator is zero (which would mean that no intersection is to be found
	if ( denom == 0 )
	{
		// no intersection could be found, return <0,0,0>
		VectorClear( out );
		return false;
	}

	VectorClear( out );

	VectorMA( out, plane1[ 3 ], n2n3, out );
	VectorMA( out, plane2[ 3 ], n3n1, out );
	VectorMA( out, plane3[ 3 ], n1n2, out );

	VectorScale( out, 1.0f / denom, out );

	return true;
}

inline void PlaneIntersectRay( const vec3_t rayPos, const vec3_t rayDir, const vec4_t plane, vec3_t res )
{
	vec3_t dir;
	float sect;

	VectorNormalize2( rayDir, dir );

	sect = - ( DotProduct( plane, rayPos ) - plane[ 3 ] ) / DotProduct( plane, rayDir );
	VectorScale( dir, sect, dir );
	VectorAdd( rayPos, dir, res );
}

inline void ProjectPointOnPlane( vec3_t dst, const vec3_t point, const vec3_t normal )
{
	float d = -DotProduct( point, normal );
	VectorMA( point, d, normal, dst );
}

/*
 * ===============
 * RotatePointAroundVector
 * ===============
 */
inline void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float sind, cosd, expr;
	vec3_t dxp;

	degrees = DEG2RAD( degrees );
	sind = sin( degrees );
	cosd = cos( degrees );
	expr = ( 1 - cosd ) * DotProduct( dir, point );
	CrossProduct( dir, point, dxp );

	dst[ 0 ] = expr * dir[ 0 ] + cosd * point[ 0 ] + sind * dxp[ 0 ];
	dst[ 1 ] = expr * dir[ 1 ] + cosd * point[ 1 ] + sind * dxp[ 1 ];
	dst[ 2 ] = expr * dir[ 2 ] + cosd * point[ 2 ] + sind * dxp[ 2 ];
}

void PerpendicularVector( vec3_t dst, const vec3_t src );

/*
 * ===============
 * RotateAroundDirection
 * ===============
 */
inline void RotateAroundDirection( vec3_t axis[ 3 ], float yaw )
{
	// create an arbitrary axis[1]
	PerpendicularVector( axis[ 1 ], axis[ 0 ] );

	// rotate it around axis[0] by yaw
	if ( yaw )
	{
		vec3_t temp;

		VectorCopy( axis[ 1 ], temp );
		RotatePointAroundVector( axis[ 1 ], axis[ 0 ], temp, yaw );
	}

	// cross to get axis[2]
	CrossProduct( axis[ 0 ], axis[ 1 ], axis[ 2 ] );
}

void vectoangles( const vec3_t value1, vec3_t angles );

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );

/*
 * =================
 * AnglesToAxis
 * =================
 */
inline void AnglesToAxis( const vec3_t angles, vec3_t axis[ 3 ] )
{
	vec3_t right;

	// angle vectors returns "right" instead of "y axis"
	AngleVectors( angles, axis[ 0 ], right, axis[ 2 ] );
	VectorSubtract( vec3_origin, right, axis[ 1 ] );
}

inline void AxisClear( vec3_t axis[ 3 ] )
{
	axis[ 0 ][ 0 ] = 1;
	axis[ 0 ][ 1 ] = 0;
	axis[ 0 ][ 2 ] = 0;
	axis[ 1 ][ 0 ] = 0;
	axis[ 1 ][ 1 ] = 1;
	axis[ 1 ][ 2 ] = 0;
	axis[ 2 ][ 0 ] = 0;
	axis[ 2 ][ 1 ] = 0;
	axis[ 2 ][ 2 ] = 1;
}

inline void AxisCopy( vec3_t in[ 3 ], vec3_t out[ 3 ] )
{
	VectorCopy( in[ 0 ], out[ 0 ] );
	VectorCopy( in[ 1 ], out[ 1 ] );
	VectorCopy( in[ 2 ], out[ 2 ] );
}

/*
 * ================
 * MakeNormalVectors
 *
 * Given a normalized forward vector, create two
 * other perpendicular vectors
 * ================
 */
inline void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up )
{
	float d;

	// this rotate and negate guarantees a vector
	// not colinear with the original

	right[ 1 ] = -forward[ 0 ];
	right[ 2 ] = forward[ 1 ];
	right[ 0 ] = forward[ 2 ];

	d = DotProduct( right, forward );
	VectorMA( right, -d, forward, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
}

/*
==================
ProjectPointOntoRectangleOutwards

Traces a ray from inside a rectangle and returns the point of
intersection with the rectangle
==================
*/
inline float ProjectPointOntoRectangleOutwards( vec2_t out, const vec2_t point, const vec2_t dir, const vec2_t bounds[ 2 ] )
{
	float t, ty;
	bool dsign[ 2 ];

	dsign[ 0 ] = ( dir[ 0 ] < 0 );
	dsign[ 1 ] = ( dir[ 1 ] < 0.0 );

	t = ( bounds[ 1 - dsign[ 0 ] ][ 0 ] - point[ 0 ] ) / dir[ 0 ];
	ty = ( bounds[ 1 - dsign[ 1 ] ][ 1 ] - point[ 1 ] ) / dir[ 1 ];

	if( ty < t )
		t = ty;

	out[ 0 ] = point[ 0 ] + dir[ 0 ] * t;
	out[ 1 ] = point[ 1 ] + dir[ 1 ] * t;

	return t;
}

//============================================================

/*
=============
ExponentialFade

Fades one value towards another exponentially
=============
*/
inline void ExponentialFade( float *value, float target, float lambda, float timedelta )
{
	*value = target + ( *value - target ) * exp( - lambda * timedelta );
}

/*
 * ===============
 * LerpAngle
 *
 * ===============
 */
inline float LerpAngle( float from, float to, float frac )
{
	if ( to - from > 180 )
	{
		to -= 360;
	}

	if ( to - from < -180 )
	{
		to += 360;
	}

	return ( from + frac * ( to - from ) );
}

/*
 * =================
 * AngleSubtract
 *
 * Always returns a value from -180 to 180
 * =================
 */
inline float AngleSubtract( float a1, float a2 )
{
	float a = a1 - a2;


	return a - 360.0f * floor( ( a + 180.0f ) / 360.0f );
}

inline void AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 )
{
	v3[ 0 ] = AngleSubtract( v1[ 0 ], v2[ 0 ] );
	v3[ 1 ] = AngleSubtract( v1[ 1 ], v2[ 1 ] );
	v3[ 2 ] = AngleSubtract( v1[ 2 ], v2[ 2 ] );
}

inline float AngleMod( float a )
{
	return ( ( 360.0 / 65536 ) * ( ( int )( a * ( 65536 / 360.0 ) ) & 65535 ) );
}

/*
 * =================
 * AngleNormalize360
 *
 * returns angle normalized to the range [0 <= angle < 360]
 * =================
 */
inline float AngleNormalize360( float angle )
{
	return ( 360.0 / 65536 ) * ( ( int )( angle * ( 65536 / 360.0 ) ) & 65535 );
}

/*
 * =================
 * AngleNormalize180
 *
 * returns angle normalized to the range [-180 < angle <= 180]
 * =================
 */
inline float AngleNormalize180( float angle )
{
	angle = AngleNormalize360( angle );

	if ( angle > 180.0 )
	{
		angle -= 360.0;
	}

	return angle;
}

/*
 * =================
 * AngleDelta
 *
 * returns the normalized delta from angle1 to angle2
 * =================
 */
inline float AngleDelta( float angle1, float angle2 )
{
	return AngleNormalize180( angle1 - angle2 );
}

/*
 * =================
 * AngleBetweenVectors
 *
 * returns the angle between two vectors normalized to the range [0 <= angle <= 180]
 * =================
 */
inline float AngleBetweenVectors( const vec3_t a, const vec3_t b )
{
	vec_t alen, blen;

	alen = VectorLength( a );
	blen = VectorLength( b );

	if ( !alen || !blen )
	{
		return 0;
	}

	// complete dot product of two vectors a, b is |a| * |b| * cos(angle)
	// this results in:
	//
	// angle = acos( (a * b) / (|a| * |b|) )
	return RAD2DEG( acos( DotProduct( a, b ) / ( alen * blen ) ) );
}

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );

//============================================================

/*
 * =================
 * RadiusFromBounds
 * =================
 */
inline float RadiusFromBounds( const vec3_t mins, const vec3_t maxs )
{
	int i;
	vec3_t corner;
	float a, b;

	for ( i = 0; i < 3; i++ )
	{
		a = Q_fabs( mins[ i ] );
		b = Q_fabs( maxs[ i ] );
		corner[ i ] = a > b ? a : b;
	}

	return VectorLength( corner );
}

inline void ZeroBounds( vec3_t mins, vec3_t maxs )
{
	mins[ 0 ] = mins[ 1 ] = mins[ 2 ] = 0;
	maxs[ 0 ] = maxs[ 1 ] = maxs[ 2 ] = 0;
}

inline void ClearBounds( vec3_t mins, vec3_t maxs )
{
	mins[ 0 ] = mins[ 1 ] = mins[ 2 ] = 99999;
	maxs[ 0 ] = maxs[ 1 ] = maxs[ 2 ] = -99999;
}

inline void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs )
{
	if ( v[ 0 ] < mins[ 0 ] )
	{
		mins[ 0 ] = v[ 0 ];
	}

	if ( v[ 0 ] > maxs[ 0 ] )
	{
		maxs[ 0 ] = v[ 0 ];
	}

	if ( v[ 1 ] < mins[ 1 ] )
	{
		mins[ 1 ] = v[ 1 ];
	}

	if ( v[ 1 ] > maxs[ 1 ] )
	{
		maxs[ 1 ] = v[ 1 ];
	}

	if ( v[ 2 ] < mins[ 2 ] )
	{
		mins[ 2 ] = v[ 2 ];
	}

	if ( v[ 2 ] > maxs[ 2 ] )
	{
		maxs[ 2 ] = v[ 2 ];
	}
}

inline void BoundsAdd( vec3_t mins, vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 )
{
	if ( mins2[ 0 ] < mins[ 0 ] )
	{
		mins[ 0 ] = mins2[ 0 ];
	}

	if ( mins2[ 1 ] < mins[ 1 ] )
	{
		mins[ 1 ] = mins2[ 1 ];
	}

	if ( mins2[ 2 ] < mins[ 2 ] )
	{
		mins[ 2 ] = mins2[ 2 ];
	}

	if ( maxs2[ 0 ] > maxs[ 0 ] )
	{
		maxs[ 0 ] = maxs2[ 0 ];
	}

	if ( maxs2[ 1 ] > maxs[ 1 ] )
	{
		maxs[ 1 ] = maxs2[ 1 ];
	}

	if ( maxs2[ 2 ] > maxs[ 2 ] )
	{
		maxs[ 2 ] = maxs2[ 2 ];
	}
}

inline bool BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 )
{
	if ( maxs[ 0 ] < mins2[ 0 ]
		|| maxs[ 1 ] < mins2[ 1 ]
		|| maxs[ 2 ] < mins2[ 2 ]
		|| mins[ 0 ] > maxs2[ 0 ]
		|| mins[ 1 ] > maxs2[ 1 ]
		|| mins[ 2 ] > maxs2[ 2 ] )
	{
		return false;
	}

	return true;
}
inline bool BoundsIntersectSphere( const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec_t radius )
{
	if ( origin[ 0 ] - radius > maxs[ 0 ]
		|| origin[ 0 ] + radius < mins[ 0 ]
		|| origin[ 1 ] - radius > maxs[ 1 ]
		|| origin[ 1 ] + radius < mins[ 1 ]
		|| origin[ 2 ] - radius > maxs[ 2 ]
		|| origin[ 2 ] + radius < mins[ 2 ] )
	{
		return false;
	}

	return true;
}

inline bool BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t origin )
{
	if ( origin[ 0 ] > maxs[ 0 ]
		|| origin[ 0 ] < mins[ 0 ]
		|| origin[ 1 ] > maxs[ 1 ]
		|| origin[ 1 ] < mins[ 1 ]
		|| origin[ 2 ] > maxs[ 2 ]
		|| origin[ 2 ] < mins[ 2 ] )
	{
		return false;
	}

	return true;
}

inline float BoundsMaxExtent( const vec3_t mins, const vec3_t maxs )
{
	float result = Q_fabs( mins[0] );

	result = std::max( result, Q_fabs( mins[ 1 ] ) );
	result = std::max( result, Q_fabs( mins[ 2 ] ) );
	result = std::max( result, Q_fabs( maxs[ 0 ] ) );
	result = std::max( result, Q_fabs( maxs[ 1 ] ) );
	result = std::max( result, Q_fabs( maxs[ 2 ] ) );

	return result;
}

inline void BoundsToCorners( const vec3_t mins, const vec3_t maxs, vec3_t corners[ 8 ] )
{
	VectorSet( corners[ 0 ], mins[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( corners[ 1 ], maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( corners[ 2 ], maxs[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( corners[ 3 ], mins[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( corners[ 4 ], mins[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( corners[ 5 ], maxs[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( corners[ 6 ], maxs[ 0 ], mins[ 1 ], mins[ 2 ] );
	VectorSet( corners[ 7 ], mins[ 0 ], mins[ 1 ], mins[ 2 ] );
}

inline int VectorCompare( const vec3_t v1, const vec3_t v2 )
{
	if ( v1[ 0 ] != v2[ 0 ] || v1[ 1 ] != v2[ 1 ] || v1[ 2 ] != v2[ 2 ] )
	{
		return 0;
	}

	return 1;
}

inline int Vector4Compare( const vec4_t v1, const vec4_t v2 )
{
	if ( v1[ 0 ] != v2[ 0 ]
		|| v1[ 1 ] != v2[ 1 ]
		|| v1[ 2 ] != v2[ 2 ]
		|| v1[ 3 ] != v2[ 3 ] )
	{
		return 0;
	}

	return 1;
}

inline void VectorLerp( const vec3_t from, const vec3_t to, float frac, vec3_t out )
{
	out[ 0 ] = from[ 0 ] + ( ( to[ 0 ] - from[ 0 ] ) * frac );
	out[ 1 ] = from[ 1 ] + ( ( to[ 1 ] - from[ 1 ] ) * frac );
	out[ 2 ] = from[ 2 ] + ( ( to[ 2 ] - from[ 2 ] ) * frac );
}

inline int VectorCompareEpsilon( const vec3_t v1, const vec3_t v2, float epsilon )
{
	vec3_t d;

	VectorSubtract( v1, v2, d );
	d[ 0 ] = fabs( d[ 0 ] );
	d[ 1 ] = fabs( d[ 1 ] );
	d[ 2 ] = fabs( d[ 2 ] );

	if ( d[ 0 ] > epsilon || d[ 1 ] > epsilon || d[ 2 ] > epsilon )
	{
		return 0;
	}

	return 1;
}

inline void VectorMin(const vec3_t a, const vec3_t b, vec3_t out)
{
	out[0] = a[0] < b[0] ? a[0] : b[0];
	out[1] = a[1] < b[1] ? a[1] : b[1];
	out[2] = a[2] < b[2] ? a[2] : b[2];
}

inline void VectorMax(const vec3_t a, const vec3_t b, vec3_t out)
{
	out[0] = a[0] > b[0] ? a[0] : b[0];
	out[1] = a[1] > b[1] ? a[1] : b[1];
	out[2] = a[2] > b[2] ? a[2] : b[2];
}

// returns vector length
inline vec_t VectorNormalize( vec3_t v )
{
	float length, ilength;

	length = DotProduct( v, v );

	if ( length != 0.0f )
	{
		ilength = Q_rsqrt( length );
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		VectorScale( v, ilength, v );
	}

	return length;
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length
//
// does NOT return vector length, uses rsqrt approximation
inline void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	VectorScale( v, ilength, v );
}

inline vec_t VectorNormalize2( const vec3_t v, vec3_t out )
{
	float length, ilength;

	length = v[ 0 ] * v[ 0 ] + v[ 1 ] * v[ 1 ] + v[ 2 ] * v[ 2 ];

	if ( length )
	{
		ilength = Q_rsqrt( length );
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		VectorScale( v, ilength, out );
	}

	else
	{
		VectorClear( out );
	}

	return length;
}

inline void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross )
{
	cross[ 0 ] = v1[ 1 ] * v2[ 2 ] - v1[ 2 ] * v2[ 1 ];
	cross[ 1 ] = v1[ 2 ] * v2[ 0 ] - v1[ 0 ] * v2[ 2 ];
	cross[ 2 ] = v1[ 0 ] * v2[ 1 ] - v1[ 1 ] * v2[ 0 ];
}

inline vec_t VectorLength( const vec3_t v )
{
	return sqrt( v[ 0 ] * v[ 0 ]
		+ v[ 1 ] * v[ 1 ]
		+ v[ 2 ] * v[ 2 ] );
}

inline vec_t VectorLengthSquared( const vec3_t v )
{
	return ( v[ 0 ] * v[ 0 ]
	+ v[ 1 ] * v[ 1 ]
	+ v[ 2 ] * v[ 2 ] );
}

inline vec_t Distance( const vec3_t p1, const vec3_t p2 )
{
	vec3_t v;

	VectorSubtract( p2, p1, v );
	return VectorLength( v );
}

inline vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 )
{
	vec3_t v;

	VectorSubtract( p2, p1, v );

	return v[ 0 ] * v[ 0 ]
		+ v[ 1 ] * v[ 1 ]
		+ v[ 2 ] * v[ 2 ];
}

inline void VectorInverse( vec3_t v )
{
	v[ 0 ] = -v[ 0 ];
	v[ 1 ] = -v[ 1 ];
	v[ 2 ] = -v[ 2 ];
}

inline int NearestPowerOfTwo( int val )
{
	int answer;

	for ( answer = 1; answer < val; answer <<= 1 )
	{
		;
	}

	return answer;
}

/*
 * ================
 * AxisMultiply
 * ================
 */
 // RB: NOTE renamed MatrixMultiply to AxisMultiply because it conflicts with most new matrix functions
// It is important for mod developers to do this change as well or they risk a memory corruption by using
// the other MatrixMultiply function.
inline void AxisMultiply( float in1[ 3 ][ 3 ], float in2[ 3 ][ 3 ], float out[ 3 ][ 3 ] )
{
	out[ 0 ][ 0 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 0 ];
	out[ 0 ][ 1 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 1 ];
	out[ 0 ][ 2 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 2 ];
	out[ 1 ][ 0 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 0 ];
	out[ 1 ][ 1 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 1 ];
	out[ 1 ][ 2 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 2 ];
	out[ 2 ][ 0 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 0 ];
	out[ 2 ][ 1 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 1 ];
	out[ 2 ][ 2 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 2 ];
}

// Ridah

/*
 * =================
 * GetPerpendicularViewVector
 *
 * Used to find an "up" vector for drawing a sprite so that it always faces the view as best as possible
 * =================
 */
inline void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up )
{
	vec3_t v1, v2;

	VectorSubtract( point, p1, v1 );
	VectorNormalize( v1 );

	VectorSubtract( point, p2, v2 );
	VectorNormalize( v2 );

	CrossProduct( v1, v2, up );
	VectorNormalize( up );
}

/*
 * ================
 * ProjectPointOntoVector
 * ================
 */
inline void ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );
}

vec_t DistanceBetweenLineSegmentsSquared( const vec3_t sP0, const vec3_t sP1, const vec3_t tP0, const vec3_t tP1, float *s, float *t );
void ProjectPointOntoVectorBounded( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj );
float DistanceFromLineSquared( vec3_t p, vec3_t lp1, vec3_t lp2 );

// TTimo: const vec_t ** would require explicit casts for ANSI C conformance
// see unix/const-arg.c
void AxisToAngles( /*const*/ vec3_t axis[ 3 ], vec3_t angles );
//void AxisToAngles ( const vec3_t axis[3], vec3_t angles );

inline float VectorDistanceSquared( vec3_t v1, vec3_t v2 )
{
	vec3_t dir;

	VectorSubtract( v2, v1, dir );
	return VectorLengthSquared( dir );
}

/*
================
VectorMatrixMultiply
================
*/
inline void VectorMatrixMultiply( const vec3_t p, vec3_t m[ 3 ], vec3_t out )
{
	out[ 0 ] = m[ 0 ][ 0 ] * p[ 0 ] + m[ 1 ][ 0 ] * p[ 1 ] + m[ 2 ][ 0 ] * p[ 2 ];
	out[ 1 ] = m[ 0 ][ 1 ] * p[ 0 ] + m[ 1 ][ 1 ] * p[ 1 ] + m[ 2 ][ 1 ] * p[ 2 ];
	out[ 2 ] = m[ 0 ][ 2 ] * p[ 0 ] + m[ 1 ][ 2 ] * p[ 1 ] + m[ 2 ][ 2 ] * p[ 2 ];
}

// done.

//=============================================

// RB: XreaL matrix math functions required by the renderer

// *INDENT-OFF*
inline void MatrixIdentity( matrix_t m )
{
	m[ 0 ] = 1;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 1;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 1;
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixClear( matrix_t m )
{
	m[ 0 ] = 0;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 0;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 0;
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 0;
}

inline void MatrixCopy( const matrix_t in, matrix_t out )
{
	out[ 0 ] = in[ 0 ];
	out[ 4 ] = in[ 4 ];
	out[ 8 ] = in[ 8 ];
	out[ 12 ] = in[ 12 ];
	out[ 1 ] = in[ 1 ];
	out[ 5 ] = in[ 5 ];
	out[ 9 ] = in[ 9 ];
	out[ 13 ] = in[ 13 ];
	out[ 2 ] = in[ 2 ];
	out[ 6 ] = in[ 6 ];
	out[ 10 ] = in[ 10 ];
	out[ 14 ] = in[ 14 ];
	out[ 3 ] = in[ 3 ];
	out[ 7 ] = in[ 7 ];
	out[ 11 ] = in[ 11 ];
	out[ 15 ] = in[ 15 ];
}

inline bool MatrixCompare( const matrix_t a, const matrix_t b )
{
	return ( a[ 0 ] == b[ 0 ] && a[ 4 ] == b[ 4 ] && a[ 8 ] == b[ 8 ] && a[ 12 ] == b[ 12 ]
		&& a[ 1 ] == b[ 1 ] && a[ 5 ] == b[ 5 ] && a[ 9 ] == b[ 9 ] && a[ 13 ] == b[ 13 ]
		&& a[ 2 ] == b[ 2 ] && a[ 6 ] == b[ 6 ] && a[ 10 ] == b[ 10 ] && a[ 14 ] == b[ 14 ]
		&& a[ 3 ] == b[ 3 ] && a[ 7 ] == b[ 7 ] && a[ 11 ] == b[ 11 ] && a[ 15 ] == b[ 15 ] );
}

inline void MatrixTranspose( const matrix_t in, matrix_t out )
{
	out[ 0 ] = in[ 0 ];
	out[ 1 ] = in[ 4 ];
	out[ 2 ] = in[ 8 ];
	out[ 3 ] = in[ 12 ];
	out[ 4 ] = in[ 1 ];
	out[ 5 ] = in[ 5 ];
	out[ 6 ] = in[ 9 ];
	out[ 7 ] = in[ 13 ];
	out[ 8 ] = in[ 2 ];
	out[ 9 ] = in[ 6 ];
	out[ 10 ] = in[ 10 ];
	out[ 11 ] = in[ 14 ];
	out[ 12 ] = in[ 3 ];
	out[ 13 ] = in[ 7 ];
	out[ 14 ] = in[ 11 ];
	out[ 15 ] = in[ 15 ];
}

// FIXME: does not exist
// void MatrixTransposeIntoXMM( const matrix_t m );

// invert any m4x4 using Kramer's rule.. return true if matrix is singular, else return false
bool MatrixInverse( matrix_t m );

inline void MatrixSetupXRotation( matrix_t m, vec_t degrees )
{
	vec_t a = DEG2RAD( degrees );

	m[ 0 ] = 1;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = cos( a );
	m[ 9 ] = -sin( a );
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = sin( a );
	m[ 10 ] = cos( a );
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupYRotation( matrix_t m, vec_t degrees )
{
	vec_t a = DEG2RAD( degrees );

	m[ 0 ] = cos( a );
	m[ 4 ] = 0;
	m[ 8 ] = sin( a );
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 1;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = -sin( a );
	m[ 6 ] = 0;
	m[ 10 ] = cos( a );
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupZRotation( matrix_t m, vec_t degrees )
{
	vec_t a = DEG2RAD( degrees );

	m[ 0 ] = cos( a );
	m[ 4 ] = -sin( a );
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = sin( a );
	m[ 5 ] = cos( a );
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 1;
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupTranslation( matrix_t m, vec_t x, vec_t y, vec_t z )
{
	m[ 0 ] = 1;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = x;
	m[ 1 ] = 0;
	m[ 5 ] = 1;
	m[ 9 ] = 0;
	m[ 13 ] = y;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 1;
	m[ 14 ] = z;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupScale( matrix_t m, vec_t x, vec_t y, vec_t z )
{
	m[ 0 ] = x;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = y;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = z;
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupShear( matrix_t m, vec_t x, vec_t y )
{
	m[ 0 ] = 1;
	m[ 4 ] = x;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = y;
	m[ 5 ] = 1;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 1;
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixMultiply( const matrix_t a, const matrix_t b, matrix_t out )
{
#if idx86_sse
	// #error MatrixMultiply
	int i;
	__m128 _t0, _t1, _t2, _t3, _t4, _t5, _t6, _t7;

	_t4 = _mm_loadu_ps( &a[ 0 ] );
	_t5 = _mm_loadu_ps( &a[ 4 ] );
	_t6 = _mm_loadu_ps( &a[ 8 ] );
	_t7 = _mm_loadu_ps( &a[ 12 ] );

	for ( i = 0; i < 4; i++ )
	{
		_t0 = _mm_load1_ps( &b[ i * 4 + 0 ] );
		_t0 = _mm_mul_ps( _t4, _t0 );

		_t1 = _mm_load1_ps( &b[ i * 4 + 1 ] );
		_t1 = _mm_mul_ps( _t5, _t1 );

		_t2 = _mm_load1_ps( &b[ i * 4 + 2 ] );
		_t2 = _mm_mul_ps( _t6, _t2 );

		_t3 = _mm_load1_ps( &b[ i * 4 + 3 ] );
		_t3 = _mm_mul_ps( _t7, _t3 );

		_t1 = _mm_add_ps( _t0, _t1 );
		_t2 = _mm_add_ps( _t1, _t2 );
		_t3 = _mm_add_ps( _t2, _t3 );

		_mm_storeu_ps( &out[ i * 4 ], _t3 );
	}

#else
	out[ 0 ] = b[ 0 ] * a[ 0 ] + b[ 1 ] * a[ 4 ] + b[ 2 ] * a[ 8 ] + b[ 3 ] * a[ 12 ];
	out[ 1 ] = b[ 0 ] * a[ 1 ] + b[ 1 ] * a[ 5 ] + b[ 2 ] * a[ 9 ] + b[ 3 ] * a[ 13 ];
	out[ 2 ] = b[ 0 ] * a[ 2 ] + b[ 1 ] * a[ 6 ] + b[ 2 ] * a[ 10 ] + b[ 3 ] * a[ 14 ];
	out[ 3 ] = b[ 0 ] * a[ 3 ] + b[ 1 ] * a[ 7 ] + b[ 2 ] * a[ 11 ] + b[ 3 ] * a[ 15 ];

	out[ 4 ] = b[ 4 ] * a[ 0 ] + b[ 5 ] * a[ 4 ] + b[ 6 ] * a[ 8 ] + b[ 7 ] * a[ 12 ];
	out[ 5 ] = b[ 4 ] * a[ 1 ] + b[ 5 ] * a[ 5 ] + b[ 6 ] * a[ 9 ] + b[ 7 ] * a[ 13 ];
	out[ 6 ] = b[ 4 ] * a[ 2 ] + b[ 5 ] * a[ 6 ] + b[ 6 ] * a[ 10 ] + b[ 7 ] * a[ 14 ];
	out[ 7 ] = b[ 4 ] * a[ 3 ] + b[ 5 ] * a[ 7 ] + b[ 6 ] * a[ 11 ] + b[ 7 ] * a[ 15 ];

	out[ 8 ] = b[ 8 ] * a[ 0 ] + b[ 9 ] * a[ 4 ] + b[ 10 ] * a[ 8 ] + b[ 11 ] * a[ 12 ];
	out[ 9 ] = b[ 8 ] * a[ 1 ] + b[ 9 ] * a[ 5 ] + b[ 10 ] * a[ 9 ] + b[ 11 ] * a[ 13 ];
	out[ 10 ] = b[ 8 ] * a[ 2 ] + b[ 9 ] * a[ 6 ] + b[ 10 ] * a[ 10 ] + b[ 11 ] * a[ 14 ];
	out[ 11 ] = b[ 8 ] * a[ 3 ] + b[ 9 ] * a[ 7 ] + b[ 10 ] * a[ 11 ] + b[ 11 ] * a[ 15 ];

	out[ 12 ] = b[ 12 ] * a[ 0 ] + b[ 13 ] * a[ 4 ] + b[ 14 ] * a[ 8 ] + b[ 15 ] * a[ 12 ];
	out[ 13 ] = b[ 12 ] * a[ 1 ] + b[ 13 ] * a[ 5 ] + b[ 14 ] * a[ 9 ] + b[ 15 ] * a[ 13 ];
	out[ 14 ] = b[ 12 ] * a[ 2 ] + b[ 13 ] * a[ 6 ] + b[ 14 ] * a[ 10 ] + b[ 15 ] * a[ 14 ];
	out[ 15 ] = b[ 12 ] * a[ 3 ] + b[ 13 ] * a[ 7 ] + b[ 14 ] * a[ 11 ] + b[ 15 ] * a[ 15 ];
#endif
}

inline void MatrixMultiply2( matrix_t m, const matrix_t m2 )
{
	matrix_t tmp;

	MatrixCopy( m, tmp );
	MatrixMultiply( tmp, m2, m );
}

void MatrixToAngles( const matrix_t m, vec3_t angles );

// Tait-Bryan angles z-y-x
inline void MatrixFromAngles( matrix_t m, vec_t pitch, vec_t yaw, vec_t roll )
{
	static float sr, sp, sy, cr, cp, cy;

	// static to help MS compiler fp bugs
	sp = sin( DEG2RAD( pitch ) );
	cp = cos( DEG2RAD( pitch ) );

	sy = sin( DEG2RAD( yaw ) );
	cy = cos( DEG2RAD( yaw ) );

	sr = sin( DEG2RAD( roll ) );
	cr = cos( DEG2RAD( roll ) );

	m[ 0 ] = cp * cy;
	m[ 4 ] = ( sr * sp * cy + cr * -sy );
	m[ 8 ] = ( cr * sp * cy + -sr * -sy );
	m[ 12 ] = 0;
	m[ 1 ] = cp * sy;
	m[ 5 ] = ( sr * sp * sy + cr * cy );
	m[ 9 ] = ( cr * sp * sy + -sr * cy );
	m[ 13 ] = 0;
	m[ 2 ] = -sp;
	m[ 6 ] = sr * cp;
	m[ 10 ] = cr * cp;
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixMultiplyRotation( matrix_t m, vec_t pitch, vec_t yaw, vec_t roll )
{
	matrix_t tmp, rot;

	MatrixCopy( m, tmp );
	MatrixFromAngles( rot, pitch, yaw, roll );

	MatrixMultiply( tmp, rot, m );
}

inline void MatrixMultiplyZRotation( matrix_t m, vec_t degrees )
{
	matrix_t tmp;
	float angle = DEG2RAD( degrees );
	float s = sin( angle );
	float c = cos( angle );

	MatrixCopy( m, tmp );

	m[ 0 ] = tmp[ 0 ] * c + tmp[ 4 ] * s;
	m[ 1 ] = tmp[ 1 ] * c + tmp[ 5 ] * s;
	m[ 2 ] = tmp[ 2 ] * c + tmp[ 6 ] * s;
	m[ 3 ] = tmp[ 3 ] * c + tmp[ 7 ] * s;

	m[ 4 ] = tmp[ 0 ] * -s + tmp[ 4 ] * c;
	m[ 5 ] = tmp[ 1 ] * -s + tmp[ 5 ] * c;
	m[ 6 ] = tmp[ 2 ] * -s + tmp[ 6 ] * c;
	m[ 7 ] = tmp[ 3 ] * -s + tmp[ 7 ] * c;
}

inline void MatrixMultiplyTranslation( matrix_t m, vec_t x, vec_t y, vec_t z )
{
	m[ 12 ] += m[ 0 ] * x + m[ 4 ] * y + m[ 8 ] * z;
	m[ 13 ] += m[ 1 ] * x + m[ 5 ] * y + m[ 9 ] * z;
	m[ 14 ] += m[ 2 ] * x + m[ 6 ] * y + m[ 10 ] * z;
	m[ 15 ] += m[ 3 ] * x + m[ 7 ] * y + m[ 11 ] * z;
}

inline void MatrixMultiplyScale( matrix_t m, vec_t x, vec_t y, vec_t z )
{
	m[ 0 ] *= x;
	m[ 4 ] *= y;
	m[ 8 ] *= z;
	m[ 1 ] *= x;
	m[ 5 ] *= y;
	m[ 9 ] *= z;
	m[ 2 ] *= x;
	m[ 6 ] *= y;
	m[ 10 ] *= z;
	m[ 3 ] *= x;
	m[ 7 ] *= y;
	m[ 11 ] *= z;
}

inline void MatrixMultiplyShear( matrix_t m, vec_t x, vec_t y )
{
	matrix_t tmp;

	MatrixCopy( m, tmp );

	m[ 0 ] += m[ 4 ] * y;
	m[ 1 ] += m[ 5 ] * y;
	m[ 2 ] += m[ 6 ] * y;
	m[ 3 ] += m[ 7 ] * y;

	m[ 4 ] += tmp[ 0 ] * x;
	m[ 5 ] += tmp[ 1 ] * x;
	m[ 6 ] += tmp[ 2 ] * x;
	m[ 7 ] += tmp[ 3 ] * x;
}

inline void MatrixFromVectorsFLU( matrix_t m, const vec3_t forward, const vec3_t left, const vec3_t up )
{
	m[ 0 ] = forward[ 0 ];
	m[ 4 ] = left[ 0 ];
	m[ 8 ] = up[ 0 ];
	m[ 12 ] = 0;
	m[ 1 ] = forward[ 1 ];
	m[ 5 ] = left[ 1 ];
	m[ 9 ] = up[ 1 ];
	m[ 13 ] = 0;
	m[ 2 ] = forward[ 2 ];
	m[ 6 ] = left[ 2 ];
	m[ 10 ] = up[ 2 ];
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixFromVectorsFRU( matrix_t m, const vec3_t forward, const vec3_t right, const vec3_t up )
{
	m[ 0 ] = forward[ 0 ];
	m[ 4 ] = -right[ 0 ];
	m[ 8 ] = up[ 0 ];
	m[ 12 ] = 0;
	m[ 1 ] = forward[ 1 ];
	m[ 5 ] = -right[ 1 ];
	m[ 9 ] = up[ 1 ];
	m[ 13 ] = 0;
	m[ 2 ] = forward[ 2 ];
	m[ 6 ] = -right[ 2 ];
	m[ 10 ] = up[ 2 ];
	m[ 14 ] = 0;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

void MatrixFromQuat( matrix_t m, const quat_t q );

inline void MatrixFromPlanes( matrix_t m, const vec4_t left, const vec4_t right, const vec4_t bottom, const vec4_t top, const vec4_t near, const vec4_t far )
{
	m[ 0 ] = ( right[ 0 ] - left[ 0 ] ) / 2;
	m[ 1 ] = ( top[ 0 ] - bottom[ 0 ] ) / 2;
	m[ 2 ] = ( far[ 0 ] - near[ 0 ] ) / 2;
	m[ 3 ] = right[ 0 ] - ( right[ 0 ] - left[ 0 ] ) / 2;

	m[ 4 ] = ( right[ 1 ] - left[ 1 ] ) / 2;
	m[ 5 ] = ( top[ 1 ] - bottom[ 1 ] ) / 2;
	m[ 6 ] = ( far[ 1 ] - near[ 1 ] ) / 2;
	m[ 7 ] = right[ 1 ] - ( right[ 1 ] - left[ 1 ] ) / 2;

	m[ 8 ] = ( right[ 2 ] - left[ 2 ] ) / 2;
	m[ 9 ] = ( top[ 2 ] - bottom[ 2 ] ) / 2;
	m[ 10 ] = ( far[ 2 ] - near[ 2 ] ) / 2;
	m[ 11 ] = right[ 2 ] - ( right[ 2 ] - left[ 2 ] ) / 2;

#if 0
	m[ 12 ] = ( right[ 3 ] - left[ 3 ] ) / 2;
	m[ 13 ] = ( top[ 3 ] - bottom[ 3 ] ) / 2;
	m[ 14 ] = ( far[ 3 ] - near[ 3 ] ) / 2;
	m[ 15 ] = right[ 3 ] - ( right[ 3 ] - left[ 3 ] ) / 2;
#else
	m[ 12 ] = ( -right[ 3 ] - -left[ 3 ] ) / 2;
	m[ 13 ] = ( -top[ 3 ] - -bottom[ 3 ] ) / 2;
	m[ 14 ] = ( -far[ 3 ] - -near[ 3 ] ) / 2;
	m[ 15 ] = -right[ 3 ] - ( -right[ 3 ] - -left[ 3 ] ) / 2;
#endif
}

inline void MatrixToVectorsFLU( const matrix_t m, vec3_t forward, vec3_t left, vec3_t up )
{
	if ( forward )
	{
		forward[ 0 ] = m[ 0 ]; // cp*cy;
		forward[ 1 ] = m[ 1 ]; // cp*sy;
		forward[ 2 ] = m[ 2 ]; //-sp;
	}

	if ( left )
	{
		left[ 0 ] = m[ 4 ]; // sr*sp*cy+cr*-sy;
		left[ 1 ] = m[ 5 ]; // sr*sp*sy+cr*cy;
		left[ 2 ] = m[ 6 ]; // sr*cp;
	}

	if ( up )
	{
		up[ 0 ] = m[ 8 ]; // cr*sp*cy+-sr*-sy;
		up[ 1 ] = m[ 9 ]; // cr*sp*sy+-sr*cy;
		up[ 2 ] = m[ 10 ]; // cr*cp;
	}
}

inline void MatrixToVectorsFRU( const matrix_t m, vec3_t forward, vec3_t right, vec3_t up )
{
	if ( forward )
	{
		forward[ 0 ] = m[ 0 ];
		forward[ 1 ] = m[ 1 ];
		forward[ 2 ] = m[ 2 ];
	}

	if ( right )
	{
		right[ 0 ] = -m[ 4 ];
		right[ 1 ] = -m[ 5 ];
		right[ 2 ] = -m[ 6 ];
	}

	if ( up )
	{
		up[ 0 ] = m[ 8 ];
		up[ 1 ] = m[ 9 ];
		up[ 2 ] = m[ 10 ];
	}
}

inline void MatrixSetupTransformFromVectorsFLU( matrix_t m, const vec3_t forward, const vec3_t left, const vec3_t up, const vec3_t origin )
{
	m[ 0 ] = forward[ 0 ];
	m[ 4 ] = left[ 0 ];
	m[ 8 ] = up[ 0 ];
	m[ 12 ] = origin[ 0 ];
	m[ 1 ] = forward[ 1 ];
	m[ 5 ] = left[ 1 ];
	m[ 9 ] = up[ 1 ];
	m[ 13 ] = origin[ 1 ];
	m[ 2 ] = forward[ 2 ];
	m[ 6 ] = left[ 2 ];
	m[ 10 ] = up[ 2 ];
	m[ 14 ] = origin[ 2 ];
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}
inline void MatrixSetupTransformFromVectorsFRU( matrix_t m, const vec3_t forward, const vec3_t right, const vec3_t up, const vec3_t origin )
{
	m[ 0 ] = forward[ 0 ];
	m[ 4 ] = -right[ 0 ];
	m[ 8 ] = up[ 0 ];
	m[ 12 ] = origin[ 0 ];
	m[ 1 ] = forward[ 1 ];
	m[ 5 ] = -right[ 1 ];
	m[ 9 ] = up[ 1 ];
	m[ 13 ] = origin[ 1 ];
	m[ 2 ] = forward[ 2 ];
	m[ 6 ] = -right[ 2 ];
	m[ 10 ] = up[ 2 ];
	m[ 14 ] = origin[ 2 ];
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupTransformFromRotation( matrix_t m, const matrix_t rot, const vec3_t origin )
{
	m[ 0 ] = rot[ 0 ];
	m[ 4 ] = rot[ 4 ];
	m[ 8 ] = rot[ 8 ];
	m[ 12 ] = origin[ 0 ];
	m[ 1 ] = rot[ 1 ];
	m[ 5 ] = rot[ 5 ];
	m[ 9 ] = rot[ 9 ];
	m[ 13 ] = origin[ 1 ];
	m[ 2 ] = rot[ 2 ];
	m[ 6 ] = rot[ 6 ];
	m[ 10 ] = rot[ 10 ];
	m[ 14 ] = origin[ 2 ];
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixSetupTransformFromQuat( matrix_t m, const quat_t quat, const vec3_t origin )
{
	matrix_t rot;

	MatrixFromQuat( rot, quat );

	m[ 0 ] = rot[ 0 ];
	m[ 4 ] = rot[ 4 ];
	m[ 8 ] = rot[ 8 ];
	m[ 12 ] = origin[ 0 ];
	m[ 1 ] = rot[ 1 ];
	m[ 5 ] = rot[ 5 ];
	m[ 9 ] = rot[ 9 ];
	m[ 13 ] = origin[ 1 ];
	m[ 2 ] = rot[ 2 ];
	m[ 6 ] = rot[ 6 ];
	m[ 10 ] = rot[ 10 ];
	m[ 14 ] = origin[ 2 ];
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixAffineInverse( const matrix_t in, matrix_t out )
{
#if 0
	MatrixCopy( in, out );
	MatrixInverse( out );
#else
	// Tr3B - cleaned up

	out[ 0 ] = in[ 0 ];
	out[ 4 ] = in[ 1 ];
	out[ 8 ] = in[ 2 ];
	out[ 1 ] = in[ 4 ];
	out[ 5 ] = in[ 5 ];
	out[ 9 ] = in[ 6 ];
	out[ 2 ] = in[ 8 ];
	out[ 6 ] = in[ 9 ];
	out[ 10 ] = in[ 10 ];
	out[ 3 ] = 0;
	out[ 7 ] = 0;
	out[ 11 ] = 0;
	out[ 15 ] = 1;

	out[ 12 ] = - ( in[ 12 ] * out[ 0 ] + in[ 13 ] * out[ 4 ] + in[ 14 ] * out[ 8 ] );
	out[ 13 ] = - ( in[ 12 ] * out[ 1 ] + in[ 13 ] * out[ 5 ] + in[ 14 ] * out[ 9 ] );
	out[ 14 ] = - ( in[ 12 ] * out[ 2 ] + in[ 13 ] * out[ 6 ] + in[ 14 ] * out[ 10 ] );
#endif
}

inline void MatrixTransformNormal( const matrix_t m, const vec3_t in, vec3_t out )
{
	out[ 0 ] = m[ 0 ] * in[ 0 ] + m[ 4 ] * in[ 1 ] + m[ 8 ] * in[ 2 ];
	out[ 1 ] = m[ 1 ] * in[ 0 ] + m[ 5 ] * in[ 1 ] + m[ 9 ] * in[ 2 ];
	out[ 2 ] = m[ 2 ] * in[ 0 ] + m[ 6 ] * in[ 1 ] + m[ 10 ] * in[ 2 ];
}

inline void MatrixTransformNormal2( const matrix_t m, vec3_t inout )
{
	vec3_t tmp;

	tmp[ 0 ] = m[ 0 ] * inout[ 0 ] + m[ 4 ] * inout[ 1 ] + m[ 8 ] * inout[ 2 ];
	tmp[ 1 ] = m[ 1 ] * inout[ 0 ] + m[ 5 ] * inout[ 1 ] + m[ 9 ] * inout[ 2 ];
	tmp[ 2 ] = m[ 2 ] * inout[ 0 ] + m[ 6 ] * inout[ 1 ] + m[ 10 ] * inout[ 2 ];

	VectorCopy( tmp, inout );
}

inline void MatrixTransformPoint( const matrix_t m, const vec3_t in, vec3_t out )
{
	out[ 0 ] = m[ 0 ] * in[ 0 ] + m[ 4 ] * in[ 1 ] + m[ 8 ] * in[ 2 ] + m[ 12 ];
	out[ 1 ] = m[ 1 ] * in[ 0 ] + m[ 5 ] * in[ 1 ] + m[ 9 ] * in[ 2 ] + m[ 13 ];
	out[ 2 ] = m[ 2 ] * in[ 0 ] + m[ 6 ] * in[ 1 ] + m[ 10 ] * in[ 2 ] + m[ 14 ];
}
inline void MatrixTransformPoint2( const matrix_t m, vec3_t inout )
{
	vec3_t tmp;

	tmp[ 0 ] = m[ 0 ] * inout[ 0 ] + m[ 4 ] * inout[ 1 ] + m[ 8 ] * inout[ 2 ] + m[ 12 ];
	tmp[ 1 ] = m[ 1 ] * inout[ 0 ] + m[ 5 ] * inout[ 1 ] + m[ 9 ] * inout[ 2 ] + m[ 13 ];
	tmp[ 2 ] = m[ 2 ] * inout[ 0 ] + m[ 6 ] * inout[ 1 ] + m[ 10 ] * inout[ 2 ] + m[ 14 ];

	VectorCopy( tmp, inout );
}

inline void MatrixTransform4( const matrix_t m, const vec4_t in, vec4_t out )
{
	out[ 0 ] = m[ 0 ] * in[ 0 ] + m[ 4 ] * in[ 1 ] + m[ 8 ] * in[ 2 ] + m[ 12 ] * in[ 3 ];
	out[ 1 ] = m[ 1 ] * in[ 0 ] + m[ 5 ] * in[ 1 ] + m[ 9 ] * in[ 2 ] + m[ 13 ] * in[ 3 ];
	out[ 2 ] = m[ 2 ] * in[ 0 ] + m[ 6 ] * in[ 1 ] + m[ 10 ] * in[ 2 ] + m[ 14 ] * in[ 3 ];
	out[ 3 ] = m[ 3 ] * in[ 0 ] + m[ 7 ] * in[ 1 ] + m[ 11 ] * in[ 2 ] + m[ 15 ] * in[ 3 ];
}

inline void MatrixTransformPlane( const matrix_t m, const vec4_t in, vec4_t out )
{
	vec3_t translation;
	vec3_t planePos;

	// rotate the plane normal
	MatrixTransformNormal( m, in, out );

	// add new position to current plane position
	VectorSet( translation, m[ 12 ], m[ 13 ], m[ 14 ] );
	VectorMA( translation, in[ 3 ], out, planePos );

	out[ 3 ] = DotProduct( out, planePos );
}

inline void MatrixTransformPlane2( const matrix_t m, vec4_t inout )
{
	vec4_t tmp;

	MatrixTransformPlane( m, inout, tmp );
	Vector4Copy( tmp, inout );
}

void MatrixTransformBounds( const matrix_t m, const vec3_t mins, const vec3_t maxs, vec3_t omins, vec3_t omaxs );

/*
 * replacement for glFrustum
 * see glspec30.pdf chapter 2.12 Coordinate Transformations
 */
inline void MatrixPerspectiveProjection( matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far )
{
	m[ 0 ] = ( 2 * near ) / ( right - left );
	m[ 4 ] = 0;
	m[ 8 ] = ( right + left ) / ( right - left );
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = ( 2 * near ) / ( top - bottom );
	m[ 9 ] = ( top + bottom ) / ( top - bottom );
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = - ( far + near ) / ( far - near );
	m[ 14 ] = - ( 2 * far * near ) / ( far - near );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = -1;
	m[ 15 ] = 0;
}

/*
 * same as D3DXMatrixPerspectiveOffCenterLH
 *
 * http://msdn.microsoft.com/en-us/library/bb205353(VS.85).aspx
 */
inline void MatrixPerspectiveProjectionLH( matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far )
{
	m[ 0 ] = ( 2 * near ) / ( right - left );
	m[ 4 ] = 0;
	m[ 8 ] = ( left + right ) / ( left - right );
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = ( 2 * near ) / ( top - bottom );
	m[ 9 ] = ( top + bottom ) / ( bottom - top );
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = far / ( far - near );
	m[ 14 ] = ( near * far ) / ( near - far );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 1;
	m[ 15 ] = 0;
}

/*
 * same as D3DXMatrixPerspectiveOffCenterRH
 *
 * http://msdn.microsoft.com/en-us/library/bb205354(VS.85).aspx
 */
inline void MatrixPerspectiveProjectionRH( matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far )
{
	m[ 0 ] = ( 2 * near ) / ( right - left );
	m[ 4 ] = 0;
	m[ 8 ] = ( left + right ) / ( right - left );
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = ( 2 * near ) / ( top - bottom );
	m[ 9 ] = ( top + bottom ) / ( top - bottom );
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = far / ( near - far );
	m[ 14 ] = ( near * far ) / ( near - far );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = -1;
	m[ 15 ] = 0;
}

/*
 * same as D3DXMatrixPerspectiveFovLH
 *
 * http://msdn.microsoft.com/en-us/library/bb205350(VS.85).aspx
 */
inline void MatrixPerspectiveProjectionFovYAspectLH( matrix_t m, vec_t fov, vec_t aspect, vec_t near, vec_t far )
{
	vec_t width, height;

	width = tanf( DEG2RAD( fov * 0.5f ) );
	height = width / aspect;

	m[ 0 ] = 1 / width;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 1 / height;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = far / ( far - near );
	m[ 14 ] = - ( near * far ) / ( far - near );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 1;
	m[ 15 ] = 0;
}

inline void MatrixPerspectiveProjectionFovXYLH( matrix_t m, vec_t fovX, vec_t fovY, vec_t near, vec_t far )
{
	vec_t width, height;

	width = tanf( DEG2RAD( fovX * 0.5f ) );
	height = tanf( DEG2RAD( fovY * 0.5f ) );

	m[ 0 ] = 1 / width;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 1 / height;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = far / ( far - near );
	m[ 14 ] = - ( near * far ) / ( far - near );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 1;
	m[ 15 ] = 0;
}

inline void MatrixPerspectiveProjectionFovXYRH( matrix_t m, vec_t fovX, vec_t fovY, vec_t near, vec_t far )
{
	vec_t width, height;

	width = tanf( DEG2RAD( fovX * 0.5f ) );
	height = tanf( DEG2RAD( fovY * 0.5f ) );

	m[ 0 ] = 1 / width;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 1 / height;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = far / ( near - far );
	m[ 14 ] = ( near * far ) / ( near - far );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = -1;
	m[ 15 ] = 0;
}

// Tr3B: far plane at infinity, see RobustShadowVolumes.pdf by Nvidia
inline void MatrixPerspectiveProjectionFovXYInfiniteRH( matrix_t m, vec_t fovX, vec_t fovY, vec_t near )
{
	vec_t width, height;

	width = tanf( DEG2RAD( fovX * 0.5f ) );
	height = tanf( DEG2RAD( fovY * 0.5f ) );

	m[ 0 ] = 1 / width;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = 0;
	m[ 1 ] = 0;
	m[ 5 ] = 1 / height;
	m[ 9 ] = 0;
	m[ 13 ] = 0;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = -1;
	m[ 14 ] = -2 * near;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = -1;
	m[ 15 ] = 0;
}

/*
 * replacement for glOrtho
 * see glspec30.pdf chapter 2.12 Coordinate Transformations
 */
inline void MatrixOrthogonalProjection( matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far )
{
	m[ 0 ] = 2 / ( right - left );
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = - ( right + left ) / ( right - left );
	m[ 1 ] = 0;
	m[ 5 ] = 2 / ( top - bottom );
	m[ 9 ] = 0;
	m[ 13 ] = - ( top + bottom ) / ( top - bottom );
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = -2 / ( far - near );
	m[ 14 ] = - ( far + near ) / ( far - near );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

/*
 * same as D3DXMatrixOrthoOffCenterLH
 *
 * http://msdn.microsoft.com/en-us/library/bb205347(VS.85).aspx
 */
inline void MatrixOrthogonalProjectionLH( matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far )
{
	m[ 0 ] = 2 / ( right - left );
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = ( left + right ) / ( left - right );
	m[ 1 ] = 0;
	m[ 5 ] = 2 / ( top - bottom );
	m[ 9 ] = 0;
	m[ 13 ] = ( top + bottom ) / ( bottom - top );
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 1 / ( far - near );
	m[ 14 ] = near / ( near - far );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

/*
 * same as D3DXMatrixOrthoOffCenterRH
 *
 * http://msdn.microsoft.com/en-us/library/bb205348(VS.85).aspx
 */
inline void MatrixOrthogonalProjectionRH( matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far )
{
	m[ 0 ] = 2 / ( right - left );
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = ( left + right ) / ( left - right );
	m[ 1 ] = 0;
	m[ 5 ] = 2 / ( top - bottom );
	m[ 9 ] = 0;
	m[ 13 ] = ( top + bottom ) / ( bottom - top );
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 1 / ( near - far );
	m[ 14 ] = near / ( near - far );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

/*
 * same as D3DXMatrixReflect
 *
 * http://msdn.microsoft.com/en-us/library/bb205356%28v=VS.85%29.aspx
 */
inline void MatrixPlaneReflection( matrix_t m, const vec4_t plane )
{
	vec4_t P;
	Vector4Copy( plane, P );

	PlaneNormalize( P );

	/*
	 *	-2 * P.a * P.a + 1  -2 * P.b * P.a      -2 * P.c * P.a        0
	 *	-2 * P.a * P.b      -2 * P.b * P.b + 1  -2 * P.c * P.b        0
	 *	-2 * P.a * P.c      -2 * P.b * P.c      -2 * P.c * P.c + 1    0
	 *	-2 * P.a * P.d      -2 * P.b * P.d      -2 * P.c * P.d        1
	 */

	// Quake uses a different plane equation
	m[ 0 ] = -2 * P[ 0 ] * P[ 0 ] + 1;
	m[ 4 ] = -2 * P[ 0 ] * P[ 1 ];
	m[ 8 ] = -2 * P[ 0 ] * P[ 2 ];
	m[ 12 ] = 2 * P[ 0 ] * P[ 3 ];
	m[ 1 ] = -2 * P[ 1 ] * P[ 0 ];
	m[ 5 ] = -2 * P[ 1 ] * P[ 1 ] + 1;
	m[ 9 ] = -2 * P[ 1 ] * P[ 2 ];
	m[ 13 ] = 2 * P[ 1 ] * P[ 3 ];
	m[ 2 ] = -2 * P[ 2 ] * P[ 0 ];
	m[ 6 ] = -2 * P[ 2 ] * P[ 1 ];
	m[ 10 ] = -2 * P[ 2 ] * P[ 2 ] + 1;
	m[ 14 ] = 2 * P[ 2 ] * P[ 3 ];
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;

#if 0
	matrix_t m2;
	MatrixCopy( m, m2 );
	MatrixTranspose( m2, m );
#endif
}

inline void MatrixLookAtLH( matrix_t m, const vec3_t eye, const vec3_t dir, const vec3_t up )
{
	vec3_t dirN;
	vec3_t upN;
	vec3_t sideN;

#if 1
	CrossProduct( up, dir, sideN );
	VectorNormalize( sideN );

	CrossProduct( dir, sideN, upN );
	VectorNormalize( upN );
#else
	CrossProduct( dir, up, sideN );
	VectorNormalize( sideN );

	CrossProduct( sideN, dir, upN );
	VectorNormalize( upN );
#endif

	VectorNormalize2( dir, dirN );

	m[ 0 ] = sideN[ 0 ];
	m[ 4 ] = sideN[ 1 ];
	m[ 8 ] = sideN[ 2 ];
	m[ 12 ] = -DotProduct( sideN, eye );
	m[ 1 ] = upN[ 0 ];
	m[ 5 ] = upN[ 1 ];
	m[ 9 ] = upN[ 2 ];
	m[ 13 ] = -DotProduct( upN, eye );
	m[ 2 ] = dirN[ 0 ];
	m[ 6 ] = dirN[ 1 ];
	m[ 10 ] = dirN[ 2 ];
	m[ 14 ] = -DotProduct( dirN, eye );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixLookAtRH( matrix_t m, const vec3_t eye, const vec3_t dir, const vec3_t up )
{
	vec3_t dirN;
	vec3_t upN;
	vec3_t sideN;

	CrossProduct( dir, up, sideN );
	VectorNormalize( sideN );

	CrossProduct( sideN, dir, upN );
	VectorNormalize( upN );

	VectorNormalize2( dir, dirN );

	m[ 0 ] = sideN[ 0 ];
	m[ 4 ] = sideN[ 1 ];
	m[ 8 ] = sideN[ 2 ];
	m[ 12 ] = -DotProduct( sideN, eye );
	m[ 1 ] = upN[ 0 ];
	m[ 5 ] = upN[ 1 ];
	m[ 9 ] = upN[ 2 ];
	m[ 13 ] = -DotProduct( upN, eye );
	m[ 2 ] = -dirN[ 0 ];
	m[ 6 ] = -dirN[ 1 ];
	m[ 10 ] = -dirN[ 2 ];
	m[ 14 ] = DotProduct( dirN, eye );
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixScaleTranslateToUnitCube( matrix_t m, const vec3_t mins, const vec3_t maxs )
{
	m[ 0 ] = 2 / ( maxs[ 0 ] - mins[ 0 ] );
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = - ( maxs[ 0 ] + mins[ 0 ] ) / ( maxs[ 0 ] - mins[ 0 ] );

	m[ 1 ] = 0;
	m[ 5 ] = 2 / ( maxs[ 1 ] - mins[ 1 ] );
	m[ 9 ] = 0;
	m[ 13 ] = - ( maxs[ 1 ] + mins[ 1 ] ) / ( maxs[ 1 ] - mins[ 1 ] );

	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = 2 / ( maxs[ 2 ] - mins[ 2 ] );
	m[ 14 ] = - ( maxs[ 2 ] + mins[ 2 ] ) / ( maxs[ 2 ] - mins[ 2 ] );

	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void MatrixCrop( matrix_t m, const vec3_t mins, const vec3_t maxs )
{
	float scaleX, scaleY, scaleZ;
	float offsetX, offsetY, offsetZ;

	scaleX = 2.0f / ( maxs[ 0 ] - mins[ 0 ] );
	scaleY = 2.0f / ( maxs[ 1 ] - mins[ 1 ] );

	offsetX = -0.5f * ( maxs[ 0 ] + mins[ 0 ] ) * scaleX;
	offsetY = -0.5f * ( maxs[ 1 ] + mins[ 1 ] ) * scaleY;

	scaleZ = 1.0f / ( maxs[ 2 ] - mins[ 2 ] );
	offsetZ = -mins[ 2 ] * scaleZ;

	m[ 0 ] = scaleX;
	m[ 4 ] = 0;
	m[ 8 ] = 0;
	m[ 12 ] = offsetX;
	m[ 1 ] = 0;
	m[ 5 ] = scaleY;
	m[ 9 ] = 0;
	m[ 13 ] = offsetY;
	m[ 2 ] = 0;
	m[ 6 ] = 0;
	m[ 10 ] = scaleZ;
	m[ 14 ] = offsetZ;
	m[ 3 ] = 0;
	m[ 7 ] = 0;
	m[ 11 ] = 0;
	m[ 15 ] = 1;
}

inline void AnglesToMatrix( const vec3_t angles, matrix_t m )
{
	MatrixFromAngles( m, angles[ PITCH ], angles[ YAW ], angles[ ROLL ] );
}

//=============================================

// RB: XreaL quaternion math functions required by the renderer

#define QuatSet(q,x,y,z,w)\
	(( q )[ 0 ] = ( x ),( q )[ 1 ] = ( y ),( q )[ 2 ] = ( z ),( q )[ 3 ] = ( w ))

#define QuatCopy(a,b) \
	(( b )[ 0 ] = ( a )[ 0 ],( b )[ 1 ] = ( a )[ 1 ],( b )[ 2 ] = ( a )[ 2 ],( b )[ 3 ] = ( a )[ 3 ] )

#define QuatCompare(a,b) \
	(( a )[ 0 ] == ( b )[ 0 ] && ( a )[ 1 ] == ( b )[ 1 ] && ( a )[ 2 ] == ( b )[ 2 ] && ( a )[ 3 ] == ( b )[ 3 ] )

inline void QuatClear( quat_t q )
{
	q[ 0 ] = 0;
	q[ 1 ] = 0;
	q[ 2 ] = 0;
	q[ 3 ] = 1;
}

inline void QuatZero( quat_t o )
{
	o[ 0 ] = 0.0f;
	o[ 1 ] = 0.0f;
	o[ 2 ] = 0.0f;
	o[ 3 ] = 0.0f;
}

inline void QuatAdd( const quat_t p, const quat_t q, quat_t o )
{
	o[ 0 ] = p[ 0 ] + q[ 0 ];
	o[ 1 ] = p[ 1 ] + q[ 1 ];
	o[ 2 ] = p[ 2 ] + q[ 2 ];
	o[ 3 ] = p[ 3 ] + q[ 3 ];
}

inline void QuatMA( const quat_t p, float f, const quat_t q, quat_t o )
{
	o[ 0 ] = p[ 0 ] + f * q[ 0 ];
	o[ 1 ] = p[ 1 ] + f * q[ 1 ];
	o[ 2 ] = p[ 2 ] + f * q[ 2 ];
	o[ 3 ] = p[ 3 ] + f * q[ 3 ];
}

inline void QuatCalcW( quat_t q )
{
	vec_t term = 1.0f - ( 
		q[ 0 ] * q[ 0 ]
		+ q[ 1 ] * q[ 1 ]
		+ q[ 2 ] * q[ 2 ] );

	if ( term < 0.0 )
	{
		q[ 3 ] = 0.0;
	}
	else
	{
		q[ 3 ] = -sqrt( term );
	}
}

inline void QuatInverse( quat_t q )
{
	q[ 0 ] = -q[ 0 ];
	q[ 1 ] = -q[ 1 ];
	q[ 2 ] = -q[ 2 ];
}

inline void QuatAntipodal( quat_t q )
{
	q[ 0 ] = -q[ 0 ];
	q[ 1 ] = -q[ 1 ];
	q[ 2 ] = -q[ 2 ];
	q[ 3 ] = -q[ 3 ];
}

inline vec_t QuatLength( const quat_t q )
{
	return ( vec_t ) sqrt( q[ 0 ] * q[ 0 ] + q[ 1 ] * q[ 1 ] + q[ 2 ] * q[ 2 ] + q[ 3 ] * q[ 3 ] );
}

inline vec_t QuatNormalize( quat_t q )
{
	float length, ilength;

	length = DotProduct4( q, q );

	if ( length )
	{
		ilength = Q_rsqrt( length );
		length *= ilength;

		q[ 0 ] *= ilength;
		q[ 1 ] *= ilength;
		q[ 2 ] *= ilength;
		q[ 3 ] *= ilength;
	}

	return length;
}

void QuatFromMatrix( quat_t q, const matrix_t m );

// Tait-Bryan angles z-y-x
inline void QuatFromAngles( quat_t q, vec_t pitch, vec_t yaw, vec_t roll )
{
#if 1
	matrix_t tmp;

	MatrixFromAngles( tmp, pitch, yaw, roll );
	QuatFromMatrix( q, tmp );
#else
	static float sr, sp, sy, cr, cp, cy;

	// static to help MS compiler fp bugs
	sp = sin(DEG2RAD(pitch) * 0.5);
	cp = cos(DEG2RAD(pitch) * 0.5);

	sy = sin(DEG2RAD(yaw) * 0.5);
	cy = cos(DEG2RAD(yaw) * 0.5);

	sr = sin(DEG2RAD(roll) * 0.5);
	cr = cos(DEG2RAD(roll) * 0.5);

	q[0] = sr * cp * cy - cr * sp * sy; // x
	q[1] = cr * sp * cy + sr * cp * sy; // y
	q[2] = cr * cp * sy - sr * sp * cy; // z
	q[3] = cr * cp * cy + sr * sp * sy; // w
#endif
}

inline void AnglesToQuat( const vec3_t angles, quat_t q )
{
	QuatFromAngles( q, angles[ PITCH ], angles[ YAW ], angles[ ROLL ] );
}

inline void QuatToVectorsFLU( const quat_t q, vec3_t forward, vec3_t left, vec3_t up )
{
	matrix_t tmp;

	MatrixFromQuat( tmp, q );
	MatrixToVectorsFRU( tmp, forward, left, up );
}

inline void QuatToVectorsFRU( const quat_t q, vec3_t forward, vec3_t right, vec3_t up )
{
	matrix_t tmp;

	MatrixFromQuat( tmp, q );
	MatrixToVectorsFRU( tmp, forward, right, up );
}

inline void QuatToAxis( const quat_t q, vec3_t axis[ 3 ] )
{
	matrix_t tmp;

	MatrixFromQuat( tmp, q );
	MatrixToVectorsFLU( tmp, axis[ 0 ], axis[ 1 ], axis[ 2 ] );
}

void QuatToAngles( const quat_t q, vec3_t angles );

// Quaternion multiplication, analogous to the matrix multiplication routines.
//
// https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation
// Two rotation quaternions can be combined into one equivalent quaternion by the relation:
// q' = q2q1
// in which q' corresponds to the rotation q1 followed by the rotation q2.

inline void QuatMultiply( const quat_t qa, const quat_t qb, quat_t qc )
{
	/*
	*	from matrix and quaternion faq
	*	x = w1x2 + x1w2 + y1z2 - z1y2
	*	y = w1y2 + y1w2 + z1x2 - x1z2
	*	z = w1z2 + z1w2 + x1y2 - y1x2
	*
	*	w = w1w2 - x1x2 - y1y2 - z1z2
	*/

	qc[0] = qa[3] * qb[0] + qa[0] * qb[3] + qa[1] * qb[2] - qa[2] * qb[1];
	qc[1] = qa[3] * qb[1] + qa[1] * qb[3] + qa[2] * qb[0] - qa[0] * qb[2];
	qc[2] = qa[3] * qb[2] + qa[2] * qb[3] + qa[0] * qb[1] - qa[1] * qb[0];
	qc[3] = qa[3] * qb[3] - qa[0] * qb[0] - qa[1] * qb[1] - qa[2] * qb[2];
}

inline void QuatMultiply2( quat_t qa, const quat_t qb)
{
	quat_t tmp;

	QuatCopy(qa, tmp);
	QuatMultiply(tmp, qb, qa);
}

void QuatSlerp( const quat_t from, const quat_t to, float frac, quat_t out );

inline void QuatTransformVector( const quat_t q, const vec3_t in, vec3_t out )
{
#if 0
	matrix_t m;

	MatrixFromQuat( m, q );
	MatrixTransformNormal( m, in, out );
#else
	vec3_t tmp, tmp2;

	CrossProduct( q, in, tmp );
	VectorScale( tmp, 2.0f, tmp );
	CrossProduct( q, tmp, tmp2 );
	VectorMA( in, q[3], tmp, out );
	VectorAdd( out, tmp2, out );
#endif
}

inline void QuatTransformVectorInverse( const quat_t q, const vec3_t in, vec3_t out )
{
	vec3_t tmp, tmp2;

	// The inverse rotation is obtained by negating the vector
	// component of q, but that is mathematically the same as
	// swapping the arguments of the cross product.
	CrossProduct( in, q, tmp );
	VectorScale( tmp, 2.0f, tmp );
	CrossProduct( tmp, q, tmp2 );
	VectorMA( in, q[3], tmp, out );
	VectorAdd( out, tmp2, out );
}

//=============================================
// combining Transformations

#if idx86_sse
	/* swizzles for _mm_shuffle_ps instruction */
	#define SWZ_XXXX 0x00
	#define SWZ_YXXX 0x01
	#define SWZ_ZXXX 0x02
	#define SWZ_WXXX 0x03
	#define SWZ_XYXX 0x04
	#define SWZ_YYXX 0x05
	#define SWZ_ZYXX 0x06
	#define SWZ_WYXX 0x07
	#define SWZ_XZXX 0x08
	#define SWZ_YZXX 0x09
	#define SWZ_ZZXX 0x0a
	#define SWZ_WZXX 0x0b
	#define SWZ_XWXX 0x0c
	#define SWZ_YWXX 0x0d
	#define SWZ_ZWXX 0x0e
	#define SWZ_WWXX 0x0f
	#define SWZ_XXYX 0x10
	#define SWZ_YXYX 0x11
	#define SWZ_ZXYX 0x12
	#define SWZ_WXYX 0x13
	#define SWZ_XYYX 0x14
	#define SWZ_YYYX 0x15
	#define SWZ_ZYYX 0x16
	#define SWZ_WYYX 0x17
	#define SWZ_XZYX 0x18
	#define SWZ_YZYX 0x19
	#define SWZ_ZZYX 0x1a
	#define SWZ_WZYX 0x1b
	#define SWZ_XWYX 0x1c
	#define SWZ_YWYX 0x1d
	#define SWZ_ZWYX 0x1e
	#define SWZ_WWYX 0x1f
	#define SWZ_XXZX 0x20
	#define SWZ_YXZX 0x21
	#define SWZ_ZXZX 0x22
	#define SWZ_WXZX 0x23
	#define SWZ_XYZX 0x24
	#define SWZ_YYZX 0x25
	#define SWZ_ZYZX 0x26
	#define SWZ_WYZX 0x27
	#define SWZ_XZZX 0x28
	#define SWZ_YZZX 0x29
	#define SWZ_ZZZX 0x2a
	#define SWZ_WZZX 0x2b
	#define SWZ_XWZX 0x2c
	#define SWZ_YWZX 0x2d
	#define SWZ_ZWZX 0x2e
	#define SWZ_WWZX 0x2f
	#define SWZ_XXWX 0x30
	#define SWZ_YXWX 0x31
	#define SWZ_ZXWX 0x32
	#define SWZ_WXWX 0x33
	#define SWZ_XYWX 0x34
	#define SWZ_YYWX 0x35
	#define SWZ_ZYWX 0x36
	#define SWZ_WYWX 0x37
	#define SWZ_XZWX 0x38
	#define SWZ_YZWX 0x39
	#define SWZ_ZZWX 0x3a
	#define SWZ_WZWX 0x3b
	#define SWZ_XWWX 0x3c
	#define SWZ_YWWX 0x3d
	#define SWZ_ZWWX 0x3e
	#define SWZ_WWWX 0x3f
	#define SWZ_XXXY 0x40
	#define SWZ_YXXY 0x41
	#define SWZ_ZXXY 0x42
	#define SWZ_WXXY 0x43
	#define SWZ_XYXY 0x44
	#define SWZ_YYXY 0x45
	#define SWZ_ZYXY 0x46
	#define SWZ_WYXY 0x47
	#define SWZ_XZXY 0x48
	#define SWZ_YZXY 0x49
	#define SWZ_ZZXY 0x4a
	#define SWZ_WZXY 0x4b
	#define SWZ_XWXY 0x4c
	#define SWZ_YWXY 0x4d
	#define SWZ_ZWXY 0x4e
	#define SWZ_WWXY 0x4f
	#define SWZ_XXYY 0x50
	#define SWZ_YXYY 0x51
	#define SWZ_ZXYY 0x52
	#define SWZ_WXYY 0x53
	#define SWZ_XYYY 0x54
	#define SWZ_YYYY 0x55
	#define SWZ_ZYYY 0x56
	#define SWZ_WYYY 0x57
	#define SWZ_XZYY 0x58
	#define SWZ_YZYY 0x59
	#define SWZ_ZZYY 0x5a
	#define SWZ_WZYY 0x5b
	#define SWZ_XWYY 0x5c
	#define SWZ_YWYY 0x5d
	#define SWZ_ZWYY 0x5e
	#define SWZ_WWYY 0x5f
	#define SWZ_XXZY 0x60
	#define SWZ_YXZY 0x61
	#define SWZ_ZXZY 0x62
	#define SWZ_WXZY 0x63
	#define SWZ_XYZY 0x64
	#define SWZ_YYZY 0x65
	#define SWZ_ZYZY 0x66
	#define SWZ_WYZY 0x67
	#define SWZ_XZZY 0x68
	#define SWZ_YZZY 0x69
	#define SWZ_ZZZY 0x6a
	#define SWZ_WZZY 0x6b
	#define SWZ_XWZY 0x6c
	#define SWZ_YWZY 0x6d
	#define SWZ_ZWZY 0x6e
	#define SWZ_WWZY 0x6f
	#define SWZ_XXWY 0x70
	#define SWZ_YXWY 0x71
	#define SWZ_ZXWY 0x72
	#define SWZ_WXWY 0x73
	#define SWZ_XYWY 0x74
	#define SWZ_YYWY 0x75
	#define SWZ_ZYWY 0x76
	#define SWZ_WYWY 0x77
	#define SWZ_XZWY 0x78
	#define SWZ_YZWY 0x79
	#define SWZ_ZZWY 0x7a
	#define SWZ_WZWY 0x7b
	#define SWZ_XWWY 0x7c
	#define SWZ_YWWY 0x7d
	#define SWZ_ZWWY 0x7e
	#define SWZ_WWWY 0x7f
	#define SWZ_XXXZ 0x80
	#define SWZ_YXXZ 0x81
	#define SWZ_ZXXZ 0x82
	#define SWZ_WXXZ 0x83
	#define SWZ_XYXZ 0x84
	#define SWZ_YYXZ 0x85
	#define SWZ_ZYXZ 0x86
	#define SWZ_WYXZ 0x87
	#define SWZ_XZXZ 0x88
	#define SWZ_YZXZ 0x89
	#define SWZ_ZZXZ 0x8a
	#define SWZ_WZXZ 0x8b
	#define SWZ_XWXZ 0x8c
	#define SWZ_YWXZ 0x8d
	#define SWZ_ZWXZ 0x8e
	#define SWZ_WWXZ 0x8f
	#define SWZ_XXYZ 0x90
	#define SWZ_YXYZ 0x91
	#define SWZ_ZXYZ 0x92
	#define SWZ_WXYZ 0x93
	#define SWZ_XYYZ 0x94
	#define SWZ_YYYZ 0x95
	#define SWZ_ZYYZ 0x96
	#define SWZ_WYYZ 0x97
	#define SWZ_XZYZ 0x98
	#define SWZ_YZYZ 0x99
	#define SWZ_ZZYZ 0x9a
	#define SWZ_WZYZ 0x9b
	#define SWZ_XWYZ 0x9c
	#define SWZ_YWYZ 0x9d
	#define SWZ_ZWYZ 0x9e
	#define SWZ_WWYZ 0x9f
	#define SWZ_XXZZ 0xa0
	#define SWZ_YXZZ 0xa1
	#define SWZ_ZXZZ 0xa2
	#define SWZ_WXZZ 0xa3
	#define SWZ_XYZZ 0xa4
	#define SWZ_YYZZ 0xa5
	#define SWZ_ZYZZ 0xa6
	#define SWZ_WYZZ 0xa7
	#define SWZ_XZZZ 0xa8
	#define SWZ_YZZZ 0xa9
	#define SWZ_ZZZZ 0xaa
	#define SWZ_WZZZ 0xab
	#define SWZ_XWZZ 0xac
	#define SWZ_YWZZ 0xad
	#define SWZ_ZWZZ 0xae
	#define SWZ_WWZZ 0xaf
	#define SWZ_XXWZ 0xb0
	#define SWZ_YXWZ 0xb1
	#define SWZ_ZXWZ 0xb2
	#define SWZ_WXWZ 0xb3
	#define SWZ_XYWZ 0xb4
	#define SWZ_YYWZ 0xb5
	#define SWZ_ZYWZ 0xb6
	#define SWZ_WYWZ 0xb7
	#define SWZ_XZWZ 0xb8
	#define SWZ_YZWZ 0xb9
	#define SWZ_ZZWZ 0xba
	#define SWZ_WZWZ 0xbb
	#define SWZ_XWWZ 0xbc
	#define SWZ_YWWZ 0xbd
	#define SWZ_ZWWZ 0xbe
	#define SWZ_WWWZ 0xbf
	#define SWZ_XXXW 0xc0
	#define SWZ_YXXW 0xc1
	#define SWZ_ZXXW 0xc2
	#define SWZ_WXXW 0xc3
	#define SWZ_XYXW 0xc4
	#define SWZ_YYXW 0xc5
	#define SWZ_ZYXW 0xc6
	#define SWZ_WYXW 0xc7
	#define SWZ_XZXW 0xc8
	#define SWZ_YZXW 0xc9
	#define SWZ_ZZXW 0xca
	#define SWZ_WZXW 0xcb
	#define SWZ_XWXW 0xcc
	#define SWZ_YWXW 0xcd
	#define SWZ_ZWXW 0xce
	#define SWZ_WWXW 0xcf
	#define SWZ_XXYW 0xd0
	#define SWZ_YXYW 0xd1
	#define SWZ_ZXYW 0xd2
	#define SWZ_WXYW 0xd3
	#define SWZ_XYYW 0xd4
	#define SWZ_YYYW 0xd5
	#define SWZ_ZYYW 0xd6
	#define SWZ_WYYW 0xd7
	#define SWZ_XZYW 0xd8
	#define SWZ_YZYW 0xd9
	#define SWZ_ZZYW 0xda
	#define SWZ_WZYW 0xdb
	#define SWZ_XWYW 0xdc
	#define SWZ_YWYW 0xdd
	#define SWZ_ZWYW 0xde
	#define SWZ_WWYW 0xdf
	#define SWZ_XXZW 0xe0
	#define SWZ_YXZW 0xe1
	#define SWZ_ZXZW 0xe2
	#define SWZ_WXZW 0xe3
	#define SWZ_XYZW 0xe4
	#define SWZ_YYZW 0xe5
	#define SWZ_ZYZW 0xe6
	#define SWZ_WYZW 0xe7
	#define SWZ_XZZW 0xe8
	#define SWZ_YZZW 0xe9
	#define SWZ_ZZZW 0xea
	#define SWZ_WZZW 0xeb
	#define SWZ_XWZW 0xec
	#define SWZ_YWZW 0xed
	#define SWZ_ZWZW 0xee
	#define SWZ_WWZW 0xef
	#define SWZ_XXWW 0xf0
	#define SWZ_YXWW 0xf1
	#define SWZ_ZXWW 0xf2
	#define SWZ_WXWW 0xf3
	#define SWZ_XYWW 0xf4
	#define SWZ_YYWW 0xf5
	#define SWZ_ZYWW 0xf6
	#define SWZ_WYWW 0xf7
	#define SWZ_XZWW 0xf8
	#define SWZ_YZWW 0xf9
	#define SWZ_ZZWW 0xfa
	#define SWZ_WZWW 0xfb
	#define SWZ_XWWW 0xfc
	#define SWZ_YWWW 0xfd
	#define SWZ_ZWWW 0xfe
	#define SWZ_WWWW 0xff

	#define sseSwizzle( a, mask ) _mm_shuffle_ps( (a), (a), SWZ_##mask )

	inline __m128 unitQuat(){
		return _mm_set_ps( 1.0f, 0.0f, 0.0f, 0.0f ); // order is reversed
	}

	inline __m128 sseLoadInts( const int vec[4] )
	{
		return *(__m128 *)vec;
	}

	inline __m128 mask_0000()
	{
		static const ALIGNED(16, int vec[4]) = { 0, 0, 0, 0 };
		return sseLoadInts( vec );
	}

	inline __m128 mask_000W()
	{
		static const ALIGNED(16, int vec[4]) = { 0, 0, 0, -1 };
		return sseLoadInts( vec );
	}

	inline __m128 mask_XYZ0()
	{
		static const ALIGNED(16, int vec[4]) = { -1, -1, -1, 0 };
		return sseLoadInts( vec );
	}

	inline __m128 sign_000W()
	{
		static const ALIGNED(16, int vec[4]) = { 0, 0, 0, 1<<31 };
		return sseLoadInts( vec );
	}

	inline __m128 sign_XYZ0()
	{
		static const ALIGNED(16, int vec[4]) = { 1<<31, 1<<31, 1<<31, 0 };
		return sseLoadInts( vec );
	}

	inline __m128 sign_XYZW()
	{
		static const ALIGNED(16, int vec[4]) = { 1<<31, 1<<31, 1<<31, 1<<31 };
		return sseLoadInts( vec );
	}

	inline __m128 sseDot4( __m128 a, __m128 b )
	{
		__m128 prod = _mm_mul_ps( a, b );
		__m128 sum1 = _mm_add_ps( prod, sseSwizzle( prod, YXWZ ) );
		__m128 sum2 = _mm_add_ps( sum1, sseSwizzle( sum1, ZWXY ) );
		return sum2;
	}

	inline __m128 sseCrossProduct( __m128 a, __m128 b )
	{
		__m128 a_yzx = sseSwizzle( a, YZXW );
		__m128 b_yzx = sseSwizzle( b, YZXW );
		__m128 c_zxy = _mm_sub_ps(
			_mm_mul_ps( a, b_yzx ),
			_mm_mul_ps( a_yzx, b ) );
		return sseSwizzle( c_zxy, YZXW );
	}

	inline __m128 sseQuatMul( __m128 a, __m128 b )
	{
		__m128 a1 = sseSwizzle( a, WWWW );
		__m128 c1 = _mm_mul_ps( a1, b );
		__m128 a2 = sseSwizzle( a, XYZX );
		__m128 b2 = sseSwizzle( b, WWWX );
		__m128 c2 = _mm_xor_ps( _mm_mul_ps(a2, b2), sign_000W() );
		__m128 a3 = sseSwizzle( a, YZXY );
		__m128 b3 = sseSwizzle( b, ZXYY );
		__m128 c3 = _mm_xor_ps( _mm_mul_ps(a3, b3), sign_000W() );
		__m128 a4 = sseSwizzle( a, ZXYZ);
		__m128 b4 = sseSwizzle( b, YZXZ);
		__m128 c4 = _mm_mul_ps( a4, b4 );
		return _mm_add_ps( _mm_add_ps(c1, c2), _mm_sub_ps(c3, c4) );
	}

	inline __m128 sseQuatNormalize( __m128 q )
	{
		__m128 p = _mm_mul_ps( q, q );
		__m128 t, h;
		p = _mm_add_ps( sseSwizzle( p, XXZZ ),
				sseSwizzle( p, YYWW ) );
		p = _mm_add_ps( sseSwizzle( p, XXXX ),
				sseSwizzle( p, ZZZZ ) );
		t = _mm_rsqrt_ps( p );
		h = _mm_mul_ps( _mm_set1_ps( 0.5f ), t );
		t = _mm_mul_ps( _mm_mul_ps( t, t ), p );
		t = _mm_sub_ps( _mm_set1_ps( 3.0f ), t );
		t = _mm_mul_ps( h, t );
		return _mm_mul_ps( q, t );
	}

	inline __m128 sseQuatTransform( __m128 q, __m128 vec )
	{
		__m128 t, t2;
		t = sseCrossProduct( q, vec );
		t = _mm_add_ps( t, t );
		t2 = sseCrossProduct( q, t );
		t = _mm_mul_ps( sseSwizzle( q, WWWW ), t );
		return _mm_add_ps( _mm_add_ps( vec, t2 ), t );
	}

	inline __m128 sseQuatTransformInverse( __m128 q, __m128 vec )
	{
		__m128 t, t2;
		t = sseCrossProduct( vec, q );
		t = _mm_add_ps( t, t );
		t2 = sseCrossProduct( t, q );
		t = _mm_mul_ps( sseSwizzle( q, WWWW ), t );
		return _mm_add_ps( _mm_add_ps( vec, t2 ), t );
	}

	inline __m128 sseLoadVec3( const vec3_t vec )
	{
		__m128 v = _mm_load_ss( &vec[ 2 ] );
		v = sseSwizzle( v, YYXY );
		v = _mm_loadl_pi( v, (__m64 *)vec );
		return v;
	}

	inline void sseStoreVec3( __m128 in, vec3_t out )
	{
		_mm_storel_pi( (__m64 *)out, in );
		__m128 v = sseSwizzle( in, ZZZZ );
		_mm_store_ss( &out[ 2 ], v );
	}

	inline void TransInit( transform_t *t )
	{
		__m128 u = unitQuat();
		t->sseRot = u;
		t->sseTransScale = u;
	}

	inline void TransCopy( const transform_t *in, transform_t *out )
	{
		out->sseRot = in->sseRot;
		out->sseTransScale = in->sseTransScale;
	}

	inline void TransformPoint( const transform_t *t, const vec3_t in, vec3_t out )
	{
		__m128 ts = t->sseTransScale;
		__m128 tmp = sseQuatTransform( t->sseRot, _mm_loadu_ps( in ) );
		tmp = _mm_mul_ps( tmp, sseSwizzle( ts, WWWW ) );
		tmp = _mm_add_ps( tmp, ts );
		sseStoreVec3( tmp, out );
	}

	inline void TransformPointInverse( const transform_t *t, const vec3_t in, vec3_t out )
	{
		__m128 ts = t->sseTransScale;
		__m128 v = _mm_sub_ps( _mm_loadu_ps( in ), ts );
		v = _mm_mul_ps( v, _mm_rcp_ps( sseSwizzle( ts, WWWW ) ) );
		v = sseQuatTransformInverse( t->sseRot, v );
		sseStoreVec3( v, out );
	}

	inline void TransformNormalVector( const transform_t *t, const vec3_t in, vec3_t out )
	{
		__m128 v = _mm_loadu_ps( in );
		v = sseQuatTransform( t->sseRot, v );
		sseStoreVec3( v, out );
	}

	inline void TransformNormalVectorInverse( const transform_t *t, const vec3_t in, vec3_t out )
	{
		__m128 v = _mm_loadu_ps( in );
		v = sseQuatTransformInverse( t->sseRot, v );
		sseStoreVec3( v, out );
	}

	inline __m128 sseAxisAngleToQuat( const vec3_t axis, float angle )
	{
		__m128 sa = _mm_set1_ps( sin( 0.5f * angle ) );
		__m128 ca = _mm_set1_ps( cos( 0.5f * angle ) );
		__m128 a = _mm_loadu_ps( axis );
		a = _mm_and_ps( a, mask_XYZ0() );
		a = _mm_mul_ps( a, sa );
		return _mm_or_ps( a, _mm_and_ps( ca, mask_000W() ) );
	}

	inline void TransInitRotationQuat( const quat_t quat, transform_t *t )
	{
		t->sseRot = _mm_loadu_ps( quat );
		t->sseTransScale = unitQuat();
	}

	inline void TransInitRotation( const vec3_t axis, float angle, transform_t *t )
	{
		t->sseRot = sseAxisAngleToQuat( axis, angle );
		t->sseTransScale = unitQuat();
	}

	inline void TransInitTranslation( const vec3_t vec, transform_t *t )
	{
		__m128 v = _mm_loadu_ps( vec );
		v = _mm_and_ps( v, mask_XYZ0() );
		t->sseRot = unitQuat();
		t->sseTransScale = _mm_or_ps( v, unitQuat() );
	}

	inline void TransInitScale( float factor, transform_t *t )
	{
		__m128 f = _mm_set1_ps( factor );
		f = _mm_and_ps( f, mask_000W() );
		t->sseRot = unitQuat();
		t->sseTransScale = f;
	}

	inline void TransInsRotationQuat( const quat_t quat, transform_t *t )
	{
		__m128 q = _mm_loadu_ps( quat );
		t->sseRot = sseQuatMul( t->sseRot, q );
	}

	inline void TransInsRotation( const vec3_t axis, float angle, transform_t *t )
	{
		__m128 q = sseAxisAngleToQuat( axis, angle );
		t->sseRot = sseQuatMul( q, t->sseRot );
	}

	inline void TransAddRotationQuat( const quat_t quat, transform_t *t )
	{
		__m128 q = _mm_loadu_ps( quat );
		__m128 transformed = sseQuatTransform( q, t->sseTransScale );
		t->sseRot = sseQuatMul( q, t->sseRot );
		t->sseTransScale = _mm_or_ps(
			_mm_and_ps( transformed, mask_XYZ0() ),
			_mm_and_ps( t->sseTransScale, mask_000W() ) );
	}

	inline void TransAddRotation( const vec3_t axis, float angle, transform_t *t )
	{
		__m128 q = sseAxisAngleToQuat( axis, angle );
		__m128 transformed = sseQuatTransform( q, t->sseTransScale );
		t->sseRot = sseQuatMul( t->sseRot, q );
		t->sseTransScale = _mm_or_ps(
			_mm_and_ps( transformed, mask_XYZ0() ),
			_mm_and_ps( t->sseTransScale, mask_000W() ) );
	}

	inline void TransInsScale( float factor, transform_t *t )
	{
		t->scale *= factor;
	}

	inline void TransAddScale( float factor, transform_t *t )
	{
		__m128 f = _mm_set1_ps( factor );
		t->sseTransScale = _mm_mul_ps( f, t->sseTransScale );
	}

	inline void TransInsTranslation( const vec3_t vec, transform_t *t )
	{
		__m128 v = _mm_loadu_ps( vec );
		__m128 ts = t->sseTransScale;
		v = sseQuatTransform( t->sseRot, v );
		v = _mm_mul_ps( v, sseSwizzle( ts, WWWW ) );
		v = _mm_and_ps( v, mask_XYZ0() );
		t->sseTransScale = _mm_add_ps( ts, v );
	}

	inline void TransAddTranslation( const vec3_t vec, transform_t *t )
	{
		__m128 v = _mm_loadu_ps( vec );
		v = _mm_and_ps( v, mask_XYZ0() );
		t->sseTransScale = _mm_add_ps( t->sseTransScale, v );
	}

	inline void TransCombine( const transform_t *a, const transform_t *b, transform_t *out )
	{
		__m128 aRot = a->sseRot;
		__m128 aTS = a->sseTransScale;
		__m128 bRot = b->sseRot;
		__m128 bTS = b->sseTransScale;
		__m128 tmp = sseQuatTransform( bRot, aTS );
		tmp = _mm_or_ps( _mm_and_ps( tmp, mask_XYZ0() ), _mm_and_ps( aTS, mask_000W() ) );
		tmp = _mm_mul_ps( tmp, sseSwizzle( bTS, WWWW ) );
		out->sseTransScale = _mm_add_ps( tmp, _mm_and_ps( bTS, mask_XYZ0() ) );
		out->sseRot = sseQuatMul( bRot, aRot );
	}

	inline void TransInverse( const transform_t *in, transform_t *out )
	{
		__m128 rot = in->sseRot;
		__m128 ts = in->sseTransScale;
		__m128 invS = _mm_rcp_ps( sseSwizzle( ts, WWWW ) );
		__m128 invRot = _mm_xor_ps( rot, sign_XYZ0() );
		__m128 invT = _mm_xor_ps( ts, sign_XYZ0() );
		__m128 tmp = sseQuatTransform( invRot, invT );
		tmp = _mm_mul_ps( tmp, invS );
		out->sseRot = invRot;
		out->sseTransScale = _mm_or_ps(
			_mm_and_ps( tmp, mask_XYZ0() ),
			_mm_and_ps( invS, mask_000W() ) );
	}

	inline void TransStartLerp( transform_t *t )
	{
		t->sseRot = mask_0000();
		t->sseTransScale = mask_0000();
	}

	inline void TransAddWeight( float weight, const transform_t *a, transform_t *out )
	{
		__m128 w = _mm_set1_ps( weight );
		__m128 d = sseDot4( a->sseRot, out->sseRot );
		out->sseTransScale = _mm_add_ps( out->sseTransScale, _mm_mul_ps( w, a->sseTransScale ) );
		w = _mm_xor_ps( w, _mm_and_ps( d, sign_XYZW() ) );
		out->sseRot = _mm_add_ps( out->sseRot, _mm_mul_ps( w, a->sseRot ) );
	}

	inline void TransEndLerp( transform_t *t )
	{
		t->sseRot = sseQuatNormalize( t->sseRot );
	}
#else // !idx86_sse
	// create an identity transform
	inline void TransInit( transform_t *t )
	{
		QuatClear( t->rot );
		VectorClear( t->trans );
		t->scale = 1.0f;
	}

	// copy a transform
	inline void TransCopy( const transform_t *in, transform_t *out )
	{
		Com_Memcpy( out, in, sizeof( transform_t ) );
	}

	// apply a transform to a point
	inline void TransformPoint( const transform_t *t, const vec3_t in, vec3_t out )
	{
		QuatTransformVector( t->rot, in, out );
		VectorScale( out, t->scale, out );
		VectorAdd( out, t->trans, out );
	}
	// apply the inverse of a transform to a point
	inline void TransformPointInverse( const transform_t *t, const vec3_t in, vec3_t out )
	{
		VectorSubtract( in, t->trans, out );
		VectorScale( out, 1.0f / t->scale, out );
		QuatTransformVectorInverse( t->rot, out, out );
	}

	inline // apply a transform to a normal vector (ignore scale and translation)
	inline void TransformNormalVector( const transform_t *t, const vec3_t in, vec3_t out )
	{
		QuatTransformVector( t->rot, in, out );
	}

	// apply the inverse of a transform to a normal vector (ignore scale
	inline // and translation)
	inline void TransformNormalVectorInverse( const transform_t *t, const vec3_t in, vec3_t out )
	{
		QuatTransformVectorInverse( t->rot, in, out );
	}

	// initialize a transform with a pure rotation
	inline void TransInitRotationQuat( const quat_t quat, transform_t *t )
	{
		QuatCopy( quat, t->rot );
		VectorClear( t->trans );
		t->scale = 1.0f;
	}

	inline void TransInitRotation( const vec3_t axis, float angle, transform_t *t )
	{
		float sa = sin( 0.5f * angle );
		float ca = cos( 0.5f * angle );
		quat_t q;

		VectorScale( axis, sa, q );
		q[3] = ca;
		TransInitRotationQuat( q, t );
	}

	// initialize a transform with a pure translation
	inline void TransInitTranslation( const vec3_t vec, transform_t *t )
	{
		QuatClear( t->rot );
		VectorCopy( vec, t->trans );
		t->scale = 1.0f;
	}

	// initialize a transform with a pure scale
	inline void TransInitScale( float factor, transform_t *t )
	{
		QuatClear( t->rot );
		VectorClear( t->trans );
		t->scale = factor;
	}

	// add a rotation to the start of an existing transform
	inline void TransInsRotationQuat( const quat_t quat, transform_t *t )
	{
		QuatMultiply2( t->rot, quat );
	}

	inline void TransInsRotation( const vec3_t axis, float angle, transform_t *t )
	{
		float sa = sin( 0.5f * angle );
		float ca = cos( 0.5f * angle );
		quat_t q;

		VectorScale( axis, sa, q );
		q[3] = ca;
		TransInsRotationQuat( q, t );
	}

	// add a rotation to the end of an existing transform
	inline void TransAddRotationQuat( const quat_t quat, transform_t *t )
	{
		quat_t tmp;

		QuatTransformVector( quat, t->trans, t->trans );
		QuatCopy( quat, tmp );
		QuatMultiply2( tmp, t->rot );
		QuatCopy( tmp, t->rot );
	}

	inline void TransAddRotation( const vec3_t axis, float angle, transform_t *t )
	{
		float sa = sin( 0.5f * angle );
		float ca = cos( 0.5f * angle );
		quat_t q;

		VectorScale( axis, sa, q );
		q[3] = ca;
		TransAddRotationQuat( q, t );
	}

	// add a scale to the start of an existing transform
	inline void TransInsScale( float factor, transform_t *t )
	{
		t->scale *= factor;
	}

	// add a scale to the end of an existing transform
	inline void TransAddScale( float factor, transform_t *t )
	{
		VectorScale( t->trans, factor, t->trans );
		t->scale *= factor;
	}

	// add a translation at the start of an existing transformation
	inline void TransInsTranslation( const vec3_t vec, transform_t *t )
	{
		vec3_t tmp;

		TransformPoint( t, vec, tmp );
		VectorAdd( t->trans, tmp, t->trans );
	}

	// add a translation at the end of an existing transformation
	inline void TransAddTranslation( const vec3_t vec, transform_t *t )
	{
		VectorAdd( t->trans, vec, t->trans );
	}

	// combine transform a and transform b into transform c
	inline void TransCombine( const transform_t *a, const transform_t *b, transform_t *out )
	{
		TransCopy( a, out );

		TransAddRotationQuat( b->rot, out );
		TransAddScale( b->scale, out );
		TransAddTranslation( b->trans, out );
	}

	// compute the inverse transform
	inline void TransInverse( const transform_t *in, transform_t *out )
	{
		quat_t inverse;
		static transform_t tmp; // static for proper alignment in QVMs

		TransInit( &tmp );
		VectorNegate( in->trans, tmp.trans );
		TransAddScale( 1.0f / in->scale, &tmp );
		QuatCopy( in->rot, inverse );
		QuatInverse( inverse );
		TransAddRotationQuat( inverse, &tmp );
		TransCopy( &tmp, out );
	}

	// lerp between transforms
	inline void TransStartLerp( transform_t *t )
	{
		QuatZero( t->rot );
		VectorClear( t->trans );
		t->scale = 0.0f;
	}

	inline void TransAddWeight( float weight, const transform_t *a, transform_t *out )
	{
		if ( DotProduct4( out->rot, a->rot ) < 0 )
		{
			QuatMA( out->rot, -weight, a->rot, out->rot );
		}

		else
		{
			QuatMA( out->rot, weight, a->rot, out->rot );
		}

		VectorMA( out->trans, weight, a->trans, out->trans );
		out->scale += a->scale * weight;
	}

	inline void TransEndLerp( transform_t *t )
	{
		QuatNormalize( t->rot );
	}
#endif // !idx86_sse

#endif /* Q_MATH_H_ */
