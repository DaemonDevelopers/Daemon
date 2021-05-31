/*
===========================================================================
Copyright (C) 2009-2011 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// computeLight_fp.glsl - Light computing helper functions

#if defined(USE_REFLECTIVE_SPECULAR)
uniform samplerCube u_EnvironmentMap0;
uniform samplerCube u_EnvironmentMap1;
uniform float u_EnvironmentInterpolation;
#endif // USE_REFLECTIVE_SPECULAR

#ifdef HAVE_ARB_uniform_buffer_object
struct light {
  vec4  center_radius;
  vec4  color_type;
  vec4  direction_angle;
};

layout(std140) uniform u_Lights {
  light lights[ MAX_REF_LIGHTS ];
};
#define GetLight(idx, component) lights[idx].component
#else // !HAVE_ARB_uniform_buffer_object
uniform sampler2D u_Lights;
#define idxToTC( idx, w, h ) vec2( floor( ( idx * ( 1.0 / w ) ) + 0.5 ) * ( 1.0 / h ), \
				   fract( ( idx + 0.5 ) * (1.0 / w ) ) )
const struct GetLightOffsets {
  int center_radius;
  int color_type;
  int direction_angle;
} getLightOffsets = GetLightOffsets(0, 1, 2);
#define GetLight(idx, component) texture2D( u_Lights, idxToTC(3 * idx + getLightOffsets.component, 64.0, float( 3 * MAX_REF_LIGHTS / 64 ) ) )
#endif // HAVE_ARB_uniform_buffer_object

uniform int u_numLights;

uniform vec2 u_SpecularExponent;

// lighting helper functions

void ReadLightGrid(in vec4 texel, out vec3 ambientColor, out vec3 lightColor) {
	float ambientScale = 2.0 * texel.a;
	float directedScale = 2.0 - ambientScale;
	ambientColor = ambientScale * texel.rgb;
	lightColor = directedScale * texel.rgb;
}

void computeLight( vec3 lightDir, vec3 normal, vec3 viewDir, vec3 lightColor,
		   vec4 diffuseColor, vec4 materialColor,
		   inout vec4 color ) {
  vec3 H = normalize( lightDir + viewDir );

#if defined(USE_PHYSICAL_MAPPING) || defined(r_specularMapping)
  float NdotH = clamp( dot( normal, H ), 0.0, 1.0 );
#endif // USE_PHYSICAL_MAPPING || r_specularMapping

#if defined(USE_PHYSICAL_MAPPING)
  // Daemon PBR packing defaults to ORM like glTF 2.0 defines
  // https://www.khronos.org/blog/art-pipeline-for-gltf
  // > ORM texture for Occlusion, Roughness, and Metallic
  // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/schema/material.pbrMetallicRoughness.schema.json
  // > The metalness values are sampled from the B channel. The roughness values are sampled from the G channel.
  // > These values are linear. If other channels are present (R or A), they are ignored for metallic-roughness calculations.
  // https://docs.blender.org/manual/en/2.80/addons/io_scene_gltf2.html
  // > glTF stores occlusion in the red (R) channel, allowing it to optionally share the same image
  // > with the roughness and metallic channels.
  float roughness = materialColor.g;
  float metalness = materialColor.b;

  float NdotV = clamp( dot( normal, viewDir ), 0.0, 1.0);
  float VdotH = clamp( dot( viewDir, H ), 0.0, 1.0);
  float NdotL = clamp( dot( normal, lightDir ), 0.0, 1.0 );

  float alpha = roughness * roughness;
  float k = 0.125 * (roughness + 1.0) * (roughness + 1.0);

  float D = alpha / ((NdotH * NdotH) * (alpha * alpha - 1.0) + 1.0);
  D *= D;

  float FexpNH = pow(1.0 - NdotH, 5.0);
  float FexpNV = pow(1.0 - NdotV, 5.0);
  vec3 F = mix(vec3(0.04), diffuseColor.rgb, metalness);
  F = F + (1.0 - F) * FexpNH;

  float G = NdotL / (NdotL * (1.0 - k) + k);
  G *= NdotV / (NdotV * (1.0 - k) + k);

  color.rgb += lightColor.rgb * (1.0 - metalness) * NdotL * diffuseColor.rgb;
  color.rgb += lightColor.rgb * vec3((D * F * G) / (4.0 * NdotV));
  color.a = mix(diffuseColor.a, 1.0, FexpNV);
#else // !USE_PHYSICAL_MAPPING
  float NdotL = dot( normal, lightDir );
#if defined(r_halfLambertLighting)
  // http://developer.valvesoftware.com/wiki/Half_Lambert
  NdotL = NdotL * 0.5 + 0.5;
  NdotL *= NdotL;
#elif defined(r_wrapAroundLighting)
  NdotL = clamp( NdotL + r_wrapAroundLighting, 0.0, 1.0) / clamp(1.0 + r_wrapAroundLighting, 0.0, 1.0);
#else
  NdotL = clamp( NdotL, 0.0, 1.0 );
#endif

#if defined(USE_REFLECTIVE_SPECULAR)
	// not implemented for PBR yet
	vec4 envColor0 = textureCube(u_EnvironmentMap0, reflect(-viewDir, normal));
	vec4 envColor1 = textureCube(u_EnvironmentMap1, reflect(-viewDir, normal));

	materialColor.rgb *= mix(envColor0, envColor1, u_EnvironmentInterpolation).rgb;
#endif // USE_REFLECTIVE_SPECULAR

  color.rgb += diffuseColor.rgb * lightColor.rgb * NdotL;
#if defined(r_specularMapping)
  color.rgb += materialColor.rgb * lightColor.rgb * pow( NdotH, u_SpecularExponent.x * materialColor.a + u_SpecularExponent.y) * r_SpecularScale;
#endif // r_specularMapping
#endif // !USE_PHYSICAL_MAPPING
}

#if defined(TEXTURE_INTEGER)
const int lightsPerLayer = 16;
uniform usampler3D u_LightTiles;
#define idxs_t uvec4
idxs_t fetchIdxs( in vec3 coords ) {
  return texture3D( u_LightTiles, coords );
}
int nextIdx( inout idxs_t idxs ) {
  uvec4 tmp = ( idxs & uvec4( 3 ) ) * uvec4( 0x40, 0x10, 0x04, 0x01 );
  idxs = idxs >> 2;
  return int( tmp.x + tmp.y + tmp.z + tmp.w );
}
#else // !TEXTURE INTEGER
const int lightsPerLayer = 4;
uniform sampler3D u_LightTiles;
#define idxs_t vec4
idxs_t fetchIdxs( in vec3 coords ) {
  return texture3D( u_LightTiles, coords ) * 255.0;
}
int nextIdx( inout idxs_t idxs ) {
  vec4 tmp = idxs;
  idxs = floor(idxs * 0.25);
  tmp -= 4.0 * idxs;
  return int( dot( tmp, vec4( 64.0, 16.0, 4.0, 1.0 ) ) );
}
#endif // TEXTURE_INTEGER

const int numLayers = MAX_REF_LIGHTS / 256;

#if defined(r_dynamicLight)
void computeDLight( int idx, vec3 P, vec3 normal, vec3 viewDir, vec4 diffuse,
		    vec4 material, inout vec4 color ) {
  vec4 center_radius = GetLight( idx, center_radius );
  vec4 color_type = GetLight( idx, color_type );
  vec3 L;
  float attenuation;

  if( color_type.w == 0.0 ) {
    // point light
    L = center_radius.xyz - P;
    attenuation = 1.0 / (1.0 + 8.0 * length(L) / center_radius.w);
    L = normalize(L);
  } else if( color_type.w == 1.0 ) {
    // spot light
    vec4 direction_angle = GetLight( idx, direction_angle );
    L = center_radius.xyz - P;
    attenuation = 1.0 / (1.0 + 8.0 * length(L) / center_radius.w);
    L = normalize( L );

    if( dot( L, direction_angle.xyz ) <= direction_angle.w ) {
      attenuation = 0.0;
    }
  } else if( color_type.w == 2.0 ) {
    // sun (directional) light
    L = GetLight( idx, direction_angle ).xyz;
    attenuation = 1.0;
  }
  computeLight( L, normal, viewDir,
		attenuation * attenuation * color_type.xyz,
		diffuse, material, color );
}

void computeDLights( vec3 P, vec3 normal, vec3 viewDir, vec4 diffuse, vec4 material,
		     inout vec4 color ) {
  vec2 tile = floor( gl_FragCoord.xy * (1.0 / float( TILE_SIZE ) ) ) + 0.5;
  vec3 tileScale = vec3( r_tileStep, 1.0/numLayers );

#if defined(r_showLightTiles)
  float numLights = 0.0;
#endif

  for( int layer = 0; layer < numLayers; layer++ ) {
    idxs_t idxs = fetchIdxs( tileScale * vec3( tile, float( layer ) + 0.5 ) );
    for( int i = 0; i < lightsPerLayer; i++ ) {
      int idx = numLayers * nextIdx( idxs ) + layer;

      if( idx >= u_numLights )
      {
#if defined(r_showLightTiles)
        if (numLights > 0.0)
        {
          color.rgb = vec3(numLights/(lightsPerLayer*numLayers));
        }
#endif
        return;
      }

      computeDLight( idx, P, normal, viewDir, diffuse, material, color );

#if defined(r_showLightTiles)
      numLights++;
#endif
    }
  }
  
#if defined(r_showLightTiles)
  if (numLights > 0.0)
  {
    color.rgb = vec3(numLights/(lightsPerLayer*numLayers));
  }
#endif
}
#endif
