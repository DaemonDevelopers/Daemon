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

// q_math.c -- stateless support routines that are included in each code module

#include "q_math.h"

vec3_t vec3_origin = { 0, 0, 0 };

vec3_t axisDefault[ 3 ] = {
	{ 1, 0, 0 },
	{ 0, 1, 0 },
	{ 0, 0, 1 }
};

matrix_t matrixIdentity = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

quat_t quatIdentity = { 0, 0, 0, 1 };

vec3_t bytedirs[ NUMVERTEXNORMALS ] = {
	{ -0.525731, 0.000000,  0.850651  }, { -0.442863, 0.238856,  0.864188  },
	{ -0.295242, 0.000000,  0.955423  }, { -0.309017, 0.500000,  0.809017  },
	{ -0.162460, 0.262866,  0.951056  }, { 0.000000,  0.000000,  1.000000  },
	{ 0.000000,  0.850651,  0.525731  }, { -0.147621, 0.716567,  0.681718  },
	{ 0.147621,  0.716567,  0.681718  }, { 0.000000,  0.525731,  0.850651  },
	{ 0.309017,  0.500000,  0.809017  }, { 0.525731,  0.000000,  0.850651  },
	{ 0.295242,  0.000000,  0.955423  }, { 0.442863,  0.238856,  0.864188  },
	{ 0.162460,  0.262866,  0.951056  }, { -0.681718, 0.147621,  0.716567  },
	{ -0.809017, 0.309017,  0.500000  }, { -0.587785, 0.425325,  0.688191  },
	{ -0.850651, 0.525731,  0.000000  }, { -0.864188, 0.442863,  0.238856  },
	{ -0.716567, 0.681718,  0.147621  }, { -0.688191, 0.587785,  0.425325  },
	{ -0.500000, 0.809017,  0.309017  }, { -0.238856, 0.864188,  0.442863  },
	{ -0.425325, 0.688191,  0.587785  }, { -0.716567, 0.681718,  -0.147621 },
	{ -0.500000, 0.809017,  -0.309017 }, { -0.525731, 0.850651,  0.000000  },
	{ 0.000000,  0.850651,  -0.525731 }, { -0.238856, 0.864188,  -0.442863 },
	{ 0.000000,  0.955423,  -0.295242 }, { -0.262866, 0.951056,  -0.162460 },
	{ 0.000000,  1.000000,  0.000000  }, { 0.000000,  0.955423,  0.295242  },
	{ -0.262866, 0.951056,  0.162460  }, { 0.238856,  0.864188,  0.442863  },
	{ 0.262866,  0.951056,  0.162460  }, { 0.500000,  0.809017,  0.309017  },
	{ 0.238856,  0.864188,  -0.442863 }, { 0.262866,  0.951056,  -0.162460 },
	{ 0.500000,  0.809017,  -0.309017 }, { 0.850651,  0.525731,  0.000000  },
	{ 0.716567,  0.681718,  0.147621  }, { 0.716567,  0.681718,  -0.147621 },
	{ 0.525731,  0.850651,  0.000000  }, { 0.425325,  0.688191,  0.587785  },
	{ 0.864188,  0.442863,  0.238856  }, { 0.688191,  0.587785,  0.425325  },
	{ 0.809017,  0.309017,  0.500000  }, { 0.681718,  0.147621,  0.716567  },
	{ 0.587785,  0.425325,  0.688191  }, { 0.955423,  0.295242,  0.000000  },
	{ 1.000000,  0.000000,  0.000000  }, { 0.951056,  0.162460,  0.262866  },
	{ 0.850651,  -0.525731, 0.000000  }, { 0.955423,  -0.295242, 0.000000  },
	{ 0.864188,  -0.442863, 0.238856  }, { 0.951056,  -0.162460, 0.262866  },
	{ 0.809017,  -0.309017, 0.500000  }, { 0.681718,  -0.147621, 0.716567  },
	{ 0.850651,  0.000000,  0.525731  }, { 0.864188,  0.442863,  -0.238856 },
	{ 0.809017,  0.309017,  -0.500000 }, { 0.951056,  0.162460,  -0.262866 },
	{ 0.525731,  0.000000,  -0.850651 }, { 0.681718,  0.147621,  -0.716567 },
	{ 0.681718,  -0.147621, -0.716567 }, { 0.850651,  0.000000,  -0.525731 },
	{ 0.809017,  -0.309017, -0.500000 }, { 0.864188,  -0.442863, -0.238856 },
	{ 0.951056,  -0.162460, -0.262866 }, { 0.147621,  0.716567,  -0.681718 },
	{ 0.309017,  0.500000,  -0.809017 }, { 0.425325,  0.688191,  -0.587785 },
	{ 0.442863,  0.238856,  -0.864188 }, { 0.587785,  0.425325,  -0.688191 },
	{ 0.688191,  0.587785,  -0.425325 }, { -0.147621, 0.716567,  -0.681718 },
	{ -0.309017, 0.500000,  -0.809017 }, { 0.000000,  0.525731,  -0.850651 },
	{ -0.525731, 0.000000,  -0.850651 }, { -0.442863, 0.238856,  -0.864188 },
	{ -0.295242, 0.000000,  -0.955423 }, { -0.162460, 0.262866,  -0.951056 },
	{ 0.000000,  0.000000,  -1.000000 }, { 0.295242,  0.000000,  -0.955423 },
	{ 0.162460,  0.262866,  -0.951056 }, { -0.442863, -0.238856, -0.864188 },
	{ -0.309017, -0.500000, -0.809017 }, { -0.162460, -0.262866, -0.951056 },
	{ 0.000000,  -0.850651, -0.525731 }, { -0.147621, -0.716567, -0.681718 },
	{ 0.147621,  -0.716567, -0.681718 }, { 0.000000,  -0.525731, -0.850651 },
	{ 0.309017,  -0.500000, -0.809017 }, { 0.442863,  -0.238856, -0.864188 },
	{ 0.162460,  -0.262866, -0.951056 }, { 0.238856,  -0.864188, -0.442863 },
	{ 0.500000,  -0.809017, -0.309017 }, { 0.425325,  -0.688191, -0.587785 },
	{ 0.716567,  -0.681718, -0.147621 }, { 0.688191,  -0.587785, -0.425325 },
	{ 0.587785,  -0.425325, -0.688191 }, { 0.000000,  -0.955423, -0.295242 },
	{ 0.000000,  -1.000000, 0.000000  }, { 0.262866,  -0.951056, -0.162460 },
	{ 0.000000,  -0.850651, 0.525731  }, { 0.000000,  -0.955423, 0.295242  },
	{ 0.238856,  -0.864188, 0.442863  }, { 0.262866,  -0.951056, 0.162460  },
	{ 0.500000,  -0.809017, 0.309017  }, { 0.716567,  -0.681718, 0.147621  },
	{ 0.525731,  -0.850651, 0.000000  }, { -0.238856, -0.864188, -0.442863 },
	{ -0.500000, -0.809017, -0.309017 }, { -0.262866, -0.951056, -0.162460 },
	{ -0.850651, -0.525731, 0.000000  }, { -0.716567, -0.681718, -0.147621 },
	{ -0.716567, -0.681718, 0.147621  }, { -0.525731, -0.850651, 0.000000  },
	{ -0.500000, -0.809017, 0.309017  }, { -0.238856, -0.864188, 0.442863  },
	{ -0.262866, -0.951056, 0.162460  }, { -0.864188, -0.442863, 0.238856  },
	{ -0.809017, -0.309017, 0.500000  }, { -0.688191, -0.587785, 0.425325  },
	{ -0.681718, -0.147621, 0.716567  }, { -0.442863, -0.238856, 0.864188  },
	{ -0.587785, -0.425325, 0.688191  }, { -0.309017, -0.500000, 0.809017  },
	{ -0.147621, -0.716567, 0.681718  }, { -0.425325, -0.688191, 0.587785  },
	{ -0.162460, -0.262866, 0.951056  }, { 0.442863,  -0.238856, 0.864188  },
	{ 0.162460,  -0.262866, 0.951056  }, { 0.309017,  -0.500000, 0.809017  },
	{ 0.147621,  -0.716567, 0.681718  }, { 0.000000,  -0.525731, 0.850651  },
	{ 0.425325,  -0.688191, 0.587785  }, { 0.587785,  -0.425325, 0.688191  },
	{ 0.688191,  -0.587785, 0.425325  }, { -0.955423, 0.295242,  0.000000  },
	{ -0.951056, 0.162460,  0.262866  }, { -1.000000, 0.000000,  0.000000  },
	{ -0.850651, 0.000000,  0.525731  }, { -0.955423, -0.295242, 0.000000  },
	{ -0.951056, -0.162460, 0.262866  }, { -0.864188, 0.442863,  -0.238856 },
	{ -0.951056, 0.162460,  -0.262866 }, { -0.809017, 0.309017,  -0.500000 },
	{ -0.864188, -0.442863, -0.238856 }, { -0.951056, -0.162460, -0.262866 },
	{ -0.809017, -0.309017, -0.500000 }, { -0.681718, 0.147621,  -0.716567 },
	{ -0.681718, -0.147621, -0.716567 }, { -0.850651, 0.000000,  -0.525731 },
	{ -0.688191, 0.587785,  -0.425325 }, { -0.587785, 0.425325,  -0.688191 },
	{ -0.425325, 0.688191,  -0.587785 }, { -0.425325, -0.688191, -0.587785 },
	{ -0.587785, -0.425325, -0.688191 }, { -0.688191, -0.587785, -0.425325 }
};

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

