/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2011 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of Daemon source code.

Daemon source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_shader.c -- this file deals with the parsing and definition of shaders
#include "tr_local.h"
#include "gl_shader.h"

static const int MAX_SHADERTABLE_HASH = 1024;
static shaderTable_t *shaderTableHashTable[ MAX_SHADERTABLE_HASH ];

static const int FILE_HASH_SIZE       = 1024;
static shader_t      *shaderHashTable[ FILE_HASH_SIZE ];

static const int MAX_SHADERTEXT_HASH  = 2048;
static const char          **shaderTextHashTable[ MAX_SHADERTEXT_HASH ];

static char          *s_shaderText;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static shaderTable_t table;
static shaderStage_t stages[ MAX_SHADER_STAGES ];
static shader_t      shader;
static texModInfo_t  texMods[ MAX_SHADER_STAGES ][ TR_MAX_TEXMODS ];

// ydnar: these are here because they are only referenced while parsing a shader
static char          implicitMap[ MAX_QPATH ];
static unsigned      implicitStateBits;
static cullType_t    implicitCullType;

static char          whenTokens[ MAX_STRING_CHARS ];

/* This parser does not assume any default normal map format.

Syntax variations must define a default normal map format.

Stage keywords like normalMap, bumpMap and normalHeightMap set
normal format to +X -Y +Z like DirectX, to keep those keywords
compatible with other idTech3 engines using those keywords.

XreaL implemented normalMap like Doom3 which is an OpenGL engine
using DirectX normal format convention. Then other renderers like
Daemon, ioquake3 renderer2, OpenJK, ET:Legacy, etc. did like XreaL.

It's possible to extend this parser with other stage keywords
from other engines to load normal map with their respective format.

The normalFormat stage keyword can be used in materials to set an
arbitrary format.
*/
static const vec3_t glNormalFormat = {  1.0,  1.0,  1.0 };
static const vec3_t dxNormalFormat = {  1.0, -1.0,  1.0 };

// DarkPlaces material compatibility
static Cvar::Cvar<bool> r_dpMaterial("r_dpMaterial", "Enable DarkPlaces material compatibility", Cvar::LATCH, false);
Cvar::Cvar<bool> r_dpBlend("r_dpBlend", "Enable DarkPlaces blend compatibility, process GT0 as GE128", Cvar::NONE, false);

/*
================
return a hash value for the filename
================
*/
static unsigned int generateHashValue( const char *fname, const int size )
{
	int  i;

	unsigned hash;
	char letter;

	hash = 0;
	i = 0;

	while ( fname[ i ] != '\0' )
	{
		letter = Str::ctolower( fname[ i ] );

		if ( letter == '.' )
		{
			break; // don't include extension
		}

		if ( letter == '\\' )
		{
			letter = '/'; // damn path names
		}

		if ( letter == PATH_SEP )
		{
			letter = '/'; // damn path names
		}

		hash += ( unsigned )( letter ) * ( i + 119 );
		i++;
	}

	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	hash &= ( size - 1 );
	return hash;
}

void R_RemapShader( const char *shaderName, const char *newShaderName, const char* )
{
	char      strippedName[ MAX_QPATH ];
	int       hash;
	shader_t  *sh, *sh2;
	qhandle_t h;

	sh = R_FindShaderByName( shaderName );

	if ( sh == nullptr || sh == tr.defaultShader )
	{
		h = RE_RegisterShader( shaderName, RSF_DEFAULT );
		sh = R_GetShaderByHandle( h );
	}

	if ( sh == nullptr || sh == tr.defaultShader )
	{
		Log::Warn("R_RemapShader: shader %s not found", shaderName );
		return;
	}

	sh2 = R_FindShaderByName( newShaderName );

	if ( sh2 == nullptr || sh2 == tr.defaultShader )
	{
		h = RE_RegisterShader( newShaderName, RSF_DEFAULT );
		sh2 = R_GetShaderByHandle( h );
	}

	if ( sh2 == nullptr || sh2 == tr.defaultShader )
	{
		Log::Warn("R_RemapShader: new shader %s not found", newShaderName );
		return;
	}

	if ( sh->autoSpriteMode != sh2->autoSpriteMode ) {
		Log::Warn("R_RemapShader: shaders %s and %s have different autoSprite modes", shaderName, newShaderName );
		return;
	}

	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	COM_StripExtension3( shaderName, strippedName, sizeof( strippedName ) );
	hash = generateHashValue( strippedName, FILE_HASH_SIZE );

	for ( sh = shaderHashTable[ hash ]; sh; sh = sh->next )
	{
		if ( Q_stricmp( sh->name, strippedName ) == 0 )
		{
			if ( sh != sh2 )
			{
				sh->remappedShader = sh2;
			}
			else
			{
				sh->remappedShader = nullptr;
			}
		}
	}
}

/*
===============
ParseVector
===============
*/
static bool ParseVector( const char **text, int count, float *v )
{
	char *token;
	int  i;

	token = COM_ParseExt2( text, false );

	if ( strcmp( token, "(" ) )
	{
		Log::Warn("missing parenthesis in shader '%s'", shader.name );
		return false;
	}

	for ( i = 0; i < count; i++ )
	{
		token = COM_ParseExt2( text, false );

		if ( !token[ 0 ] )
		{
			Log::Warn("missing vector element in shader '%s'", shader.name );
			return false;
		}

		v[ i ] = atof( token );
	}

	token = COM_ParseExt2( text, false );

	if ( strcmp( token, ")" ) )
	{
		Log::Warn("missing parenthesis in shader '%s'", shader.name );
		return false;
	}

	return true;
}

const opstring_t opStrings[] = {
	{"bad",                opcode_t::OP_BAD},

	{"&&",                 opcode_t::OP_LAND},
	{"||",                 opcode_t::OP_LOR},
	{">=",                 opcode_t::OP_GE},
	{"<=",                 opcode_t::OP_LE},
	{"==",                 opcode_t::OP_LEQ},
	{"!=",                 opcode_t::OP_LNE},

	{"+",                  opcode_t::OP_ADD},
	{"-",                  opcode_t::OP_SUB},
	{"/",                  opcode_t::OP_DIV},
	{"%",                  opcode_t::OP_MOD},
	{"*",                  opcode_t::OP_MUL},
	{"neg",                opcode_t::OP_NEG},

	{"<",                  opcode_t::OP_LT},
	{">",                  opcode_t::OP_GT},

	{"(",                  opcode_t::OP_LPAREN},
	{")",                  opcode_t::OP_RPAREN},
	{"[",                  opcode_t::OP_LBRACKET},
	{"]",                  opcode_t::OP_RBRACKET},

	{"c",                  opcode_t::OP_NUM},
	{"time",               opcode_t::OP_TIME},
	{"parm0",              opcode_t::OP_PARM0},
	{"parm1",              opcode_t::OP_PARM1},
	{"parm2",              opcode_t::OP_PARM2},
	{"parm3",              opcode_t::OP_PARM3},
	{"parm4",              opcode_t::OP_PARM4},
	{"parm5",              opcode_t::OP_PARM5},
	{"parm6",              opcode_t::OP_PARM6},
	{"parm7",              opcode_t::OP_PARM7},
	{"parm8",              opcode_t::OP_PARM8},
	{"parm9",              opcode_t::OP_PARM9},
	{"parm10",             opcode_t::OP_PARM10},
	{"parm11",             opcode_t::OP_PARM11},
	{"global0",            opcode_t::OP_GLOBAL0},
	{"global1",            opcode_t::OP_GLOBAL1},
	{"global2",            opcode_t::OP_GLOBAL2},
	{"global3",            opcode_t::OP_GLOBAL3},
	{"global4",            opcode_t::OP_GLOBAL4},
	{"global5",            opcode_t::OP_GLOBAL5},
	{"global6",            opcode_t::OP_GLOBAL6},
	{"global7",            opcode_t::OP_GLOBAL7},
	{"fragmentShaders",    opcode_t::OP_FRAGMENTSHADERS},
	{"frameBufferObjects", opcode_t::OP_FRAMEBUFFEROBJECTS},
	{"sound",              opcode_t::OP_SOUND},
	{"distance",           opcode_t::OP_DISTANCE},

	{"table",              opcode_t::OP_TABLE},

	{nullptr,              opcode_t::OP_BAD},
};

const char* GetOpName(opcode_t type)
{
	return opStrings[ Util::ordinal(type) ].s;
}

static void GetOpType( char *token, expOperation_t *op )
{
	const opstring_t *opString;
	char          tableName[ MAX_QPATH ];
	int           hash;
	shaderTable_t *tb;

	if ( ( token[ 0 ] >= '0' && token[ 0 ] <= '9' ) ||
	     //(token[0] == '-' && token[1] >= '0' && token[1] <= '9')   ||
	     //(token[0] == '+' && token[1] >= '0' && token[1] <= '9')   ||
	     ( token[ 0 ] == '.' && token[ 1 ] >= '0' && token[ 1 ] <= '9' ) )
	{
		op->type = opcode_t::OP_NUM;
		return;
	}

	Q_strncpyz( tableName, token, sizeof( tableName ) );
	hash = generateHashValue( tableName, MAX_SHADERTABLE_HASH );

	for ( tb = shaderTableHashTable[ hash ]; tb; tb = tb->next )
	{
		if ( Q_stricmp( tb->name, tableName ) == 0 )
		{
			// match found
			op->type = opcode_t::OP_TABLE;
			op->value = tb->index;
			return;
		}
	}

	for ( opString = opStrings; opString->s; opString++ )
	{
		if ( !Q_stricmp( token, opString->s ) )
		{
			op->type = opString->type;
			return;
		}
	}

	op->type = opcode_t::OP_BAD;
}

static bool IsOperand( opcode_t oc )
{
	switch ( oc )
	{
		case opcode_t::OP_NUM:
		case opcode_t::OP_TIME:
		case opcode_t::OP_PARM0:
		case opcode_t::OP_PARM1:
		case opcode_t::OP_PARM2:
		case opcode_t::OP_PARM3:
		case opcode_t::OP_PARM4:
		case opcode_t::OP_PARM5:
		case opcode_t::OP_PARM6:
		case opcode_t::OP_PARM7:
		case opcode_t::OP_PARM8:
		case opcode_t::OP_PARM9:
		case opcode_t::OP_PARM10:
		case opcode_t::OP_PARM11:
		case opcode_t::OP_GLOBAL0:
		case opcode_t::OP_GLOBAL1:
		case opcode_t::OP_GLOBAL2:
		case opcode_t::OP_GLOBAL3:
		case opcode_t::OP_GLOBAL4:
		case opcode_t::OP_GLOBAL5:
		case opcode_t::OP_GLOBAL6:
		case opcode_t::OP_GLOBAL7:
		case opcode_t::OP_FRAGMENTSHADERS:
		case opcode_t::OP_FRAMEBUFFEROBJECTS:
		case opcode_t::OP_SOUND:
		case opcode_t::OP_DISTANCE:
			return true;

		default:
			return false;
	}
}

static bool IsOperator( opcode_t oc )
{
	switch ( oc )
	{
		case opcode_t::OP_LAND:
		case opcode_t::OP_LOR:
		case opcode_t::OP_GE:
		case opcode_t::OP_LE:
		case opcode_t::OP_LEQ:
		case opcode_t::OP_LNE:
		case opcode_t::OP_ADD:
		case opcode_t::OP_SUB:
		case opcode_t::OP_DIV:
		case opcode_t::OP_MOD:
		case opcode_t::OP_MUL:
		case opcode_t::OP_NEG:
		case opcode_t::OP_LT:
		case opcode_t::OP_GT:
		case opcode_t::OP_TABLE:
			return true;

		default:
			return false;
	}
}

static int GetOpPrecedence( opcode_t oc )
{
	switch ( oc )
	{
		case opcode_t::OP_LOR:
			return 1;

		case opcode_t::OP_LAND:
			return 2;

		case opcode_t::OP_LEQ:
		case opcode_t::OP_LNE:
			return 3;

		case opcode_t::OP_GE:
		case opcode_t::OP_LE:
		case opcode_t::OP_LT:
		case opcode_t::OP_GT:
			return 4;

		case opcode_t::OP_ADD:
		case opcode_t::OP_SUB:
			return 5;

		case opcode_t::OP_DIV:
		case opcode_t::OP_MOD:
		case opcode_t::OP_MUL:
			return 6;

		case opcode_t::OP_NEG:
			return 7;

		case opcode_t::OP_TABLE:
			return 8;

		default:
			return 0;
	}
}

static char    *ParseExpressionElement( const char **data_p )
{
	int         c = 0, len;
	const char        *data;
	const char  *const *punc;
	static char token[ MAX_TOKEN_CHARS ];

	// multiple character punctuation tokens
	static const char *const punctuation[] =
	{
		"&&", "||", "<=", ">=", "==", "!=", nullptr
	};

	if ( !data_p )
	{
		Sys::Error( "ParseExpressionElement: NULL data_p" );
	}

	data = *data_p;
	len = 0;
	token[ 0 ] = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = nullptr;
		return token;
	}

	// skip whitespace
	while ( true )
	{
		// skip whitespace
		while ( ( c = *data ) <= ' ' )
		{
			if ( !c )
			{
				*data_p = nullptr;
				return token;
			}
			else if ( c == '\n' )
			{
				data++;
				*data_p = data;
				return token;
			}
			else
			{
				data++;
			}
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[ 1 ] == '/' )
		{
			data += 2;

			while ( *data && *data != '\n' )
			{
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[ 1 ] == '*' )
		{
			data += 2;

			while ( *data && ( *data != '*' || data[ 1 ] != '/' ) )
			{
				data++;
			}

			if ( *data )
			{
				data += 2;
			}
		}
		else
		{
			// a real token to parse
			break;
		}
	}

	// handle quoted strings
	if ( c == '\"' )
	{
		data++;

		while ( true )
		{
			c = *data++;

			if ( ( c == '\\' ) && ( *data == '\"' ) )
			{
				// allow quoted strings to use \" to indicate the " character
				data++;
			}
			else if ( c == '\"' || !c )
			{
				token[ len ] = 0;
				*data_p = ( char * ) data;
				return token;
			}

			if ( len < MAX_TOKEN_CHARS - 1 )
			{
				token[ len ] = c;
				len++;
			}
		}
	}

	// check for a number
	if ( ( c >= '0' && c <= '9' ) ||
	     ( c == '.' && data[ 1 ] >= '0' && data[ 1 ] <= '9' ) )
	{
		do
		{
			if ( len < MAX_TOKEN_CHARS - 1 )
			{
				token[ len ] = c;
				len++;
			}

			data++;

			c = *data;
		}
		while ( ( c >= '0' && c <= '9' ) || c == '.' );

		// parse the exponent
		if ( c == 'e' || c == 'E' )
		{
			if ( len < MAX_TOKEN_CHARS - 1 )
			{
				token[ len ] = c;
				len++;
			}

			data++;
			c = *data;

			if ( c == '-' || c == '+' )
			{
				if ( len < MAX_TOKEN_CHARS - 1 )
				{
					token[ len ] = c;
					len++;
				}

				data++;
				c = *data;
			}

			do
			{
				if ( len < MAX_TOKEN_CHARS - 1 )
				{
					token[ len ] = c;
					len++;
				}

				data++;

				c = *data;
			}
			while ( c >= '0' && c <= '9' );
		}

		if ( len == MAX_TOKEN_CHARS )
		{
			len = 0;
		}

		token[ len ] = 0;

		*data_p = ( char * ) data;
		return token;
	}

	// check for a regular word
	if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c == '_' ) )
	{
		do
		{
			if ( len < MAX_TOKEN_CHARS - 1 )
			{
				token[ len ] = c;
				len++;
			}

			data++;

			c = *data;
		}
		while ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c == '_' ) || ( c >= '0' && c <= '9' ) );

		if ( len == MAX_TOKEN_CHARS )
		{
			len = 0;
		}

		token[ len ] = 0;

		*data_p = ( char * ) data;
		return token;
	}

	// check for multi-character punctuation token
	for ( punc = punctuation; *punc; punc++ )
	{
		int l;
		int j;

		l = strlen( *punc );

		for ( j = 0; j < l; j++ )
		{
			if ( data[ j ] != ( *punc ) [ j ] )
			{
				break;
			}
		}

		if ( j == l )
		{
			// a valid multi-character punctuation
			memcpy( token, *punc, l );
			token[ l ] = 0;
			data += l;
			*data_p = ( char * ) data;
			return token;
		}
	}

	// single character punctuation
	token[ 0 ] = *data;
	token[ 1 ] = 0;
	data++;
	*data_p = ( char * ) data;

	return token;
}

/*
===============
ParseExpression
===============
*/
static void ParseExpression( const char **text, expression_t *exp )
{
	int            i;
	char           *token;

	expOperation_t op, op2;

	expOperation_t inFixOps[ MAX_EXPRESSION_OPS ];
	int            numInFixOps;

	// convert stack
	expOperation_t tmpOps[ MAX_EXPRESSION_OPS ];
	int            numTmpOps;

	numInFixOps = 0;
	numTmpOps = 0;

	exp->numOps = 0;

	// push left parenthesis on the stack
	op.type = opcode_t::OP_LPAREN;
	op.value = 0;
	inFixOps[ numInFixOps++ ] = op;

	while ( true )
	{
		token = ParseExpressionElement( text );

		if ( token[ 0 ] == 0 || token[ 0 ] == ',' )
		{
			break;
		}

		if ( numInFixOps == MAX_EXPRESSION_OPS )
		{
			Log::Warn("too many arithmetic expression operations in shader '%s'", shader.name );
			SkipRestOfLine( text );
			return;
		}

		GetOpType( token, &op );

		switch ( op.type )
		{
			case opcode_t::OP_BAD:
				Log::Warn("unknown token '%s' for arithmetic expression in shader '%s'", token,
				           shader.name );
				break;

			case opcode_t::OP_LBRACKET:
				inFixOps[ numInFixOps++ ] = op;

				// add extra (
				op2.type = opcode_t::OP_LPAREN;
				op2.value = 0;
				inFixOps[ numInFixOps++ ] = op2;
				break;

			case opcode_t::OP_RBRACKET:
				// add extra )
				op2.type = opcode_t::OP_RPAREN;
				op2.value = 0;
				inFixOps[ numInFixOps++ ] = op2;

				inFixOps[ numInFixOps++ ] = op;
				break;

			case opcode_t::OP_NUM:
				op.value = atof( token );
				inFixOps[ numInFixOps++ ] = op;
				break;

			case opcode_t::OP_TABLE:
				// value already set by GetOpType
				inFixOps[ numInFixOps++ ] = op;
				break;

			default:
				op.value = 0;
				inFixOps[ numInFixOps++ ] = op;
				break;
		}
	}

	// push right parenthesis on the stack
	op.type = opcode_t::OP_RPAREN;
	op.value = 0;
	inFixOps[ numInFixOps++ ] = op;

	for ( i = 0; i < ( numInFixOps - 1 ); i++ )
	{
		op = inFixOps[ i ];
		op2 = inFixOps[ i + 1 ];

		// convert OP_SUBs that should be unary into OP_NEG
		if ( op2.type == opcode_t::OP_SUB && op.type != opcode_t::OP_RPAREN && op.type != opcode_t::OP_TABLE && !IsOperand( op.type ) )
		{
			inFixOps[ i + 1 ].type = opcode_t::OP_NEG;
		}
	}

	// http://cis.stvincent.edu/swd/stl/stacks/stacks.html
	// http://www.qiksearch.com/articles/cs/infix-postfix/
	// http://www.experts-exchange.com/Programming/Programming_Languages/C/Q_20394130.html

	//
	// convert infix representation to postfix
	//

	for ( i = 0; i < numInFixOps; i++ )
	{
		op = inFixOps[ i ];

		// if current operator in infix is digit
		if ( IsOperand( op.type ) )
		{
			exp->ops[ exp->numOps++ ] = op;
		}
		// if current operator in infix is left parenthesis
		else if ( op.type == opcode_t::OP_LPAREN )
		{
			tmpOps[ numTmpOps++ ] = op;
		}
		// if current operator in infix is operator
		else if ( IsOperator( op.type ) )
		{
			while ( true )
			{
				if ( !numTmpOps )
				{
					Log::Warn("invalid infix expression in shader '%s'", shader.name );
					return;
				}
				else
				{
					// get top element
					op2 = tmpOps[ numTmpOps - 1 ];

					if ( IsOperator( op2.type ) )
					{
						if ( GetOpPrecedence( op2.type ) >= GetOpPrecedence( op.type ) )
						{
							exp->ops[ exp->numOps++ ] = op2;
							numTmpOps--;
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
			}

			tmpOps[ numTmpOps++ ] = op;
		}
		// if current operator in infix is right parenthesis
		else if ( op.type == opcode_t::OP_RPAREN )
		{
			while ( true )
			{
				if ( !numTmpOps )
				{
					Log::Warn("invalid infix expression in shader '%s'", shader.name );
					return;
				}
				else
				{
					// get top element
					op2 = tmpOps[ numTmpOps - 1 ];

					if ( op2.type != opcode_t::OP_LPAREN )
					{
						exp->ops[ exp->numOps++ ] = op2;
						numTmpOps--;
					}
					else
					{
						numTmpOps--;
						break;
					}
				}
			}
		}
	}

	// everything went ok
	exp->active = true;
}

/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		if ( *r_dpBlend )
		{
			// DarkPlaces only supports one alphaFunc operation: GE128
			Log::Warn("alphaFunc 'GT0' will be replaced by 'GE128' in shader '%s' because r_dpBlend compatibility layer is enabled", shader.name );
			return GLS_ATEST_GE_128;
		}
		else
		{
			if ( *r_dpMaterial )
			{
				Log::Warn("alphaFunc 'GT0' will not be replaced by 'GE128' in shader '%s' because r_dpBlend compatibility layer is disabled", shader.name );
			}
			return GLS_ATEST_GT_0;
		}
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return GLS_ATEST_LT_128;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return GLS_ATEST_GE_128;
	}
	else if ( !Q_stricmp( funcname, "LTENT" ) )
	{
		return GLS_ATEST_LT_ENT;
	}
	else if ( !Q_stricmp( funcname, "GTENT" ) )
	{
		return GLS_ATEST_GT_ENT;
	}

	Log::Warn("invalid alphaFunc name '%s' in shader '%s'", funcname, shader.name );
	return 0;
}

/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	Log::Warn("unknown blend mode '%s' in shader '%s', substituting GL_ONE", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	Log::Warn("unknown blend mode '%s' in shader '%s', substituting GL_ONE", name, shader.name );
	return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return genFunc_t::GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return genFunc_t::GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return genFunc_t::GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return genFunc_t::GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return genFunc_t::GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return genFunc_t::GF_NOISE;
	}

	Log::Warn("invalid genfunc name '%s' in shader '%s'", funcname, shader.name );
	return genFunc_t::GF_SIN;
}

