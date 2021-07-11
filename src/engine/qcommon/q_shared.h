/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

#ifndef Q_SHARED_H_
#define Q_SHARED_H_

#include "common/Defs.h"

// q_shared.h -- included first by ALL program modules.
// A user mod should never modify this file

#define ENGINE_NAME             "Daemon Engine"
#define ENGINE_VERSION          PRODUCT_VERSION

#ifdef REVISION
# define Q3_VERSION             PRODUCT_NAME " " PRODUCT_VERSION " " REVISION
#else
# define Q3_VERSION             PRODUCT_NAME " " PRODUCT_VERSION
#endif

#define Q3_ENGINE               ENGINE_NAME " " ENGINE_VERSION
#define Q3_ENGINE_DATE          __DATE__

#define CLIENT_WINDOW_TITLE     PRODUCT_NAME
#define CLIENT_WINDOW_MIN_TITLE PRODUCT_NAME_LOWER

template<class T>
void ignore_result(T) {}

// C standard library headers
#include <errno.h>
#include <iso646.h>
#include <limits.h>
#include <locale.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

// C++ standard library headers
#include <utility>
#include <functional>
#include <chrono>
#include <type_traits>
#include <initializer_list>
#include <tuple>
#include <new>
#include <memory>
#include <limits>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <forward_list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <queue>
#include <algorithm>
#include <iterator>
#include <random>
#include <numeric>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <valarray>
#include <sstream>
#include <iostream>

// vsnprintf is ISO/IEC 9899:1999
// abstracting this to make it portable
#ifdef _MSC_VER
//vsnprintf is non-conformant in MSVC--fails to null-terminate in case of overflow
#define Q_vsnprintf(dest, size, fmt, args) _vsnprintf_s( dest, size, _TRUNCATE, fmt, args )
#define Q_snprintf(dest, size, fmt, ...) _snprintf_s( dest, size, _TRUNCATE, fmt, __VA_ARGS__ )
#elif defined( _WIN32 )
#define Q_vsnprintf _vsnprintf
#define Q_snprintf  _snprintf
#else
#define Q_vsnprintf vsnprintf
#define Q_snprintf  snprintf
#endif

#include "common/Platform.h"
#include "common/Compiler.h"
#include "common/Endian.h"

enum class qtrinary {qno, qyes, qmaybe};

using qhandle_t = int;
using sfxHandle_t = int;
using fileHandle_t = int;
using clipHandle_t = int;

#define PAD(x,y)                ((( x ) + ( y ) - 1 ) & ~(( y ) - 1 ))
#define PADLEN(base, alignment) ( PAD(( base ), ( alignment )) - ( base ))
#define PADP(base, alignment)   ((void *) PAD((intptr_t) ( base ), ( alignment )))

#define STRING(s)  #s
// expand constants before stringifying them
#define XSTRING(s) STRING(s)

#define HUGE_QFLT 3e38f // TODO: Replace HUGE_QFLT with MAX_QFLT

#ifndef BIT
#define BIT(x) ( 1 << ( x ) )
#endif

// the game guarantees that no string from the network will ever
// exceed MAX_STRING_CHARS
#define MAX_STRING_CHARS  1024 // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS 256 // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS   1024 // max length of an individual token

#define MAX_ADDR_CHARS    sizeof("[1111:2222:3333:4444:5555:6666:7777:8888]:99999")

#define MAX_INFO_STRING   1024
#define MAX_INFO_KEY      1024
#define MAX_INFO_VALUE    1024

#define BIG_INFO_STRING   8192 // used for system info key only
#define BIG_INFO_KEY      8192
#define BIG_INFO_VALUE    8192

#define MAX_QPATH         64 // max length of a quake game pathname
#define MAX_OSPATH        256 // max length of a filesystem pathname
#define MAX_CMD           1024 // max length of a command line

// rain - increased to 36 to match MAX_NETNAME, fixes #13 - UI stuff breaks
// with very long names
#define MAX_NAME_LENGTH    128 // max length of a client name, in bytes

#define MAX_SAY_TEXT       400