/*
 * =================
 * PlaneTypeForNormal
 * =================
 */

// perpendicular vector could be replaced by this
// a macro is defined in q_math.h instead

/*
inline int PlaneTypeForNormal( vec3_t normal )
{
	if ( normal[0] == 1.0 )
		return PLANE_X;
	if ( normal[1] == 1.0 )
		return PLANE_Y;
	if ( normal[2] == 1.0 )
		return PLANE_Z;

	return PLANE_NON_AXIAL;
}
*/

/*
 * =================
 * SetPlaneSignbits
 * =================
 */
void SetPlaneSignbits( cplane_t *out )
{
	int bits, j;

	// for fast box on planeside test
	bits = 0;

	for ( j = 0; j < 3; j++ )
	{
		if ( out->normal[ j ] < 0 )
		{
			bits |= 1 << j;
		}
	}

	out->signbits = bits;
}

/*
 * ==================
 * BoxOnPlaneSide
 *
 * Returns 1, 2, or 1 + 2
 * ==================
 */

int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const cplane_t *p )
{
	float dist[ 2 ];
	int   sides, b, i;

	// fast axial cases
	if ( p->type < 3 )
	{
		if ( p->dist <= emins[ p->type ] )
		{
			return 1;
		}

		if ( p->dist >= emaxs[ p->type ] )
		{
			return 2;
		}

		return 3;
	}

	// general case
	dist[ 0 ] = dist[ 1 ] = 0;

	if ( p->signbits < 8 ) // >= 8: default case is original code (dist[0]=dist[1]=0)
	{
		for ( i = 0; i < 3; i++ )
		{
			b = ( p->signbits >> i ) & 1;
			dist[ b ] += p->normal[ i ] * emaxs[ i ];
			dist[ !b ] += p->normal[ i ] * emins[ i ];
		}
	}

	sides = 0;

	if ( dist[ 0 ] >= p->dist )
	{
		sides = 1;
	}

	if ( dist[ 1 ] < p->dist )
	{
		sides |= 2;
	}

	return sides;
}