/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm( const char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing waveform parm in shader '%s'", shader.name );
		return;
	}

	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing waveform parm in shader '%s'", shader.name );
		return;
	}

	wave->base = atof( token );

	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing waveform parm in shader '%s'", shader.name );
		return;
	}

	wave->amplitude = atof( token );

	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing waveform parm in shader '%s'", shader.name );
		return;
	}

	wave->phase = atof( token );

	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing waveform parm in shader '%s'", shader.name );
		return;
	}

	wave->frequency = atof( token );
}

/*
===================
ParseTexMod
===================
*/
static bool ParseTexMod( const char **text, shaderStage_t *stage )
{
	const char   *token;
	texModInfo_t *tmi;

	if ( stage->bundle[ 0 ].numTexMods == TR_MAX_TEXMODS )
	{
		Sys::Drop( "ERROR: too many tcMod stages in shader '%s'", shader.name );
	}

	tmi = &stage->bundle[ 0 ].texMods[ stage->bundle[ 0 ].numTexMods ];
	stage->bundle[ 0 ].numTexMods++;

	token = COM_ParseExt2( text, false );

	// turb
	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing tcMod turb parms in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.base = atof( token );
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing tcMod turb in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.amplitude = atof( token );
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing tcMod turb in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.phase = atof( token );
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing tcMod turb in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.frequency = atof( token );

		tmi->type = texMod_t::TMOD_TURBULENT;
	}
	// scale
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing scale parms in shader '%s'", shader.name );
			return false;
		}

		tmi->scale[ 0 ] = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing scale parms in shader '%s'", shader.name );
			return false;
		}

		tmi->scale[ 1 ] = atof( token );
		tmi->type = texMod_t::TMOD_SCALE;
	}
	// scroll
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing scale scroll parms in shader '%s'", shader.name );
			return false;
		}

		tmi->scroll[ 0 ] = atof( token );
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing scale scroll parms in shader '%s'", shader.name );
			return false;
		}

		tmi->scroll[ 1 ] = atof( token );
		tmi->type = texMod_t::TMOD_SCROLL;
	}
	// stretch
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing stretch parms in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing stretch parms in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.base = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing stretch parms in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.amplitude = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing stretch parms in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.phase = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing stretch parms in shader '%s'", shader.name );
			return false;
		}

		tmi->wave.frequency = atof( token );

		tmi->type = texMod_t::TMOD_STRETCH;
	}
	// transform
	else if ( !Q_stricmp( token, "transform" ) )
	{
		MatrixIdentity( tmi->matrix );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing transform parms in shader '%s'", shader.name );
			return false;
		}

		tmi->matrix[ 0 ] = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing transform parms in shader '%s'", shader.name );
			return false;
		}

		tmi->matrix[ 1 ] = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing transform parms in shader '%s'", shader.name );
			return false;
		}

		tmi->matrix[ 4 ] = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing transform parms in shader '%s'", shader.name );
			return false;
		}

		tmi->matrix[ 5 ] = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing transform parms in shader '%s'", shader.name );
			return false;
		}

		tmi->matrix[ 12 ] = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing transform parms in shader '%s'", shader.name );
			return false;
		}

		tmi->matrix[ 13 ] = atof( token );

		tmi->type = texMod_t::TMOD_TRANSFORM;
	}
	// rotate
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing tcMod rotate parms in shader '%s'", shader.name );
			return false;
		}

		tmi->rotateSpeed = atof( token );
		tmi->type = texMod_t::TMOD_ROTATE;
	}
	// entityTranslate
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = texMod_t::TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		Log::Warn("unknown tcMod '%s' in shader '%s'", token, shader.name );
		return false;
	}

	return true;
}

static bool ParseMap( const char **text, char *buffer, int bufferSize )
{
	int  len;
	char *token;

	// example
	// map textures/caves/tembrick1crum_local.tga

	while ( true )
	{
		token = COM_ParseExt2( text, false );

		if ( !token[ 0 ] )
		{
			// end of line
			break;
		}

		Q_strcat( buffer, bufferSize, token );
		Q_strcat( buffer, bufferSize, " " );
	}

	if ( !buffer[ 0 ] )
	{
		Log::Warn("'map' missing parameter in shader '%s'", shader.name );
		return false;
	}

	len = strlen( buffer );
	buffer[ len - 1 ] = 0; // replace last ' ' with tailing zero

	return true;
}

static bool LoadMap( shaderStage_t *stage, const char *buffer, const int bundleIndex = TB_COLORMAP )
{
	char         *token;
	filterType_t filterType;
	const char         *buffer_p = &buffer[ 0 ];

	if ( !buffer || !buffer[ 0 ] )
	{
		Log::Warn("NULL parameter for LoadMap in shader '%s'", shader.name );
		return false;
	}

	token = COM_ParseExt2( &buffer_p, false );

	// NOTE: Normal map can ship height map in alpha channel.
	if ( ( stage->type == stageType_t::ST_NORMALMAP && !r_normalMapping->integer && !r_reliefMapping->integer )
		|| ( stage->type == stageType_t::ST_HEIGHTMAP && !r_reliefMapping->integer )
		|| ( stage->type == stageType_t::ST_SPECULARMAP && !r_specularMapping->integer )
		|| ( stage->type == stageType_t::ST_PHYSICALMAP && !r_physicalMapping->integer )
		|| ( stage->type == stageType_t::ST_GLOWMAP && !r_glowMapping->integer )
		|| ( stage->type == stageType_t::ST_REFLECTIONMAP && !r_reflectionMapping->integer ) )
	{
		return true;
	}

	if ( !Q_stricmp( token, "$whiteimage" ) || !Q_stricmp( token, "$white" ) || !Q_stricmp( token, "_white" ) ||
	     !Q_stricmp( token, "*white" ) )
	{
		stage->bundle[ bundleIndex ].image[ 0 ] = tr.whiteImage;
		return true;
	}
	else if ( !Q_stricmp( token, "$blackimage" ) || !Q_stricmp( token, "$black" ) || !Q_stricmp( token, "_black" ) ||
	          !Q_stricmp( token, "*black" ) )
	{
		stage->bundle[ bundleIndex ].image[ 0 ] = tr.blackImage;
		return true;
	}
	else if ( !Q_stricmp( token, "$flatimage" ) || !Q_stricmp( token, "$flat" ) || !Q_stricmp( token, "_flat" ) ||
	          !Q_stricmp( token, "*flat" ) )
	{
		stage->bundle[ bundleIndex ].image[ 0 ] = tr.flatImage;
		return true;
	}

	else if ( !Q_stricmp( token, "$lightmap" ) )
	{
		stage->type = stageType_t::ST_LIGHTMAP;
		return true;
	}

	imageParams_t imageParams = {};
	imageParams.bits = 0;
	imageParams.minDimension = shader.imageMinDimension;
	imageParams.maxDimension = shader.imageMaxDimension;

	// determine image options
	if ( stage->overrideNoPicMip || shader.noPicMip || stage->highQuality || stage->forceHighQuality )
	{
		imageParams.bits |= IF_NOPICMIP;
	}

	switch ( stage->type )
	{
		case stageType_t::ST_NORMALMAP:
		case stageType_t::ST_HEATHAZEMAP:
		case stageType_t::ST_LIQUIDMAP:
			imageParams.bits |= IF_NORMALMAP;
		default:
			// silence warning for other types, we don't have to take care of them:
			//    warning: enumeration value ‘ST_GLOWMAP’ not handled in switch [-Wswitch]
			break;
	}

	if ( stage->stateBits & ( GLS_ATEST_BITS ) )
	{
		imageParams.bits |= IF_ALPHATEST; // FIXME: this is unused
	}

	if ( stage->overrideFilterType )
	{
		imageParams.filterType = stage->filterType;
	}
	else
	{
		imageParams.filterType = shader.filterType;
	}

	imageParams.wrapType = stage->overrideWrapType ? stage->wrapType : shader.wrapType;

	// try to load the image
	if ( stage->isCubeMap )
	{
		stage->bundle[ bundleIndex ].image[ 0 ] = R_FindCubeImage( buffer, &imageParams );

		if ( !stage->bundle[ bundleIndex ].image[ 0 ] )
		{
			Log::Warn("R_FindCubeImage could not find image '%s' in shader '%s'", buffer, shader.name );
			return false;
		}
	}
	else
	{
		stage->bundle[ bundleIndex ].image[ 0 ] = R_FindImageFile( buffer, &imageParams );

		if ( !stage->bundle[ bundleIndex ].image[ 0 ] )
		{
			Log::Warn("R_FindImageFile could not find image '%s' in shader '%s'", buffer, shader.name );
			return false;
		}
	}

	return true;
}

/*
===================
ParseClampType
===================
*/
static bool ParseClampType( char *token, wrapType_t *clamp )
{
	bool s = true, t = true;
	wrapTypeEnum_t type;

	// handle prefixing with 'S' or 'T'
	switch ( token[ 0 ] & 0xDF )
	{
	case 'S': t = false; ++token; break;
	case 'T': s = false; ++token; break;
	}

	// get the clamp type
	if      ( !Q_stricmp( token, "clamp"          ) ) { type = wrapTypeEnum_t::WT_CLAMP; }
	else if ( !Q_stricmp( token, "edgeClamp"      ) ) { type = wrapTypeEnum_t::WT_EDGE_CLAMP; }
	else if ( !Q_stricmp( token, "zeroClamp"      ) ) { type = wrapTypeEnum_t::WT_ZERO_CLAMP; }
	else if ( !Q_stricmp( token, "alphaZeroClamp" ) ) { type = wrapTypeEnum_t::WT_ALPHA_ZERO_CLAMP; }
	else if ( !Q_stricmp( token, "noClamp"        ) ) { type = wrapTypeEnum_t::WT_REPEAT; }
	else // not recognised
	{
		return false;
	}

	if (s) { clamp->s = type; }
	if (t) { clamp->t = type; }

	return true;
}

/*
===================
ParseDifuseMap
and others
===================
*/
static void ParseDiffuseMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_DIFFUSEMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_DIFFUSEMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseNormalMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_NORMALMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_NORMALMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;

	// because of collapsing, this affects other textures (diffuse, specular…) too
	if ( r_highQualityNormalMapping->integer )
	{
		stage->overrideFilterType = true;
		stage->filterType = filterType_t::FT_LINEAR;

		stage->overrideNoPicMip = true;
	}

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseNormalMapDetectHeightMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_NORMALMAP )
{
	/* Always call this function on assets known to never use RGTC format
	or the engine running on hardware with driver not implementing the
	GL_ARB_texture_compression_rgtc extension will wrongly assume a normal
	component stored in alpha channel is an heightmap and the renderer will
	select the wrong GLSL code for it, resulting in serious graphical issues.

	See https://github.com/DaemonEngine/Daemon/issues/375

	This code is meant to be used as compatibility code for games known
	to not use RGTC format and to requires alpha channel detection,
	like Xonotic/Darkplaces.

	Please do not implement material keyword for this function, never. */

	const char* initialText = *text;

	ParseNormalMap( stage, text, bundleIndex );

	/* Tell renderer to enable relief mapping since an heightmap is found,
	also tell renderer to not abuse normalmap alpha channel because it's an heightmap.

	See https://github.com/DaemonEngine/Daemon/issues/183#issuecomment-473691252 */

	if ( stage->bundle[ bundleIndex ].image[ 0 ]
		&& stage->bundle[ bundleIndex ].image[ 0 ]->bits & IF_NORMALMAP
		&& stage->bundle[ bundleIndex ].image[ 0 ]->bits & IF_ALPHA )
	{
		Log::defaultLogger.DoDebugCode([&] {
			char buffer[ 1024 ];
			buffer[ 0 ] = '\0';
			if ( !ParseMap( &initialText, buffer, sizeof( buffer ) ) )
			{
				ASSERT( false );
			}
			Log::Debug("Found heightmap embedded in normalmap '%s'", buffer);
		});

		stage->isHeightMapInNormalMap = true;
	}
}

static void ParseHeightMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_HEIGHTMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_HEIGHTMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseSpecularMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_SPECULARMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_SPECULARMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParsePhysicalMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_PHYSICALMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_PHYSICALMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseGlowMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_GLOWMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_GLOWMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE; // blend add

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseReflectionMap( shaderStage_t *stage, const char **text, const int bundleIndex = TB_REFLECTIONMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_REFLECTIONMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;
	stage->overrideWrapType = true;
	stage->wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		stage->isCubeMap = true;
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseReflectionMapBlended( shaderStage_t *stage, const char **text, const int bundleIndex = TB_REFLECTIONMAP )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_REFLECTIONMAP;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE;
	stage->overrideWrapType = true;
	stage->wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		stage->isCubeMap = true;
		LoadMap( stage, buffer, bundleIndex );
	}
}

static void ParseLightFalloffImage( shaderStage_t *stage, const char **text )
{
	char buffer[ 1024 ] = "";

	stage->active = true;
	stage->type = stageType_t::ST_ATTENUATIONMAP_Z;
	stage->rgbGen = colorGen_t::CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;
	stage->overrideWrapType = true;
	stage->wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;

	if ( ParseMap( text, buffer, sizeof( buffer ) ) )
	{
		LoadMap( stage, buffer );
	}
}

/* SetNormalFormat: set normal format for given stage if normal format
is not already set or it must be overwritten.

Set force option to true to overwrite existing normalFormat.

The idea is to call SetNormalFormat with force set to true when called by normalFormat keyword
so existing per-keyword format is overwritten, but keyword like normalMap does not set force
to true so their per-keyword format is only set if there is no normal format already set.

This ensures those two materials are rendered the same:

```
textures/castle/brick
{
		diffuseMap textures/castle/brick_d
		// set normalMap-specific normal format
		normalMap textures/castle/brick_n
		// overwrite with custom normal format
		normalFormat X Y Z
}
```

```
textures/castle/brick
{
		diffuseMap textures/castle/brick_d
		// set custom normal format
		normalFormat X Y Z
		// do not set normalMap-specific normal format
		//keep existing custom one
		normalMap textures/castle/brick_n
}
```
*/

void SetNormalFormat( shaderStage_t *stage, const vec3_t normalFormat, bool force = false )
{
	if ( !stage->hasNormalFormat || force )
	{
		stage->hasNormalFormat = true;

		for ( int i = 0; i < 3; i++ )
		{
			stage->normalFormat[ i ] = normalFormat[ i ];
		}
	}
}

struct extraMapParser_t
{
	const char *suffix;
	const char *description;
	void ( *parser ) ( shaderStage_t*, const char**, int );
	int bundleIndex;
};

static const extraMapParser_t dpExtraMapParsers[] =
{
	{ "_norm",    "DarkPlaces normal map",     ParseNormalMapDetectHeightMap,   TB_NORMALMAP, },
	{ "_bump",    "DarkPlaces height map",     ParseHeightMap,     TB_HEIGHTMAP, },
	{ "_gloss",   "DarkPlaces specular map",   ParseSpecularMap,   TB_SPECULARMAP, },
	{ "_glow",    "DarkPlaces glow map",       ParseGlowMap,       TB_GLOWMAP, },
	// DarkPlaces implemented _luma for Tenebrae compatibility, the
	// probability of finding this suffix with Xonotic maps is very low.
	{ "_luma",    "Tenebrae glow map",         ParseGlowMap,       TB_GLOWMAP, },
};

/*
=================
LoadExtraMaps

Look for implicit extra maps (normal, specular…) for the given image path
=================
*/
void LoadExtraMaps( shaderStage_t *stage, const char *colorMapName )
{
	if ( *r_dpMaterial )
	{
		/* DarkPlaces material compatibility

		note that the DarkPlaces renderer code is very different than Dæmon one
		so that code is just trying to produce the same results but it shares
		no one code at all

		note that dp* keywords will be processed elsewhere

		look if the stage to be parsed has a map keyword without a stage keyword,
		if there is look for extra maps based on their suffixes,
		if they exist load them,
		and tell the stage to be parsed is a diffuseMap one just before parsing it

		basically it will turn this:

		```
			textures/map_solarium/water4
			{
				qer_editorimage textures/map_solarium/water4/water4.tga
				qer_trans 20
				surfaceparm nomarks
				surfaceparm trans
				surfaceparm water
				surfaceparm nolightmap
				q3map_globaltexture
				tessSize 256
				cull none
				{
					map textures/map_solarium/water4/water4.tga
					tcmod scale  0.3  0.4
					tcMod scroll 0.05 0.05
					blendfunc add
					alphaGen  vertex
				}
				dpreflectcube cubemaps/default/sky
				{
					map $lightmap
					blendfunc add
					tcGen lightmap
				}
				dp_water 0.1 1.2  1.4 0.7  1 1 1  1 1 1  0.1
			}
		```

		into this:

		```
			textures/map_solarium/water4
			{
				qer_editorimage textures/map_solarium/water4/water4
				qer_trans 20
				surfaceparm nomarks
				surfaceparm trans
				surfaceparm water
				surfaceparm nolightmap
				q3map_globaltexture
				tessSize 256
				cull none
				{
					diffuseMap  textures/map_solarium/water4/water4
					normalMap   textures/map_solarium/water4/water4_norm
					specularMap textures/map_solarium/water4/water4_gloss
					normalFormat +X +Y +Z
					tcmod scale  0.3  0.4
					tcMod scroll 0.05 0.05
					blendfunc add
					alphaGen  vertex
				}
				dpreflectcube cubemaps/default/sky
				dp_water 0.1 1.2  1.4 0.7  1 1 1  1 1 1  0.1
			}
		```

		*/

		// do not look for extramap for $whiteimage etc.
		switch ( colorMapName[0] )
		{
			case '$':
			case '_':
			case '*':
				return;
			default:
				break;
		}

		char colorMapBaseName[ MAX_QPATH ];
		bool foundExtraMap = false;

		Log::Debug( "looking for DarkPlaces extra maps for color map: '%s'", colorMapName );

		COM_StripExtension3( colorMapName, colorMapBaseName, sizeof( colorMapBaseName ) );

		for ( const extraMapParser_t parser: dpExtraMapParsers )
		{
			std::string extraMapName = Str::Format( "%s%s", colorMapBaseName, parser.suffix );
			if( R_FindImageLoader( extraMapName.c_str() ) >= 0 )
			{
				foundExtraMap = true;
				Log::Debug( "found extra %s '%s'", parser.description, extraMapName.c_str() );

				const char *name = extraMapName.c_str();
				parser.parser( stage, &name, parser.bundleIndex );

				if ( parser.bundleIndex == TB_NORMALMAP )
				{
					// Xonotic uses +X +Y +Z (OpenGL)
					SetNormalFormat( stage, glNormalFormat );
				}
			}
		}

		if ( foundExtraMap )
		{
			stage->type = stageType_t::ST_COLLAPSE_lighting_PHONG;
			stage->collapseType = collapseType_t::COLLAPSE_lighting_PHONG;
			stage->dpMaterial = true;
		}
	}
}