//
// these aren't needed by any of the VMs.  put in another header?
//
#define MAX_MAP_AREA_BYTES 32 // bit vector of area visibility

	enum class ha_pref
	{
	  h_high,
	  h_low,
	  h_dontcare
	};

#define Com_Memset   memset
#define Com_Memcpy   memcpy

#define Com_Allocate malloc
#define Com_Dealloc  free

MALLOC_LIKE void *Com_Allocate_Aligned( size_t alignment, size_t size );
void  Com_Free_Aligned( void *ptr );

#define ARRAY_LEN(x) ( sizeof( x ) / sizeof( *( x ) ) )

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH     640
#define SCREEN_HEIGHT    480

#define SMALLCHAR_WIDTH  8
#define SMALLCHAR_HEIGHT 16

#define BIGCHAR_WIDTH    16
#define BIGCHAR_HEIGHT   16

#define GIANTCHAR_WIDTH  32
#define GIANTCHAR_HEIGHT 48

#define GAME_INIT_FRAMES 6
#define FRAMETIME        100 // msec

#include "logging.h"

#include "q_math.h"

//=============================================

	struct growList_t
	{
		bool frameMemory;
		int      currentElements;
		int      maxElements; // will reallocate and move when exceeded
		void     **elements;
	};

// you don't need to init the growlist if you don't mind it growing and moving
// the list as it expands
	void Com_InitGrowList( growList_t *list, int maxElements );
	void Com_DestroyGrowList( growList_t *list );
	int  Com_AddToGrowList( growList_t *list, void *data );
	void *Com_GrowListElement( const growList_t *list, int index );

//=============================================================================

	float      Com_Clamp( float min, float max, float value );

	char       *COM_SkipPath( char *pathname );
	char       *Com_SkipTokens( char *s, int numTokens, const char *sep );
	void       COM_FixPath( char *pathname );
	const char *COM_GetExtension( const char *name );
	void       COM_StripExtension( const char *in, char *out );
	void       COM_StripExtension2( const char *in, char *out, int destsize );
	void       COM_StripExtension3( const char *src, char *dest, int destsize );
	void       COM_DefaultExtension( char *path, int maxSize, const char *extension );

	void       COM_BeginParseSession( const char *name );
	char       *COM_Parse( const char **data_p );

// RB: added COM_Parse2 for having a Doom 3 style tokenizer.
	char       *COM_Parse2( const char **data_p );
	char       *COM_ParseExt2( const char **data_p, bool allowLineBreak );

	char       *COM_ParseExt( const char **data_p, bool allowLineBreak );
	int        COM_Compress( char *data_p );
	void       COM_ParseError( const char *format, ... ) PRINTF_LIKE(1);
	void       COM_ParseWarning( const char *format, ... ) PRINTF_LIKE(1);

	int        Com_ParseInfos( const char *buf, int max, char infos[][ MAX_INFO_STRING ] );

	int        Com_HashKey( char *string, int maxlen );

// data is an in/out parm, returns a parsed out token

	void      COM_MatchToken( char **buf_p, char *match );

	bool  SkipBracedSection( const char **program );
	bool  SkipBracedSection_Depth( const char **program, int depth );  // start at given depth if already
	void      SkipRestOfLine( const char **data );

	void      Parse1DMatrix( const char **buf_p, int x, float *m );
	void      Parse2DMatrix( const char **buf_p, int y, int x, float *m );
	void      Parse3DMatrix( const char **buf_p, int z, int y, int x, float *m );

	int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) PRINTF_LIKE(3);

// mode parm for FS_FOpenFile
	enum class fsMode_t
	{
	  FS_READ,
	  FS_WRITE,
	  FS_APPEND,
	  FS_APPEND_SYNC,
	  FS_READ_DIRECT,
	  FS_UPDATE,
	  FS_WRITE_VIA_TEMPORARY,
	};

	enum class fsOrigin_t : int
	{
	  FS_SEEK_CUR,
	  FS_SEEK_END,
	  FS_SEEK_SET
	};

	int        Com_HexStrToInt( const char *str );

	const char *Com_ClearForeignCharacters( const char *str );

//=============================================

    bool Q_strtoi( const char *s, int *outNum );