//============================================================

// this isn't a real cheap function to call!
int DirToByte( vec3_t dir )
{
	int i, best;
	float d, bestd;

	if ( !dir )
	{
		return 0;
	}

	bestd = 0;
	best = 0;

	for ( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		d = DotProduct( dir, bytedirs[ i ] );

		if ( d > bestd )
		{
			bestd = d;
			best = i;
		}
	}

	return best;
}

void vectoangles( const vec3_t value1, vec3_t angles )
{
	float forward;
	float yaw, pitch;

	if ( value1[ 1 ] == 0 && value1[ 0 ] == 0 )
	{
		yaw = 0;

		if ( value1[ 2 ] > 0 )
		{
			pitch = 90;
		}

		else
		{
			pitch = 270;
		}
	}

	else
	{
		if ( value1[ 0 ] )
		{
			yaw = ( atan2( value1[ 1 ], value1[ 0 ] ) * 180 / M_PI );
		}

		else if ( value1[ 1 ] > 0 )
		{
			yaw = 90;
		}

		else
		{
			yaw = 270;
		}

		if ( yaw < 0 )
		{
			yaw += 360;
		}

		forward = sqrt( value1[ 0 ] * value1[ 0 ] + value1[ 1 ] * value1[ 1 ] );
		pitch = ( atan2( value1[ 2 ], forward ) * 180 / M_PI );

		if ( pitch < 0 )
		{
			pitch += 360;
		}
	}

	angles[ PITCH ] = -pitch;
	angles[ YAW ] = yaw;
	angles[ ROLL ] = 0;
}

//============================================================

/*
 * =================
 * PerpendicularVector
 *
 * assumes "src" is normalized
 * =================
 */
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	 * * find the smallest magnitude axially aligned vector
	 */
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( Q_fabs( src[ i ] ) < minelem )
		{
			pos = i;
			minelem = Q_fabs( src[ i ] );
		}
	}

	tempvec[ 0 ] = tempvec[ 1 ] = tempvec[ 2 ] = 0.0F;
	tempvec[ pos ] = 1.0F;

	/*
	 * * project the point onto the plane defined by src
	 */
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	 * * normalize the result
	 */
	VectorNormalize( dst );
}

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up )
{
	float angle;
	static float sr, sp, sy, cr, cp, cy;

	// static to help MS compiler fp bugs

	angle = angles[ YAW ] * ( M_PI * 2 / 360 );
	sy = sin( angle );
	cy = cos( angle );

	angle = angles[ PITCH ] * ( M_PI * 2 / 360 );
	sp = sin( angle );
	cp = cos( angle );

	angle = angles[ ROLL ] * ( M_PI * 2 / 360 );
	sr = sin( angle );
	cr = cos( angle );

	if ( forward )
	{
		forward[ 0 ] = cp * cy;
		forward[ 1 ] = cp * sy;
		forward[ 2 ] = -sp;
	}

	if ( right )
	{
		right[ 0 ] = ( -1 * sr * sp * cy + -1 * cr * -sy );
		right[ 1 ] = ( -1 * sr * sp * sy + -1 * cr * cy );
		right[ 2 ] = -1 * sr * cp;
	}

	if ( up )
	{
		up[ 0 ] = ( cr * sp * cy + -sr * -sy );
		up[ 1 ] = ( cr * sp * sy + -sr * cy );
		up[ 2 ] = cr * cp;
	}
}

// Ridah

/*
 * ================
 * DistanceBetweenLineSegmentsSquared
 * Return the smallest distance between two line segments, squared
 * ================
 */