/*
===================
ParseStage
===================
*/
static bool ParseStage( shaderStage_t *stage, const char **text )
{
	char         *token;
	int          colorMaskBits = 0;
	int depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0, polyModeBits = 0;
	bool     depthMaskExplicit = false;
	int          imageBits = 0;
	filterType_t filterType;
	char         buffer[ 1024 ] = "";
	bool     loadMap = false;

	while ( true )
	{
		token = COM_ParseExt2( text, true );

		if ( !token[ 0 ] )
		{
			Log::Warn("no matching '}' found" );
			return false;
		}

		if ( token[ 0 ] == '}' )
		{
			break;
		}
		// if(<condition>)
		else if ( !Q_stricmp( token, "if" ) )
		{
			ParseExpression( text, &stage->ifExp );
		}
		// map <name>
		else if ( !Q_stricmp( token, "map" ) )
		{
			if ( !ParseMap( text, buffer, sizeof( buffer ) ) )
			{
				return false;
			}
			else
			{
				LoadExtraMaps( stage, buffer );

				loadMap = true;
			}
		}
		// collapsed diffuse stage enables implicit lightmap
		else if ( !Q_stricmp( token, "diffuseMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_none )
			{
				stage->collapseType = collapseType_t::COLLAPSE_generic;
			}

			ParseDiffuseMap( stage, text );
			stage->implicitLightmap = true;
		}
		else if ( !Q_stricmp( token, "normalMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_none )
			{
				stage->collapseType = collapseType_t::COLLAPSE_generic;
			}

			ParseNormalMap( stage, text );
			SetNormalFormat( stage, dxNormalFormat );
		}
		else if ( !Q_stricmp( token, "normalHeightMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_none )
			{
				stage->collapseType = collapseType_t::COLLAPSE_generic;
			}

			stage->isHeightMapInNormalMap = true;
			ParseNormalMap( stage, text );
			SetNormalFormat( stage, dxNormalFormat );
		}
		else if ( !Q_stricmp( token, "heightMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_none )
			{
				stage->collapseType = collapseType_t::COLLAPSE_generic;
			}

			ParseHeightMap( stage, text );
		}
		else if ( !Q_stricmp( token, "specularMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_reflection_CB )
			{
				Log::Warn("Supposedly you shouldn't have a physicalMap after a reflectionMap (in shader '%s')?", shader.name);
				// use PHONG instead
			}

			if ( stage->collapseType == collapseType_t::COLLAPSE_lighting_PBR )
			{
				Log::Warn("Supposedly you shouldn't have a specularMap after a physicalMap (in shader '%s')?", shader.name);
				// keep PBR instead
			}
			else
			{
				stage->collapseType = collapseType_t::COLLAPSE_lighting_PHONG;
				ParseSpecularMap( stage, text );
			}
		}
		else if ( !Q_stricmp( token, "physicalMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_reflection_CB )
			{
				Log::Warn("Supposedly you shouldn't have a physicalMap after a reflectionMap (in shader '%s')?", shader.name);
				// use PBR instead
			}
			else if ( stage->collapseType == collapseType_t::COLLAPSE_lighting_PHONG )
			{
				Log::Warn("Supposedly you shouldn't have a physicalMap after a specularMap (in shader '%s')?", shader.name);
				// use PBR instead
			}

			// Daemon PBR packing defaults to ORM like glTF 2.0
			stage->collapseType = collapseType_t::COLLAPSE_lighting_PBR;
			ParsePhysicalMap( stage, text );
		}
		else if ( !Q_stricmp( token, "glowMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_none )
			{
				stage->collapseType = collapseType_t::COLLAPSE_generic;
			}

			ParseGlowMap( stage, text );
		}
		else if ( !Q_stricmp( token, "reflectionMap" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_lighting_PBR
				|| stage->collapseType == collapseType_t::COLLAPSE_lighting_PHONG )
			{
				Log::Warn("Supposedly you shouldn't have a reflectionMap after a physicalMap or a specularMap (in shader '%s')?", shader.name);
				// use reflectionMap instead
			}

			stage->collapseType = collapseType_t::COLLAPSE_reflection_CB;
			ParseReflectionMap( stage, text );
		}
		else if ( !Q_stricmp( token, "reflectionMapBlended" ) )
		{
			if ( stage->collapseType == collapseType_t::COLLAPSE_lighting_PBR
				|| stage->collapseType == collapseType_t::COLLAPSE_lighting_PHONG )
			{
				Log::Warn("Supposedly you shouldn't have a reflectionMapBlended after a physicalMap or a specularMap (in shader '%s')?", shader.name);
				// use reflectionMapBlended instead
			}

			stage->collapseType = collapseType_t::COLLAPSE_reflection_CB;
			ParseReflectionMapBlended( stage, text );
		}
		/* not handled yet:
		else if ( !Q_stricmp( token, "refractionMap" ) )
		else if ( !Q_stricmp( token, "dispersionMap" ) )
		else if ( !Q_stricmp( token, "skyboxMap" ) )
		else if ( !Q_stricmp( token, "screenMap" ) )
		else if ( !Q_stricmp( token, "portalMap" ) )
		else if ( !Q_stricmp( token, "heathazeMap" ) )
		else if ( !Q_stricmp( token, "liquidMap" ) )
		else if ( !Q_stricmp( token, "attenuationMapXY" ) )
		else if ( !Q_stricmp( token, "attenuationMapZ" ) )
		*/
		// lightmap <name>
		else if ( !Q_stricmp( token, "lightmap" ) )
		{
			if ( !ParseMap( text, buffer, sizeof( buffer ) ) )
			{
				return false;
			}
			else
			{
				loadMap = true;
				imageBits |= IF_LIGHTMAP;
				imageBits |= IF_NOPICMIP;
			}
		}
		// clampmap <name>
		else if ( !Q_stricmp( token, "clampmap" ) )
		{
			if ( stage->collapseType != collapseType_t::COLLAPSE_none )
			{
				Log::Warn("keyword '%s' cannot be used in collapsed shader '%s'", token, shader.name );
			}

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parameter for 'clampmap' keyword in shader '%s'", shader.name );
				return false;
			}

			imageBits = 0;

			if ( stage->overrideNoPicMip || shader.noPicMip )
			{
				imageBits |= IF_NOPICMIP;
			}

			if ( stage->overrideFilterType )
			{
				filterType = stage->filterType;
			}
			else
			{
				filterType = shader.filterType;
			}

			imageParams_t imageParams = {};
			imageParams.bits = imageBits;
			imageParams.filterType = filterType;
			imageParams.wrapType = wrapTypeEnum_t::WT_CLAMP;
			imageParams.minDimension = shader.imageMinDimension;
			imageParams.maxDimension = shader.imageMaxDimension;

			stage->bundle[ 0 ].image[ 0 ] = R_FindImageFile( token, &imageParams );

			if ( !stage->bundle[ 0 ].image[ 0 ] )
			{
				Log::Warn("R_FindImageFile could not find '%s' in shader '%s'", token, shader.name );
				return false;
			}
		}
		// animMap <frequency> <image1> .... <imageN>
		else if ( !Q_stricmp( token, "animMap" ) )
		{
			if ( stage->collapseType != collapseType_t::COLLAPSE_none )
			{
				Log::Warn("keyword '%s' cannot be used in collapsed shader '%s'", token, shader.name );
			}

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parameter for 'animMap' keyword in shader '%s'", shader.name );
				return false;
			}

			stage->bundle[ 0 ].imageAnimationSpeed = atof( token );

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while ( true )
			{
				int num;

				token = COM_ParseExt2( text, false );

				if ( !token[ 0 ] )
				{
					break;
				}

				num = stage->bundle[ 0 ].numImages;

				if ( num < MAX_IMAGE_ANIMATIONS )
				{
					imageParams_t imageParams = {};
					imageParams.bits = IF_NONE;
					imageParams.filterType = filterType_t::FT_DEFAULT;
					imageParams.wrapType = wrapTypeEnum_t::WT_REPEAT;
					imageParams.minDimension = shader.imageMinDimension;
					imageParams.maxDimension = shader.imageMaxDimension;

					stage->bundle[ 0 ].image[ num ] = R_FindImageFile( token, &imageParams );

					if ( !stage->bundle[ 0 ].image[ num ] )
					{
						Log::Warn("R_FindImageFile could not find '%s' in shader '%s'", token,
						           shader.name );
						return false;
					}

					stage->bundle[ 0 ].numImages++;
				}
			}
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
			Log::Warn("videoMap unsupported");
			// Discussion about video support: https://github.com/DaemonEngine/Daemon/pull/391
			COM_ParseExt2( text, false );
		}
		// cubeMap <map>
		else if ( !Q_stricmp( token, "cubeMap" ) || !Q_stricmp( token, "cameraCubeMap" ) )
		{
			if ( stage->collapseType != collapseType_t::COLLAPSE_none )
			{
				Log::Warn("keyword '%s' cannot be used in collapsed shader '%s'", token, shader.name );
			}

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parameter for 'cubeMap' keyword in shader '%s'", shader.name );
				return false;
			}

			imageBits = 0;

			if ( stage->overrideNoPicMip || shader.noPicMip )
			{
				imageBits |= IF_NOPICMIP;
			}

			if ( stage->overrideFilterType )
			{
				filterType = stage->filterType;
			}
			else
			{
				filterType = shader.filterType;
			}

			imageParams_t imageParams = {};
			imageParams.bits = imageBits;
			imageParams.filterType = filterType;
			imageParams.wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;
			imageParams.minDimension = shader.imageMinDimension;
			imageParams.maxDimension = shader.imageMaxDimension;

			stage->bundle[ 0 ].image[ 0 ] = R_FindCubeImage( token, &imageParams );

			if ( !stage->bundle[ 0 ].image[ 0 ] )
			{
				Log::Warn("R_FindCubeImage could not find '%s' in shader '%s'", token, shader.name );
				return false;
			}
		}
		// alphafunc <func>
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parameter for 'alphaFunc' keyword in shader '%s'", shader.name );
				return false;
			}

			atestBits = NameToAFunc( token );
		}
		// alphaTest <exp>
		else if ( !Q_stricmp( token, "alphaTest" ) )
		{
			atestBits = GLS_ATEST_GE_128;
			ParseExpression( text, &stage->alphaTestExp );
		}
		// depthFunc <func>
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parameter for 'depthfunc' keyword in shader '%s'", shader.name );
				return false;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthFuncBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				Log::Warn("unknown depthfunc '%s' in shader '%s'", token, shader.name );
				continue;
			}
		}
		// ignoreAlphaTest
		else if ( !Q_stricmp( token, "ignoreAlphaTest" ) )
		{
			depthFuncBits = 0;
		}
		// nearest
		else if ( !Q_stricmp( token, "nearest" ) )
		{
			stage->overrideFilterType = true;
			stage->filterType = filterType_t::FT_NEAREST;
		}
		// linear
		else if ( !Q_stricmp( token, "linear" ) )
		{
			stage->overrideFilterType = true;
			stage->filterType = filterType_t::FT_LINEAR;

			stage->overrideNoPicMip = true;
		}
		// noPicMip
		else if ( !Q_stricmp( token, "noPicMip" ) )
		{
			stage->overrideNoPicMip = true;
		}
		// clamp, edgeClamp etc.
		else if ( ParseClampType( token, &stage->wrapType ) )
		{
			stage->overrideWrapType = true;
		}
		// uncompressed (removed option)
		else if ( !Q_stricmp( token, "uncompressed" ) )
		{
		}
		// highQuality
		else if ( !Q_stricmp( token, "highQuality" ) )
		{
			stage->highQuality = true;
			stage->overrideNoPicMip = true;
		}
		// forceHighQuality
		else if ( !Q_stricmp( token, "forceHighQuality" ) )
		{
			stage->forceHighQuality = true;
			stage->overrideNoPicMip = true;
		}
		// ET fog
		else if ( !Q_stricmp( token, "fog" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing parm for fog in shader '%s'", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "on" ) )
			{
				stage->noFog = false;
			}
			else
			{
				stage->noFog = true;
			}
		}
		else if ( !Q_stricmp( token, "depthFade" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parameter for 'depthFade' keyword in shader '%s'", shader.name );
				return false;
			}

			stage->hasDepthFade = true;
			stage->depthFadeValue = atof( token );

			if ( stage->depthFadeValue <= 0.0f )
			{
				Log::Warn("depthFade parameter <= 0 in '%s'", shader.name );
				stage->depthFadeValue = 1.0f;
			}

		}
		//    blend[func] <srcFactor> [,] <dstFactor>
		// or blend[func] <add | filter | blend>
		// or blend[func] <diffusemap | bumpmap | specularmap>
		else if ( !Q_stricmp( token, "blendfunc" ) ||
			  !Q_stricmp( token, "blend" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing parm for blend in shader '%s'", shader.name );
				continue;
			}

			// check for "simple" blends first
			if ( !Q_stricmp( token, "add" ) )
			{
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			else if ( !Q_stricmp( token, "filter" ) )
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if ( !Q_stricmp( token, "modulate" ) )
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if ( !Q_stricmp( token, "blend" ) )
			{
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			}
			else if ( !Q_stricmp( token, "none" ) )
			{
				blendSrcBits = GLS_SRCBLEND_ZERO;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			// check for other semantic meanings
			else if ( !Q_stricmp( token, "diffuseMap" ) )
			{
				Log::Warn("deprecated idTech4 blend parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_DIFFUSEMAP;
				stage->implicitLightmap = true;
			}
			else if ( !Q_stricmp( token, "normalMap" ) )
			{
				Log::Warn("deprecated idTech4 blend parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_NORMALMAP;
				SetNormalFormat( stage, dxNormalFormat );
			}
			else if ( !Q_stricmp( token, "bumpMap" ) )
			{
				Log::Warn("deprecated idTech4 blend parameter '%s' in shader '%s', better use 'normalMap' keyword in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_NORMALMAP;
				SetNormalFormat( stage, dxNormalFormat );
			}
			else if ( !Q_stricmp( token, "specularMap" ) )
			{
				Log::Warn("deprecated idTech4 blend parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_SPECULARMAP;
			}
			else if ( !Q_stricmp( token, "physicalMap" ) )
			{
				Log::Warn("deprecated idTech4 blend parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_PHYSICALMAP;
			}
			else if ( !Q_stricmp( token, "glowMap" ) )
			{
				Log::Warn("deprecated idTech4 blend parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_GLOWMAP;
			}
			else
			{
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt2( text, false );

				if ( !Q_stricmp(token, "," ) ) {
					token = COM_ParseExt2( text, false );
				}

				if ( token[ 0 ] == 0 )
				{
					Log::Warn("missing parm for blend in shader '%s'", shader.name );
					continue;
				}

				blendDstBits = NameToDstBlendMode( token );
			}

			// clear depth mask for blended surfaces
			if ( !depthMaskExplicit &&
			     (stage->type == stageType_t::ST_COLORMAP ||
			      stage->type == stageType_t::ST_DIFFUSEMAP ||
			      stage->collapseType != collapseType_t::COLLAPSE_none ) &&
			     blendSrcBits != 0 && blendDstBits != 0
			     && !(blendSrcBits == GLS_SRCBLEND_ONE &&
				  blendDstBits == GLS_DSTBLEND_ZERO) )
			{
				depthMaskBits = 0;
			}
		}
		// stage <type>
		else if ( !Q_stricmp( token, "stage" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing parameters for stage in shader '%s'", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "diffuseMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_DIFFUSEMAP;
				stage->implicitLightmap = true;
			}
			else if ( !Q_stricmp( token, "normalMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_NORMALMAP;
				SetNormalFormat( stage, dxNormalFormat );
			}
			else if ( !Q_stricmp( token, "bumpMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use 'normalMap' keyword in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_NORMALMAP;
				SetNormalFormat( stage, dxNormalFormat );
			}
			else if ( !Q_stricmp( token, "specularMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_SPECULARMAP;
			}
			else if ( !Q_stricmp( token, "physicalMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_PHYSICALMAP;
			}
			else if ( !Q_stricmp( token, "glowMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_GLOWMAP;
			}
			else if ( !Q_stricmp( token, "reflectionMap" ) )
			{
				Log::Warn("deprecated XreaL stage parameter '%s' in shader '%s', better use it in place of 'map' keyword and pack related textures within the same stage", token, shader.name );
				stage->type = stageType_t::ST_REFLECTIONMAP;
			}
			else if ( !Q_stricmp( token, "refractionMap" ) )
			{
				stage->type = stageType_t::ST_REFRACTIONMAP;
			}
			else if ( !Q_stricmp( token, "dispersionMap" ) )
			{
				stage->type = stageType_t::ST_DISPERSIONMAP;
			}
			else if ( !Q_stricmp( token, "skyboxMap" ) )
			{
				stage->type = stageType_t::ST_SKYBOXMAP;
			}
			else if ( !Q_stricmp( token, "screenMap" ) )
			{
				stage->type = stageType_t::ST_SCREENMAP;
			}
			else if ( !Q_stricmp( token, "portalMap" ) )
			{
				stage->type = stageType_t::ST_PORTALMAP;
			}
			else if ( !Q_stricmp( token, "heathazeMap" ) )
			{
				stage->type = stageType_t::ST_HEATHAZEMAP;
				SetNormalFormat( stage, dxNormalFormat );
			}
			else if ( !Q_stricmp( token, "liquidMap" ) )
			{
				stage->type = stageType_t::ST_LIQUIDMAP;
				SetNormalFormat( stage, dxNormalFormat );
			}
			else if ( !Q_stricmp( token, "attenuationMapXY" ) )
			{
				stage->type = stageType_t::ST_ATTENUATIONMAP_XY;
			}
			else if ( !Q_stricmp( token, "attenuationMapZ" ) )
			{
				stage->type = stageType_t::ST_ATTENUATIONMAP_Z;
			}
			else
			{
				Log::Warn("unknown stage parameter '%s' in shader '%s'", token, shader.name );
				continue;
			}
		}
		// rgbGen
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing parameters for rgbGen in shader '%s'", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = colorGen_t::CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				vec3_t color;

				ParseVector( text, 3, color );
				stage->constantColor.SetRed( 255 * color[0] );
				stage->constantColor.SetGreen( 255 * color[1] );
				stage->constantColor.SetBlue( 255 * color[2] );

				stage->rgbGen = colorGen_t::CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_VERTEX;

				if ( stage->alphaGen == alphaGen_t::AGEN_IDENTITY )
				{
					stage->alphaGen = alphaGen_t::AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				//Log::Warn("obsolete rgbGen lightingDiffuse keyword not supported in shader '%s'", shader.name);
				stage->type = stageType_t::ST_DIFFUSEMAP;
				stage->rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				Log::Warn("unknown rgbGen parameter '%s' in shader '%s'", token, shader.name );
				continue;
			}
		}
		// rgb <arithmetic expression>
		else if ( !Q_stricmp( token, "rgb" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_CUSTOM_RGB;
			ParseExpression( text, &stage->rgbExp );
		}
		// red <arithmetic expression>
		else if ( !Q_stricmp( token, "red" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_CUSTOM_RGBs;
			ParseExpression( text, &stage->redExp );
		}
		// green <arithmetic expression>
		else if ( !Q_stricmp( token, "green" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_CUSTOM_RGBs;
			ParseExpression( text, &stage->greenExp );
		}
		// blue <arithmetic expression>
		else if ( !Q_stricmp( token, "blue" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_CUSTOM_RGBs;
			ParseExpression( text, &stage->blueExp );
		}
		// colored
		else if ( !Q_stricmp( token, "colored" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_ENTITY;
			stage->alphaGen = alphaGen_t::AGEN_ENTITY;
		}
		// vertexColor
		else if ( !Q_stricmp( token, "vertexColor" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_VERTEX;

			if ( stage->alphaGen == alphaGen_t::AGEN_IDENTITY )
			{
				stage->alphaGen = alphaGen_t::AGEN_VERTEX;
			}
		}
		// inverseVertexColor
		else if ( !Q_stricmp( token, "inverseVertexColor" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_ONE_MINUS_VERTEX;

			if ( stage->alphaGen == alphaGen_t::AGEN_IDENTITY )
			{
				stage->alphaGen = alphaGen_t::AGEN_ONE_MINUS_VERTEX;
			}
		}
		// alphaGen
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing parameters for alphaGen in shader '%s'", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = alphaGen_t::AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt2( text, false );
				stage->constantColor.SetAlpha( 255 * atof( token ) );
				stage->alphaGen = alphaGen_t::AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				Log::Warn("alphaGen lightingSpecular keyword not supported in shader '%s'", shader.name );
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				Log::Warn("alphaGen portal keyword not supported in shader '%s'", shader.name );
				//stage->type = ST_PORTALMAP;
				stage->alphaGen = alphaGen_t::AGEN_CONST;
				stage->constantColor.SetAlpha( 0 );
				SkipRestOfLine( text );
			}
			else
			{
				Log::Warn("unknown alphaGen parameter '%s' in shader '%s'", token, shader.name );
				continue;
			}
		}
		// alpha <arithmetic expression>
		else if ( !Q_stricmp( token, "alpha" ) )
		{
			stage->alphaGen = alphaGen_t::AGEN_CUSTOM;
			ParseExpression( text, &stage->alphaExp );
		}
		// color <exp>, <exp>, <exp>, <exp>
		else if ( !Q_stricmp( token, "color" ) )
		{
			stage->rgbGen = colorGen_t::CGEN_CUSTOM_RGBs;
			stage->alphaGen = alphaGen_t::AGEN_CUSTOM;
			ParseExpression( text, &stage->redExp );
			ParseExpression( text, &stage->greenExp );
			ParseExpression( text, &stage->blueExp );
			ParseExpression( text, &stage->alphaExp );
		}
		// tcGen <function>
		else if ( !Q_stricmp( token, "texGen" ) || !Q_stricmp( token, "tcGen" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing texGen parm in shader '%s'", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				//Log::Warn("texGen environment keyword not supported in shader '%s'", shader.name);
				stage->tcGen_Environment = true;
				stage->tcGen_Lightmap = false;
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				imageBits |= IF_LIGHTMAP;
				imageBits |= IF_NOPICMIP;
				stage->tcGen_Lightmap = true;
				stage->tcGen_Environment = false;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
			}
			else if ( !Q_stricmp( token, "reflect" ) )
			{
				stage->type = stageType_t::ST_REFLECTIONMAP;
			}
			else if ( !Q_stricmp( token, "skybox" ) )
			{
				stage->type = stageType_t::ST_SKYBOXMAP;
			}
			else
			{
				Log::Warn("unknown tcGen keyword '%s' not supported in shader '%s'", token, shader.name );
				SkipRestOfLine( text );
			}
		}
		// tcMod <type> <...>
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			if ( !ParseTexMod( text, stage ) )
			{
				return false;
			}
		}
		// scroll
		else if ( !Q_stricmp( token, "scroll" ) || !Q_stricmp( token, "translate" ) )
		{
			texModInfo_t *tmi;

			if ( stage->bundle[ 0 ].numTexMods == TR_MAX_TEXMODS )
			{
				Sys::Drop( "ERROR: too many tcMod stages in shader '%s'", shader.name );
			}

			tmi = &stage->bundle[ 0 ].texMods[ stage->bundle[ 0 ].numTexMods ];
			stage->bundle[ 0 ].numTexMods++;

			ParseExpression( text, &tmi->sExp );
			ParseExpression( text, &tmi->tExp );

			tmi->type = texMod_t::TMOD_SCROLL2;
		}
		// scale
		else if ( !Q_stricmp( token, "scale" ) )
		{
			texModInfo_t *tmi;

			if ( stage->bundle[ 0 ].numTexMods == TR_MAX_TEXMODS )
			{
				Sys::Drop( "ERROR: too many tcMod stages in shader '%s'", shader.name );
			}

			tmi = &stage->bundle[ 0 ].texMods[ stage->bundle[ 0 ].numTexMods ];
			stage->bundle[ 0 ].numTexMods++;

			ParseExpression( text, &tmi->sExp );
			ParseExpression( text, &tmi->tExp );

			tmi->type = texMod_t::TMOD_SCALE2;
		}
		// centerScale
		else if ( !Q_stricmp( token, "centerScale" ) )
		{
			texModInfo_t *tmi;

			if ( stage->bundle[ 0 ].numTexMods == TR_MAX_TEXMODS )
			{
				Sys::Drop( "ERROR: too many tcMod stages in shader '%s'", shader.name );
			}

			tmi = &stage->bundle[ 0 ].texMods[ stage->bundle[ 0 ].numTexMods ];
			stage->bundle[ 0 ].numTexMods++;

			ParseExpression( text, &tmi->sExp );
			ParseExpression( text, &tmi->tExp );

			tmi->type = texMod_t::TMOD_CENTERSCALE;
		}
		// shear
		else if ( !Q_stricmp( token, "shear" ) )
		{
			texModInfo_t *tmi;

			if ( stage->bundle[ 0 ].numTexMods == TR_MAX_TEXMODS )
			{
				Sys::Drop( "ERROR: too many tcMod stages in shader '%s'", shader.name );
			}

			tmi = &stage->bundle[ 0 ].texMods[ stage->bundle[ 0 ].numTexMods ];
			stage->bundle[ 0 ].numTexMods++;

			ParseExpression( text, &tmi->sExp );
			ParseExpression( text, &tmi->tExp );

			tmi->type = texMod_t::TMOD_SHEAR;
		}
		// rotate
		else if ( !Q_stricmp( token, "rotate" ) )
		{
			texModInfo_t *tmi;

			if ( stage->bundle[ 0 ].numTexMods == TR_MAX_TEXMODS )
			{
				Sys::Drop( "ERROR: too many tcMod stages in shader '%s'", shader.name );
			}

			tmi = &stage->bundle[ 0 ].texMods[ stage->bundle[ 0 ].numTexMods ];
			stage->bundle[ 0 ].numTexMods++;

			ParseExpression( text, &tmi->rExp );

			tmi->type = texMod_t::TMOD_ROTATE2;
		}
		// depthwrite
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = true;
			continue;
		}
		// maskRed
		else if ( !Q_stricmp( token, "maskRed" ) )
		{
			colorMaskBits |= GLS_REDMASK_FALSE;
		}
		// maskGreen
		else if ( !Q_stricmp( token, "maskGreen" ) )
		{
			colorMaskBits |= GLS_GREENMASK_FALSE;
		}
		// maskBlue
		else if ( !Q_stricmp( token, "maskBlue" ) )
		{
			colorMaskBits |= GLS_BLUEMASK_FALSE;
		}
		// maskAlpha
		else if ( !Q_stricmp( token, "maskAlpha" ) )
		{
			colorMaskBits |= GLS_ALPHAMASK_FALSE;
		}
		// maskColor
		else if ( !Q_stricmp( token, "maskColor" ) )
		{
			colorMaskBits |= GLS_REDMASK_FALSE | GLS_GREENMASK_FALSE | GLS_BLUEMASK_FALSE;
		}
		// maskColorAlpha
		else if ( !Q_stricmp( token, "maskColorAlpha" ) )
		{
			colorMaskBits |= GLS_REDMASK_FALSE | GLS_GREENMASK_FALSE | GLS_BLUEMASK_FALSE | GLS_ALPHAMASK_FALSE;
		}
		// maskDepth
		else if ( !Q_stricmp( token, "maskDepth" ) )
		{
			depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = false;
		}
		// wireFrame
		else if ( !Q_stricmp( token, "wireFrame" ) )
		{
			polyModeBits |= GLS_POLYMODE_LINE;
		}
		else if ( !Q_stricmp( token, "specularExponentMin" ) )
		{
			ParseExpression( text, &stage->specularExponentMin );
		}
		else if ( !Q_stricmp( token, "specularExponentMax" ) )
		{
			ParseExpression( text, &stage->specularExponentMax );
		}
		// refractionIndex <arithmetic expression>
		else if ( !Q_stricmp( token, "refractionIndex" ) )
		{
			ParseExpression( text, &stage->refractionIndexExp );
		}
		// fresnelPower <arithmetic expression>
		else if ( !Q_stricmp( token, "fresnelPower" ) )
		{
			ParseExpression( text, &stage->fresnelPowerExp );
		}
		// fresnelScale <arithmetic expression>
		else if ( !Q_stricmp( token, "fresnelScale" ) )
		{
			ParseExpression( text, &stage->fresnelScaleExp );
		}
		// fresnelBias <arithmetic expression>
		else if ( !Q_stricmp( token, "fresnelBias" ) )
		{
			ParseExpression( text, &stage->fresnelBiasExp );
		}
		/* normalFormat <[-]X> <[-]Y> <[-]Z>

		Describes the normal map format for the the given normal map file.

		Since this parser assumes DirectX format with normalMap keyword
		to keep compatibility with materials created for ioquake3

		To load normal maps in OpenGL format using normalMap keyword, use:
		normalFormat X Y Z

		OpenGL format is described there:
		https://github.com/KhronosGroup/glTF/tree/2.0/specification/2.0#materialnormaltexture

		> The normal vector uses OpenGL conventions where +X is right, +Y is up, and +Z points toward the viewer.

		examples of formats:

		 X  Y  Z OpenGL format
		 X -Y  Z DirectX format with reverted green channel (reverted Y component)
		-X  Y  Z weird other format with reverted red channel (reverted X component)

		Unlike normalScale that can be used to revert channels by setting them
		negative, the normalFormat keyword is meant to be engine agnostic:
		engines implementing either OpenGL or DirectX format internally
		are expected to do the required normal channel flip themselves.
		*/
		else if ( !Q_stricmp( token, "normalFormat" ) )
		{
			const char* components[3] = { "X", "Y", "Z" };
			vec3_t normalFormat;

			for ( int i = 0; i < 3; i++ )
			{
				token = COM_ParseExt( text, false );

				if ( !Q_stricmp( token, components[ i ] ) )
				{
					normalFormat[ i ] = 1.0;
				}
				else if ( token[ 0 ] == '-' && !Q_stricmp( token + 1, components[ i ] ) )
				{
					normalFormat[ i ] = -1.0;
				}
				else
				{
					if ( token[ 0 ] == '\0' )
					{
						Log::Warn("missing normalFormat parm in shader '%s'", shader.name );
					}
					else
					{
						Log::Warn("unknown normalFormat parm in shader '%s': '%s'", shader.name, token );
					}

					break;
				}

				if ( i == 2 )
				{
					// If all three components are read properly,
					// set normal format, replace existing one if exists.
					SetNormalFormat( stage, normalFormat, true );
				}
			}

			SkipRestOfLine( text );
			continue;
		}
		/* normalScale <float> <float> <float>

		Scales normal map channels.

		For compatibility purpose, ioquake3 renderer2 syntax is supported:
		normalScale <float> <float>

		In this case it will be read like this:
		normalScale <float> <float> 1.0

		Using 1.0 as multiplier will have no effect on Z channel.

		The normalScale keyword can also be used to flip normal map channels
		by using negative values but this is strongly discouraged,
		use normalFormat keyword instead.
		*/
		else if ( !Q_stricmp( token, "normalScale" ) )
		{
			for ( int i = 0; i < 3; i++ )
			{
				token = COM_ParseExt2( text, false );

				if ( token[ 0 ] == '\0' )
				{
					if ( i == 2 )
					{
						/* Since ioquake3 renderer2 materials only tell X and Y
						components, we set the missing Z component to 1.0.
						It will have no effect.
						*/
						stage->normalScale[ 2 ] = 1.0;
					}
					else
					{
						Log::Warn("missing normalScale parm in shader '%s'", shader.name );
						continue;
					}
				}

				float j = atof( token );

				if ( j < 0.0 )
				{
					Log::Warn("not recommended negative normalScale parm in shader '%s': '%f', better use 'normalFormat' to swap normal map channel direction", shader.name, j );
				}

				stage->normalScale[ i ] = j;
				stage->hasNormalScale = true;
			}

			SkipRestOfLine( text );
			continue;
		}
		// normalIntensity <arithmetic expression>
		else if ( !Q_stricmp( token, "normalIntensity" ) )
		{
			ParseExpression( text, &stage->normalIntensityExp );
		}
		// fogDensity <arithmetic expression>
		else if ( !Q_stricmp( token, "fogDensity" ) )
		{
			ParseExpression( text, &stage->fogDensityExp );
		}
		// depthScale <arithmetic expression>
		else if ( !Q_stricmp( token, "depthScale" ) )
		{
			ParseExpression( text, &stage->depthScaleExp );
		}
		// deformMagnitude <arithmetic expression>
		else if ( !Q_stricmp( token, "deformMagnitude" ) )
		{
			ParseExpression( text, &stage->deformMagnitudeExp );
		}
		// wrapAroundLighting <arithmetic expression>
		else if ( !Q_stricmp( token, "wrapAroundLighting" ) )
		{
			ParseExpression( text, &stage->wrapAroundLightingExp );
		}
		else
		{
			Log::Warn("unknown shader stage parameter '%s' in shader '%s'", token, shader.name );
			SkipRestOfLine( text );
			continue;
		}
	}

	// parsing succeeded
	stage->active = true;

	// if cgen isn't explicitly specified, use either identity or identitylighting
	if ( stage->rgbGen == colorGen_t::CGEN_BAD )
	{
		if ( blendSrcBits == 0 || blendSrcBits == GLS_SRCBLEND_ONE || blendSrcBits == GLS_SRCBLEND_SRC_ALPHA )
		{
			stage->rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
		else
		{
			stage->rgbGen = colorGen_t::CGEN_IDENTITY;
		}
	}

	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	// tell shader if this stage has an alpha test
	if ( atestBits & GLS_ATEST_BITS )
	{
		shader.alphaTest = true;
	}

	// check that depthFade and depthWrite are mutually exclusive
	if ( depthMaskBits && stage->hasDepthFade ) {
		Log::Warn( "depth fade conflicts with depth mask in shader '%s'\n", shader.name );
		stage->hasDepthFade = false;
	}

	// compute state bits
	stage->stateBits = colorMaskBits | depthMaskBits | blendSrcBits | blendDstBits | atestBits | depthFuncBits | polyModeBits;

	/* If light style external light map and light mapping is disabled,
	do not load the image and disable the stage */
	if ( r_vertexLighting->integer && stage->tcGen_Lightmap == true )
	{
		stage->active = false;
		return true;
	}

	// load image
	if ( loadMap && !LoadMap( stage, buffer ) )
	{
		return false;
	}

	return true;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes rotgrow <growSpeed> <rotSlices> <rotSpeed>
deformVertexes autoSprite
deformVertexes autoSprite2
===============
*/
static void ParseDeform( const char **text )
{
	char          *token;
	deformStage_t *ds;

	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing deform parm in shader '%s'", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS )
	{
		Log::Warn("MAX_SHADER_DEFORMS in '%s'", shader.name );
		return;
	}

	ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "autosprite" ) )
	{
		shader.autoSpriteMode = 1;
		shader.numDeforms--;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) )
	{
		shader.autoSpriteMode = 2;
		shader.numDeforms--;
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes bulge parm in shader '%s'", shader.name );
			return;
		}

		ds->bulgeWidth = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes bulge parm in shader '%s'", shader.name );
			return;
		}

		ds->bulgeHeight = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes bulge parm in shader '%s'", shader.name );
			return;
		}

		ds->bulgeSpeed = atof( token );

		ds->deformation = deform_t::DEFORM_BULGE;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes parm in shader '%s'", shader.name );
			return;
		}

		if ( atof( token ) != 0 )
		{
			ds->deformationSpread = 1.0f / atof( token );
		}
		else
		{
			ds->deformationSpread = 100.0f;
			Log::Warn("illegal div value of 0 in deformVertexes command for shader '%s'", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = deform_t::DEFORM_WAVE;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes parm in shader '%s'", shader.name );
			return;
		}

		ds->deformationWave.amplitude = atof( token );

		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes parm in shader '%s'", shader.name );
			return;
		}

		ds->deformationWave.frequency = atof( token );

		ds->deformation = deform_t::DEFORM_NORMALS;
		return;
	}

	if ( !Q_stricmp( token, "move" ) )
	{
		int i;

		for ( i = 0; i < 3; i++ )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing deformVertexes parm in shader '%s'", shader.name );
				return;
			}

			ds->moveVector[ i ] = atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = deform_t::DEFORM_MOVE;
		return;
	}

	if ( !Q_stricmp( token, "rotgrow" ) )
	{
		int i;

		for ( i = 0; i < 3; i++ )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing deformVertexes parm in shader '%s'", shader.name );
				return;
			}

			ds->moveVector[ i ] = atof( token );
		}

		ds->deformation = deform_t::DEFORM_ROTGROW;
		return;
	}

	if ( !Q_stricmp( token, "sprite" ) )
	{
		shader.autoSpriteMode = 1;
		shader.numDeforms--;
		return;
	}

	if ( !Q_stricmp( token, "flare" ) )
	{
		token = COM_ParseExt2( text, false );

		if ( token[ 0 ] == 0 )
		{
			Log::Warn("missing deformVertexes flare parm in shader '%s'", shader.name );
			return;
		}

		ds->flareSize = atof( token );
		return;
	}

	Log::Warn("unknown deformVertexes subtype '%s' found in shader '%s'", token, shader.name );
}

/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms( const char **text )
{
	char *token;
	char prefix[ MAX_QPATH ];

	// outerbox
	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("'skyParms' missing parameter in shader '%s'", shader.name );
		return;
	}

	if ( strcmp( token, "-" ) )
	{
		Q_strncpyz( prefix, token, sizeof( prefix ) );

		imageParams_t imageParams = {};
		imageParams.bits = IF_NONE;
		imageParams.filterType = filterType_t::FT_DEFAULT;
		imageParams.wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;
		imageParams.minDimension = shader.imageMinDimension;
		imageParams.maxDimension = shader.imageMaxDimension;

		shader.sky.outerbox = R_FindCubeImage( prefix, &imageParams );

		if ( !shader.sky.outerbox )
		{
			Log::Warn("could not find cubemap '%s' for outer skybox in shader '%s'", prefix, shader.name );
			shader.sky.outerbox = tr.blackCubeImage;
		}
	}

	// cloudheight
	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("'skyParms' missing parameter in shader '%s'", shader.name );
		return;
	}

	shader.sky.cloudHeight = atof( token );

	if ( !shader.sky.cloudHeight )
	{
		shader.sky.cloudHeight = 512;
	}

	R_InitSkyTexCoords( shader.sky.cloudHeight );

	// innerbox
	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("'skyParms' missing parameter in shader '%s'", shader.name );
		return;
	}

	if ( strcmp( token, "-" ) )
	{
		Q_strncpyz( prefix, token, sizeof( prefix ) );

		imageParams_t imageParams = {};
		imageParams.bits = IF_NONE;
		imageParams.filterType = filterType_t::FT_DEFAULT;
		imageParams.wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;
		imageParams.minDimension = shader.imageMinDimension;
		imageParams.maxDimension = shader.imageMaxDimension;

		shader.sky.innerbox = R_FindCubeImage( prefix, &imageParams );

		if ( !shader.sky.innerbox )
		{
			Log::Warn("could not find cubemap '%s' for inner skybox in shader '%s'", prefix, shader.name );
			shader.sky.innerbox = tr.blackCubeImage;
		}
	}

	shader.isSky = true;
}

/*
=================
ParseSort
=================
*/
static void ParseSort( const char **text )
{
	char *token;

	token = COM_ParseExt2( text, false );

	if ( token[ 0 ] == 0 )
	{
		Log::Warn("missing sort parameter in shader '%s'", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) || !Q_stricmp( token, "subview" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_PORTAL);
	}
	else if ( !Q_stricmp( token, "sky" ) || !Q_stricmp( token, "environment" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_ENVIRONMENT_FOG);
	}
	else if ( !Q_stricmp( token, "opaque" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_OPAQUE);
	}
	else if ( !Q_stricmp( token, "decal" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_DECAL);
	}
	else if ( !Q_stricmp( token, "seeThrough" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_SEE_THROUGH);
	}
	else if ( !Q_stricmp( token, "banner" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_BANNER);
	}
	else if ( !Q_stricmp( token, "underwater" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_UNDERWATER);
	}
	else if ( !Q_stricmp( token, "far" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_FAR);
	}
	else if ( !Q_stricmp( token, "medium" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_MEDIUM);
	}
	else if ( !Q_stricmp( token, "close" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_CLOSE);
	}
	else if ( !Q_stricmp( token, "additive" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_BLEND1);
	}
	else if ( !Q_stricmp( token, "almostNearest" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_ALMOST_NEAREST);
	}
	else if ( !Q_stricmp( token, "nearest" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_NEAREST);
	}
	else if ( !Q_stricmp( token, "postProcess" ) )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_POST_PROCESS);
	}
	else
	{
		shader.sort = atof( token );
	}
}

// this table is also present in q3map

struct infoParm_t
{
	const char *name;
	int  clearSolid, surfaceFlags, contents;
};

// *INDENT-OFF*
static const infoParm_t infoParms[] =
{
	// server relevant contents

	{ "clipmissile",        1,                         0,                      CONTENTS_MISSILECLIP                   }, // impact only specific weapons (rl, gl)
	{ "water",              1,                         0,                      CONTENTS_WATER                         },
	{ "slag",               1,                         0,                      CONTENTS_SLIME                         }, // uses the CONTENTS_SLIME flag, but the shader reference is changed to 'slag'
	// to identify that this doesn't work the same as 'slime' did.

	{ "lava",               1,                         0,                      CONTENTS_LAVA                          }, // very damaging
	{ "playerclip",         1,                         0,                      CONTENTS_PLAYERCLIP                    },
	{ "monsterclip",        1,                         0,                      CONTENTS_MONSTERCLIP                   },
	{ "nodrop",             1,                         0,                      int(CONTENTS_NODROP)                   }, // don't drop items or leave bodies (death fog, lava, etc)
	{ "nonsolid",           1,                         SURF_NONSOLID,          0                                      }, // clears the solid flag

	{ "blood",              1,                         0,                      CONTENTS_WATER                         },

	// utility relevant attributes
	{ "origin",             1,                         0,                      CONTENTS_ORIGIN                        }, // center of rotating brushes

	{ "trans",              0,                         0,                      CONTENTS_TRANSLUCENT                   }, // don't eat contained surfaces
	{ "translucent",        0,                         0,                      CONTENTS_TRANSLUCENT                   }, // don't eat contained surfaces

	{ "detail",             0,                         0,                      CONTENTS_DETAIL                        }, // don't include in structural bsp
	{ "structural",         0,                         0,                      CONTENTS_STRUCTURAL                    }, // force into structural bsp even if trnas
	{ "areaportal",         1,                         0,                      CONTENTS_AREAPORTAL                    }, // divides areas
	{ "clusterportal",      1,                         0,                      CONTENTS_CLUSTERPORTAL                 }, // for bots
	{ "donotenter",         1,                         0,                      CONTENTS_DONOTENTER                    }, // for bots

	{ "donotenterlarge",    1,                         0,                      CONTENTS_DONOTENTER_LARGE              }, // for larger bots
	{ "fog",                1,                         0,                      CONTENTS_FOG                           }, // carves surfaces entering
	{ "sky",                0,                         SURF_SKY,               0                                      }, // emit light from an environment map
	{ "lightfilter",        0,                         SURF_LIGHTFILTER,       0                                      }, // filter light going through it
	{ "alphashadow",        0,                         SURF_ALPHASHADOW,       0                                      }, // test light on a per-pixel basis
	{ "hint",               0,                         SURF_HINT,              0                                      }, // use as a primary splitter

	// server attributes
	{ "slick",              0,                         SURF_SLICK,             0                                      },

	{ "noimpact",           0,                         SURF_NOIMPACT,          0                                      }, // don't make impact explosions or marks

	{ "nomarks",            0,                         SURF_NOMARKS,           0                                      }, // don't make impact marks, but still explode
	{ "nooverlays",         0,                         SURF_NOMARKS,           0                                      }, // don't make impact marks, but still explode

	{ "ladder",             0,                         SURF_LADDER,            0                                      },

	{ "nodamage",           0,                         SURF_NODAMAGE,          0                                      },

	{ "monsterslick",       0,                         SURF_MONSTERSLICK,      0                                      }, // surf only slick for monsters

	{ "glass",              0,                         SURF_GLASS,             0                                      }, //----(SA) added
	{ "splash",             0,                         SURF_SPLASH,            0                                      }, //----(SA) added

	// steps
	{ "metal",              0,                         SURF_METAL,             0                                      },
	{ "metalsteps",         0,                         SURF_METAL,             0                                      },

	{ "nosteps",            0,                         SURF_NOSTEPS,           0                                      },
	{ "woodsteps",          0,                         SURF_WOOD,              0                                      },
	{ "grasssteps",         0,                         SURF_GRASS,             0                                      },
	{ "gravelsteps",        0,                         SURF_GRAVEL,            0                                      },
	{ "carpetsteps",        0,                         SURF_CARPET,            0                                      },
	{ "snowsteps",          0,                         SURF_SNOW,              0                                      },
	{ "roofsteps",          0,                         SURF_ROOF,              0                                      }, // tile roof

	{ "rubble",             0,                         SURF_RUBBLE,            0                                      },

	// drawsurf attributes
	{ "nodraw",             0,                         SURF_NODRAW,            0                                      }, // don't generate a drawsurface (or a lightmap)
	{ "pointlight",         0,                         SURF_POINTLIGHT,        0                                      }, // sample lighting at vertexes
	{ "nolightmap",         0,                         SURF_NOLIGHTMAP,        0                                      }, // don't generate a lightmap
	{ "nodlight",           0,                         0,                      0                                      }, // OBSOLETE: don't ever add dynamic lights

	// monster ai
	{ "monsterslicknorth",  0,                         SURF_MONSLICK_N,        0                                      },
	{ "monsterslickeast",   0,                         SURF_MONSLICK_E,        0                                      },
	{ "monsterslicksouth",  0,                         SURF_MONSLICK_S,        0                                      },
	{ "monsterslickwest",   0,                         SURF_MONSLICK_W,        0                                      },

	// unsupported Doom3 surface types for sound effects and blood splats
	{ "metal",              0,                         SURF_METAL,             0                                      },

	{ "stone",              0,                         0,                      0                                      },
	{ "wood",               0,                         SURF_WOOD,              0                                      },
	{ "cardboard",          0,                         0,                      0                                      },
	{ "liquid",             0,                         0,                      0                                      },
	{ "glass",              0,                         0,                      0                                      },
	{ "plastic",            0,                         0,                      0                                      },
	{ "ricochet",           0,                         0,                      0                                      },
	{ "surftype10",         0,                         0,                      0                                      },
	{ "surftype11",         0,                         0,                      0                                      },
	{ "surftype12",         0,                         0,                      0                                      },
	{ "surftype13",         0,                         0,                      0                                      },
	{ "surftype14",         0,                         0,                      0                                      },
	{ "surftype15",         0,                         0,                      0                                      },

	// other unsupported Doom3 surface types
	{ "trigger",            0,                         0,                      0                                      },
	{ "flashlight_trigger", 0,                         0,                      0                                      },
	{ "aassolid",           0,                         0,                      0                                      },
	{ "aasobstacle",        0,                         0,                      0                                      },
	{ "nullNormal",         0,                         0,                      0                                      },
	{ "discrete",           0,                         0,                      0                                      },
};
// *INDENT-ON*

/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static bool SurfaceParm( const char *token )
{
	int numInfoParms = ARRAY_LEN( infoParms );
	int i;

	for ( i = 0; i < numInfoParms; i++ )
	{
		if ( !Q_stricmp( token, infoParms[ i ].name ) )
		{
			shader.surfaceFlags |= infoParms[ i ].surfaceFlags;
			shader.contentFlags |= infoParms[ i ].contents;
			return true;
		}
	}

	return false;
}

static void ParseSurfaceParm( const char **text )
{
	char *token;

	token = COM_ParseExt2( text, false );
	SurfaceParm( token );
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static bool ParseShader( const char *_text )
{
	const char **text;
	char *token;
	int  s;

	s = 0;
	text = &_text;

	token = COM_ParseExt2( text, true );

	if ( token[ 0 ] != '{' )
	{
		Log::Warn("no preceding '}' in shader %s", shader.name );
		return false;
	}

	while ( true )
	{
		token = COM_ParseExt2( text, true );

		if ( !token[ 0 ] )
		{
			Log::Warn("no concluding '}' in shader %s", shader.name );
			return false;
		}

		// end of shader definition
		if ( token[ 0 ] == '}' )
		{
			break;
		}
		// stage definition
		else if ( token[ 0 ] == '{' )
		{
			if ( s >= ( MAX_SHADER_STAGES - 1 ) )
			{
				Log::Warn("too many stages in shader %s", shader.name );
				return false;
			}

			if ( !ParseStage( &stages[ s ], text ) )
			{
				return false;
			}

			stages[ s ].active = true;
			s++;
			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if ( !Q_strnicmp( token, "qer", 3 ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		// skip description
		else if ( !Q_stricmp( token, "description" ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		// skip renderbump
		else if ( !Q_stricmp( token, "renderbump" ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		// skip guiSurf
		else if ( !Q_stricmp( token, "guiSurf" ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		// sun parms
		else if ( !Q_stricmp( token, "q3map_sun" ) )
		{
			float a, b;

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'q3map_sun' keyword in shader '%s'", shader.name );
				continue;
			}

			tr.sunLight[ 0 ] = atof( token );

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'q3map_sun' keyword in shader '%s'", shader.name );
				continue;
			}

			tr.sunLight[ 1 ] = atof( token );

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'q3map_sun' keyword in shader '%s'", shader.name );
				continue;
			}

			tr.sunLight[ 2 ] = atof( token );

			VectorNormalize( tr.sunLight );

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'q3map_sun' keyword in shader '%s'", shader.name );
				continue;
			}

			a = atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight );

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'q3map_sun' keyword in shader '%s'", shader.name );
				continue;
			}

			a = atof( token );
			a = a / 180 * M_PI;

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'q3map_sun' keyword in shader '%s'", shader.name );
				continue;
			}

			b = atof( token );
			b = b / 180 * M_PI;

			tr.sunDirection[ 0 ] = cos( a ) * cos( b );
			tr.sunDirection[ 1 ] = sin( a ) * cos( b );
			tr.sunDirection[ 2 ] = sin( b );
			continue;
		}
		// noShadows
		else if ( !Q_stricmp( token, "noShadows" ) )
		{
			shader.noShadows = true;
			continue;
		}
		// translucent
		else if ( !Q_stricmp( token, "translucent" ) )
		{
			shader.translucent = true;
			continue;
		}
		// forceOpaque
		else if ( !Q_stricmp( token, "forceOpaque" ) )
		{
			shader.forceOpaque = true;
			continue;
		}
		// forceSolid
		else if ( !Q_stricmp( token, "forceSolid" ) || !Q_stricmp( token, "solid" ) )
		{
			continue;
		}
		else if ( !Q_stricmp( token, "deformVertexes" ) || !Q_stricmp( token, "deform" ) )
		{
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) )
		{
			SkipRestOfLine( text );
			continue;
		}

		// skip noFragment
		if ( !Q_stricmp( token, "noFragment" ) )
		{
			continue;
		}
		// skip stuff that only the q3map needs
		else if ( !Q_strnicmp( token, "q3map", 5 ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if ( !Q_stricmp( token, "surfaceParm" ) )
		{
			ParseSurfaceParm( text );
			continue;
		}
		// no mip maps
		else if ( !Q_stricmp( token, "nomipmap" ) || !Q_stricmp( token, "nomipmaps" ) )
		{
			shader.filterType = filterType_t::FT_LINEAR;
			shader.noPicMip = true;
			continue;
		}
		// no picmip adjustment
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			shader.noPicMip = true;
			continue;
		}
		// imageMinDimension enforcement
		else if ( !Q_stricmp( token, "imageMinDimension" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] )
			{
				shader.imageMinDimension = atof( token );
			}

			continue;
		}
		// imageMaxDimension enforcement
		else if ( !Q_stricmp( token, "imageMaxDimension" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] )
			{
				shader.imageMaxDimension = atof( token );
			}

			continue;
		}
		// RF, allow each shader to permit compression if available (removed option)
		else if ( !Q_stricmp( token, "allowcompress" ) )
		{
			continue;
		}
		// (removed option)
		else if ( !Q_stricmp( token, "nocompress" ) )
		{
			continue;
		}
		// polygonOffset
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			shader.polygonOffset = true;

			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] )
			{
				shader.polygonOffsetValue = atof( token );
			}

			continue;
		}
		// relief mapping
		else if ( !Q_stricmp( token, "parallax" ) )
		{
			/* Legacy lone “parallax” XreaL material keyword was never used,
			its purpose was to enable relief mapping for the current shader.

			The engine relied on it to know the heightmap was stored in
			normalmap alpha channel but there was no other storage options.

			The engine also had to rely on it to know that heightmap
			was stored in normalmap because of a design flaw that used
			depthmap (black is up) instead of heightmap (black is down).
			I meant a missing depthmap would cause a wrong displacement
			if not discarded.

			So that option was there to tell the engine to not discard
			heightmap when the artist knew it was not flat.

			Since the engine knows how to automatically load and enable
			heightmap stored in normalmap if detection is required, and
			has not problem with flat heightmap in any way because of a
			better design, this keyword is now pretty useless.

			Artists can use “normalMap” material keyword for normalmap
			without heightmap, and can use “normalHeightMap” keyword for
			normalmap with heightmap in alpha channel.

			Developers can use ParseNormalMapDetectHeightMap function to detect
			heightmap in normal map alpha channel, please never add a material
			keyword for this behaviour because some texture format can lead to
			wrong detection when GL_ARB_texture_compression_rgtc extension is
			missing. Only use this function for compatibility layers for games
			that relies on detection (and were already unable to use such
			texture format because of that). */

			Log::Warn("deprecated keyword '%s' in shader '%s', this was a workaround for a design flaw, there is no need for it", token, shader.name );

			SkipRestOfLine( text );
			continue;
		}
		else if ( *r_dpMaterial && !Q_stricmp( token, "dpoffsetmapping" ) )
		{
			char* keyword = token;
			token = COM_ParseExt2( text, false );

			if ( !Q_stricmp( token, "none" ) )
			{
				shader.disableReliefMapping = true;
			}
			else if ( !Q_stricmp( token, "disable" ) )
			{
				shader.disableReliefMapping = true;
			}
			else if ( !Q_stricmp( token, "off" ) )
			{
				shader.disableReliefMapping = true;
			}
			else if ( !Q_stricmp( token, "default" ) )
			{
				// do nothing more
			}
			else if ( !Q_stricmp( token, "normal" ) )
			{
				// do nothing more (same as default)
			}
			else if ( !Q_stricmp( token, "linear" ) )
			{
				// not implemented yet
				Log::Warn("unsupported parm for '%s' keyword in shader '%s'", keyword, shader.name );
			}
			else if ( !Q_stricmp( token, "relief" ) )
			{
				// the only implemented algorithm
				// hence the default
				// do nothing more
			}
			else if ( !Q_stricmp( token, "-" ) )
			{
				// do nothing, that's just a filler
			}
			else
			{
				Log::Warn("invalid parm for '%s' keyword in shader '%s'", keyword, shader.name );
				SkipRestOfLine( text );
				continue;
			}

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for '%s' keyword in shader '%s'", keyword, shader.name );
				continue;
			}

			shader.reliefDepthScale = atof( token );

			token = COM_ParseExt2( text, false );

			// dpoffsetmapping - 2
			if ( !token[ 0 ] )
			{
				continue;
			}

			// dpoffsetmapping - 2 match8 65
			float off;
			float div;

			if ( !Q_stricmp( token, "bias" ) )
			{
				off = 0.0f;
				div = 1.0f;
			}
			else if ( !Q_stricmp( token, "match" ) )
			{
				off = 1.0f;
				div = 1.0f;
			}
			else if ( !Q_stricmp( token, "match8" ) )
			{
				off = 1.0f;
				div = 255.0f;
			}
			else if ( !Q_stricmp( token, "match16" ) )
			{
				off = 1.0f;
				div = 65535.0f;
			}
			else
			{
				Log::Warn("invalid parm for '%s' keyword in shader '%s'", keyword, shader.name );
				SkipRestOfLine( text );
				continue;
			}

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for '%s' keyword in shader '%s'", keyword, shader.name );
				continue;
			}

			float bias = atof( token );
			shader.reliefOffsetBias = off - bias / div;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = true;
			continue;
		}
		// fogParms
		else if ( !Q_stricmp( token, "fogParms" ) )
		{
			/*
			Log::Warn("fogParms keyword not supported in shader '%s'", shader.name);
			SkipRestOfLine(text);

			*/

			if ( !ParseVector( text, 3, shader.fogParms.color ) )
			{
				return false;
			}

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing parm for 'fogParms' keyword in shader '%s'", shader.name );
				continue;
			}

			shader.fogParms.depthForOpaque = atof( token );

			shader.sort = Util::ordinal(shaderSort_t::SS_FOG);

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		// noFog
		else if ( !Q_stricmp( token, "noFog" ) )
		{
			shader.noFog = true;
			continue;
		}
		// portal
		else if ( !Q_stricmp( token, "portal" ) )
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_PORTAL);
			shader.isPortal = true;

			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] )
			{
				shader.portalRange = atof( token );
			}
			else
			{
				shader.portalRange = 256;
			}

			continue;
		}
		// portal or mirror
		else if ( !Q_stricmp( token, "mirror" ) )
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_PORTAL);
			shader.isPortal = true;
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			ParseSkyParms( text );
			continue;
		}
		// ET sunshader <name>
		else if ( !Q_stricmp( token, "sunshader" ) )
		{
			int tokenLen;

			token = COM_ParseExt2( text, false );

			if ( !token[ 0 ] )
			{
				Log::Warn("missing shader name for 'sunshader'" );
				continue;
			}

			/*
			RB: don't call tr.sunShader = R_FindShader(token, SHADER_3D_STATIC, RSF_DEFAULT);
			        because it breaks the computation of the current shader
			*/
			tokenLen = strlen( token ) + 1;
			tr.sunShaderName = (char*) ri.Hunk_Alloc( sizeof( char ) * tokenLen, ha_pref::h_low );
			Q_strncpyz( tr.sunShaderName, token, tokenLen );
		}
		// light <value> determines flaring in q3map, not needed here
		else if ( !Q_stricmp( token, "light" ) )
		{
			token = COM_ParseExt2( text, false );
			continue;
		}
		// cull <face>
		else if ( !Q_stricmp( token, "cull" ) )
		{
			token = COM_ParseExt2( text, false );

			if ( token[ 0 ] == 0 )
			{
				Log::Warn("missing cull parms in shader '%s'", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twoSided" ) || !Q_stricmp( token, "disable" ) )
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				Log::Warn("invalid cull parm '%s' in shader '%s'", token, shader.name );
			}

			continue;
		}
		// twoSided
		else if ( !Q_stricmp( token, "twoSided" ) )
		{
			shader.cullType = CT_TWO_SIDED;
			continue;
		}
		// backSided
		else if ( !Q_stricmp( token, "backSided" ) )
		{
			shader.cullType = CT_BACK_SIDED;
			continue;
		}
		// clamp, edgeClamp etc.
		else if ( ParseClampType( token, &shader.wrapType ) )
		{
			continue;
		}
		// sort
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		// ydnar: implicit default mapping to eliminate redundant/incorrect explicit shader stages
		else if ( !Q_strnicmp( token, "implicit", 8 ) )
		{
			bool GL1 = false;

			// set implicit mapping state
			if ( !Q_stricmp( token, "implicitBlend" ) )
			{
				implicitStateBits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				implicitCullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "implicitMask" ) )
			{
				implicitStateBits = GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_128;
				implicitCullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "implicitMapGL1" ) )
			{
				GL1 = true;
			}
			else // "implicitMap"
			{
				implicitStateBits = GLS_DEPTHMASK_TRUE;
				implicitCullType = CT_FRONT_SIDED;
			}

			// get image
			token = COM_ParseExt( text, false );

			if ( !GL1 )
			{
				if ( token[ 0 ] != '\0' )
				{
					Q_strncpyz( implicitMap, token, sizeof( implicitMap ) );
				}
				else
				{
					implicitMap[ 0 ] = '-';
					implicitMap[ 1 ] = '\0';
				}
			}

			continue;
		}
		// diffuseMap <image>
		else if ( !Q_stricmp( token, "diffuseMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseDiffuseMap( &stages[ s ], text, TB_COLORMAP );
			stages[ s ].implicitLightmap = true;
			s++;
			continue;
		}
		// normalMap <image>
		else if ( !Q_stricmp( token, "normalMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseNormalMap( &stages[ s ], text, TB_COLORMAP );
			SetNormalFormat( &stages[ s ], dxNormalFormat );
			s++;
			continue;
		}
		// bumpMap <image>
		else if ( !Q_stricmp( token, "bumpMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better use 'normalMap' keyword and move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseNormalMap( &stages[ s ], text, TB_COLORMAP );
			SetNormalFormat( &stages[ s ], dxNormalFormat );
			s++;
			continue;
		}
		// specularMap <image>
		else if ( !Q_stricmp( token, "specularMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseSpecularMap( &stages[ s ], text, TB_COLORMAP );
			s++;
			continue;
		}
		// physicalMap <image>
		else if ( !Q_stricmp( token, "physicalMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParsePhysicalMap( &stages[ s ], text, TB_COLORMAP );
			s++;
			continue;
		}
		// glowMap <image>
		else if ( !Q_stricmp( token, "glowMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseGlowMap( &stages[ s ], text, TB_COLORMAP );
			s++;
			continue;
		}
		// reflectionMap <image>
		else if ( !Q_stricmp( token, "reflectionMap" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseReflectionMap( &stages[ s ], text, TB_COLORMAP );
			s++;
			continue;
		}
		// reflectionMapBlended <image>
		else if ( !Q_stricmp( token, "reflectionMapBlended" ) )
		{
			Log::Warn("deprecated idTech4 standalone stage '%s' in shader '%s', better move this line and pack related textures within one single curly bracket stage pair", token, shader.name );
			ParseReflectionMapBlended( &stages[ s ], text, TB_COLORMAP );
			s++;
			continue;
		}
		else if ( !Q_stricmp( token, "dpreflectcube" ) )
		{
			if ( *r_dpMaterial )
			{
				ParseReflectionMapBlended( &stages[ s ], text, TB_COLORMAP );
				s++;
			}
			else
			{
				Log::Warn("disabled DarkPlaces shader parameter '%s' in '%s'", token, shader.name );
			}
			continue;
		}
		// lightFalloffImage <image>
		else if ( !Q_stricmp( token, "lightFalloffImage" ) )
		{
			ParseLightFalloffImage( &stages[ s ], text );
			s++;
			continue;
		}
		// Doom 3 DECAL_MACRO
		else if ( !Q_stricmp( token, "DECAL_MACRO" ) )
		{
			shader.polygonOffset = true;
			shader.polygonOffsetValue = 1;
			shader.sort = Util::ordinal(shaderSort_t::SS_DECAL);
			SurfaceParm( "discrete" );
			SurfaceParm( "noShadows" );
			continue;
		}
		// Prey DECAL_ALPHATEST_MACRO
		else if ( !Q_stricmp( token, "DECAL_ALPHATEST_MACRO" ) )
		{
			// what's different?
			shader.polygonOffset = true;
			shader.polygonOffsetValue = 1;
			shader.sort = Util::ordinal(shaderSort_t::SS_DECAL);
			SurfaceParm( "discrete" );
			SurfaceParm( "noShadows" );
			continue;
		}
		// when <state> <shader name>
		else if ( !Q_stricmp( token, "when" ) )
		{
			int i;
			const char *p;
			int index = 0;

			token = COM_ParseExt2( text, false );

			for ( i = 1, p = whenTokens; i < MAX_ALTSHADERS && *p; ++i, p += strlen( p ) + 1 )
			{
				if ( !Q_stricmp( token, p ) )
				{
					index = i;
					break;
				}
			}

			if ( index == 0 )
			{
				Log::Warn("unknown parameter '%s' for 'when' in '%s'", token, shader.name );
			}
			else
			{
				int tokenLen;

				token = COM_ParseExt( text, false );

				if ( !token[ 0 ] )
				{
					Log::Warn("missing shader name for 'when'" );
					continue;
				}

				tokenLen = strlen( token ) + 1;
				shader.altShader[ index ].index = 0;
				shader.altShader[ index ].name = ( char* )ri.Hunk_Alloc( sizeof( char ) * tokenLen, ha_pref::h_low );
				Q_strncpyz( shader.altShader[ index ].name, token, tokenLen );
			}
		}
		else if ( SurfaceParm( token ) )
		{
			continue;
		}
		else if ( !Q_strnicmp( token, "dp", 2 ) )
		{
			if ( *r_dpMaterial )
			{
				Log::Warn("unknown DarkPlaces shader parameter '%s' in '%s'", token, shader.name );
			}
			else
			{
				Log::Warn("disabled DarkPlaces shader parameter '%s' in '%s'", token, shader.name );
			}
			SkipRestOfLine( text );
			continue;
		}
		else
		{
			Log::Warn("unknown general shader parameter '%s' in '%s'", token, shader.name );
			SkipRestOfLine( text );
			continue;
		}
	}

	// ignore shaders that don't have any stages, unless it is a sky or fog
	if ( s == 0 && !shader.forceOpaque && !shader.isSky && !( shader.contentFlags & CONTENTS_FOG ) && implicitMap[ 0 ] == '\0' )
	{
		return false;
	}

	return true;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
================
CollapseMultitexture
=================
*/
// *INDENT-OFF*
static void CollapseStages()
{
	int diffuseStage = -1;
	int normalStage = -1;
	int specularStage = -1;
	int physicalStage = -1;
	int reflectionStage = -1;
	int lightStage = -1;
	int glowStage = -1;

	int lightMapCount = 0;

	for ( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if ( !stages[ i ].active )
		{
			continue;
		}
		else if ( stages[ i ].collapseType == collapseType_t::COLLAPSE_generic
			|| stages[ i ].collapseType == collapseType_t::COLLAPSE_lighting_PBR )
		{
			stages[ i ].type = stageType_t::ST_COLLAPSE_lighting_PBR;
			continue;
		}
		else if ( stages[ i ].collapseType == collapseType_t::COLLAPSE_lighting_PHONG )
		{
			stages[ i ].type = stageType_t::ST_COLLAPSE_lighting_PHONG;
			continue;
		}
		else if ( stages[ i ].collapseType == collapseType_t::COLLAPSE_reflection_CB )
		{
			stages[ i ].type = stageType_t::ST_COLLAPSE_reflection_CB;
			continue;
		}
		else if ( stages[ i ].type == stageType_t::ST_DIFFUSEMAP )
		{
			if ( diffuseStage != -1 )
			{
				Log::Warn( "more than one diffuse map stage in shader '%s'", shader.name );
			}
			else
			{
				diffuseStage = i;
			}
		}
		else if ( stages[ i ].type == stageType_t::ST_NORMALMAP )
		{
			if ( normalStage != -1 )
			{
				Log::Warn( "more than one normal map stage in shader '%s'", shader.name );
			}
			else
			{
				normalStage = i;
			}
		}
		else if ( stages[ i ].type == stageType_t::ST_SPECULARMAP )
		{
			if ( specularStage != -1 )
			{
				Log::Warn( "more than one specular map stage in shader '%s'", shader.name );
			}
			else
			{
				specularStage = i;
			}
		}
		else if ( stages[ i ].type == stageType_t::ST_PHYSICALMAP )
		{
			if ( physicalStage != -1 )
			{
				Log::Warn( "more than one physical map stage in shader '%s'", shader.name );
			}
			else
			{
				physicalStage = i;
			}
		}
		else if ( stages[ i ].type == stageType_t::ST_REFLECTIONMAP )
		{
			if ( reflectionStage != -1 )
			{
				Log::Warn( "more than one reflection map stage in shader '%s'", shader.name );
			}
			else
			{
				reflectionStage = i;
			}
		}
		else if ( stages[ i ].type == stageType_t::ST_LIGHTMAP )
		{
			lightMapCount++;

			if ( lightStage == -1 )
			{
				lightStage = i;
			}
		}
		else if ( stages[ i ].type == stageType_t::ST_GLOWMAP )
		{
			if ( glowStage != -1 )
			{
				Log::Warn( "more than one glow map stage in shader '%s'", shader.name );
			}
			else
			{
				glowStage = i;
			}
		}
	}

	if ( lightMapCount > 1 )
	{
		Log::Debug( "found %d light map stages in shader '%s'", lightMapCount, shader.name );
	}

	for ( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		/* Store hethaze and liquid normal map in normal map slot.

		NOTE: liquidMap may be entirely redesigned in the future
		to be a variant of diffuseMap (hence supporting all others textures).
		*/
		if ( stages[ i ].type == stageType_t::ST_HEATHAZEMAP
			|| stages[ i ].type == stageType_t::ST_LIQUIDMAP )
		{
			stages[ i ].bundle[ TB_NORMALMAP ] = stages[ i ].bundle[ TB_COLORMAP ];
			stages[ i ].bundle[ TB_COLORMAP ] = {};
		}

		if ( lightStage != -1 && stages[ i ].collapseType != collapseType_t::COLLAPSE_none )
		{
			if ( stages[ i ].dpMaterial )
			{
				// DarkPlaces only supports one kind of lightmap blend:
				//   https://gitlab.com/xonotic/darkplaces/blob/324a5329d33ef90df59e6488abce6433d90ac04c/model_shared.c#L1886-1887
				Log::Debug("found custom lightmap stage in DarkPlaces '%s' shader, disable it and enable implicit one instead", shader.name);
				stages[ i ].implicitLightmap = true;
				stages[ lightStage ].active = false;
				lightStage = -1;
			}
			else
			{
				// custom lightmap stage, disable the implicit light stage
				Log::Debug("found custom lightmap stage in '%s' shader, disabling implicit one", shader.name);
				stages[ i ].implicitLightmap = false;
			}
		}
	}

	// note that same stage can be merged twice
	// like the normal stage being merged in both
	// reflection stage and diffuse stage

	if ( reflectionStage != -1 && normalStage != -1 )
	{
		// note that if uncollapsed reflectionStage had to be merged in another stage
		// it would have to be backed-up somewhere


		Log::Debug("found reflection collapsable stage in shader '%s':", shader.name);
		stages[ reflectionStage ].collapseType = collapseType_t::COLLAPSE_reflection_CB;
		stages[ reflectionStage ].type = stageType_t::ST_COLLAPSE_reflection_CB;

		// merge with reflection stage
		stages[ reflectionStage ].bundle[ TB_NORMALMAP ] = stages[ normalStage ].bundle[ TB_COLORMAP ];
		// disable since it's merged
		stages[ normalStage ].active = false;
	}

	if ( diffuseStage != -1
		&& ( specularStage != -1
			|| normalStage != -1
			|| physicalStage != -1
			|| lightStage != -1
			|| glowStage != -1 ) )
	{
		// note that if uncollapsed diffuseStage had to be merged in another stage
		// it would have to be backed-up somewhere


		if ( specularStage != -1 && physicalStage != -1 )
		{
			Log::Warn("Supposedly you shouldn't have both specularMap and physicalMap (in shader '%s')?", shader.name);
		}
		else
		{
			if ( physicalStage != -1 )
			{
				Log::Debug("found PBR lighting collapsible stage in shader '%s'", shader.name);
				stages[ diffuseStage ].collapseType = collapseType_t::COLLAPSE_lighting_PBR;
				stages[ diffuseStage ].type = stageType_t::ST_COLLAPSE_lighting_PBR;
			}
			else
			{
				Log::Debug("found Phong lighting collapsible stage in shader '%s'", shader.name);
				stages[ diffuseStage ].collapseType = collapseType_t::COLLAPSE_lighting_PHONG;
				stages[ diffuseStage ].type = stageType_t::ST_COLLAPSE_lighting_PHONG;
			}

			if ( normalStage != -1 )
			{
				// merge with diffuse stage
				stages[ diffuseStage ].bundle[ TB_NORMALMAP ] = stages[ normalStage ].bundle[ TB_COLORMAP ];

				stages[ diffuseStage ].normalIntensityExp = stages[ normalStage ].normalIntensityExp;

				for ( int i = 0; i < 3; i++ )
				{
					stages[ diffuseStage ].normalFormat[ i ] = stages[ normalStage ].normalFormat[ i ];
					stages[ diffuseStage ].normalScale[ i ] = stages[ normalStage ].normalScale[ i ];
				}

				stages[ diffuseStage ].hasNormalFormat = stages[ normalStage ].hasNormalFormat;
				stages[ diffuseStage ].hasNormalScale = stages[ normalStage ].hasNormalScale;

				stages[ diffuseStage ].isHeightMapInNormalMap = stages[ normalStage ].isHeightMapInNormalMap;

				// disable since it's merged
				stages[ normalStage ].active = false;
			}

			if ( specularStage != -1 )
			{
				// merge with diffuse stage
				stages[ diffuseStage ].bundle[ TB_SPECULARMAP ] = stages[ specularStage ].bundle[ TB_COLORMAP ];
				stages[ diffuseStage ].specularExponentMin = stages[ specularStage ].specularExponentMin;
				stages[ diffuseStage ].specularExponentMax = stages[ specularStage ].specularExponentMax;
				// disable since it's merged
				stages[ specularStage ].active = false;
			}

			if ( physicalStage != -1 )
			{
				// merge with diffuse stage
				stages[ diffuseStage ].bundle[ TB_PHYSICALMAP ] = stages[ physicalStage ].bundle[ TB_COLORMAP ];
				// disable since it's merged
				stages[ physicalStage ].active = false;
			}

			// always test for this stage before glow stage
			if ( lightStage != -1 )
			{
				// “blendFunc filter” is same as “blendFunc GL_DST_COLOR GL_ZERO” and the default
				if ( ( stages[ lightStage ].stateBits & GLS_SRCBLEND_BITS ) == GLS_SRCBLEND_DST_COLOR
					&& ( stages[ lightStage ].stateBits & GLS_DSTBLEND_BITS ) == GLS_DSTBLEND_ZERO )
				{
					// common lightmap stage
					// disable to not paint it over implicit light stage and glow map
					stages[ lightStage ].active = false;
					stages[ diffuseStage ].implicitLightmap = true;
				}
				else
				{
					// custom lightmap stage, disable the implicit light stage and keep
					// this one uncollapsed
					Log::Debug("found custom lightmap stage in '%s' shader, not collapsing", shader.name);
					stages[ diffuseStage ].implicitLightmap = false;
				}
			}

			// always test for this stage after light stage
			if ( glowStage != -1 )
			{
				// if there is no custom light stage, collapse to the diffuse stage
				// that will rely on the implicit light stage
				if ( stages[ diffuseStage ].implicitLightmap )
				{
					// merge with diffuse stage
					stages[ diffuseStage ].bundle[ TB_GLOWMAP ] = stages[ glowStage ].bundle[ TB_COLORMAP ];
					// disable since it's merged
					stages[ glowStage ].active = false;
				}
				// if there is a custom light stage, keep the glow stage uncollapsed
				// and make sure the diffuse stage precedes the light stage
				// and the light stage (known to exist because of implicitLightmap == false) precedes the glow stage
				else
				{
					ASSERT_GE(lightStage, 0);
					Log::Debug("found glow map with custom lightmap stage in '%s' shader, not collapsing", shader.name);
					stages[ glowStage ].type = stageType_t::ST_COLORMAP;
					// not required since it's already how it is expected to be
					// stages[ glowStage ].bundle[ TB_COLORMAP ] = stages[ glowStage ].bundle[ TB_COLORMAP ];

					// put diffuse stage in the first of the three spots
					int& minStageIndex = lightStage < glowStage ? lightStage : glowStage; // note the reference!
					if ( minStageIndex < diffuseStage )
					{
						std::swap(stages[diffuseStage], stages[minStageIndex]);
						std::swap(diffuseStage, minStageIndex);
					}
					// fix 2nd and 3rd spots
					if ( glowStage < lightStage )
					{
						std::swap(stages[glowStage], stages[lightStage]);
						std::swap(glowStage, lightStage);
					}
				}
			}
		}
	}

	// move all active stages at beginning
	// note that the 'active' field is still used instead of numStages in some code that runs later
	int numActiveStages = 0;
	for ( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if ( stages[ i ].active )
		{
			if ( i != numActiveStages )
			{
				stages[ numActiveStages ] = stages[ i ];

				stages[ i ].active = false;
			}

			numActiveStages++;
		}
	}

	shader.numStages = numActiveStages;

	// Do some precomputation.
	for ( int s = 0; s < shader.numStages; s++ )
	{
		shaderStage_t *stage = &stages[ s ];

		// Available textures.
		stage->hasNormalMap = stage->bundle[ TB_NORMALMAP ].image[ 0 ] != nullptr;
		stage->hasHeightMap = stage->bundle[ TB_HEIGHTMAP ].image[ 0 ] != nullptr;
		stage->isHeightMapInNormalMap = stage->isHeightMapInNormalMap && stage->hasNormalMap;
		stage->hasMaterialMap = stage->bundle[ TB_MATERIALMAP ].image[ 0 ] != nullptr;
		stage->hasGlowMap = stage->bundle[ TB_GLOWMAP ].image[ 0 ] != nullptr;
		stage->isMaterialPhysical = stage->collapseType == collapseType_t::COLLAPSE_lighting_PBR;

		// Available features.
		stage->enableNormalMapping = r_normalMapping->integer && stage->hasNormalMap;
		stage->enableDeluxeMapping = r_deluxeMapping->integer && stage->hasNormalMap;
		stage->enableReliefMapping = r_reliefMapping->integer && !shader.disableReliefMapping && ( stage->hasHeightMap || stage->isHeightMapInNormalMap );
		stage->enablePhysicalMapping = r_physicalMapping->integer && stage->hasMaterialMap && stage->isMaterialPhysical;
		stage->enableSpecularMapping = r_specularMapping->integer && stage->hasMaterialMap && !stage->isMaterialPhysical;
		stage->enableGlowMapping = r_glowMapping->integer && stage->hasGlowMap;

		// FIXME: Workaround for textures having both an alpha mask (like gratings) with an height map,
		// The engine does not displace the depth test map yet so we disable relief mapping to prevent garbage
		// to appears between the displaced diffuse map and the non-displaced alpha mask.
		// See https://github.com/DaemonEngine/Daemon/issues/334
		if ( stage->enableReliefMapping && stage->stateBits & ~GLS_DEPTHMASK_TRUE )
		{
			Log::Debug( "Workaround: disabling relief mapping for stage %d in shader '%s' because of alpha mask", s, shader.name );
			stage->enableReliefMapping = false;
		}

		// Finally disable useless heightMapInNormalMap if both normal and relief mapping are disabled.
		// see https://github.com/DaemonEngine/Daemon/issues/376
		stage->isHeightMapInNormalMap = stage->isHeightMapInNormalMap && ( stage->enableNormalMapping || stage->enableReliefMapping );

		// Bind fallback textures if required.
		if ( !stage->enableNormalMapping && !( stage->enableReliefMapping && stage->isHeightMapInNormalMap) )
		{
			stage->bundle[ TB_NORMALMAP ].image[ 0 ] = tr.flatImage;
		}

		if ( !stage->enablePhysicalMapping && !stage->enableSpecularMapping )
		{
			stage->bundle[ TB_MATERIALMAP ].image[ 0 ] = tr.blackImage;
		}

		if ( !stage->enableGlowMapping )
		{
			stage->bundle[ TB_GLOWMAP ].image[ 0 ] = tr.blackImage;
		}

		// Compute normal scale.
		for ( int i = 0; i < 3; i++ )
		{
			if ( stage->hasNormalMap )
			{
				if ( !stage->hasNormalFormat )
				{
					// Please make sure the parser sets a normal format
					// when adding a new normal map syntax.
					ASSERT_UNREACHABLE();
				}

				if ( !stage->hasNormalScale )
				{
					stage->normalScale[ i ] = 1.0;
				}

				/* Because Z reconstruction is destructive on alpha channel
				Z reconstruction is never done on normal map shipping height
				map in alpha channel and Z is read from the file itself.
				Those files always provide Z anyway.

				If height map is not stored in normal map alpha channel,
				the Z component will be reconstructed from X and Y whatever
				Z is provided by the file or not) and Z will be fine from
				the start, so we must not apply the format translation on
				the Z channel.

				So this test means X and Y formats are always applied,
				but Z format is applied only when Z is not reconstructed.

				Note the XYZ format translations are not done on RGB channels
				from the file but on the RGB channels as seen in GLSL shader,
				there is no need to worry about DXn storing X in alpha channel.

				This way the material syntax is expected to work the same with
				both the PNG source and the released CRN.
				*/
				if ( i < 2 || stage->isHeightMapInNormalMap )
				{
					stage->normalScale[ i ] *= stage->normalFormat[ i ];
				}
			}
			else
			{
				stage->normalScale[ i ] = 1.0;
			}
		}
	}
}

// *INDENT-ON*

/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted relative to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader()
{
	int      i;
	float    sort;
	shader_t *newShader;

	newShader = tr.shaders[ tr.numShaders - 1 ];
	sort = newShader->sort;

	for ( i = tr.numShaders - 2; i >= 0; i-- )
	{
		if ( tr.sortedShaders[ i ]->sort <= sort )
		{
			break;
		}

		tr.sortedShaders[ i + 1 ] = tr.sortedShaders[ i ];
		tr.sortedShaders[ i + 1 ]->sortedIndex++;
	}

	newShader->sortedIndex = i + 1;
	tr.sortedShaders[ i + 1 ] = newShader;
}

/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader()
{
	shader_t *newShader;
	int      i, b;
	int      size, hash;

	if ( tr.numShaders == MAX_SHADERS )
	{
		Log::Warn("GeneratePermanentShader - MAX_SHADERS hit" );
		return tr.defaultShader;
	}

	newShader = (shader_t*) ri.Hunk_Alloc( sizeof( shader_t ), ha_pref::h_low );

	*newShader = shader;

	if ( shader.sort <= Util::ordinal(shaderSort_t::SS_OPAQUE) )
	{
		newShader->fogPass = fogPass_t::FP_EQUAL;
	}
	else if ( shader.contentFlags & CONTENTS_FOG )
	{
		newShader->fogPass = fogPass_t::FP_LE;
	}

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for ( i = 0; i < newShader->numStages; i++ )
	{
		if ( !stages[ i ].active )
		{
			break;
		}

		newShader->stages[ i ] = (shaderStage_t*) ri.Hunk_Alloc( sizeof( stages[ i ] ), ha_pref::h_low );
		*newShader->stages[ i ] = stages[ i ];

		for ( b = 0; b < MAX_TEXTURE_BUNDLES; b++ )
		{
			size = newShader->stages[ i ]->bundle[ b ].numTexMods * sizeof( texModInfo_t );
			newShader->stages[ i ]->bundle[ b ].texMods = (texModInfo_t*) ri.Hunk_Alloc( size, ha_pref::h_low );
			Com_Memcpy( newShader->stages[ i ]->bundle[ b ].texMods, stages[ i ].bundle[ b ].texMods, size );
		}
	}

	SortNewShader();

	hash = generateHashValue( newShader->name, FILE_HASH_SIZE );
	newShader->next = shaderHashTable[ hash ];
	shaderHashTable[ hash ] = newShader;

	return newShader;
}

/*
====================
GeneratePermanentShaderTable
====================
*/
static void GeneratePermanentShaderTable( float *values, int numValues )
{
	shaderTable_t *newTable;
	int           i;
	int           hash;

	if ( tr.numTables == MAX_SHADER_TABLES )
	{
		Log::Warn("GeneratePermanentShaderTable - MAX_SHADER_TABLES hit" );
		return;
	}

	newTable = (shaderTable_t*) ri.Hunk_Alloc( sizeof( shaderTable_t ), ha_pref::h_low );

	*newTable = table;

	tr.shaderTables[ tr.numTables ] = newTable;
	newTable->index = tr.numTables;

	tr.numTables++;

	newTable->numValues = numValues;
	newTable->values = (float*) ri.Hunk_Alloc( sizeof( float ) * numValues, ha_pref::h_low );

	for ( i = 0; i < numValues; i++ )
	{
		newTable->values[ i ] = values[ i ];
	}

	hash = generateHashValue( newTable->name, MAX_SHADERTABLE_HASH );
	newTable->next = shaderTableHashTable[ hash ];
	shaderTableHashTable[ hash ] = newTable;
}

/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader()
{
	int      stage, i;
	shader_t *ret;

	// set sky stuff appropriate
	if ( shader.isSky )
	{
		if ( shader.noFog )
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_ENVIRONMENT_NOFOG);
		}
		else
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_ENVIRONMENT_FOG);
		}
	}

	if ( shader.forceOpaque )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_OPAQUE);
	}

	// set polygon offset
	if ( shader.polygonOffset && !shader.sort )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_DECAL);
	}

	// all light materials need at least one z attenuation stage as first stage
	if ( shader.type == shaderType_t::SHADER_LIGHT )
	{
		if ( stages[ 0 ].type != stageType_t::ST_ATTENUATIONMAP_Z )
		{
			// move up subsequent stages
			memmove( &stages[ 1 ], &stages[ 0 ], sizeof( stages[ 0 ] ) * ( MAX_SHADER_STAGES - 1 ) );

			stages[ 0 ].active = true;
			stages[ 0 ].type = stageType_t::ST_ATTENUATIONMAP_Z;
			stages[ 0 ].rgbGen = colorGen_t::CGEN_IDENTITY;
			stages[ 0 ].stateBits = GLS_DEFAULT;
			stages[ 0 ].overrideWrapType = true;
			stages[ 0 ].wrapType = wrapTypeEnum_t::WT_EDGE_CLAMP;

			const char *squarelight1a = "lights/squarelight1a";
			Log::Debug( "loading '%s' image as shader", squarelight1a );
			LoadMap( &stages[ 0 ], squarelight1a );
		}

		// force following shader stages to be xy attenuation stages
		for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ )
		{
			shaderStage_t *pStage = &stages[ stage ];

			if ( !pStage->active )
			{
				break;
			}

			pStage->type = stageType_t::ST_ATTENUATIONMAP_XY;
		}
	}

	// set appropriate stage information
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = &stages[ stage ];

		if ( !pStage->active )
		{
			break;
		}

		// check for a missing texture
		switch ( pStage->type )
		{
			case stageType_t::ST_LIQUIDMAP:
			case stageType_t::ST_LIGHTMAP:
				// skip
				break;

			case stageType_t::ST_COLORMAP:
			default:
				{
					if ( !pStage->bundle[ 0 ].image[ 0 ] )
					{
						Log::Warn("Shader %s has a colormap stage with no image", shader.name );
						pStage->active = false;
						continue;
					}

					break;
				}

			case stageType_t::ST_DIFFUSEMAP:
				{
					if ( !shader.isSky )
					{
						shader.interactLight = true;
					}

					if ( !pStage->bundle[ 0 ].image[ 0 ] )
					{
						Log::Warn("Shader %s has a diffusemap stage with no image", shader.name );
						pStage->bundle[ 0 ].image[ 0 ] = tr.defaultImage;
					}

					break;
				}

			case stageType_t::ST_NORMALMAP:
				{
					if ( !shader.isSky )
					{
						shader.interactLight = true;
					}

					if ( !pStage->bundle[ 0 ].image[ 0 ] )
					{
						Log::Warn("Shader %s has a normalmap stage with no image", shader.name );
						pStage->bundle[ 0 ].image[ 0 ] = tr.flatImage;
					}

					break;
				}

			case stageType_t::ST_SPECULARMAP:
				{
					if ( !pStage->bundle[ 0 ].image[ 0 ] )
					{
						Log::Warn("Shader %s has a specularmap stage with no image", shader.name );
						pStage->bundle[ 0 ].image[ 0 ] = tr.blackImage;
					}

					break;
				}

			case stageType_t::ST_ATTENUATIONMAP_XY:
				{
					if ( !pStage->bundle[ 0 ].image[ 0 ] )
					{
						Log::Warn("Shader %s has a xy attenuationmap stage with no image", shader.name );
						pStage->active = false;
						continue;
					}

					break;
				}

			case stageType_t::ST_ATTENUATIONMAP_Z:
				{
					if ( !pStage->bundle[ 0 ].image[ 0 ] )
					{
						Log::Warn("Shader %s has a z attenuationmap stage with no image", shader.name );
						pStage->active = false;
						continue;
					}

					break;
				}
		}

		if ( shader.forceOpaque )
		{
			pStage->stateBits |= GLS_DEPTHMASK_TRUE;
		}

		if ( shader.isSky && pStage->noFog )
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_ENVIRONMENT_NOFOG);
		}

		// determine sort order and fog color adjustment
		if ( ( pStage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
		     ( stages[ 0 ].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) )
		{
			// don't screw with sort order if this is a portal or environment
			if ( !shader.sort )
			{
				// see through item, like a grill or grate
				if ( pStage->stateBits & GLS_DEPTHMASK_TRUE )
				{
					shader.sort = Util::ordinal(shaderSort_t::SS_SEE_THROUGH);
				}
				else
				{
					shader.sort = Util::ordinal(shaderSort_t::SS_BLEND0);
				}
			}
		}
	}

	shader.numStages = stage;

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if ( !shader.sort )
	{
		if ( shader.translucent && !shader.forceOpaque )
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_DECAL);
		}
		else
		{
			shader.sort = Util::ordinal(shaderSort_t::SS_OPAQUE);
		}
	}

	// HACK: allow alpha tested surfaces to create shadowmaps
	if ( r_shadows->integer >= Util::ordinal(shadowingMode_t::SHADOWING_ESM16) )
	{
		if ( shader.noShadows && shader.alphaTest )
		{
			shader.noShadows = false;
		}
	}

	// look for multitexture potential
	CollapseStages();

	// fogonly shaders don't have any stage passes
	if ( shader.numStages == 0 && !shader.isSky )
	{
		shader.sort = Util::ordinal(shaderSort_t::SS_FOG);
	}

	if ( shader.numDeforms > 0 ) {
		for( i = 0; i < shader.numStages; i++ ) {
			stages[i].deformIndex = gl_shaderManager.getDeformShaderIndex( shader.deforms, shader.numDeforms );
		}
	}

	ret = GeneratePermanentShader();

	// generate depth-only shader if necessary
	if( !shader.isSky &&
	    shader.numStages > 0 &&
	    (stages[0].stateBits & GLS_DEPTHMASK_TRUE) &&
	    !(stages[0].stateBits & GLS_DEPTHFUNC_EQUAL) &&
	    !(shader.type == shaderType_t::SHADER_2D) &&
	    !shader.polygonOffset ) {
		// keep only the first stage
		stages[1].active = false;
		shader.numStages = 1;
		strcat(shader.name, "$depth");

		if( stages[0].stateBits & GLS_ATEST_BITS ) {
			// alpha test requires a custom depth shader
			shader.sort = Util::ordinal( shaderSort_t::SS_DEPTH );
			stages[0].stateBits &= ~GLS_SRCBLEND_BITS & ~GLS_DSTBLEND_BITS;
			stages[0].stateBits |= GLS_COLORMASK_BITS;
			stages[0].type = stageType_t::ST_COLORMAP;

			ret->depthShader = GeneratePermanentShader();
		} else if ( shader.cullType == 0 &&
			    shader.numDeforms == 0 &&
			    tr.defaultShader ) {
			// can use the default depth shader
			ret->depthShader = tr.defaultShader->depthShader;
		} else {
			// requires a custom depth shader, but can skip
			// the texturing
			shader.sort = Util::ordinal( shaderSort_t::SS_DEPTH );
			stages[0].stateBits &= ~GLS_SRCBLEND_BITS & ~GLS_DSTBLEND_BITS;
			stages[0].stateBits |= GLS_COLORMASK_BITS;
			stages[0].type = stageType_t::ST_COLORMAP;

			stages[0].bundle[0].image[0] = tr.whiteImage;
			stages[0].bundle[0].numTexMods = 0;
			stages[0].tcGen_Environment = false;
			stages[0].tcGen_Lightmap = false;
			stages[0].rgbGen = colorGen_t::CGEN_IDENTITY;
			stages[0].alphaGen = alphaGen_t::AGEN_IDENTITY;

			ret->depthShader = GeneratePermanentShader();
		}
		// disable depth writes in the main pass
		ret->stages[0]->stateBits &= ~GLS_DEPTHMASK_TRUE;
	} else {
		ret->depthShader = nullptr;
	}

	// load all altShaders recursively
	for ( i = 1; i < MAX_ALTSHADERS; ++i )
	{
		if ( ret->altShader[ i ].name )
		{
			// flags were previously stashed in altShader[0].index
			shader_t *sh = R_FindShader( ret->altShader[ i ].name, ret->type, (RegisterShaderFlags_t)ret->altShader[ 0 ].index );

			ret->altShader[ i ].index = sh->defaultShader ? 0 : sh->index;
		}
	}

	return ret;
}