// portable case insensitive compare
	int        Q_stricmp( const char *s1, const char *s2 );
	int        Q_strncmp( const char *s1, const char *s2, int n );
	int        Q_strnicmp( const char *s1, const char *s2, int n );
	char       *Q_strlwr( char *s1 );
	char       *Q_strupr( char *s1 );
	const char *Q_stristr( const char *s, const char *find );

// buffer size safe library replacements
	void Q_strncpyz( char *dest, const char *src, int destsize );

	void     Q_strcat( char *dest, int destsize, const char *src );

	int      Com_Filter( const char *filter, const char *name, int casesensitive );

// parse "\n" into '\n'
	void     Q_ParseNewlines( char *dest, const char *src, int destsize );

// Count the number of char tocount encountered in string
	int      Q_CountChar( const char *string, char tocount );

//=============================================

	char     *QDECL va( const char *format, ... ) PRINTF_LIKE(1);

//=============================================

//
// key / value info strings
//
	/*
	 * Separator used by info strings
	 */
	static CONSTEXPR char INFO_SEPARATOR = '\\';

	/*
	 * Associative container with string values and keys
	 */
	using InfoMap = std::map<std::string, std::string>;
	/*
	 * Formats an InfoMap to an info string, it will discard elements with
	 * an invalid key or value
	 */
	std::string InfoMapToString( const InfoMap& map );
	/*
	 * Formats an InfoMap to an info string, it will discard elements with
	 * an invalid key or value
	 */
	InfoMap InfoStringToMap( const std::string& string );

	/*
	 * Whether string is a valid info key or value
	 */
	bool InfoValidItem(const std::string& string);

	// DEPRECATED: Use InfoMap
	const char *Info_ValueForKey( const char *s, const char *key );
	// DEPRECATED: Use InfoMap
	void       Info_RemoveKey( char *s, const char *key , bool big );
	// DEPRECATED: Use InfoMap
	void       Info_SetValueForKey( char *s, const char *key, const char *value , bool big );
	// DEPRECATED: Use InfoMap
	void       Info_SetValueForKeyRocket( char *s, const char *key, const char *value, bool big );
	// DEPRECATED: Use InfoMap
	bool   Info_Validate( const char *s );
	// DEPRECATED: Use InfoMap
	void       Info_NextPair( const char **s, char *key, char *value );

//=============================================
/*
 * CVARS (console variables)
 *
 * Many variables can be used for cheating purposes, so when
 * cheats is zero, force all unspecified variables to their
 * default values.
 */

/**
 * set to cause it to be saved to autogen
 * used for system variables, not for player
 * specific configurations
 */
#define CVAR_ARCHIVE             BIT(0)
#define CVAR_USERINFO            BIT(1)    /*< sent to server on connect or change */
#define CVAR_SERVERINFO          BIT(2)    /*< sent in response to front end requests */
#define CVAR_SYSTEMINFO          BIT(3)    /*< these cvars will be duplicated on all clients */

/**
 * don't allow change from console at all,
 * but can be set from the command line
 */
#define CVAR_INIT                BIT(4)

/**
 * will only change when C code next does a Cvar_Get(),
 * so it can't be changed without proper initialization.
 * modified will be set, even though the value hasn't changed yet
 */
#define CVAR_LATCH               BIT(5)
#define CVAR_ROM                 BIT(6)   /*< display only, cannot be set by user at all */
#define CVAR_USER_CREATED        BIT(7)   /*< created by a set command */
#define CVAR_TEMP                BIT(8)   /*< can be set even when cheats are disabled, but is not archived */
#define CVAR_CHEAT               BIT(9)   /*< can not be changed if cheats are disabled */
#define CVAR_NORESTART           BIT(10)  /*< do not clear when a cvar_restart is issued */

/**
 * unsafe system cvars (renderer, sound settings,
 * anything that might cause a crash)
 */
#define CVAR_UNSAFE              BIT(12)

/**
 * won't automatically be send to clients,
 * but server browsers will see it
 */
#define CVAR_SERVERINFO_NOUPDATE BIT(13)
#define CVAR_USER_ARCHIVE        BIT(14)

#define CVAR_ARCHIVE_BITS        (CVAR_ARCHIVE | CVAR_USER_ARCHIVE)