vec_t DistanceBetweenLineSegmentsSquared( const vec3_t sP0, const vec3_t sP1, const vec3_t tP0, const vec3_t tP1, float *s, float *t )
{
	vec3_t sMag, tMag, diff;
	float a, b, c, d, e;
	float D;
	float sN, sD;
	float tN, tD;
	vec3_t separation;

	VectorSubtract( sP1, sP0, sMag );
	VectorSubtract( tP1, tP0, tMag );
	VectorSubtract( sP0, tP0, diff );
	a = DotProduct( sMag, sMag );
	b = DotProduct( sMag, tMag );
	c = DotProduct( tMag, tMag );
	d = DotProduct( sMag, diff );
	e = DotProduct( tMag, diff );
	sD = tD = D = a * c - b * b;

	if ( D < LINE_DISTANCE_EPSILON )
	{
		// the lines are almost parallel
		sN = 0.0; // force using point P0 on segment S1
		sD = 1.0; // to prevent possible division by 0.0 later
		tN = e;
		tD = c;
	}

	else
	{
		// get the closest points on the infinite lines
		sN = ( b * e - c * d );
		tN = ( a * e - b * d );

		if ( sN < 0.0 )
		{
			// sN < 0 => the s=0 edge is visible
			sN = 0.0;
			tN = e;
			tD = c;
		}

		else if ( sN > sD )
		{
			// sN > sD => the s=1 edge is visible
			sN = sD;
			tN = e + b;
			tD = c;
		}
	}

	if ( tN < 0.0 )
	{
		// tN < 0 => the t=0 edge is visible
		tN = 0.0;

		// recompute sN for this edge
		if ( -d < 0.0 )
		{
			sN = 0.0;
		}

		else if ( -d > a )
		{
			sN = sD;
		}

		else
		{
			sN = -d;
			sD = a;
		}
	}

	else if ( tN > tD )
	{
		// tN > tD => the t=1 edge is visible
		tN = tD;

		// recompute sN for this edge
		if ( ( -d + b ) < 0.0 )
		{
			sN = 0;
		}

		else if ( ( -d + b ) > a )
		{
			sN = sD;
		}

		else
		{
			sN = ( -d + b );
			sD = a;
		}
	}

	// finally do the division to get *s and *t
	*s = ( fabs( sN ) < LINE_DISTANCE_EPSILON ? 0.0 : sN / sD );
	*t = ( fabs( tN ) < LINE_DISTANCE_EPSILON ? 0.0 : tN / tD );

	// get the difference of the two closest points
	VectorScale( sMag, *s, sMag );
	VectorScale( tMag, *t, tMag );
	VectorAdd( diff, sMag, separation );
	VectorSubtract( separation, tMag, separation );

	return VectorLengthSquared( separation );
}

/*
 * ================
 * ProjectPointOntoVectorBounded
 * ================
 */
void ProjectPointOntoVectorBounded( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;
	int j;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );

	// check bounds
	for ( j = 0; j < 3; j++ )
	{
		if ( ( vProj[ j ] > vStart[ j ] && vProj[ j ] > vEnd[ j ] ) || ( vProj[ j ] < vStart[ j ] && vProj[ j ] < vEnd[ j ] ) )
		{
			break;
		}
	}

	if ( j < 3 )
	{
		if ( Q_fabs( vProj[ j ] - vStart[ j ] ) < Q_fabs( vProj[ j ] - vEnd[ j ] ) )
		{
			VectorCopy( vStart, vProj );
		}

		else
		{
			VectorCopy( vEnd, vProj );
		}
	}
}

/*
 * ================
 * DistanceFromLineSquared
 * ================
 */
float DistanceFromLineSquared( vec3_t p, vec3_t lp1, vec3_t lp2 )
{
	vec3_t proj, t;
	int j;

	ProjectPointOntoVector( p, lp1, lp2, proj );

	for ( j = 0; j < 3; j++ )
	{
		if ( ( proj[ j ] > lp1[ j ] && proj[ j ] > lp2[ j ] ) || ( proj[ j ] < lp1[ j ] && proj[ j ] < lp2[ j ] ) )
		{
			break;
		}
	}

	if ( j < 3 )
	{
		if ( Q_fabs( proj[ j ] - lp1[ j ] ) < Q_fabs( proj[ j ] - lp2[ j ] ) )
		{
			VectorSubtract( p, lp1, t );
		}

		else
		{
			VectorSubtract( p, lp2, t );
		}

		return VectorLengthSquared( t );
	}

	VectorSubtract( p, proj, t );
	return VectorLengthSquared( t );
}

/*
 * =================
 * AxisToAngles
 *
 * Used to convert the MD3 tag axis to MDC tag angles, which are much smaller
 *
 * This doesn't have to be fast, since it's only used for conversion in utils, try to avoid
 * using this during gameplay
 * =================
 */
void AxisToAngles( /*const*/ vec3_t axis[ 3 ], vec3_t angles )
{
	float length1;
	float yaw, pitch, roll = 0.0f;

	if ( axis[ 0 ][ 1 ] == 0 && axis[ 0 ][ 0 ] == 0 )
	{
		yaw = 0;

		if ( axis[ 0 ][ 2 ] > 0 )
		{
			pitch = 90;
		}

		else
		{
			pitch = 270;
		}
	}

	else
	{
		if ( axis[ 0 ][ 0 ] )
		{
			yaw = ( atan2( axis[ 0 ][ 1 ], axis[ 0 ][ 0 ] ) * 180 / M_PI );
		}

		else if ( axis[ 0 ][ 1 ] > 0 )
		{
			yaw = 90;
		}

		else
		{
			yaw = 270;
		}

		if ( yaw < 0 )
		{
			yaw += 360;
		}

		length1 = sqrt( axis[ 0 ][ 0 ] * axis[ 0 ][ 0 ] + axis[ 0 ][ 1 ] * axis[ 0 ][ 1 ] );
		pitch = ( atan2( axis[ 0 ][ 2 ], length1 ) * 180 / M_PI );

		if ( pitch < 0 )
		{
			pitch += 360;
		}

		roll = ( atan2( axis[ 1 ][ 2 ], axis[ 2 ][ 2 ] ) * 180 / M_PI );

		if ( roll < 0 )
		{
			roll += 360;
		}
	}

	angles[ PITCH ] = -pitch;
	angles[ YAW ] = yaw;
	angles[ ROLL ] = roll;
}

// done.