//========================================================================================

//bani - dynamic shader list
struct dynamicshader_t
{
	char            *shadertext;
	dynamicshader_t *next;
};

static dynamicshader_t *dshader = nullptr;

/*
====================
RE_LoadDynamicShader

bani - load a new dynamic shader

if shadertext is nullptr, looks for matching shadername and removes it

returns true if request was successful, false if the gods were angered
====================
*/
bool RE_LoadDynamicShader( const char *shadername, const char *shadertext )
{
	const char      *func_err = "RE_LoadDynamicShader";
	dynamicshader_t *dptr, *lastdptr;
	const char            *q, *token;

	Log::Warn("RE_LoadDynamicShader( name = '%s', text = '%s' )", shadername, shadertext );

	if ( !shadername && shadertext )
	{
		Log::Warn("%s called with NULL shadername and non-NULL shadertext:\n%s", func_err, shadertext );
		return false;
	}

	if ( shadername && strlen( shadername ) >= MAX_QPATH )
	{
		Log::Warn("%s shadername %s exceeds MAX_QPATH", func_err, shadername );
		return false;
	}

	//empty the whole list
	if ( !shadername && !shadertext )
	{
		dptr = dshader;

		while ( dptr )
		{
			lastdptr = dptr->next;
			ri.Free( dptr->shadertext );
			ri.Free( dptr );
			dptr = lastdptr;
		}

		dshader = nullptr;
		return true;
	}

	//walk list for existing shader to delete, or end of the list
	dptr = dshader;
	lastdptr = nullptr;

	while ( dptr )
	{
		q = dptr->shadertext;

		token = COM_ParseExt( &q, true );

		if ( ( token[ 0 ] != 0 ) && !Q_stricmp( token, shadername ) )
		{
			//request to nuke this dynamic shader
			if ( !shadertext )
			{
				if ( !lastdptr )
				{
					dshader = nullptr;
				}
				else
				{
					lastdptr->next = dptr->next;
				}

				ri.Free( dptr->shadertext );
				ri.Free( dptr );
				return true;
			}

			Log::Warn("%s shader %s already exists!", func_err, shadername );
			return false;
		}

		lastdptr = dptr;
		dptr = dptr->next;
	}

	//can't add a new one with empty shadertext
	if ( !shadertext || !strlen( shadertext ) )
	{
		Log::Warn("%s new shader %s has NULL shadertext!", func_err, shadername );
		return false;
	}

	//create a new shader
	dptr = ( dynamicshader_t * ) ri.Z_Malloc( sizeof( *dptr ) );

	if ( !dptr )
	{
		Sys::Error( "Couldn't allocate struct for dynamic shader %s", shadername );
	}

	if ( lastdptr )
	{
		lastdptr->next = dptr;
	}

	dptr->shadertext = (char*) ri.Z_Malloc( strlen( shadertext ) + 1 );

	if ( !dptr->shadertext )
	{
		Sys::Error( "Couldn't allocate buffer for dynamic shader %s", shadername );
	}

	Q_strncpyz( dptr->shadertext, shadertext, strlen( shadertext ) + 1 );
	dptr->next = nullptr;

	if ( !dshader )
	{
		dshader = dptr;
	}

	return true;
}