#define MAX_CVAR_VALUE_STRING 256

	using cvarHandle_t = int;

/**
 * the modules that run in the virtual machine can't access the cvar_t directly,
 * so they must ask for structured updates
 */
	struct vmCvar_t
	{
		cvarHandle_t handle;
		int          modificationCount;
		float        value;
		int          integer;
		char         string[ MAX_CVAR_VALUE_STRING ];
	};

#include "SurfaceFlags.h" // shared with the q3map utility

//=====================================================================

// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE 0x0001
#define KEYCATCH_UI      0x0002

#define KEYEVSTATE_DOWN 1
#define KEYEVSTATE_CHAR 2
#define KEYEVSTATE_SUP  8

// sound channels
// channel 0 never willingly overrides
// other channels will always override a playing sound on that channel
	enum class soundChannel_t
	{
	  CHAN_AUTO,
	  CHAN_LOCAL, // menu sounds, etc
	  CHAN_WEAPON,
	  CHAN_VOICE,
	  CHAN_ITEM,
	  CHAN_BODY,
	  CHAN_LOCAL_SOUND, // chat messages, etc
	  CHAN_ANNOUNCER, // announcer voices, etc
	  CHAN_VOICE_BG, // xkan - background sound for voice (radio static, etc.)
	};

	/*
	========================================================================

	  ELEMENTS COMMUNICATED ACROSS THE NET

	========================================================================
	*/
#define ANIM_BITS 10

#define SNAPFLAG_RATE_DELAYED 1
#define SNAPFLAG_NOT_ACTIVE   2 // snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT  4 // toggled every map_restart so transitions can be detected

//
// per-level limits
//
#define MAX_CLIENTS         64 // JPW NERVE back to q3ta default was 128    // absolute limit

#define GENTITYNUM_BITS     10 // JPW NERVE put q3ta default back for testing // don't need to send any more

#define MAX_GENTITIES       ( 1 << GENTITYNUM_BITS )

// entitynums are communicated with GENTITY_BITS, so any reserved
// values that are going to be communicated over the net need to
// also be in this range
#define ENTITYNUM_NONE           ( MAX_GENTITIES - 1 )
#define ENTITYNUM_WORLD          ( MAX_GENTITIES - 2 )
#define ENTITYNUM_MAX_NORMAL     ( MAX_GENTITIES - 2 )

#define MODELINDEX_BITS          9 // minimum requirement for MAX_SUBMODELS and MAX_MODELS

#define MAX_SUBMODELS            512 // 9 bits sent (see qcommon/msg.c); q3map2 limits to 1024 via MAX_MAP_MODELS; not set to 512 to avoid overlap with fake handles
#define MAX_MODELS               256 // 9 bits sent (see qcommon/msg.c), but limited by game VM
#define MAX_SOUNDS               256 // 8 bits sent (via eventParm; see qcommon/msg.c)
#define MAX_CS_SKINS             64
#define MAX_CSSTRINGS            32

#define MAX_CS_SHADERS           32
#define MAX_SERVER_TAGS          256
#define MAX_TAG_FILES            64

#define MAX_CONFIGSTRINGS        1024

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define CS_SERVERINFO          0 // an info string with all the serverinfo cvars
#define CS_SYSTEMINFO          1 // an info string for server system to client system configuration (timescale, etc)

#define RESERVED_CONFIGSTRINGS 2 // game can't modify below this, only the system can

using GameStateCSs = std::array<std::string, MAX_CONFIGSTRINGS>;

#define REF_FORCE_DLIGHT       ( 1 << 31 ) // RF, passed in through overdraw parameter, force this dlight under all conditions
#define REF_JUNIOR_DLIGHT      ( 1 << 30 ) // (SA) this dlight does not light surfaces.  it only affects dynamic light grid
#define REF_DIRECTED_DLIGHT    ( 1 << 29 ) // ydnar: global directional light, origin should be interpreted as a normal vector
#define REF_RESTRICT_DLIGHT    ( 1 << 1 ) // dlight is restricted to following entities
#define REF_INVERSE_DLIGHT     ( 1 << 0 ) // inverse dlight for dynamic shadows

// bit field limits
#define MAX_STATS              16
#define MAX_PERSISTANT         16
#define MAX_MISC               16