// helper functions for MatrixInverse from GtkRadiant C mathlib
inline float m3_det( matrix3x3_t mat )
{
	float det;

	det = mat[ 0 ] * ( mat[ 4 ] * mat[ 8 ] - mat[ 7 ] * mat[ 5 ] )
		- mat[ 1 ] * ( mat[ 3 ] * mat[ 8 ] - mat[ 6 ] * mat[ 5 ] )
		+ mat[ 2 ] * ( mat[ 3 ] * mat[ 7 ] - mat[ 6 ] * mat[ 4 ] );

	return ( det );
}

/*
inline int m3_inverse( matrix3x3_t mr, matrix3x3_t ma )
{
	float det = m3_det( ma );

	if (det == 0 )
	{
		return 1;
	}

	mr[0] =    ma[4]*ma[8] - ma[5]*ma[7]   / det;
	mr[1] = -( ma[1]*ma[8] - ma[7]*ma[2] ) / det;
	mr[2] =    ma[1]*ma[5] - ma[4]*ma[2]   / det;

	mr[3] = -( ma[3]*ma[8] - ma[5]*ma[6] ) / det;
	mr[4] =    ma[0]*ma[8] - ma[6]*ma[2]   / det;
	mr[5] = -( ma[0]*ma[5] - ma[3]*ma[2] ) / det;

	mr[6] =    ma[3]*ma[7] - ma[6]*ma[4]   / det;
	mr[7] = -( ma[0]*ma[7] - ma[6]*ma[1] ) / det;
	mr[8] =    ma[0]*ma[4] - ma[1]*ma[3]   / det;

	return 0;
}
*/

static void m4_submat( matrix_t mr, matrix3x3_t mb, int i, int j )
{
	int ti, tj, idst = 0, jdst = 0;

	for ( ti = 0; ti < 4; ti++ )
	{
		if ( ti < i )
		{
			idst = ti;
		}

		else if ( ti > i )
		{
			idst = ti - 1;
		}

		for ( tj = 0; tj < 4; tj++ )
		{
			if ( tj < j )
			{
				jdst = tj;
			}

			else if ( tj > j )
			{
				jdst = tj - 1;
			}

			if ( ti != i && tj != j )
			{
				mb[ idst * 3 + jdst ] = mr[ ti * 4 + tj ];
			}
		}
	}
}

static float m4_det( matrix_t mr )
{
	float det, result = 0, i = 1;
	matrix3x3_t msub3;
	int n;

	for ( n = 0; n < 4; n++, i *= -1 )
	{
		m4_submat( mr, msub3, 0, n );

		det = m3_det( msub3 );
		result += mr[ n ] * det * i;
	}

	return result;
}

// invert any m4x4 using Kramer's rule.. return true if matrix is singular, else return false
bool MatrixInverse( matrix_t matrix )
{
	float mdet = m4_det( matrix );
	matrix3x3_t mtemp;
	int i, j, sign;
	matrix_t m4x4_temp;

#if 0

	if ( fabs( mdet ) < 0.0000000001 )
	{
		return true;
	}

#endif

	MatrixCopy( matrix, m4x4_temp );

	for ( i = 0; i < 4; i++ )
	{
		for ( j = 0; j < 4; j++ )
		{
			sign = 1 - ( ( i + j ) % 2 ) * 2;

			m4_submat( m4x4_temp, mtemp, i, j );

			// FIXME: try using * inverse det and see if speed/accuracy are good enough
			matrix[ i + j * 4 ] = ( m3_det( mtemp ) * sign ) / mdet;
		}
	}

	return false;
}

void MatrixToAngles( const matrix_t m, vec3_t angles )
{
#if 1
	float theta;
	float cp;
	float sp;

	sp = m[ 2 ];

	// cap off our sin value so that we don't get any NANs
	if ( sp > 1.0 )
	{
		sp = 1.0;
	}

	else if ( sp < -1.0 )
	{
		sp = -1.0;
	}

	theta = -asin( sp );
	cp = cos( theta );

	if ( cp > 8192 * FLT_EPSILON )
	{
		angles[ PITCH ] = RAD2DEG( theta );
		angles[ YAW ] = RAD2DEG( atan2( m[ 1 ], m[ 0 ] ) );
		angles[ ROLL ] = RAD2DEG( atan2( m[ 6 ], m[ 10 ] ) );
	}

	else
	{
		angles[ PITCH ] = RAD2DEG( theta );
		angles[ YAW ] = RAD2DEG( -atan2( m[ 4 ], m[ 5 ] ) );
		angles[ ROLL ] = 0;
	}

#else
	float a;
	float ca;

	a = asin( -m[ 2 ] );
	ca = cos( a );

	if ( fabs( ca ) > 0.005 ) // Gimbal lock?
	{
		angles[ PITCH ] = RAD2DEG( atan2( m[ 6 ] / ca, m[ 10 ] / ca ) );
		angles[ YAW ] = RAD2DEG( a );
		angles[ ROLL ] = RAD2DEG( atan2( m[ 1 ] / ca, m[ 0 ] / ca ) );
	}

	else
	{
		// Gimbal lock has occurred
		angles[ PITCH ] = RAD2DEG( atan2( -m[ 9 ], m[ 5 ] ) );
		angles[ YAW ] = RAD2DEG( a );
		angles[ ROLL ] = 0;
	}

#endif
}