//========================================================================================

/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return nullptr if not found

If found, it will return a valid shader
=====================
*/
static const char    *FindShaderInShaderText( const char *shaderName )
{
	const char *token, *p;

	int  i, hash;

	hash = generateHashValue( shaderName, MAX_SHADERTEXT_HASH );

	for ( i = 0; shaderTextHashTable[ hash ][ i ]; i++ )
	{
		p = shaderTextHashTable[ hash ][ i ];
		token = COM_ParseExt2( &p, true );

		if ( !Q_stricmp( token, shaderName ) )
		{
			return p;
		}
	}

	// if the shader is not in the table, it must not exist
	return nullptr;
}

/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.
==================
*/
shader_t       *R_FindShaderByName( const char *name )
{
	char     strippedName[ MAX_QPATH ];
	int      hash;
	shader_t *sh;

	if ( ( name == nullptr ) || ( name[ 0 ] == 0 ) )
	{
		// bk001205
		return tr.defaultShader;
	}

	COM_StripExtension3( name, strippedName, sizeof( strippedName ) );

	hash = generateHashValue( strippedName, FILE_HASH_SIZE );

	// see if the shader is already loaded
	for ( sh = shaderHashTable[ hash ]; sh; sh = sh->next )
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with type == SHADER_3D_DYNAMIC, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( Q_stricmp( sh->name, strippedName ) == 0 )
		{
			// match found
			return sh;
		}
	}

	return tr.defaultShader;
}