#define MAX_EVENTS             4 // max events per frame before we drop events

#define PS_PMOVEFRAMECOUNTBITS 6

struct netField_t
{
	std::string name;
	int  offset;
	int  bits;
	int  used;
};
#define STATS_GROUP_FIELD 99 // magic number in `bits` for 'int stats[16]' (but these ints must fit in a signed short)
#define STATS_GROUP_NUM_STATS 16
#define MAX_PLAYERSTATE_SIZE 600 // HACK: limit size
#define PLAYERSTATE_FIELD_SIZE 4
using NetcodeTable = std::vector<netField_t>;

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur

// the full definition of playerState_t is found in the gamelogic. The OpaquePlayerState
// struct includes only player state fields which are used by the engine.

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
//
// NOTE: all fields in here must be 32 bits (or those within sub-structures)

union OpaquePlayerState {
	byte storage[MAX_PLAYERSTATE_SIZE];
	struct {
		// These fields must be identical to ones at the start of playerState_t
		vec3_t origin;
		int ping; // shouldn't even be here?
		int persistant[16];
		int    viewheight;
		int clientNum;
		int   delta_angles[3]; // add to command angles to get view direction
		vec3_t viewangles; // for fixed views
		int    commandTime; // cmd->serverTime of last executed command

		// this is just for determining the size of the unnamed struct
		int END;
	};
};

//====================================================================

//
// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
//

#define USERCMD_BUTTONS     16 // bits allocated for buttons (multiple of 8)

#define BUTTON_ATTACK       0
#define BUTTON_TALK         1  // disables actions
#define BUTTON_USE_HOLDABLE 2
#define BUTTON_GESTURE      3
#define BUTTON_WALKING      4  // walking can't just be inferred from MOVE_RUN
                               // because a key pressed late in the frame will
                               // only generate a small move value for that
                               // frame; walking will use different animations
                               // and won't generate footsteps
#define BUTTON_SPRINT       5
#define BUTTON_ACTIVATE     6
#define BUTTON_ANY          7  // if any key is pressed
#define BUTTON_ATTACK2      8
// BUTTON_ATTACK3           9  // defined in game code
//                          10
//                          11
//                          12
// BUTTON_DECONSTRUCT       13 // defined in game code
#define BUTTON_RALLY        14
//                          15

#define MOVE_RUN          120 // if forwardmove or rightmove are >= MOVE_RUN,
// then BUTTON_WALKING should be set

// Arnout: doubleTap buttons - DT_NUM can be max 8
	enum class dtType_t : byte
	{
	  DT_NONE,
	  DT_MOVELEFT,
	  DT_MOVERIGHT,
	  DT_FORWARD,
	  DT_BACK,
	  DT_UP,
	  DT_NUM
	};

// usercmd_t is sent to the server each client frame
	struct usercmd_t
	{
		int         serverTime;
		int         angles[ 3 ];

		signed char forwardmove, rightmove, upmove;
		dtType_t    doubleTap; // Arnout: only 3 bits used

		byte        weapon;
		byte        flags;

		byte        buttons[ USERCMD_BUTTONS / 8 ];
	};

// Some functions for buttons manipulation & testing
	inline void usercmdPressButton( byte *buttons, int bit )
	{
		buttons[bit / 8] |= 1 << ( bit & 7 );
	}

	inline void usercmdReleaseButton( byte *buttons, int bit )
	{
		buttons[bit / 8] &= ~( 1 << ( bit & 7 ) );
	}

	inline void usercmdClearButtons( byte *buttons )
	{
		memset( buttons, 0, USERCMD_BUTTONS / 8 );
	}

	inline void usercmdCopyButtons( byte *dest, const byte *source )
	{
		memcpy( dest, source, USERCMD_BUTTONS / 8 );
	}

	inline void usercmdLatchButtons( byte *dest, const byte *srcNew, const byte *srcOld )
	{
		int i;
		for ( i = 0; i < USERCMD_BUTTONS / 8; ++i )
		{
			 dest[i] |= srcNew[i] & ~srcOld[i];
		}
	}

	inline bool usercmdButtonPressed( const byte *buttons, int bit )
	{
		return ( buttons[bit / 8] & ( 1 << ( bit & 7 ) ) ) ? true : false;
	}

	inline bool usercmdButtonsDiffer( const byte *a, const byte *b )
	{
		return memcmp( a, b, USERCMD_BUTTONS / 8 ) ? true : false;
	}