void MatrixFromQuat( matrix_t m, const quat_t q )
{
#if 1

	/*
	 *	From Quaternion to Matrix and Back
	 *	February 27th 2005
	 *	J.M.P. van Waveren
	 *
	 *	http://www.intel.com/cd/ids/developer/asmo-na/eng/293748.htm
	 */
	float x2, y2, z2 /*, w2*/;
	float yy2, xy2;
	float xz2, yz2, zz2;
	float wz2, wy2, wx2, xx2;

	x2 = q[ 0 ] + q[ 0 ];
	y2 = q[ 1 ] + q[ 1 ];
	z2 = q[ 2 ] + q[ 2 ];
	//w2 = q[3] + q[3]; //Is this used for some underlying optimization?

	yy2 = q[ 1 ] * y2;
	xy2 = q[ 0 ] * y2;

	xz2 = q[ 0 ] * z2;
	yz2 = q[ 1 ] * z2;
	zz2 = q[ 2 ] * z2;

	wz2 = q[ 3 ] * z2;
	wy2 = q[ 3 ] * y2;
	wx2 = q[ 3 ] * x2;
	xx2 = q[ 0 ] * x2;

	m[ 0 ] = -yy2 - zz2 + 1.0f;
	m[ 1 ] = xy2 + wz2;
	m[ 2 ] = xz2 - wy2;

	m[ 4 ] = xy2 - wz2;
	m[ 5 ] = -xx2 - zz2 + 1.0f;
	m[ 6 ] = yz2 + wx2;

	m[ 8 ] = xz2 + wy2;
	m[ 9 ] = yz2 - wx2;
	m[ 10 ] = -xx2 - yy2 + 1.0f;

	m[ 3 ] = m[ 7 ] = m[ 11 ] = m[ 12 ] = m[ 13 ] = m[ 14 ] = 0;
	m[ 15 ] = 1;

#else

	/*
	 *	http://www.gamedev.net/reference/articles/article1691.asp#Q54
	 *	Q54. How do I convert a quaternion to a rotation matrix?
	 *
	 *	Assuming that a quaternion has been created in the form:
	 *
	 *	Q = |X Y Z W|
	 *
	 *	Then the quaternion can then be converted into a 4x4 rotation
	 *	matrix using the following expression (Warning: you might have to
	 *	transpose this matrix if you (do not) follow the OpenGL order!):
	 *
	 *	 ?        2     2                                      ?
	 *	 ? 1 - (2Y  + 2Z )   2XY - 2ZW         2XZ + 2YW       ?
	 *	 ?                                                     ?
	 *	 ?                          2     2                    ?
	 *	M = ? 2XY + 2ZW         1 - (2X  + 2Z )   2YZ - 2XW       ?
	 *	 ?                                                     ?
	 *	 ?                                            2     2  ?
	 *	 ? 2XZ - 2YW         2YZ + 2XW         1 - (2X  + 2Y ) ?
	 *	 ?                                                     ?
	 */

	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm

	float xx, xy, xz, xw, yy, yz, yw, zz, zw;

	xx = q[ 0 ] * q[ 0 ];
	xy = q[ 0 ] * q[ 1 ];
	xz = q[ 0 ] * q[ 2 ];
	xw = q[ 0 ] * q[ 3 ];
	yy = q[ 1 ] * q[ 1 ];
	yz = q[ 1 ] * q[ 2 ];
	yw = q[ 1 ] * q[ 3 ];
	zz = q[ 2 ] * q[ 2 ];
	zw = q[ 2 ] * q[ 3 ];

	m[ 0 ] = 1 - 2 * ( yy + zz );
	m[ 1 ] = 2 * ( xy + zw );
	m[ 2 ] = 2 * ( xz - yw );
	m[ 4 ] = 2 * ( xy - zw );
	m[ 5 ] = 1 - 2 * ( xx + zz );
	m[ 6 ] = 2 * ( yz + xw );
	m[ 8 ] = 2 * ( xz + yw );
	m[ 9 ] = 2 * ( yz - xw );
	m[ 10 ] = 1 - 2 * ( xx + yy );

	m[ 3 ] = m[ 7 ] = m[ 11 ] = m[ 12 ] = m[ 13 ] = m[ 14 ] = 0;
	m[ 15 ] = 1;
#endif
}

/*
* =================
* MatrixTransformBounds
*
* Achieves the same result as:
*
*   BoundsClear(omins, omaxs);
*	for each corner c in bounds{imins, imaxs}
*   {
*	    vec3_t p;
*		MatrixTransformPoint(m, c, p);
*       AddPointToBounds(p, omins, omaxs);
*   }
*
* With fewer operations
*
* Pseudocode:
*	omins = min(mins.x*m.c1, maxs.x*m.c1) + min(mins.y*m.c2, maxs.y*m.c2) + min(mins.z*m.c3, maxs.z*m.c3) + c4
*	omaxs = max(mins.x*m.c1, maxs.x*m.c1) + max(mins.y*m.c2, maxs.y*m.c2) + max(mins.z*m.c3, maxs.z*m.c3) + c4
* =================
*/
void MatrixTransformBounds(const matrix_t m, const vec3_t mins, const vec3_t maxs, vec3_t omins, vec3_t omaxs)
{
	vec3_t minx, maxx;
	vec3_t miny, maxy;
	vec3_t minz, maxz;

	const float* c1 = m;
	const float* c2 = m + 4;
	const float* c3 = m + 8;
	const float* c4 = m + 12;

	VectorScale(c1, mins[0], minx);
	VectorScale(c1, maxs[0], maxx);

	VectorScale(c2, mins[1], miny);
	VectorScale(c2, maxs[1], maxy);

	VectorScale(c3, mins[2], minz);
	VectorScale(c3, maxs[2], maxz);

	vec3_t tmins, tmaxs;
	vec3_t tmp;

	VectorMin(minx, maxx, tmins);
	VectorMax(minx, maxx, tmaxs);
	VectorAdd(tmins, c4, tmins);
	VectorAdd(tmaxs, c4, tmaxs);

	VectorMin(miny, maxy, tmp);
	VectorAdd(tmp, tmins, tmins);

	VectorMax(miny, maxy, tmp);
	VectorAdd(tmp, tmaxs, tmaxs);

	VectorMin(minz, maxz, tmp);
	VectorAdd(tmp, tmins, omins);

	VectorMax(minz, maxz, tmp);
	VectorAdd(tmp, tmaxs, omaxs);
}