/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If type == SHADER_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If type == SHADER_3D_DYNAMIC, then the image will have
dynamic diffuse lighting applied to it, as appropriate for most
entity skin surfaces.

If type == SHADER_3D_STATIC, then the image will use
the vertex rgba modulate values, as appropriate for misc_model
pre-lit surfaces.
===============
*/
shader_t       *R_FindShader( const char *name, shaderType_t type,
			      RegisterShaderFlags_t flags )
{
	char     strippedName[ MAX_QPATH ];
	char     fileName[ MAX_QPATH ];
	int      i, hash, bits;
	const char     *shaderText;
	image_t  *image;
	shader_t *sh;

	if ( name[ 0 ] == 0 )
	{
		return tr.defaultShader;
	}

	COM_StripExtension3( name, strippedName, sizeof( strippedName ) );

	hash = generateHashValue( strippedName, FILE_HASH_SIZE );

	// see if the shader is already loaded
	for ( sh = shaderHashTable[ hash ]; sh; sh = sh->next )
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with type == SHADER_3D_DYNAMIC, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( ( sh->type == type || sh->defaultShader ) && !Q_stricmp( sh->name, strippedName ) )
		{
			// match found
			return sh;
		}
	}

	shader.altShader[ 0 ].index = flags; // save for later use (in case of alternative shaders)

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	if ( r_smp->integer )
	{
		R_SyncRenderThread();
	}

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz( shader.name, strippedName, sizeof( shader.name ) );
	shader.type = type;

	for ( i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		stages[ i ].bundle[ 0 ].texMods = texMods[ i ];
	}

	// ydnar: default to no implicit mappings
	implicitMap[ 0 ] = '\0';
	implicitStateBits = GLS_DEFAULT;
	if( shader.type == shaderType_t::SHADER_2D )
	{
		implicitCullType = CT_TWO_SIDED;
	}
	else
	{
		implicitCullType = CT_FRONT_SIDED;
	}

	if( flags & RSF_SPRITE ) {
		shader.autoSpriteMode = 1;
	}

	// attempt to define shader from an explicit parameter file
	shaderText = FindShaderInShaderText( strippedName );

	if ( shaderText )
	{
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if ( r_printShaders->integer )
		{
			Log::Notice("loading explicit shader '%s'", strippedName );
		}

		if ( !ParseShader( shaderText ) )
		{
			// had errors, so use default shader
			shader.defaultShader = true;
			sh = FinishShader();
			return sh;
		}

		// ydnar: allow implicit mappings
		if ( implicitMap[ 0 ] == '\0' )
		{
			sh = FinishShader();
			return sh;
		}
	}

	// ydnar: allow implicit mapping ('-' = use shader name)
	if ( implicitMap[ 0 ] == '\0' || implicitMap[ 0 ] == '-' )
	{
		Q_strncpyz( fileName, strippedName, sizeof( fileName ) );
	}
	else
	{
		Q_strncpyz( fileName, implicitMap, sizeof( fileName ) );
	}

	// if not defined in the in-memory shader descriptions,
	// look for a single supported image file
	bits = IF_NONE;
	if( flags & RSF_NOMIP )
		bits |= IF_NOPICMIP;
	else
		shader.noPicMip = true;

	if( flags & RSF_NOLIGHTSCALE )
		bits |= IF_NOLIGHTSCALE;

	Log::Debug( "loading '%s' image as shader", fileName );

	// choosing filter based on the NOMIP flag seems strange,
	// maybe it should be changed to type == SHADER_2D
	if( !(bits & RSF_NOMIP) ) {
		LoadExtraMaps( &stages[ 0 ], fileName );

		imageParams_t imageParams = {};
		imageParams.bits = bits;
		imageParams.filterType = filterType_t::FT_DEFAULT;
		imageParams.wrapType = wrapTypeEnum_t::WT_REPEAT;

		image = R_FindImageFile( fileName, &imageParams );
	} else {
		imageParams_t imageParams = {};
		imageParams.bits = bits;
		imageParams.filterType = filterType_t::FT_LINEAR;
		imageParams.wrapType = wrapTypeEnum_t::WT_CLAMP;

		image = R_FindImageFile( fileName, &imageParams );
	}

	if ( !image )
	{
		Log::Warn("Couldn't find image file '%s'", name );
		shader.defaultShader = true;
		return FinishShader();
	}

	// set implicit cull type
	if ( implicitCullType && !shader.cullType )
	{
		shader.cullType = implicitCullType;
	}

	// create the default shading commands
	switch ( shader.type )
	{
		case shaderType_t::SHADER_2D:
			{
				// GUI elements
				stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
				stages[ 0 ].active = true;
				stages[ 0 ].rgbGen = colorGen_t::CGEN_VERTEX;
				stages[ 0 ].alphaGen = alphaGen_t::AGEN_VERTEX;
				stages[ 0 ].stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				break;
			}

		case shaderType_t::SHADER_3D_DYNAMIC:
			{
				// dynamic colors at vertexes
				stages[ 0 ].type = stageType_t::ST_DIFFUSEMAP;
				stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
				stages[ 0 ].active = true;
				stages[ 0 ].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
				stages[ 0 ].stateBits = implicitStateBits;
				break;
			}

		case shaderType_t::SHADER_3D_STATIC:
			{
				// explicit colors at vertexes
				stages[ 0 ].type = stageType_t::ST_DIFFUSEMAP;
				stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
				stages[ 0 ].active = true;
				stages[ 0 ].rgbGen = colorGen_t::CGEN_IDENTITY;
				stages[ 0 ].stateBits = implicitStateBits;
				break;
			}

		case shaderType_t::SHADER_LIGHT:
			{
				stages[ 0 ].type = stageType_t::ST_ATTENUATIONMAP_Z;
				stages[ 0 ].bundle[ 0 ].image[ 0 ] = tr.noFalloffImage; // FIXME should be attenuationZImage
				stages[ 0 ].active = true;
				stages[ 0 ].rgbGen = colorGen_t::CGEN_IDENTITY;
				stages[ 0 ].stateBits = GLS_DEFAULT;

				stages[ 1 ].type = stageType_t::ST_ATTENUATIONMAP_XY;
				stages[ 1 ].bundle[ 0 ].image[ 0 ] = image;
				stages[ 1 ].active = true;
				stages[ 1 ].rgbGen = colorGen_t::CGEN_IDENTITY;
				stages[ 1 ].stateBits = GLS_DEFAULT;
				break;
			}

		default:
			break;
	}

	return FinishShader();
}