//===================================================================

// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define SOLID_BMODEL 0xffffff

	enum class trType_t
	{
	  TR_STATIONARY,
	  TR_INTERPOLATE, // non-parametric, but interpolate between snapshots
	  TR_LINEAR,
	  TR_LINEAR_STOP,
	  TR_SINE, // value = base + sin( time / duration ) * delta
	  TR_GRAVITY,
	  TR_BUOYANCY,
	};

	struct trajectory_t
	{
		trType_t trType;
		int      trTime;
		int      trDuration; // if non 0, trTime + trDuration = stop time
//----(SA)  removed
		vec3_t   trBase;
		vec3_t   trDelta; // velocity, etc
//----(SA)  removed
	};

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large
//
// NOTE: all fields in here must be 32 bits (or those within sub-structures)
//
// You can use Com_EntityTypeName to get a String representation of this enum
	enum class entityType_t
	{
		ET_GENERAL,
		ET_PLAYER,
		ET_ITEM,

		ET_BUILDABLE,       // buildable type

		ET_LOCATION,

		ET_MISSILE,
		ET_MOVER,
		ET_UNUSED,
		ET_PORTAL,
		ET_SPEAKER,
		ET_PUSHER,
		ET_TELEPORTER,
		ET_INVISIBLE,
		ET_FIRE,

		ET_CORPSE,
		ET_PARTICLE_SYSTEM,
		ET_ANIMMAPOBJ,
		ET_MODELDOOR,
		ET_LIGHTFLARE,
		ET_LEV2_ZAP_CHAIN,

		ET_BEACON,

		ET_EVENTS       // any of the EV_* events can be added freestanding
		// by setting eType to ET_EVENTS + eventNum
		// this avoids having to set eFlags and eventNum
	};

	const char *Com_EntityTypeName(entityType_t entityType);

	struct entityState_t
	{
		int          number; // entity index
		entityType_t eType; // entityType_t
		int          eFlags;

		trajectory_t pos; // for calculating position
		trajectory_t apos; // for calculating angles

		int          time;
		int          time2;

		vec3_t       origin;
		vec3_t       origin2;

		vec3_t       angles;
		vec3_t       angles2;

		int          otherEntityNum; // shotgun sources, etc
		int          otherEntityNum2;

// FIXME: separate field, but doing this for compat reasons
#define otherEntityNum3 groundEntityNum

		int          groundEntityNum; // ENTITYNUM_NONE = in air

		int          constantLight; // r + (g<<8) + (b<<16) + (intensity<<24)
		int          loopSound; // constantly loop this sound

		int          modelindex;
		int          modelindex2;
		int          clientNum; // 0 to (MAX_CLIENTS - 1), for players and corpses
		int          frame;

		int          solid; // for client side prediction, trap_linkentity sets this properly

		// old style events, in for compatibility only
		int event; // impulse events -- muzzle flashes, footsteps, etc
		int eventParm;

		int eventSequence; // pmove generated events
		int events[ MAX_EVENTS ];
		int eventParms[ MAX_EVENTS ];

		// for players
		int weapon; // determines weapon and flash model, etc
		int legsAnim; // mask off ANIM_TOGGLEBIT
		int torsoAnim; // mask off ANIM_TOGGLEBIT

		int           misc; // bit flags
		int           generic1;
		int           weaponAnim; // mask off ANIM_TOGGLEBIT
	};

	enum class connstate_t
	{
	  CA_UNINITIALIZED,
	  CA_DISCONNECTED, // not talking to a server
	  CA_CONNECTING, // sending request packets to the server
	  CA_CHALLENGING, // sending challenge packets to the server
	  CA_CONNECTED, // netchan_t established, getting gamestate
	  CA_DOWNLOADING, // downloading a file
	  CA_LOADING, // only during cgame initialization, never during main loop
	  CA_PRIMED, // got gamestate, waiting for first frame
	  CA_ACTIVE, // game views should be displayed
	};