//=============================================

// RB: XreaL quaternion math functions

void QuatFromMatrix( quat_t q, const matrix_t m )
{
#if 1

	/*
	 * From Quaternion to Matrix and Back
	 * February 27th 2005
	 * J.M.P. van Waveren
	 *
	 * http://www.intel.com/cd/ids/developer/asmo-na/eng/293748.htm
	 */
	float t, s;

	if ( m[ 0 ] + m[ 5 ] + m[ 10 ] > 0.0f )
	{
		t = m[ 0 ] + m[ 5 ] + m[ 10 ] + 1.0f;
		s = ( 1.0f / sqrtf( t ) ) * 0.5f;

		q[ 3 ] = s * t;
		q[ 2 ] = ( m[ 1 ] - m[ 4 ] ) * s;
		q[ 1 ] = ( m[ 8 ] - m[ 2 ] ) * s;
		q[ 0 ] = ( m[ 6 ] - m[ 9 ] ) * s;
	}

	else if ( m[ 0 ] > m[ 5 ] && m[ 0 ] > m[ 10 ] )
	{
		t = m[ 0 ] - m[ 5 ] - m[ 10 ] + 1.0f;
		s = ( 1.0f / sqrtf( t ) ) * 0.5f;

		q[ 0 ] = s * t;
		q[ 1 ] = ( m[ 1 ] + m[ 4 ] ) * s;
		q[ 2 ] = ( m[ 8 ] + m[ 2 ] ) * s;
		q[ 3 ] = ( m[ 6 ] - m[ 9 ] ) * s;
	}

	else if ( m[ 5 ] > m[ 10 ] )
	{
		t = -m[ 0 ] + m[ 5 ] - m[ 10 ] + 1.0f;
		s = ( 1.0f / sqrtf( t ) ) * 0.5f;

		q[ 1 ] = s * t;
		q[ 0 ] = ( m[ 1 ] + m[ 4 ] ) * s;
		q[ 3 ] = ( m[ 8 ] - m[ 2 ] ) * s;
		q[ 2 ] = ( m[ 6 ] + m[ 9 ] ) * s;
	}

	else
	{
		t = -m[ 0 ] - m[ 5 ] + m[ 10 ] + 1.0f;
		s = ( 1.0f / sqrtf( t ) ) * 0.5f;

		q[ 2 ] = s * t;
		q[ 3 ] = ( m[ 1 ] - m[ 4 ] ) * s;
		q[ 0 ] = ( m[ 8 ] + m[ 2 ] ) * s;
		q[ 1 ] = ( m[ 6 ] + m[ 9 ] ) * s;
	}

#else
	float trace;

	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/index.htm

	trace = 1.0f + m[ 0 ] + m[ 5 ] + m[ 10 ];

	if ( trace > 0.0f )
	{
		vec_t s = 0.5f / sqrt( trace );

		q[ 0 ] = ( m[ 6 ] - m[ 9 ] ) * s;
		q[ 1 ] = ( m[ 8 ] - m[ 2 ] ) * s;
		q[ 2 ] = ( m[ 1 ] - m[ 4 ] ) * s;
		q[ 3 ] = 0.25f / s;
	}

	else
	{
		if ( m[ 0 ] > m[ 5 ] && m[ 0 ] > m[ 10 ] )
		{
			// column 0
			float s = sqrt( 1.0f + m[ 0 ] - m[ 5 ] - m[ 10 ] ) * 2.0f;

			q[ 0 ] = 0.25f * s;
			q[ 1 ] = ( m[ 4 ] + m[ 1 ] ) / s;
			q[ 2 ] = ( m[ 8 ] + m[ 2 ] ) / s;
			q[ 3 ] = ( m[ 9 ] - m[ 6 ] ) / s;
		}

		else if ( m[ 5 ] > m[ 10 ] )
		{
			// column 1
			float s = sqrt( 1.0f + m[ 5 ] - m[ 0 ] - m[ 10 ] ) * 2.0f;

			q[ 0 ] = ( m[ 4 ] + m[ 1 ] ) / s;
			q[ 1 ] = 0.25f * s;
			q[ 2 ] = ( m[ 9 ] + m[ 6 ] ) / s;
			q[ 3 ] = ( m[ 8 ] - m[ 2 ] ) / s;
		}

		else
		{
			// column 2
			float s = sqrt( 1.0f + m[ 10 ] - m[ 0 ] - m[ 5 ] ) * 2.0f;

			q[ 0 ] = ( m[ 8 ] + m[ 2 ] ) / s;
			q[ 1 ] = ( m[ 9 ] + m[ 6 ] ) / s;
			q[ 2 ] = 0.25f * s;
			q[ 3 ] = ( m[ 4 ] - m[ 1 ] ) / s;
		}
	}

	QuatNormalize( q );
#endif
}