qhandle_t RE_RegisterShaderFromImage( const char *name, image_t *image )
{
	int      i, hash;
	shader_t *sh;

	hash = generateHashValue( name, FILE_HASH_SIZE );

	// see if the shader is already loaded
	for ( sh = shaderHashTable[ hash ]; sh; sh = sh->next )
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with type == SHADER_3D_DYNAMIC, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( ( sh->type == shaderType_t::SHADER_2D || sh->defaultShader ) && !Q_stricmp( sh->name, name ) )
		{
			// match found
			return sh->index;
		}
	}

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	if ( r_smp->integer )
	{
		R_SyncRenderThread();
	}

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz( shader.name, name, sizeof( shader.name ) );
	shader.type = shaderType_t::SHADER_2D;
	shader.cullType = CT_TWO_SIDED;

	for ( i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		stages[ i ].bundle[ 0 ].texMods = texMods[ i ];
	}

	// create the default shading commands

	// GUI elements
	stages[ 0 ].bundle[ 0 ].image[ 0 ] = image;
	stages[ 0 ].active = true;
	stages[ 0 ].rgbGen = colorGen_t::CGEN_VERTEX;
	stages[ 0 ].alphaGen = alphaGen_t::AGEN_VERTEX;
	stages[ 0 ].stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

	sh = FinishShader();
	return sh->index;
}