// font support

#define GLYPH_START     0
#define GLYPH_END       255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND   127
#define GLYPHS_PER_FONT ( GLYPH_END - GLYPH_START + 1 )
struct glyphInfo_t
{
	int       height; // number of scan lines
	int       top; // top of glyph in buffer
	int       bottom; // bottom of glyph in buffer
	int       pitch; // width for copying
	int       xSkip; // x adjustment
	int       imageWidth; // width of actual image
	int       imageHeight; // height of actual image
	float     s; // x offset in image where glyph starts
	float     t; // y offset in image where glyph starts
	float     s2;
	float     t2;
	qhandle_t glyph; // handle to the shader with the glyph
	char      shaderName[ 32 ];
};

// Unlike with many other handle types, 0 is valid, not an error or default return value.
using fontHandle_t = int;

using glyphBlock_t = glyphInfo_t[256];

struct fontInfo_t
{
	void         *face, *faceData, *fallback, *fallbackData;
	glyphInfo_t  *glyphBlock[0x110000 / 256]; // glyphBlock_t
	int           pointSize;
	int           height;
	float         glyphScale;
	char          name[ MAX_QPATH ];
};

// real time
//=============================================

struct qtime_t
{
    int tm_sec; /* seconds after the minute - [0,59] */
    int tm_min; /* minutes after the hour - [0,59] */
    int tm_hour; /* hours since midnight - [0,23] */
    int tm_mday; /* day of the month - [1,31] */
    int tm_mon; /* months since January - [0,11] */
    int tm_year; /* years since 1900 */
    int tm_wday; /* days since Sunday - [0,6] */
    int tm_yday; /* days since January 1 - [0,365] */
    int tm_isdst; /* daylight savings time flag */
};

int        Com_RealTime( qtime_t *qtime );
int        Com_GMTime( qtime_t *qtime );
// Com_Time: client gets local time, server gets GMT
#ifdef BUILD_SERVER
#define Com_Time(t) Com_GMTime(t)
#else
#define Com_Time(t) Com_RealTime(t)
#endif

// server browser sources
#define AS_LOCAL     0
#define AS_GLOBAL    1 // NERVE - SMF - modified
#define AS_FAVORITES 2

#define MAX_GLOBAL_SERVERS       4096
#define MAX_OTHER_SERVERS        128
#define MAX_PINGREQUESTS         16
#define MAX_SERVERSTATUSREQUESTS 16

#define GENTITYNUM_MASK           ( MAX_GENTITIES - 1 )

#define MAX_EMOTICON_NAME_LEN     16
#define MAX_EMOTICONS             64

#define MAX_OBJECTIVES            64
#define MAX_LOCATIONS             64
#define MAX_MODELS                256 // these are sent over the net as 8 bits
#define MAX_SOUNDS                256 // so they cannot be blindly increased
#define MAX_GAME_SHADERS          64
#define MAX_GRADING_TEXTURES      64
#define MAX_REVERB_EFFECTS        64
#define MAX_GAME_PARTICLE_SYSTEMS 64
#define MAX_HOSTNAME_LENGTH       80
#define MAX_NEWS_STRING           10000

	struct emoticon_t
	{
		char      name[ MAX_EMOTICON_NAME_LEN ];
#ifndef GAME
		int       width;
		qhandle_t shader;
#endif
	};

	struct clientList_t
	{
		uint hi;
		uint lo;
	};

	bool Com_ClientListContains( const clientList_t *list, int clientNum );
	void     Com_ClientListAdd( clientList_t *list, int clientNum );
	void     Com_ClientListRemove( clientList_t *list, int clientNum );
	char     *Com_ClientListString( const clientList_t *list );
	void     Com_ClientListParse( clientList_t *list, const char *s );

	/* This should not be changed because this value is
	* expected to be the same on the client and on the server */
#define RSA_PUBLIC_EXPONENT 65537
#define RSA_KEY_LENGTH      2048
#define RSA_STRING_LENGTH   ( RSA_KEY_LENGTH / 4 + 1 )

#include "common/Common.h"

#endif /* Q_SHARED_H_ */