// Tait-Bryan angles z-y-x
void QuatToAngles( const quat_t q, vec3_t angles )
{
	quat_t q2;
	q2[0] = q[0] * q[0];
	q2[1] = q[1] * q[1];
	q2[2] = q[2] * q[2];
	q2[3] = q[3] * q[3];

	// Technical Concepts Orientation, Rotation, Velocity and Acceleration, and the SRM
	// http://www.sedris.org/wg8home/Documents/WG80485.pdf

	// test for gimbal lock
	// 0.499 and -0.499 correspond to about 87.44 degrees, this can be set closer to 90 if necessary, but some inaccuracy is required 
	float unit = q2[0] + q2[1] + q2[2] + q2[3];
	float test = (q[3] * q[1] - q[2] * q[0])/unit; // divide gives result equivalent to normalized quaternion without a sqrt

	if ( test > 0.4995 )
	{
		angles[YAW] = RAD2DEG(-2 * atan2(q[0], q[3]));
		angles[PITCH] = 90;
		angles[ROLL] = 0;
		return;
	}

	if ( test < -0.4995 )
	{
		angles[YAW] = RAD2DEG(2 * atan2(q[0], q[3]));
		angles[PITCH] = -90;
		angles[ROLL] = 0;
		return;
	}

	// original for normalized quaternions:
	// angles[PITCH] = RAD2DEG(asin( 2.0f * (q[3] * q[1] - q[2] * q[0])));
	// angles[YAW]   = RAD2DEG(atan2(2.0f * (q[3] * q[2] + q[0] * q[1]), 1.0f - 2.0f * (q2[1] + q2[2])));
	// angles[ROLL]  = RAD2DEG(atan2(2.0f * (q[3] * q[0] + q[1] * q[2]), 1.0f - 2.0f * (q2[0] + q2[1])));

	// optimized to work with both normalized and unnormalized quaternions:
	angles[PITCH] = RAD2DEG(asin(2.0f * test));
	angles[YAW]   = RAD2DEG(atan2(2.0f * (q[3] * q[2] + q[0] * q[1]), q2[0] - q2[1] - q2[2] + q2[3]));
	angles[ROLL]  = RAD2DEG(atan2(2.0f * (q[3] * q[0] + q[1] * q[2]), -q2[0] - q2[1] + q2[2] + q2[3]));
}

void QuatSlerp( const quat_t from, const quat_t to, float frac, quat_t out )
{
#if 0
	quat_t to1;
	float omega, cosom, sinom, scale0, scale1;

	cosom = from[ 0 ] * to[ 0 ] + from[ 1 ] * to[ 1 ] + from[ 2 ] * to[ 2 ] + from[ 3 ] * to[ 3 ];

	if ( cosom < 0.0 )
	{
		cosom = -cosom;

		QuatCopy( to, to1 );
		QuatAntipodal( to1 );
	}

	else
	{
		QuatCopy( to, to1 );
	}

	if ( ( 1.0 - cosom ) > 0 )
	{
		omega = acos( cosom );
		sinom = sin( omega );
		scale0 = sin( ( 1.0 - frac ) * omega ) / sinom;
		scale1 = sin( frac * omega ) / sinom;
	}

	else
	{
		scale0 = 1.0 - frac;
		scale1 = frac;
	}

	out[ 0 ] = scale0 * from[ 0 ] + scale1 * to1[ 0 ];
	out[ 1 ] = scale0 * from[ 1 ] + scale1 * to1[ 1 ];
	out[ 2 ] = scale0 * from[ 2 ] + scale1 * to1[ 2 ];
	out[ 3 ] = scale0 * from[ 3 ] + scale1 * to1[ 3 ];
#else

	/*
	 * Slerping Clock Cycles
	 * February 27th 2005
	 * J.M.P. van Waveren
	 *
	 * http://www.intel.com/cd/ids/developer/asmo-na/eng/293747.htm
	 */
	float cosom, absCosom, sinom, sinSqr, omega, scale0, scale1;

	if ( frac <= 0.0f )
	{
		QuatCopy( from, out );
		return;
	}

	if ( frac >= 1.0f )
	{
		QuatCopy( to, out );
		return;
	}

	if ( QuatCompare( from, to ) )
	{
		QuatCopy( from, out );
		return;
	}

	cosom = from[ 0 ] * to[ 0 ] + from[ 1 ] * to[ 1 ] + from[ 2 ] * to[ 2 ] + from[ 3 ] * to[ 3 ];
	absCosom = fabs( cosom );

	if ( ( 1.0f - absCosom ) > 1e-6f )
	{
		sinSqr = 1.0f - absCosom * absCosom;
		sinom = 1.0f / sqrt( sinSqr );
		omega = atan2( sinSqr * sinom, absCosom );

		scale0 = sin( ( 1.0f - frac ) * omega ) * sinom;
		scale1 = sin( frac * omega ) * sinom;
	}

	else
	{
		scale0 = 1.0f - frac;
		scale1 = frac;
	}

	scale1 = ( cosom >= 0.0f ) ? scale1 : -scale1;

	out[ 0 ] = scale0 * from[ 0 ] + scale1 * to[ 0 ];
	out[ 1 ] = scale0 * from[ 1 ] + scale1 * to[ 1 ];
	out[ 2 ] = scale0 * from[ 2 ] + scale1 * to[ 2 ];
	out[ 3 ] = scale0 * from[ 3 ] + scale1 * to[ 3 ];
#endif
}