/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader( const char *name, RegisterShaderFlags_t flags )
{
	shader_t *sh;

	if ( strlen( name ) >= MAX_QPATH )
	{
		Log::Notice( "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name,
			   (flags & RSF_LIGHT_ATTENUATION) ? shaderType_t::SHADER_LIGHT : shaderType_t::SHADER_2D,
			   flags );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader )
	{
		return 0;
	}

	return sh->index;
}

/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t       *R_GetShaderByHandle( qhandle_t hShader )
{
	if ( hShader < 0 )
	{
		Log::Warn("R_GetShaderByHandle: out of range hShader '%d'", hShader );  // bk: FIXME name
		return tr.defaultShader;
	}

	if ( hShader >= tr.numShaders )
	{
		Log::Warn("R_GetShaderByHandle: out of range hShader '%d'", hShader );
		return tr.defaultShader;
	}

	return tr.shaders[ hShader ];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void R_ShaderList_f()
{
	int      i;
	int      count;
	shader_t *shader;
	const char *s = nullptr;

	Log::Notice("-----------------------" );

	if ( ri.Cmd_Argc() > 1 )
	{
		s = ri.Cmd_Argv( 1 );
	}

	count = 0;

	for ( i = 0; i < tr.numShaders; i++ )
	{
		if ( ri.Cmd_Argc() > 2 )
		{
			shader = tr.sortedShaders[ i ];
		}
		else
		{
			shader = tr.shaders[ i ];
		}

		if ( s && !Com_Filter( s, shader->name, false ) )
		{
			continue;
		}

		std::string str;

		switch ( shader->type )
		{
			case shaderType_t::SHADER_2D:
				str += "2D   ";
				break;

			case shaderType_t::SHADER_3D_DYNAMIC:
				str += "3D_D ";
				break;

			case shaderType_t::SHADER_3D_STATIC:
				str += "3D_S ";
				break;

			case shaderType_t::SHADER_LIGHT:
				str += "ATTN ";
				break;
		}

		// there may be multiple kind of collapse stage in one shader,
		// display first one since there is only one column to tell about it
		bool foundCollapseType = false;
		for ( int i = 0; i < shader->numStages; i++ )
		{
			if ( shader->stages[ i ]->active )
			{
				if ( shader->stages[ i ]->collapseType == collapseType_t::COLLAPSE_lighting_PBR )
				{
					str += "lighting_PBR   ";
					foundCollapseType = true;
					break;
				}
				else if ( shader->stages[ i ]->collapseType == collapseType_t::COLLAPSE_lighting_PHONG )
				{
					str += "lighting_PHONG ";
					foundCollapseType = true;
					break;
				}
				else if ( shader->stages[ i ]->collapseType == collapseType_t::COLLAPSE_reflection_CB )
				{
					str += "reflection_CB  ";
					foundCollapseType = true;
					break;
				}
			}
		}
		if ( !foundCollapseType )
		{
			str += "               ";
		}

		if ( shader->sort == Util::ordinal(shaderSort_t::SS_BAD) )
		{
			str += "SS_BAD              ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_PORTAL) )
		{
			str += "SS_PORTAL           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_DEPTH) )
		{
			str += "SS_DEPTH            ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_ENVIRONMENT_FOG) )
		{
			str += "SS_ENVIRONMENT_FOG  ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_ENVIRONMENT_NOFOG) )
		{
			str += "SS_ENVIRONMENT_NOFOG";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_OPAQUE) )
		{
			str += "SS_OPAQUE           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_DECAL) )
		{
			str += "SS_DECAL            ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_SEE_THROUGH) )
		{
			str += "SS_SEE_THROUGH      ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_BANNER) )
		{
			str += "SS_BANNER           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_FOG) )
		{
			str += "SS_FOG              ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_UNDERWATER) )
		{
			str += "SS_UNDERWATER       ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_WATER) )
		{
			str += "SS_WATER            ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_FAR) )
		{
			str += "SS_FAR              ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_MEDIUM) )
		{
			str += "SS_MEDIUM           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_CLOSE) )
		{
			str += "SS_CLOSE            ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_BLEND0) )
		{
			str += "SS_BLEND0           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_BLEND1) )
		{
			str += "SS_BLEND1           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_BLEND2) )
		{
			str += "SS_BLEND2           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_BLEND3) )
		{
			str += "SS_BLEND3           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_BLEND6) )
		{
			str += "SS_BLEND6           ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_ALMOST_NEAREST) )
		{
			str += "SS_ALMOST_NEAREST   ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_NEAREST) )
		{
			str += "SS_NEAREST          ";
		}
		else if ( shader->sort == Util::ordinal(shaderSort_t::SS_POST_PROCESS) )
		{
			str += "SS_POST_PROCESS     ";
		}
		else
		{
			str += "                    ";
		}

		if ( shader->interactLight )
		{
			str += "IA ";
		}
		else
		{
			str += "   ";
		}

		if ( shader->defaultShader )
		{
			Log::Notice("%s: %s (DEFAULTED)", str.c_str(), shader->name );
		}
		else
		{
			Log::Notice("%s: %s", str.c_str(), shader->name );
		}

		count++;
	}

	Log::Notice("%i total shaders", count );
	Log::Notice("------------------" );
}

void R_ShaderExp_f()
{
	expression_t exp;

	strcpy( shader.name, "dummy" );

	Log::Notice("-----------------------" );

	std::string buffer;

	for ( int i = 1; i < ri.Cmd_Argc(); i++ )
	{
		if ( i > 1 )
		{
			buffer += ' ';
		}

		buffer += ri.Cmd_Argv( i );
	}

	const char* buffer_p = buffer.c_str();
	ParseExpression( &buffer_p, &exp );

	Log::Notice("%i total ops", exp.numOps );
	Log::Notice("%f result", RB_EvalExpression( &exp, 0 ) );
	Log::Notice("------------------" );
}

/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static const int MAX_SHADER_FILES = 4096;
static void ScanAndLoadShaderFiles()
{
	char **shaderFiles;
	char *buffers[ MAX_SHADER_FILES ];
	const char *p;
	int  numShaderFiles;
	int  i;
	const char *oldp, *token;
	char *textEnd;
	const char **hashMem;
	int  shaderTextHashTableSizes[ MAX_SHADERTEXT_HASH ], hash, size;
	char filename[ MAX_QPATH ];
	size_t sum = 0, summand;

	Log::Debug("----- ScanAndLoadShaderFiles -----" );

	// scan for shader files
	shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );

	if ( !shaderFiles || !numShaderFiles )
	{
		Log::Warn("no shader files found" );
	}

	if ( numShaderFiles > MAX_SHADER_FILES )
	{
		numShaderFiles = MAX_SHADER_FILES;
	}

	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{
		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[ i ] );

		Log::Debug("loading '%s' shader file", filename );
		summand = ri.FS_ReadFile( filename, ( void ** ) &buffers[ i ] );

		if ( !buffers[ i ] )
		{
			Log::Warn( "Couldn't load shader file %s", filename );
			continue;
		}

		p = buffers[ i ];

		while ( true )
		{
			token = COM_ParseExt2( &p, true );

			if ( !*token )
			{
				break;
			}

			// Step over the "table" and the name
			if ( !Q_stricmp( token, "table" ) )
			{
				token = COM_ParseExt2( &p, true );

				if ( !*token )
				{
					break;
				}
			}

			token = COM_ParseExt2( &p, true );

			if ( token[ 0 ] != '{' || token[ 1 ] != '\0' || !SkipBracedSection_Depth( &p, 1 ) )
			{
				Log::Warn("Bad shader file %s has incorrect syntax.", filename );
				ri.FS_FreeFile( buffers[ i ] );
				buffers[ i ] = nullptr;
				break;
			}
		}

		if ( buffers[ i ] )
		{
			sum += summand;
		}
	}

	// build single large buffer
	s_shaderText = (char*) ri.Hunk_Alloc( sum + numShaderFiles * 2, ha_pref::h_low );
	s_shaderText[ 0 ] = '\0';
	textEnd = s_shaderText;

	// free in reverse order, so the temp files are all dumped
	for ( i = numShaderFiles - 1; i >= 0; i-- )
	{
		if ( !buffers[ i ] )
		{
			continue;
		}

		strcat( textEnd, buffers[ i ] );
		strcat( textEnd, "\n" );
		textEnd += strlen( textEnd );
		ri.FS_FreeFile( buffers[ i ] );
	}

	// ydnar: unixify all shaders
	COM_FixPath( s_shaderText );

	COM_Compress( s_shaderText );

	// free up memory
	ri.FS_FreeFileList( shaderFiles );

	Com_Memset( shaderTextHashTableSizes, 0, sizeof( shaderTextHashTableSizes ) );
	size = 0;

	p = s_shaderText;

	// look for shader names
	while ( true )
	{
		token = COM_ParseExt2( &p, true );

		if ( token[ 0 ] == 0 )
		{
			break;
		}

		// skip shader tables
		if ( !Q_stricmp( token, "table" ) )
		{
			// skip table name
			token = COM_ParseExt2( &p, true );

			SkipBracedSection( &p );
		}
		else
		{
			hash = generateHashValue( token, MAX_SHADERTEXT_HASH );
			shaderTextHashTableSizes[ hash ]++;
			size++;
			SkipBracedSection( &p );
		}
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = (const char**) ri.Hunk_Alloc( size * sizeof( char * ), ha_pref::h_low );

	for ( i = 0; i < MAX_SHADERTEXT_HASH; i++ )
	{
		shaderTextHashTable[ i ] = hashMem;
		hashMem += shaderTextHashTableSizes[ i ] + 1;
	}

	Com_Memset( shaderTextHashTableSizes, 0, sizeof( shaderTextHashTableSizes ) );

	p = s_shaderText;

	// look for shader names
	while ( true )
	{
		oldp = p;
		token = COM_ParseExt( &p, true );

		if ( token[ 0 ] == 0 )
		{
			break;
		}

		// parse shader tables
		if ( !Q_stricmp( token, "table" ) )
		{
			int           depth;
			float         values[ FUNCTABLE_SIZE ];
			int           numValues;
			shaderTable_t *tb;
			bool      alreadyCreated;

			// zeroes all shaders, booleans can be assumed as false
			Com_Memset( &table, 0, sizeof( table ) );

			token = COM_ParseExt2( &p, true );

			Q_strncpyz( table.name, token, sizeof( table.name ) );

			// check if already created
			alreadyCreated = false;
			hash = generateHashValue( table.name, MAX_SHADERTABLE_HASH );

			for ( tb = shaderTableHashTable[ hash ]; tb; tb = tb->next )
			{
				if ( Q_stricmp( tb->name, table.name ) == 0 )
				{
					// match found
					alreadyCreated = true;
					break;
				}
			}

			depth = 0;
			numValues = 0;

			do
			{
				token = COM_ParseExt2( &p, true );

				if ( !Q_stricmp( token, "snap" ) )
				{
					table.snap = true;
				}
				else if ( !Q_stricmp( token, "clamp" ) )
				{
					table.clamp = true;
				}
				else if ( token[ 0 ] == '{' )
				{
					depth++;
				}
				else if ( token[ 0 ] == '}' )
				{
					depth--;
				}
				else if ( token[ 0 ] == ',' )
				{
					continue;
				}
				else
				{
					if ( numValues == FUNCTABLE_SIZE )
					{
						Log::Warn("FUNCTABLE_SIZE hit" );
						break;
					}

					values[ numValues++ ] = atof( token );
				}
			}
			while ( depth && p );

			if ( !alreadyCreated )
			{
				Log::Debug("...generating '%s'", table.name );
				GeneratePermanentShaderTable( values, numValues );
			}
		}
		else
		{
			hash = generateHashValue( token, MAX_SHADERTEXT_HASH );
			shaderTextHashTable[ hash ][ shaderTextHashTableSizes[ hash ]++ ] = oldp;

			SkipBracedSection( &p );
		}
	}
}

/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders()
{
	Log::Debug("----- CreateInternalShaders -----" );

	tr.numShaders = 0;

	// init the default shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );

	Q_strncpyz( shader.name, "<default>", sizeof( shader.name ) );

	shader.type = shaderType_t::SHADER_3D_DYNAMIC;
	stages[ 0 ].type = stageType_t::ST_DIFFUSEMAP;
	stages[ 0 ].bundle[ 0 ].image[ 0 ] = tr.defaultImage;
	stages[ 0 ].active = true;
	stages[ 0 ].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();
}

static void CreateExternalShaders()
{
	Log::Debug("----- CreateExternalShaders -----" );

	tr.flareShader = R_FindShader( "flareShader", shaderType_t::SHADER_3D_DYNAMIC, RSF_DEFAULT );
	tr.sunShader = R_FindShader( "sun", shaderType_t::SHADER_3D_DYNAMIC, RSF_DEFAULT );

	tr.defaultPointLightShader = R_FindShader( "lights/defaultPointLight", shaderType_t::SHADER_LIGHT, RSF_DEFAULT );
	tr.defaultProjectedLightShader = R_FindShader( "lights/defaultProjectedLight", shaderType_t::SHADER_LIGHT, RSF_DEFAULT );
	tr.defaultDynamicLightShader = R_FindShader( "lights/defaultDynamicLight", shaderType_t::SHADER_LIGHT, RSF_DEFAULT );
}

/*
==================
R_InitShaders
==================
*/
void R_InitShaders()
{
	Com_Memset( shaderTableHashTable, 0, sizeof( shaderTableHashTable ) );
	Com_Memset( shaderHashTable, 0, sizeof( shaderHashTable ) );

	CreateInternalShaders();

	ScanAndLoadShaderFiles();

	CreateExternalShaders();
}

/*
==================
R_SetAltShaderTokens
==================
*/
void R_SetAltShaderTokens( const char *list )
{
	char *p;

	memset( whenTokens, 0, sizeof( whenTokens ) );
	Q_strncpyz( whenTokens, list, sizeof( whenTokens ) - 1 ); // will have double-NUL termination

	p = whenTokens - 1;

	while ( ( p = strchr( p + 1, ',' ) ) )
	{
		*p = 0;
	}
}

/*
==================
RE_GetShaderNameFromHandle
==================
*/

const char *RE_GetShaderNameFromHandle( qhandle_t shader )
{
	return R_GetShaderByHandle( shader )->name;
}
