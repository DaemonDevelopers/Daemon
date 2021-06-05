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

// cl_main.c  -- client main loop

#include "client.h"
#include "qcommon/q_unicode.h"
#include "qcommon/sys.h"

#include "common/Defs.h"

#include "framework/CommandSystem.h"
#include "framework/CvarSystem.h"

#include "mumblelink/libmumblelink.h"
#include "qcommon/crypto.h"
#include "framework/Rcon.h"
#include "framework/Crypto.h"
#include "framework/Network.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif
#ifdef BUILD_GRAPHICAL_CLIENT
#include <SDL.h>
#endif

cvar_t *cl_useMumble;
cvar_t *cl_mumbleScale;

cvar_t *cl_nodelta;

cvar_t *cl_noprint;

cvar_t *cl_timeout;
cvar_t *cl_maxpackets;
cvar_t *cl_packetdup;
cvar_t *cl_timeNudge;
cvar_t *cl_showTimeDelta;

cvar_t *cl_shownet = nullptr; // NERVE - SMF - This is referenced in msg.c and we need to make sure it is nullptr
cvar_t *cl_shownuments; // DHM - Nerve
cvar_t *cl_showSend;
cvar_t *cl_showServerCommands; // NERVE - SMF

Cvar::Cvar<bool> cvar_demo_status_isrecording(
    "demo.status.isrecording",
    "(Read-only) Whether there is a demo currently being recorded",
    Cvar::ROM,
    false
);

Cvar::Cvar<std::string> cvar_demo_status_filename(
    "demo.status.filename",
    "(Read-only) Name of the demo currently being recorded",
    Cvar::ROM,
    ""
);

cvar_t *cl_aviFrameRate;

cvar_t *cl_freelook;
cvar_t *cl_sensitivity;
cvar_t *cl_gameControllerAvailable;

cvar_t *cl_mouseAccelOffset;
cvar_t *cl_mouseAccel;
cvar_t *cl_mouseAccelStyle;
cvar_t *cl_showMouseRate;

cvar_t *m_pitch;
cvar_t *m_yaw;
cvar_t *m_forward;
cvar_t *m_side;
cvar_t *m_filter;

cvar_t *j_pitch;
cvar_t *j_yaw;
cvar_t *j_forward;
cvar_t *j_side;
cvar_t *j_up;
cvar_t *j_pitch_axis;
cvar_t *j_yaw_axis;
cvar_t *j_forward_axis;
cvar_t *j_side_axis;
cvar_t *j_up_axis;

cvar_t *cl_activeAction;

cvar_t *cl_autorecord;

cvar_t *cl_allowDownload;
cvar_t *cl_inGameVideo;

cvar_t *cl_serverStatusResendTime;

cvar_t                 *cl_packetloss; //bani
cvar_t                 *cl_packetdelay; //bani

cvar_t                 *cl_consoleFont;
cvar_t                 *cl_consoleFontSize;
cvar_t                 *cl_consoleFontScaling;
cvar_t                 *cl_consoleFontKerning;
cvar_t                 *cl_consoleCommand; //see also com_consoleCommand for terminal consoles

cvar_t	*cl_logs;

cvar_t             *p_team; /*<team id without team semantics (to not break the relationship between client and cgame)*/

struct rsa_public_key  public_key;
struct rsa_private_key private_key;

cvar_t             *cl_gamename;

cvar_t             *cl_altTab;

// XreaL BEGIN
cvar_t             *cl_aviMotionJpeg;
// XreaL END

cvar_t             *cl_rate;

cvar_t             *cl_cgameSyscallStats;

clientActive_t     cl;
clientConnection_t clc;
clientStatic_t     cls;
CGameVM            cgvm;

// Structure containing functions exported from refresh DLL
refexport_t        re;

ping_t             cl_pinglist[ MAX_PINGREQUESTS ];

struct serverStatus_t
{
	char     string[ BIG_INFO_STRING ];
	netadr_t address;
	int      time, startTime;
	bool pending;
	bool print;
	bool retrieved;
};

serverStatus_t cl_serverStatusList[ MAX_SERVERSTATUSREQUESTS ];
int            serverStatusCount;

void        CL_CheckForResend();
void        CL_ShowIP_f();
void        CL_ServerStatus_f();
void        CL_ServerStatusResponse( const netadr_t& from, msg_t *msg );

static void CL_UpdateMumble()
{
	vec3_t pos, forward, up;
	float  scale = cl_mumbleScale->value;
	float  tmp;

	if ( !cl_useMumble->integer )
	{
		return;
	}

	// !!! FIXME: not sure if this is even close to correct.
	AngleVectors( cl.snap.ps.viewangles, forward, nullptr, up );

	pos[ 0 ] = cl.snap.ps.origin[ 0 ] * scale;
	pos[ 1 ] = cl.snap.ps.origin[ 2 ] * scale;
	pos[ 2 ] = cl.snap.ps.origin[ 1 ] * scale;

	tmp = forward[ 1 ];
	forward[ 1 ] = forward[ 2 ];
	forward[ 2 ] = tmp;

	tmp = up[ 1 ];
	up[ 1 ] = up[ 2 ];
	up[ 2 ] = tmp;

	if ( cl_useMumble->integer > 1 )
	{
		fprintf( stderr, "%f %f %f, %f %f %f, %f %f %f\n",
		         pos[ 0 ], pos[ 1 ], pos[ 2 ],
		         forward[ 0 ], forward[ 1 ], forward[ 2 ],
		         up[ 0 ], up[ 1 ], up[ 2 ] );
	}

	mumble_update_coordinates( pos, forward, up );
}

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand( const char *cmd )
{
	int index;

	// catch empty commands
	while ( *cmd && *cmd <= ' ' )
	{
		++cmd;
	}

	if ( !*cmd )
	{
		return;
	}

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	if ( clc.reliableSequence - clc.reliableAcknowledge > MAX_RELIABLE_COMMANDS )
	{
		Sys::Drop( "Client command overflow" );
	}

	clc.reliableSequence++;
	index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}

/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage( msg_t *msg, int headerBytes )
{
	int len, swlen;

	// write the packet sequence
	len = clc.serverMessageSequence;
	swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.demofile );

	// skip the packet sequencing information
	len = msg->cursize - headerBytes;
	swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.demofile );
	FS_Write( msg->data + headerBytes, len, clc.demofile );
}


/**
 * If a demo is being recorded, this stops it
 */
static void CL_StopRecord()
{
    if ( !clc.demorecording )
        return;

    // finish up
    int len = -1;
    FS_Write( &len, 4, clc.demofile );
    FS_Write( &len, 4, clc.demofile );
    FS_FCloseFile( clc.demofile );
    clc.demofile = 0;

    clc.demorecording = false;
    Cvar::SetValueForce(cvar_demo_status_isrecording.Name(), "0");
    Cvar::SetValueForce(cvar_demo_status_filename.Name(), "");
    Log::Notice("Stopped demo." );
}

class DemoRecordStopCmd: public Cmd::StaticCmd
{
public:
    DemoRecordStopCmd()
        : Cmd::StaticCmd("demo_record_stop", Cmd::SYSTEM, "Stops recording a demo")
    {}

    void Run(const Cmd::Args&) const override
    {
        if ( !clc.demorecording )
        {
            Log::Notice("Not recording a demo." );
            return;
        }
        CL_StopRecord();
    }
};
static DemoRecordStopCmd DemoRecordStopCmdRegistration;


class DemoRecordCmd : public Cmd::StaticCmd
{
public:
    DemoRecordCmd()
        : Cmd::StaticCmd("demo_record", Cmd::SYSTEM, "Begins recording a demo from the current position")
    {}

    void Run(const Cmd::Args& args) const override
    {
        if ( args.size() > 2 )
        {
            PrintUsage(args, "[demoname]", "Begins recording a demo from the current position");
            return;
        }

        if ( clc.demorecording )
        {
            Log::Warn("Already recording.");
            return;
        }

        if ( cls.state != connstate_t::CA_ACTIVE )
        {
            Log::Warn("You must be in a level to record.");
            return;
        }

        // ATVI Wolfenstein Misc #479 - changing this to a warning
        // sync 0 doesn't prevent recording, so not forcing it off .. everyone does g_sync 1 ; record ; g_sync 0 ..
        if ( NET_IsLocalAddress(clc.serverAddress) && !Cvar_VariableValue("g_synchronousClients") )
        {
            Log::Warn("You should set '%s' for smoother demo recording" , "g_synchronousClients 1");
        }

        std::string demo_name;

        if ( args.size() == 2 )
        {
            demo_name = args[1];
        }

        CL_Record(demo_name);
    }

};
static DemoRecordCmd DemoRecordCmdRegistration;

/**
 * Returns a demo name from the current map and time
 */
std::string GenerateDemoName()
{
    qtime_t time;
    Com_RealTime(&time);

    std::string map_name = cl.mapname;
    map_name.erase(map_name.rfind('.'));
    auto last_slash = map_name.rfind('/');
    if ( last_slash != std::string::npos )
        map_name.erase(0, last_slash + 1);

    return Str::Format(
        "%s-%s_%04i-%02i-%02i_%02i%02i%02i",
        NET_AdrToString(clc.serverAddress),
        map_name,
        1900 + time.tm_year, time.tm_mon + 1, time.tm_mday,
        time.tm_hour, time.tm_min, time.tm_sec
    );
}

void CL_Record(std::string demo_name)
{
    if ( demo_name.empty() )
        demo_name = GenerateDemoName();

    std::string file_name = Str::Format("demos/%s.dm_%d", demo_name, PROTOCOL_VERSION);
    clc.demofile = FS_FOpenFileWrite(file_name.c_str());
    if ( !clc.demofile )
    {
        Log::Warn("couldn't open %s.", file_name);
        return;
    }
    Log::Notice( "recording to %s.", file_name );

    clc.demorecording = true;
    Q_strncpyz(clc.demoName, demo_name.c_str(), std::min<std::size_t>(demo_name.size(), MAX_QPATH));
    Cvar::SetValueForce(cvar_demo_status_isrecording.Name(), "1");
    Cvar::SetValueForce(cvar_demo_status_filename.Name(), demo_name);

    // don't start saving messages until a non-delta compressed message is received
    clc.demowaiting = true;

    msg_t buf;
    byte bufData[ MAX_MSGLEN ];
    // write out the gamestate message
    MSG_Init( &buf, bufData, sizeof( bufData ) );
    MSG_Bitstream( &buf );

    // NOTE, MRE: all server->client messages now acknowledge
    MSG_WriteLong( &buf, clc.reliableSequence );

    MSG_WriteByte( &buf, svc_gamestate );
    MSG_WriteLong( &buf, clc.serverCommandSequence );


    // configstrings
    for ( int i = 0; i < MAX_CONFIGSTRINGS; i++ )
    {
        if ( cl.gameState[i].empty() )
        {
            continue;
        }

        MSG_WriteByte( &buf, svc_configstring );
        MSG_WriteShort( &buf, i );
        MSG_WriteBigString( &buf, cl.gameState[i].c_str() );
    }

    // baselines
    entityState_t nullstate;
    memset( &nullstate, 0, sizeof( nullstate ) );

    for ( int i = 0; i < MAX_GENTITIES; i++ )
    {
        entityState_t *ent = &cl.entityBaselines[ i ];

        if ( !ent->number )
        {
            continue;
        }

        MSG_WriteByte( &buf, svc_baseline );
        MSG_WriteDeltaEntity( &buf, &nullstate, ent, true );
    }

    MSG_WriteByte( &buf, svc_EOF );

    // finished writing the gamestate stuff

    // write the client num
    MSG_WriteLong( &buf, clc.clientNum );
    // write the checksum feed
    MSG_WriteLong( &buf, clc.checksumFeed );

    // finished writing the client packet
    MSG_WriteByte( &buf, svc_EOF );

    // write it to the demo file
    int len = LittleLong( clc.serverMessageSequence - 1 );
    FS_Write( &len, 4, clc.demofile );

    len = LittleLong( buf.cursize );
    FS_Write( &len, 4, clc.demofile );
    FS_Write( buf.data, buf.cursize, clc.demofile );

    // the rest of the demo file will be copied from net messages
}

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

/*
=================
CL_DemoCompleted
=================
*/

NORETURN static void CL_DemoCompleted()
{
	if ( cvar_demo_timedemo.Get() )
	{
		int time;

		time = Sys_Milliseconds() - clc.timeDemoStart;

		if ( time > 0 )
		{
			Log::Notice( "%i frames, %3.1fs: %3.1f fps", clc.timeDemoFrames,
			            time / 1000.0, clc.timeDemoFrames * 1000.0 / time );
		}
	}

	throw Sys::DropErr(false, "Demo completed");
}

/*
=================
CL_ReadDemoMessage
=================
*/

void CL_ReadDemoMessage()
{
	int   r;
	msg_t buf;
	byte  bufData[ MAX_MSGLEN ];
	int   s;

	if ( !clc.demofile )
	{
		CL_DemoCompleted();
	}

	// get the sequence number
	r = FS_Read( &s, 4, clc.demofile );

	if ( r != 4 )
	{
		CL_DemoCompleted();
	}

	clc.serverMessageSequence = LittleLong( s );

	// init the message
	MSG_Init( &buf, bufData, sizeof( bufData ) );

	// get the length
	r = FS_Read( &buf.cursize, 4, clc.demofile );

	if ( r != 4 )
	{
		CL_DemoCompleted();
	}

	buf.cursize = LittleLong( buf.cursize );

	if ( buf.cursize == -1 )
	{
		CL_DemoCompleted();
	}

	if ( buf.cursize > buf.maxsize )
	{
		Sys::Drop( "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );
	}

	r = FS_Read( buf.data, buf.cursize, clc.demofile );

	if ( r != buf.cursize )
	{
		Log::Notice("Demo file was truncated.");
		CL_DemoCompleted();
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;
	CL_ParseServerMessage( &buf );
}


class DemoPlayCmd: public Cmd::StaticCmd {
    public:
        DemoPlayCmd(): Cmd::StaticCmd("demo_play", Cmd::SYSTEM, "Starts playing a demo file") {
        }

        void Run(const Cmd::Args& args) const override {
            if (args.Argc() != 2) {
                PrintUsage(args, "<demoname>", "starts playing a demo file");
                return;
            }

            // make sure a local server is killed
            Cvar_Set( "sv_killserver", "1" );
            CL_Disconnect( true );

            // open the demo file
            const std::string& fileName = args.Argv(1);

            const char* arg = fileName.c_str();
            int prot_ver = PROTOCOL_VERSION - 1;

            char extension[32];
            char name[ MAX_OSPATH ];
            while (prot_ver <= PROTOCOL_VERSION && !clc.demofile) {
                Com_sprintf(extension, sizeof(extension), ".dm_%d", prot_ver );

                if (!Q_stricmp(arg + strlen(arg) - strlen(extension), extension)) {
                    Com_sprintf(name, sizeof(name), "demos/%s", arg);

                } else {
                    Com_sprintf(name, sizeof(name), "demos/%s.dm_%d", arg, prot_ver);
                }

                FS_FOpenFileRead(name, &clc.demofile);
                prot_ver++;
            }

            if (!clc.demofile) {
                Sys::Drop("couldn't open %s", name);
            }

            Q_strncpyz(clc.demoName, arg, sizeof(clc.demoName));

            Con_Close();

            cls.state = connstate_t::CA_CONNECTED;
            clc.demoplaying = true;

            // read demo messages until connected
            while (cls.state >= connstate_t::CA_CONNECTED && cls.state < connstate_t::CA_PRIMED) {
                CL_ReadDemoMessage();
            }

            // don't get the first snapshot this frame, to prevent the long
            // time from the gamestate load from messing causing a time skip
            clc.firstDemoFrameSkipped = false;
        }

        Cmd::CompletionResult Complete(int argNum, const Cmd::Args&, Str::StringRef prefix) const override {
            if (argNum == 1) {
                return FS::HomePath::CompleteFilename(prefix, "demos", ".dm_" XSTRING(PROTOCOL_VERSION), false, true);
            }

            return {};
        }
};
static DemoPlayCmd DemoPlayCmdRegistration;

// stop demo recording and playback
static void StopDemos()
{
	// stop demo recording
	CL_StopRecord();

	// stop demo playback
	if ( clc.demofile )
	{
		FS_FCloseFile( clc.demofile );
		clc.demofile = 0;
	}
}

//======================================================================

// stop video recording
static void StopVideo()
{
	// stop recording any video
	if ( CL_VideoRecording() )
	{
		// finish rendering current frame
		//SCR_UpdateScreen();
		CL_CloseAVI();
	}
}

/*
=====================
CL_ShutdownAll
=====================
*/
void CL_ShutdownAll()
{
	// clear sounds
	Audio::StopAllSounds();
	// download subsystem
	DL_Shutdown();
	// shutdown CGame
	CL_ShutdownCGame();

	// Clear Faces
	if ( cls.consoleFont )
	{
		re.UnregisterFont( cls.consoleFont );
		cls.consoleFont = nullptr;
	}

	// shutdown the renderer
	if ( re.Shutdown )
	{
		re.Shutdown( false );  // don't destroy window or context
	}

	cls.rendererStarted = false;
	cls.soundRegistered = false;

	StopVideo();
	// Gordon: stop recording on map change etc, demos aren't valid over map changes anyway
	CL_StopRecord();

	if ( !com_sv_running->integer )
	{
		void SV_ShutdownGameProgs();
		SV_ShutdownGameProgs();

		// clear collision map data
		CM_ClearMap();
	}

	Hunk_Clear();
}

/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
void CL_MapLoading()
{
	if ( !com_cl_running->integer )
	{
		return;
	}

	Con_Close();
	cls.keyCatchers = 0;

	// if we are already connected to the local host, stay connected
	if ( cls.state >= connstate_t::CA_CONNECTED && !Q_stricmp( cls.servername, "loopback" ) )
	{
		cls.state = connstate_t::CA_CONNECTED; // so the connect screen is drawn
		memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
		cl.gameState.fill("");
		clc.lastPacketSentTime = -9999;
		SCR_UpdateScreen();
	}
	else
	{
		try {
			CL_Disconnect( false );
		} catch (Sys::DropErr& err) {
			Sys::Error( "CL_Disconnect error during map load: %s", err.what() );
		}
		Q_strncpyz( cls.servername, "loopback", sizeof( cls.servername ) );
		*cls.reconnectCmd = 0; // can't reconnect to this!
		cls.state = connstate_t::CA_CHALLENGING; // so the connect screen is drawn
		cls.keyCatchers = 0;
		SCR_UpdateScreen();
		clc.connectTime = -RETRANSMIT_TIMEOUT;
		NET_StringToAdr( cls.servername, &clc.serverAddress, netadrtype_t::NA_UNSPEC );
		// we don't need a challenge on the localhost

		CL_CheckForResend();
	}
}

/*
=====================
CL_ClearState

Called before parsing a gamestate
=====================
*/
void CL_ClearState()
{
	cl.~clientActive_t();
	new(&cl) clientActive_t{}; // Using {} instead of () to work around MSVC bug
}

/*
=====================
CL_Disconnect

Called when a connection or demo is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
void CL_SendDisconnect()
{
	// send a disconnect message to the server
	// send it a few times in case one is dropped
	if ( com_cl_running && com_cl_running->integer && cls.state >= connstate_t::CA_CONNECTED )
	{
		CL_AddReliableCommand( "disconnect" );
		CL_WritePacket();
		CL_WritePacket();
		CL_WritePacket();
	}
}

void CL_Disconnect( bool showMainMenu )
{
	if ( !com_cl_running || !com_cl_running->integer )
	{
		return;
	}

	CL_SendDisconnect();

	if ( cl_useMumble->integer && mumble_islinked() )
	{
		Log::Notice("Mumble: Unlinking from Mumble application" );
		mumble_unlink();
	}

	if ( clc.download )
	{
		FS_FCloseFile( clc.download );
		clc.download = 0;
	}

	*cls.downloadTempName = *cls.downloadName = 0;
	Cvar_Set( "cl_downloadName", "" );

	StopVideo();
	StopDemos();

	// allow cheats locally again
	if (showMainMenu) {
		Cvar::SetValueForce("sv_cheats", "1");
	}

	CL_ClearState();

	// wipe the client connection
	clc.~clientConnection_t();
	new(&clc) clientConnection_t{}; // Using {} instead of () to work around MSVC bug

	CL_ClearStaticDownload();

	FS::PakPath::ClearPaks();
	FS_LoadBasePak();

	// show_bug.cgi?id=589
	// don't try a restart if rocket is nullptr, as we might be in the middle of a restart already
	if ( cgvm.IsActive() && cls.state > connstate_t::CA_DISCONNECTED )
	{
		// restart the UI
		cls.state = connstate_t::CA_DISCONNECTED;

		// shutdown the UI
		CL_ShutdownCGame();

		// init the UI
		cgvm.Start();
		cgvm.CGameRocketInit();
	}
	else
	{
		cls.state = connstate_t::CA_DISCONNECTED;
	}

	CL_OnTeamChanged( 0 );
}

/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer( const char *string )
{
	const char *cmd = Cmd_Argv( 0 );

	// ignore key up commands
	if ( cmd[ 0 ] == '-' )
	{
		return;
	}

	if ( clc.demoplaying || cls.state < connstate_t::CA_CONNECTED || cmd[ 0 ] == '+' || cmd[ 0 ] == '\0' )
	{
		Log::Notice( "Unknown command \"%s\"\n", cmd );
		return;
	}

	if ( Cmd_Argc() > 1 )
	{
		CL_AddReliableCommand( string );
	}
	else
	{
		CL_AddReliableCommand( cmd );
	}
}

/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f()
{
	if ( cls.state != connstate_t::CA_ACTIVE || clc.demoplaying )
	{
		Log::Notice("Not connected to a server." );
		return;
	}

	// don't forward the first argument
	if ( Cmd_Argc() > 1 )
	{
		CL_AddReliableCommand( Cmd_Args() );
	}
}

/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f()
{
	throw Sys::DropErr(false, "Disconnecting.");
}

/*
================
CL_Reconnect_f

================
*/
void CL_Reconnect_f()
{
	if ( !*cls.servername )
	{
		Log::Notice("Can't reconnect to nothing." );
	}
	else if ( !*cls.reconnectCmd )
	{
		Log::Notice("Can't reconnect to localhost." );
	}
	else
	{
		Cmd::BufferCommandTextAfter(cls.reconnectCmd, true);
	}
}

/*
================
CL_Connect_f

================
*/
void CL_Connect_f()
{
	char         *server;
	char password[ 64 ];
	const char   *serverString;
	char         *offset;
	int          argc = Cmd_Argc();
	netadrtype_t family = netadrtype_t::NA_UNSPEC;

	if ( argc != 2 && argc != 3 )
	{
		Cmd_PrintUsage("[-4|-6] <server>", nullptr);
		return;
	}

	if ( argc == 2 )
	{
		server = (char *) Cmd_Argv( 1 );
	}
	else
	{
		if ( !strcmp( Cmd_Argv( 1 ), "-4" ) )
		{
			family = netadrtype_t::NA_IP;
		}
		else if ( !strcmp( Cmd_Argv( 1 ), "-6" ) )
		{
			family = netadrtype_t::NA_IP6;
		}
		else
		{
			Log::Warn("only -4 or -6 as address type understood." );
		}

		server = (char *) Cmd_Argv( 2 );
	}

	// Skip the URI scheme.
	if ( !Q_strnicmp( server, URI_SCHEME, URI_SCHEME_LENGTH ) )
	{
		server += URI_SCHEME_LENGTH;
	}

	// Set and skip the password.
	if ( ( offset = strchr( server, '@' ) ) != nullptr )
	{
		Q_strncpyz( password, server, std::min( sizeof( password ), (size_t)( offset - server + 1 ) ) );
		Cvar_Set( "password", password );
		server = offset + 1;
	}

	if ( ( offset = strchr( server, '/' ) ) != nullptr )
	{
		// trailing slash, or path supplied - chop it off since we don't use it
		*offset = 0;
	}

	//Copy the arguments before they can be overwritten, after that server is invalid
	Q_strncpyz( cls.servername, server, sizeof( cls.servername ) );
	Q_strncpyz( cls.reconnectCmd, Cmd::GetCurrentArgs().EscapedArgs(0).c_str(), sizeof( cls.reconnectCmd ) );

	Audio::StopAllSounds(); // NERVE - SMF

	// clear any previous "server full" type messages
	clc.serverMessage[ 0 ] = 0;

	if ( com_sv_running->integer && !strcmp( server, "loopback" ) )
	{
		// if running a local server, kill it
		SV_Shutdown( "Server quit" );
	}

	// make sure a local server is killed
	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	try {
		CL_Disconnect( true );
	} catch (Sys::DropErr& err) {
		Sys::Error( "CL_Disconnect error during /connect: %s", err.what() );
	}
	Con_Close();

	if ( !NET_StringToAdr( cls.servername, &clc.serverAddress, family ) )
	{
		Log::Notice("Bad server address" );
		cls.state = connstate_t::CA_DISCONNECTED;
		return;
	}

	if ( clc.serverAddress.port == 0 )
	{
		clc.serverAddress.port = BigShort( PORT_SERVER );
	}

	serverString = NET_AdrToStringwPort( clc.serverAddress );

	Log::Debug( "%s resolved to %s", cls.servername, serverString );

	// if we aren't playing on a LAN, we need to authenticate
	// with the cd key
	if ( NET_IsLocalAddress( clc.serverAddress ) )
	{
		cls.state = connstate_t::CA_CHALLENGING;
	}
	else
	{
		cls.state = connstate_t::CA_CONNECTING;
	}

	// we need to setup a correct default for this, otherwise the first val we set might reappear
	Cvar_Set( "com_errorMessage", "" );

	cls.keyCatchers = 0;
	clc.connectTime = -99999; // CL_CheckForResend() will fire immediately
	clc.connectPacketCount = 0;

	// server connection string
	Cvar_Set( "cl_currentServerAddress", server );
	Cvar_Set( "cl_currentServerIP", serverString );
}


static Cvar::Cvar<std::string> cvar_rcon_client_destination(
    "rcon.client.destination",
    "Destination address for rcon commands, if empty the current server.",
    Cvar::NONE,
    ""
);

static Cvar::Cvar<std::string> cvar_rcon_client_password(
    "rcon.client.password",
    "Password used to protect the remote console",
    Cvar::NONE,
    ""
);

static Cvar::Range<Cvar::Cvar<int>> cvar_rcon_client_secure(
    "rcon.client.secure",
    "How secure the Rcon protocol should be: "
        "0: Allow unencrypted rcon, "
        "1: Require encryption, "
        "2: Require encryption and challenge check",
    Cvar::NONE,
    0,
    0,
    2
);

/*
 * Sends the message to the remote server
 */
static void CL_RconDeliver(const Rcon::Message &message)
{
    if ( message.secure == Rcon::Secure::Unencrypted )
    {
        Net::OutOfBandPrint(netsrc_t::NS_CLIENT, message.remote, "rcon %s %s", message.password, message.command);
        return;
    }

    std::string method = "PLAIN";
    Crypto::Data key = Crypto::Hash::Sha256(Crypto::FromString(message.password));
    std::string plaintext = message.command;

    if ( message.secure == Rcon::Secure::EncryptedChallenge )
    {
        method = "CHALLENGE";
        plaintext = message.challenge + ' ' + plaintext;
    }

    Crypto::Data cypher;
    if ( Crypto::Aes256Encrypt(Crypto::FromString(plaintext), key, cypher) )
    {
        Net::OutOfBandPrint(netsrc_t::NS_CLIENT, message.remote, "srcon %s %s",
            method,
            Crypto::ToString(Crypto::Encoding::Base64Encode(cypher))
        );
    }
}

static void CL_RconSend(const Rcon::Message &message)
{
	std::string invalid_reason;
	if ( message.Valid(&invalid_reason) )
	{
		CL_RconDeliver(message);
	}
	else
	{
		Log::Notice("Invalid rcon message: %s\n", invalid_reason.c_str());
	}
}

/*
 * Queue for commands that need a challenge from the server
 */
class RconMessageQueue
{
public:

	void Push(const Rcon::Message& message)
	{
		auto lock = std::unique_lock<std::mutex>(mutex);
		requests.push_back(message);
	}

	bool Empty() const
	{
		auto lock = std::unique_lock<std::mutex>(mutex);
		return requests.empty();
	}

	/*
	 * Pops a queued command and executes it whenever there's a matching challenge.
	 * Returns whether the command has been successful
	 */
	bool Pop(const netadr_t& server, const Str::StringRef& challenge)
	{
		auto lock = std::unique_lock<std::mutex>(mutex);

		if ( requests.empty() )
		{
			return false;
		}

		auto it = std::find_if(requests.begin(), requests.end(),
			[server](const Rcon::Message& message) {
				return NET_CompareAdr(message.remote, server);
			});

		if ( it != requests.end() )
		{
			it->challenge = challenge;
			CL_RconSend(*it);
			requests.erase(it);
			return true;
		}

		return false;
	}


private:
	std::vector<Rcon::Message> requests;
	mutable std::mutex mutex;
};

static netadr_t CL_RconDestinationAddress()
{
	if ( cls.state >= connstate_t::CA_CONNECTED )
	{
		return clc.netchan.remoteAddress;
	}

	netadr_t to;

	NET_StringToAdr( cvar_rcon_client_destination.Get().c_str(), &to, netadrtype_t::NA_UNSPEC );

	if ( to.port == 0 )
	{
		to.port = BigShort( PORT_SERVER );
	}

	return to;
}

/*
  Send the rest of the command line over as
  an unconnected command.
*/
class RconCmd: public Cmd::StaticCmd
{
public:
	RconCmd():
		StaticCmd("rcon", Cmd::SYSTEM, "Sends a remote console command")
	{}

	void Run(const Cmd::Args& args) const override
	{
		if ( cvar_rcon_client_password.Get().empty() )
		{
			Log::Notice("You must set '%s' before issuing an rcon command.",
				cvar_rcon_client_password.Name());
			return;
		}

		if ( cls.state < connstate_t::CA_CONNECTED && cvar_rcon_client_destination.Get().empty() )
		{
			Log::Notice( "Connect to a server or set the '%s' cvar to issue rcon commands",
				cvar_rcon_client_destination.Name()
			);

			return;
		}

		Rcon::Message message(
			CL_RconDestinationAddress(),
			args.EscapedArgs(1),
			Rcon::Secure(cvar_rcon_client_secure.Get()),
			cvar_rcon_client_password.Get()
		);

		if ( message.secure == Rcon::Secure::EncryptedChallenge )
		{
			queue.Push(message);
			Net::OutOfBandPrint(netsrc_t::NS_CLIENT, message.remote, "getchallenge");
		}
		else
		{
			CL_RconSend(message);
		}
	}

	/*
	 * If there's a command planned for execution on the given server, it will
	 * use the challenge to do so.
	 * Returns true if a command has been executed
	 */
	bool HandleChallenge(const netadr_t& server, const Str::StringRef& challenge)
	{
		return queue.Pop(server, challenge);
	}

private:
	static RconMessageQueue queue;
};

RconMessageQueue RconCmd::queue;
static RconCmd RconCmdRegistration;


/*
  Populates rcon.client cvars from the server
*/
class RconDiscoverCmd: public Cmd::StaticCmd
{
public:
	RconDiscoverCmd():
		StaticCmd("rconDiscover", Cmd::SYSTEM, "Sends a request to the server to populate rcon.client cvars")
	{}

	void Run(const Cmd::Args&) const override
	{
		if ( cls.state < connstate_t::CA_CONNECTED && cvar_rcon_client_destination.Get().empty() )
		{
			Log::Notice( "Connect to a server or set the '%s' cvar to discover rcon settings",
				cvar_rcon_client_destination.Name()
			);
			return;
		}

		Net::OutOfBandPrint(netsrc_t::NS_CLIENT, CL_RconDestinationAddress(), "rconinfo");
	}
};

static RconDiscoverCmd RconDiscoverCmdRegistration;

static void CL_ServerRconInfoPacket( netadr_t, msg_t *msg )
{
	InfoMap info = InfoStringToMap( MSG_ReadString( msg ) );
	int value;
	if ( Str::ParseInt( value, info["secure"] ) )
	{
		cvar_rcon_client_secure.Set( value );
	}
}

static void CL_GetRSAKeysFileName( char *buffer, size_t size )
{
	Q_snprintf( buffer, size, "%s", RSAKEY_FILE );
}

static void CL_GenerateRSAKeys( const char *fileName )
{
	struct nettle_buffer key_buffer;
	fileHandle_t         f;

	mpz_set_ui( public_key.e, RSA_PUBLIC_EXPONENT );

	if ( !rsa_generate_keypair( &public_key, &private_key, nullptr, qnettle_random, nullptr, nullptr, RSA_KEY_LENGTH, 0 ) )
	{
		Sys::Error( "Error generating RSA keypair" );
	}

	nettle_buffer_init( &key_buffer );

	if ( !rsa_keypair_to_sexp( &key_buffer, nullptr, &public_key, &private_key ) )
	{
		Sys::Error( "Error converting RSA keypair to sexp" );
	}

	Log::Notice( "^5Regenerating RSA keypair; writing to %s\n" , fileName );

#ifndef _WIN32
	int old_umask = umask(0077);
#endif
	f = FS_FOpenFileWrite( fileName );
#ifndef _WIN32
	umask(old_umask);
#endif

	if ( !f )
	{
		Sys::Error( "Daemon could not open %s for writing the RSA keypair", RSAKEY_FILE );
	}

	FS_Write( key_buffer.contents, key_buffer.size, f );
	FS_FCloseFile( f );

	nettle_buffer_clear( &key_buffer );
	Log::Notice( "Daemon RSA keys generated" );
}

/*
===============
CL_LoadRSAKeys

Attempt to load the RSA keys from a file
If this fails then generate a new keypair
===============
*/
static void CL_LoadRSAKeys()
{
	int                  len;
	fileHandle_t         f;
	char                 fileName[ MAX_QPATH ];
	uint8_t              *buf;

	rsa_public_key_init( &public_key );
	rsa_private_key_init( &private_key );

	CL_GetRSAKeysFileName( fileName, sizeof( fileName ) );

	Log::Notice( "Loading RSA keys from %s" , fileName );

	len = FS_FOpenFileRead( fileName, &f );

	if ( !f || len < 1 )
	{
		Log::Notice( "^2%s", "Daemon RSA public-key file not found, regenerating\n" );
		CL_GenerateRSAKeys( fileName );
		return;
	}

	buf = (uint8_t*) Z_TagMalloc( len, memtag_t::TAG_CRYPTO );
	FS_Read( buf, len, f );
	FS_FCloseFile( f );

	if ( !rsa_keypair_from_sexp( &public_key, &private_key, 0, len, buf ) )
	{
		Log::Notice( "^1%s", "Invalid RSA keypair in file, regenerating"  );
		Z_Free( buf );
		CL_GenerateRSAKeys( fileName );
		return;
	}

	Z_Free( buf );
	Log::Notice( "Daemon RSA public-key found." );
}

static void CL_ClearRSAKeys()
{
	rsa_private_key_clear( &private_key );
	rsa_public_key_clear( &public_key );
}


/*
=================
CL_Vid_Restart_f

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/

#ifdef _WIN32
extern void IN_Restart();  // fretn

#endif

void CL_Vid_Restart_f()
{
	// settings may have changed so stop recording now
	StopVideo();

	// don't let them loop during the restart
	Audio::StopAllSounds();
	// shutdown the CGame
	CL_ShutdownCGame();
	// clear the font cache
	re.UnregisterFont( nullptr );
	cls.consoleFont = nullptr;
	// shutdown the renderer and clear the renderer interface
	CL_ShutdownRef();

	cls.rendererStarted = false;
	cls.soundRegistered = false;

	Hunk_Clear();

	// startup all the client stuff
	CL_StartHunkUsers();

#ifdef _WIN32
	IN_Restart(); // fretn
#endif

	// start the cgame if connected
	if ( cls.state > connstate_t::CA_CONNECTED )
	{
		cls.cgameStarted = true;
		CL_InitCGame();
	}
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame must also be forced to restart because
handles will be invalid
=================
*/
void CL_Snd_Restart_f()
{
	Audio::Shutdown();

	if( !cls.cgameStarted )
	{
		// In the main menu case the cgame is not restarted... but is there anything preventing
		// the main menu from also using sound handles?
		if (!Audio::Init()) {
			Sys::Error("Couldn't initialize the audio subsystem.");
		}
		//TODO S_BeginRegistration()
	}
	else
	{
		if (!Audio::Init()) {
			Sys::Error("Couldn't initialize the audio subsystem.");
		}
		CL_Vid_Restart_f();
	}
}

/*
==================
CL_Configstrings_f
==================
*/
void CL_Configstrings_f()
{
	int i;

	if ( cls.state != connstate_t::CA_ACTIVE )
	{
		Log::Notice("Not connected to a server." );
		return;
	}

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ )
	{
		if (cl.gameState[i].empty())
		{
			continue;
		}

		Log::Notice( "%4i: %s\n", i, cl.gameState[i].c_str() );
	}
}

/*
==============
CL_Clientinfo_f
==============
*/
void CL_Clientinfo_f()
{
	Log::Notice( "--------- Client Information ---------" );
	Log::Notice( "state: %s", Util::enum_str(cls.state));
	Log::Notice( "Server: %s", cls.servername );
	Log::Notice("User info settings:" );
	Info_Print( Cvar_InfoString( CVAR_USERINFO, false ) );
	Log::Notice("--------------------------------------" );
}


class DemoVideoCmd: public Cmd::StaticCmd
{
public:
    DemoVideoCmd()
        : Cmd::StaticCmd("demo_video", Cmd::SYSTEM,
                         "Begins recording a video from the current demo")
    {}

    void Run(const Cmd::Args& args) const override
    {
        if ( args.size() > 2 )
        {
            PrintUsage(args, "[filename]", "Begins recording a video from the current demo");
            return;
        }

        if ( !clc.demoplaying )
        {
            Log::Notice("The demo_video command can only be used when playing back demos");
            return;
        }

        std::string filename;
        if ( args.size() == 2 )
        {
            // explicit filename
            filename = Str::Format( "videos/%s.avi", args[1] );
        }
        else
        {
            // scan for a free filename
            int i;
            for ( i = 0; i <= 9999; i++ )
            {

                filename = Str::Format("videos/%s-%04d.avi", clc.demoName, i);

                if ( !FS::HomePath::FileExists(filename) )
                {
                    break; // file doesn't exist
                }
            }

            if ( i > 9999 )
            {
                Log::Warn("no free file names to create video" );
                return;
            }
        }

        Log::Notice("Writing demo video to %s", filename);

        CL_OpenAVIForWriting( filename.c_str() );
    }
};
static DemoVideoCmd DemoVideoCmdRegistration;


class DemoStopVideoCmd: public Cmd::StaticCmd
{
public:
    DemoStopVideoCmd()
        : Cmd::StaticCmd("demo_video_stop", Cmd::SYSTEM, "Stops recording a video")
    {}

    void Run(const Cmd::Args&) const override
    {
        CL_CloseAVI();
    }
};
static DemoStopVideoCmd DemoStopVideoCmdRegistration;


/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend()
{
	int  port;
	char info[ MAX_INFO_STRING ];
	char data[ MAX_INFO_STRING ];

	// don't send anything if playing back a demo
	if ( clc.demoplaying )
	{
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( cls.state != connstate_t::CA_CONNECTING && cls.state != connstate_t::CA_CHALLENGING )
	{
		return;
	}

	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT )
	{
		return;
	}

	clc.connectTime = cls.realtime; // for retransmit requests
	clc.connectPacketCount++;

	switch ( cls.state )
	{
		case connstate_t::CA_CONNECTING:
			Net::OutOfBandPrint( netsrc_t::NS_CLIENT, clc.serverAddress, "getchallenge" );
			break;

		case connstate_t::CA_CHALLENGING:
		{
			char key[ RSA_STRING_LENGTH ];

			mpz_get_str( key, 16, public_key.n);
			// sending back the challenge
			port = Cvar_VariableValue( "net_qport" );

			Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO, false ), sizeof( info ) );
			Info_SetValueForKey( info, "protocol", va( "%i", PROTOCOL_VERSION ), false );
			Info_SetValueForKey( info, "qport", va( "%i", port ), false );
			Info_SetValueForKey( info, "challenge", clc.challenge.c_str(), false );
			Info_SetValueForKey( info, "pubkey", key, false );

			Com_sprintf( data, sizeof(data), "connect %s", Cmd_QuoteString( info ) );

			Net::OutOfBandData( netsrc_t::NS_CLIENT, clc.serverAddress,
				reinterpret_cast<byte*>( data ), strlen( data ) );
			// the most current userinfo has been sent, so watch for any
			// newer changes to userinfo variables
			cvar_modifiedFlags &= ~CVAR_USERINFO;
			break;
		}
		default:
			Sys::Error( "CL_CheckForResend: bad cls.state" );
	}
}

/*
===================
CL_DisconnectPacket

Sometimes the server can drop the client and the netchan based
disconnect can be lost.  If the client continues to send packets
to the server, the server will send out of band disconnect packets
to the client so it doesn't have to wait for the full timeout period.
===================
*/
static void CL_DisconnectPacket( const netadr_t& from )
{
	if ( cls.state < connstate_t::CA_CONNECTING )
	{
		return;
	}

	// if not from our server, ignore it
	if ( !NET_CompareAdr( from, clc.netchan.remoteAddress ) )
	{
		return;
	}

	// if we have received packets within three seconds, ignore (it might be a malicious spoof)
	// NOTE TTimo:
	// there used to be a  clc.lastPacketTime = cls.realtime; line in CL_PacketEvent before calling CL_ConnectionlessPacket
	// therefore .. packets never got through this check, clients never disconnected
	// switched the clc.lastPacketTime = cls.realtime to happen after the connectionless packets have been processed
	// you still can't spoof disconnects, cause legal netchan packets will maintain realtime - lastPacketTime below the threshold
	if ( cls.realtime - clc.lastPacketTime < 3000 )
	{
		return;
	}

	// drop the connection
	const char* message = "Server disconnected for unknown reason";
	Cvar_Set( "com_errorMessage", message );
	Sys::Drop( message );
}

/*
===================
CL_PrintPacket
an OOB message from server, with potential markups
print OOB are the only messages we handle markups in
[err_dialog]: used to indicate that the connection should be aborted
  no further information, just do an error diagnostic screen afterwards
[err_prot]: HACK. This is a protocol error. The client uses a custom
  protocol error message (client sided) in the diagnostic window.
  The space for the error message on the connection screen is limited
  to 256 chars.
===================
*/
void CL_PrintPacket( msg_t *msg )
{
	char *s;

	s = MSG_ReadBigString( msg );

	if ( !Q_strnicmp( s, "[err_dialog]", 12 ) )
	{
		Q_strncpyz( clc.serverMessage, s + 12, sizeof( clc.serverMessage ) );
		// Cvar_Set("com_errorMessage", clc.serverMessage );
		Sys::Drop( "%s", clc.serverMessage );
	}
	else
	{
		Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
	}

	Log::Notice("%s\n", clc.serverMessage );
}

/*
===================
CL_InitServerInfo
===================
*/
void CL_InitServerInfo( serverInfo_t *server, netadr_t *address )
{
	server->adr = *address;
	server->clients = 0;
	server->hostName[ 0 ] = '\0';
	server->mapName[ 0 ] = '\0';
	server->label[ 0 ] = '\0';
	server->maxClients = 0;
	server->maxPing = 0;
	server->minPing = 0;
	server->ping = -1;
	server->game[ 0 ] = '\0';
	server->netType = netadrtype_t::NA_BOT;
}

/*
===================
CL_GSRSequenceInformation

Parses this packet's index and the number of packets from a master server's
response. Updates the packet count and returns the index. Advances the data
pointer as appropriate (but only when parsing was successful)

The sequencing information isn't terribly useful at present (we can skip
duplicate packets, but we don't bother to make sure we've got all of them).
===================
*/
int CL_GSRSequenceInformation( byte **data )
{
	char *p = ( char * ) *data, *e;
	int  ind, num;

	// '\0'-delimited fields: this packet's index, total number of packets
	if ( *p++ != '\0' )
	{
		return -1;
	}

	ind = strtol( p, &e, 10 );

	if ( *e++ != '\0' )
	{
		return -1;
	}

	num = strtol( e, &p, 10 );

	if ( *p++ != '\0' )
	{
		return -1;
	}

	if ( num <= 0 || ind <= 0 || ind > num )
	{
		return -1; // nonsensical response
	}

	if ( cls.numMasterPackets > 0 && num != cls.numMasterPackets )
	{
		// Assume we sent two getservers and somehow they changed in
		// between - only use the results that arrive later
		Log::Debug( "Master changed its mind about packet count!" );
		cls.receivedMasterPackets = 0;
		cls.numglobalservers = 0;
		cls.numGlobalServerAddresses = 0;
	}

	cls.numMasterPackets = num;

	// successfully parsed
	*data = ( byte * ) p;
	return ind;
}

/*
===================
CL_GSRFeaturedLabel

Parses from the data an arbitrary text string labelling the servers in the
following getserversresponse packet.
The result is copied to *buf, and *data is advanced as appropriate
===================
*/
void CL_GSRFeaturedLabel( byte **data, char *buf, int size )
{
	char *l = buf;
	buf[ 0 ] = '\0';

	// copy until '\0' which indicates field break
	// or slash which indicates beginning of server list
	while ( **data && **data != '\\' && **data != '/' )
	{
		if ( l < &buf[ size - 1 ] )
		{
			*l = **data;
		}
		else if ( l == &buf[ size - 1 ] )
		{
			Log::Warn( "CL_GSRFeaturedLabel: overflow" );
		}

		l++, ( *data ) ++;
	}
}

static const int MAX_SERVERSPERPACKET = 256;

/*
===================
CL_ServerLinksResponsePacket
===================
*/
void CL_ServerLinksResponsePacket( msg_t *msg )
{
	int      port;
	byte      *buffptr;
	byte      *buffend;

	Log::Debug( "CL_ServerLinksResponsePacket" );

	if ( msg->data[ 30 ] != 0 )
	{
		return;
	}

	// parse through server response string
	cls.numserverLinks = 0;
	buffptr = msg->data + 31; // skip header
	buffend = msg->data + msg->cursize;

	// Each link contains:
	// * an IPv4 address & port
	// * an IPv6 address & port
	while ( buffptr < buffend - 20 && cls.numserverLinks < ARRAY_LEN( cls.serverLinks ) )
	{
		cls.serverLinks[ cls.numserverLinks ].type = netadrtype_t::NA_IP_DUAL;

		// IPv4 address
		memcpy( cls.serverLinks[ cls.numserverLinks ].ip, buffptr, 4 );
		port = buffptr[ 4 ] << 8 | buffptr[ 5 ];
		cls.serverLinks[ cls.numserverLinks ].port4 = BigShort( port );
		buffptr += 6;

		// IPv4 address
		memcpy( cls.serverLinks[ cls.numserverLinks ].ip6, buffptr, 16 );
		port = buffptr[ 16 ] << 8 | buffptr[ 17 ];
		cls.serverLinks[ cls.numserverLinks ].port6 = BigShort( port );
		buffptr += 18;

		++cls.numserverLinks;
	}

	Log::Debug( "%d server address pairs parsed", cls.numserverLinks );
}

/*
===================
CL_ServersResponsePacket
===================
*/
void CL_ServersResponsePacket( const netadr_t *from, msg_t *msg, bool extended )
{
	int      i, count, duplicate_count, parsed_count, total;
	netadr_t addresses[ MAX_SERVERSPERPACKET ];
	int      numservers;
	byte     *buffptr;
	byte     *buffend;
	char     label[ MAX_FEATLABEL_CHARS ] = "";

	Log::Debug( "CL_ServersResponsePacket" );

	duplicate_count = 0;

	if ( cls.numglobalservers == -1 )
	{
		// state to detect lack of servers or lack of response
		cls.numglobalservers = 0;
		cls.numGlobalServerAddresses = 0;
		cls.numMasterPackets = 0;
		cls.receivedMasterPackets = 0;
	}

	// parse through server response string
	numservers = 0;
	buffptr = msg->data;
	buffend = buffptr + msg->cursize;

	// advance to initial token
	// skip header
	buffptr += 4;

	// advance to initial token
	// I considered using strchr for this but I don't feel like relying
	// on its behaviour with '\0'
	while ( *buffptr && *buffptr != '\\' && *buffptr != '/' )
	{
		buffptr++;

		if ( buffptr + 1 >= buffend )
		{
			break;
		}
	}

	if ( *buffptr == '\0' )
	{
		int ind = CL_GSRSequenceInformation( &buffptr );

		if ( ind >= 0 )
		{

			// TODO: detect dropped packets and make another
			// request
			Log::Debug( "CL_ServersResponsePacket: packet "
			             "%d of %d", ind, cls.numMasterPackets );
			cls.receivedMasterPackets |= ( 1 << ( ind - 1 ) );

			CL_GSRFeaturedLabel( &buffptr, label, sizeof( label ) );
		}

		// now skip to the server list
		for ( ; buffptr < buffend && *buffptr != '\\' && *buffptr != '/';
		      buffptr++ ) {; }
	}

	while ( buffptr + 1 < buffend )
	{
		bool duplicate = false;
		byte ip6[16];
		byte ip[4];
		short port;

		// IPv4 address
		if ( *buffptr == '\\' )
		{
			buffptr++;

			if ( buffend - buffptr < (ptrdiff_t) (sizeof( addresses[ numservers ].ip ) + sizeof( addresses[ numservers ].port ) + 1) )
			{
				break;
			}

			// parse out ip
			memcpy( ip, buffptr, sizeof( ip ) );
			buffptr += sizeof( ip );

			// parse out port
			port = ( *buffptr++ ) << 8;
			port += *buffptr++;
			port = BigShort( port );;

			// deduplicate server list, do not add known server
			for ( int i = 0; i < cls.numglobalservers; i++ )
			{
				if ( cls.globalServers[ i ].adr.port == port && !memcmp( cls.globalServers[ i ].adr.ip, ip, sizeof( ip ) ) )
				{
					duplicate = true;
					duplicate_count++;
					break;
				}
			}

			memcpy( addresses[ numservers ].ip, ip, sizeof( ip ) );

			addresses[ numservers ].port = port;
			addresses[ numservers ].type = netadrtype_t::NA_IP;

			// look up this address in the links list
			for (unsigned j = 0; j < cls.numserverLinks && !duplicate; ++j )
			{
				if ( addresses[ numservers ].port == cls.serverLinks[ j ].port4 && !memcmp( addresses[ numservers ].ip, cls.serverLinks[ j ].ip, sizeof( addresses[ numservers ].ip ) ) )
				{
					// found it, so look up the corresponding address
					char s[ NET_ADDR_W_PORT_STR_MAX_LEN ];

					// hax to get the IP address & port as a string (memcmp etc. SHOULD work, but...)
					cls.serverLinks[ j ].type = netadrtype_t::NA_IP6;
					cls.serverLinks[ j ].port = cls.serverLinks[ j ].port6;
					strcpy( s, NET_AdrToStringwPort( cls.serverLinks[ j ] ) );
					cls.serverLinks[j].type = netadrtype_t::NA_IP_DUAL;

					for ( i = 0; i < numservers; ++i )
					{
						if ( !strcmp( s, NET_AdrToStringwPort( addresses[ i ] ) ) )
						{
							// found: replace with the preferred address, exit both loops
							addresses[ i ] = cls.serverLinks[ j ];
							addresses[ i ].type = NET_TYPE( cls.serverLinks[ j ].type );
							addresses[ i ].port = ( addresses[ i ].type == netadrtype_t::NA_IP ) ? cls.serverLinks[ j ].port4 : cls.serverLinks[ j ].port6;
							duplicate = true;
							duplicate_count++;
							break;
						}
					}
				}
			}
		}
		// IPv6 address, if it's an extended response
		else if ( extended && *buffptr == '/' )
		{
			buffptr++;

			if ( buffend - buffptr < (ptrdiff_t) (sizeof( addresses[ numservers ].ip6 ) + sizeof( addresses[ numservers ].port ) + 1) )
			{
				break;
			}

			for ( unsigned i = 0; i < sizeof( addresses[ numservers ].ip6 ); i++ )
			{
				addresses[ numservers ].ip6[ i ] = *buffptr++;
			}

			// parse out ip
			memcpy( ip6, buffptr, sizeof( ip6 ) );
			buffptr += sizeof( ip6 );

			// parse out port
			port = ( *buffptr++ ) << 8;
			port += *buffptr++;
			port = BigShort( port );;

			// deduplicate server list, do not add known server
			for ( int i = 0; i < cls.numglobalservers; i++ )
			{
				if ( cls.globalServers[ i ].adr.port == port && !memcmp( cls.globalServers[ i ].adr.ip6, ip6, sizeof( ip6 ) ) )
				{
					duplicate = true;
					duplicate_count++;
					break;
				}
			}

			memcpy( addresses[ numservers ].ip6, ip6, sizeof( ip6 ) );

			addresses[ numservers ].port = port;
			addresses[ numservers ].type = netadrtype_t::NA_IP6;
			addresses[ numservers ].scope_id = from->scope_id;

			// look up this address in the links list
			for ( unsigned j = 0; j < cls.numserverLinks && !duplicate; ++j )
			{
				if ( addresses[ numservers ].port == cls.serverLinks[ j ].port6 && !memcmp( addresses[ numservers ].ip6, cls.serverLinks[ j ].ip6, sizeof( addresses[ numservers ].ip6 ) ) )
				{
					// found it, so look up the corresponding address
					char s[ NET_ADDR_W_PORT_STR_MAX_LEN ];

					// hax to get the IP address & port as a string (memcmp etc. SHOULD work, but...)
					cls.serverLinks[ j ].type = netadrtype_t::NA_IP;
					cls.serverLinks[ j ].port = cls.serverLinks[ j ].port4;
					strcpy( s, NET_AdrToStringwPort( cls.serverLinks[ j ] ) );
					cls.serverLinks[j].type = netadrtype_t::NA_IP_DUAL;

					for ( i = 0; i < numservers; ++i )
					{
						if ( !strcmp( s, NET_AdrToStringwPort( addresses[ i ] ) ) )
						{
							// found: replace with the preferred address, exit both loops
							addresses[ i ] = cls.serverLinks[ j ];
							addresses[ i ].type = NET_TYPE( cls.serverLinks[ j ].type );
							addresses[ i ].port = ( addresses[ i ].type == netadrtype_t::NA_IP ) ? cls.serverLinks[ j ].port4 : cls.serverLinks[ j ].port6;
							duplicate = true;
							duplicate_count++;
							break;
						}
					}
				}
			}
		}
		else
		{
			// syntax error!
			break;
		}

		// syntax check
		if ( *buffptr != '\\' && *buffptr != '/' )
		{
			break;
		}

		if ( !duplicate )
		{
			++numservers;

			if ( numservers >= MAX_SERVERSPERPACKET )
			{
				break;
			}
		}
	}

	count = cls.numglobalservers;

	for ( i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++ )
	{
		// build net address
		serverInfo_t *server = &cls.globalServers[ count ];

		CL_InitServerInfo( server, &addresses[ i ] );
		Q_strncpyz( server->label, label, sizeof( server->label ) );
		// advance to next slot
		count++;
	}

	// if getting the global list
	if ( count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS )
	{
		// if we couldn't store the servers in the main list anymore
		for ( ; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++ )
		{
			// just store the addresses in an additional list
			cls.globalServerAddresses[ cls.numGlobalServerAddresses++ ] = addresses[ i ];
		}
	}

	cls.numglobalservers = count;
	total = count + cls.numGlobalServerAddresses;
	parsed_count = numservers + duplicate_count;

	Log::Debug( "%d servers parsed, %s new, %d duplicate (total %d)", parsed_count, numservers, duplicate_count, total );
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
static void CL_ConnectionlessPacket( const netadr_t& from, msg_t *msg )
{
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );  // skip the -1

	Cmd::Args args(MSG_ReadStringLine( msg ));

	if ( args.Argc() < 1 )
	{
		return;
	}

	Log::Debug( "CL packet %s: %s", NET_AdrToStringwPort( from ), args.Argv(0).c_str() );

	// challenge from the server we are connecting to

	if ( args.Argv(0) == "challengeResponse" )
	{
		if ( cls.state == connstate_t::CA_CONNECTING )
		{
			if ( args.Argc() < 2 )
			{
				return;
			}
			// start sending challenge response instead of challenge request packets
			clc.challenge = args.Argv(1);
			cls.state = connstate_t::CA_CHALLENGING;
			clc.connectPacketCount = 0;
			clc.connectTime = -99999;

			// take this address as the new server address.  This allows
			// a server proxy to hand off connections to multiple servers
			clc.serverAddress = from;
			Log::Debug( "challenge: %s", clc.challenge.c_str() );
		}
		else if ( !RconCmdRegistration.HandleChallenge(from, args.Argv(1)) )
		{
			Log::Notice( "Unwanted challenge response received.  Ignored.\n" );
		}

		return;
	}

	// server connection
	if ( args.Argv(0) == "connectResponse" )
	{
		if ( cls.state >= connstate_t::CA_CONNECTED )
		{
			Log::Notice( "Dup connect received. Ignored.\n" );
			return;
		}

		if ( cls.state != connstate_t::CA_CHALLENGING )
		{
			Log::Notice( "connectResponse packet while not connecting. Ignored.\n" );
			return;
		}

		if ( !NET_CompareAdr( from, clc.serverAddress ) )
		{
			Log::Notice( "connectResponse from a different address. Ignored.\n" );
			Log::Notice( "%s should have been %s\n", NET_AdrToString( from ),
			            NET_AdrToStringwPort( clc.serverAddress ) );
			return;
		}

		Netchan_Setup( netsrc_t::NS_CLIENT, &clc.netchan, from, Cvar_VariableValue( "net_qport" ) );
		cls.state = connstate_t::CA_CONNECTED;
		clc.lastPacketSentTime = -9999; // send first packet immediately
		return;
	}

	// server responding to an info broadcast
	if ( args.Argv(0) == "infoResponse" )
	{
		CL_ServerInfoPacket( from, msg );
		return;
	}

	// server responding to a get playerlist
	if ( args.Argv(0) == "statusResponse" )
	{
		CL_ServerStatusResponse( from, msg );
		return;
	}

	// a disconnect message from the server, which will happen if the server
	// dropped the connection but it is still getting packets from us
	if ( args.Argv(0) == "disconnect" )
	{
		CL_DisconnectPacket( from );
		return;
	}

	// echo request from server
	if ( args.Argv(0) == "echo" && args.Argc() >= 2)
	{
		Net::OutOfBandPrint( netsrc_t::NS_CLIENT, from, "%s", args.Argv(1) );
		return;
	}

	// echo request from server
	if ( args.Argv(0) == "print" )
	{
		CL_PrintPacket( msg );
		return;
	}

	// echo request from server
	if ( args.Argv(0) == "getserversResponse" )
	{
		CL_ServersResponsePacket( &from, msg, false );
		return;
	}

	// list of servers with both IPv4 and IPv6 addresses; sent back by a master server (extended)
	if ( args.Argv(0) == "getserversExtResponseLinks" )
	{
		CL_ServerLinksResponsePacket( msg );
		return;
	}

	// list of servers sent back by a master server (extended)
	if ( args.Argv(0) == "getserversExtResponse" )
	{
		CL_ServersResponsePacket( &from, msg, true );
		return;
	}

	// prints a n error message returned by the server
	if ( args.Argv(0) == "error" )
	{
		Log::Warn( MSG_ReadStringLine(msg) );
		return;
	}

	// prints a n error message returned by the server
	if ( args.Argv(0) == "rconInfoResponse" )
	{
		CL_ServerRconInfoPacket( from, msg );
		return;
	}

	Log::Debug( "Unknown connectionless packet command." );
}

/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent( const netadr_t& from, msg_t *msg )
{
	int headerBytes;

	if ( msg->cursize >= 4 && * ( int * ) msg->data == -1 )
	{
		CL_ConnectionlessPacket( from, msg );
		return;
	}

	clc.lastPacketTime = cls.realtime;

	if ( cls.state < connstate_t::CA_CONNECTED )
	{
		return; // can't be a valid sequenced packet
	}

	if ( msg->cursize < 4 )
	{
		Log::Notice( "%s: Runt packet\n", NET_AdrToStringwPort( from ) );
		return;
	}

	//
	// packet from server
	//
	if ( !NET_CompareAdr( from, clc.netchan.remoteAddress ) )
	{
		Log::Debug( "%s: sequenced packet without connection", NET_AdrToStringwPort( from ) );
		// FIXME: send a client disconnect?
		return;
	}

	if ( !Netchan_Process( &clc.netchan, msg ) )
	{
		return; // out of order, duplicated, etc
	}

	// the header is different lengths for reliable and unreliable messages
	headerBytes = msg->readcount;

	// track the last message received so it can be returned in
	// client messages, allowing the server to detect a dropped
	// gamestate
	clc.serverMessageSequence = LittleLong( * ( int * ) msg->data );

	clc.lastPacketTime = cls.realtime;
	CL_ParseServerMessage( msg );

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//

	if ( clc.demorecording && !clc.demowaiting )
	{
		CL_WriteDemoMessage( msg, headerBytes );
	}
}

/*
==================
CL_CheckTimeout

==================
*/
void CL_CheckTimeout()
{
	//
	// check timeout
	//
	if ( cls.state >= connstate_t::CA_CONNECTED && cls.realtime - clc.lastPacketTime > cl_timeout->value * 1000 )
	{
		if ( ++cl.timeoutcount > 5 )
		{
			// timeoutcount saves debugger
			const char* message = "Server connection timed out.";
			Cvar_Set( "com_errorMessage", message );
			Sys::Drop( message );
		}
	}
	else
	{
		cl.timeoutcount = 0;
	}
}

//============================================================================

/*
==================
CL_CheckUserinfo

==================
*/
void CL_CheckUserinfo()
{
	// don't add reliable commands when not yet connected
	if ( cls.state < connstate_t::CA_CHALLENGING )
	{
		return;
	}

	// send a reliable userinfo update if needed
	if ( cvar_modifiedFlags & CVAR_USERINFO )
	{
		cvar_modifiedFlags &= ~CVAR_USERINFO;
		CL_AddReliableCommand( va( "userinfo %s", Cmd_QuoteString( Cvar_InfoString( CVAR_USERINFO, false ) ) ) );
	}
}

/*
==================
CL_Frame

==================
*/
void CL_Frame( int msec )
{
	if ( !com_cl_running->integer )
	{
		return;
	}

	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && cl_aviFrameRate->integer && msec )
	{
		// save the current screen
		if ( cls.state == connstate_t::CA_ACTIVE )
		{
			CL_TakeVideoFrame();

			// fixed time for next frame
			msec = ( int ) ceil( ( 1000.0f / cl_aviFrameRate->value ) * com_timescale->value );

			if ( msec == 0 )
			{
				msec = 1;
			}
		}

		// XreaL END
	}

	// save the msec before checking pause
	cls.realFrametime = msec;

	// decide the simulation time
	cls.frametime = msec;

	cls.realtime += cls.frametime;

	// see if we need to update any userinfo
	CL_CheckUserinfo();

	// if we haven't gotten a packet in a long time,
	// drop the connection
	CL_CheckTimeout();

	// wwwdl download may survive a server disconnect
	if ( cls.state == connstate_t::CA_DOWNLOADING && clc.bWWWDl )
	{
		CL_WWWDownload();
	}

	// send intentions now
	CL_SendCmd();

	// resend a connection request if necessary
	CL_CheckForResend();

	// decide on the serverTime to render
	CL_SetCGameTime();

	// update the screen
	SCR_UpdateScreen();

	// update the sound
	Audio::Update();

	CL_UpdateMumble();

	Con_RunConsole();

	cls.framecount++;
}

/*
================
CL_SetRecommended_f
================
*/
void CL_SetRecommended_f()
{
	Com_SetRecommended();
}

static bool CL_InitRef();
/*
============
CL_InitRenderer
============
*/
bool CL_InitRenderer()
{
	if ( !CL_InitRef() )
	{
		return false;
	}

	fileHandle_t f;

	// this sets up the renderer and calls R_Init
	if ( !re.BeginRegistration( &cls.glconfig, &cls.glconfig2 ) )
	{
		return false;
	}

	cl_consoleFont = Cvar_Get( "cl_consoleFont", "fonts/unifont.ttf",  CVAR_LATCH );
	cl_consoleFontSize = Cvar_Get( "cl_consoleFontSize", "16",  CVAR_LATCH );
	cl_consoleFontScaling = Cvar_Get( "cl_consoleFontScaling", "1", CVAR_LATCH );

	// load character sets
	cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars", RSF_DEFAULT );
	cls.useLegacyConsoleFont = cls.useLegacyConsoleFace = true;

	// Register console font specified by cl_consoleFont, if any
	// filehandle is unused but forces FS_FOpenFileRead() to heed purecheck because it does not when filehandle is nullptr
	if ( cl_consoleFont->string[0] )
	{
		if ( FS_FOpenFileRead( cl_consoleFont->string, &f ) >= 0 )
		{
			if ( cl_consoleFontScaling->value == 0 )
			{
				cls.consoleFont = re.RegisterFont( cl_consoleFont->string, nullptr, cl_consoleFontSize->integer );
			}
			else
			{
				// This gets 12px on 1920×1080 screen, which is libRocket default for 1em
				int fontScale = std::min(cls.glconfig.vidWidth, cls.glconfig.vidHeight) / 90;

				// fontScale / 12px gets 1px on 1920×1080 screen
				cls.consoleFont = re.RegisterFont( cl_consoleFont->string, nullptr, cl_consoleFontSize->integer * fontScale / 12 );
			}

			if ( cls.consoleFont != nullptr )
				cls.useLegacyConsoleFont = false;
		}
		else
		{
			Log::Warn("Font file '%s' not found", cl_consoleFont->string);
		}

		FS_FCloseFile( f );
	}

	cls.whiteShader = re.RegisterShader( "white", RSF_NOMIP );
	cls.consoleShader = re.RegisterShader( "console", RSF_DEFAULT );

	g_console_field_width = cls.glconfig.vidWidth / SMALLCHAR_WIDTH - 2;
	g_consoleField.SetWidth(g_console_field_width);

	return true;
}

/*
============================
CL_StartHunkUsers

Starts any of {renderer, audio, cgame vm} that is not already started.
Only the renderer is really a hunk user.
============================
*/
void CL_StartHunkUsers()
{
	if ( !com_cl_running )
	{
		return;
	}

	if ( !com_cl_running->integer )
	{
		return;
	}

	Cvar::SetLatchedValues();

	if ( !cls.rendererStarted && CL_InitRenderer() )
	{
		cls.rendererStarted = true;
	}

	if ( !cls.rendererStarted )
	{
		CL_ShutdownRef();
		Sys::Error( "Couldn't load a renderer." );
	}

	if ( !Audio::Init() ) {
		Sys::Error("Couldn't initialize the audio subsystem.");
	}

	if ( !cls.soundRegistered )
	{
		cls.soundRegistered = true;
		//TODO
		//S_BeginRegistration();
	}

	if ( !cgvm.IsActive() )
	{
		cgvm.Start();
		cgvm.CGameRocketInit();
	}
}

/*
============
CL_RefMalloc
============
*/
MALLOC_LIKE void           *CL_RefMalloc( int size )
{
	return Z_TagMalloc( size, memtag_t::TAG_RENDERER );
}

/*
============
CL_RefTagFree
============
*/
void CL_RefTagFree()
{
}

int CL_ScaledMilliseconds()
{
	return Sys_Milliseconds() * com_timescale->value;
}

extern refexport_t *GetRefAPI( int apiVersion, refimport_t *rimp );

/*
============
CL_InitRef
============
*/
static bool CL_InitRef()
{
	refimport_t ri;
	refexport_t *ret;

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_QuoteString = Cmd_QuoteString;

	ri.Milliseconds = Sys::Milliseconds;
	ri.RealTime = Com_RealTime;

	ri.Z_Malloc = CL_RefMalloc;
	ri.Free = Z_Free;
	ri.Tag_Free = CL_RefTagFree;
	ri.Hunk_Alloc = Hunk_Alloc;
	ri.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	ri.Hunk_FreeTempMemory = Hunk_FreeTempMemory;

	ri.CM_PointContents = CM_PointContents;
	ri.CM_DrawDebugSurface = CM_DrawDebugSurface;

	ri.FS_ReadFile = FS_ReadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_WriteFile = FS_WriteFile;
	ri.FS_FreeFileList = FS_FreeFileList;
	ri.FS_ListFiles = FS_ListFiles;
	ri.FS_FileExists = FS_FileExists;
	ri.FS_Seek = FS_Seek;
	ri.FS_FTell = FS_FTell;
	ri.FS_Read = FS_Read;
	ri.FS_FCloseFile = FS_FCloseFile;
	ri.FS_FOpenFileRead = FS_FOpenFileRead;

	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;

	ri.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

	// XreaL BEGIN
	ri.CL_VideoRecording = CL_VideoRecording;
	ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	// XreaL END

	ri.IN_Init = IN_Init;
	ri.IN_Shutdown = IN_Shutdown;
	ri.IN_Restart = IN_Restart;

	Log::Notice("Calling GetRefAPI…" );
	ret = GetRefAPI( REF_API_VERSION, &ri );

	if ( !ret )
	{
		Log::Notice( "Couldn't initialize refresh module\n" );
		return false;
	}

	re = *ret;

	return true;
}

/*
============
CL_ShutdownRef
============
*/
void CL_ShutdownRef()
{
	if ( !re.Shutdown )
	{
		return;
	}

	re.Shutdown( true );
	memset( &re, 0, sizeof( re ) );
}

//===========================================================================================

/*
====================
CL_Init
====================
*/
void CL_Init()
{
	PrintBanner( "Client Initialization" )

	Con_Init();

	CL_ClearState();

	cls.state = connstate_t::CA_DISCONNECTED; // no longer CA_UNINITIALIZED

	cls.realtime = 0;

	CL_InitInput();

	CL_LoadRSAKeys();

	//
	// register our variables
	//
	cl_noprint = Cvar_Get( "cl_noprint", "0", 0 );

	cl_timeout = Cvar_Get( "cl_timeout", "200", 0 );

	cl_timeNudge = Cvar_Get( "cl_timeNudge", "0", CVAR_TEMP );
	cl_shownet = Cvar_Get( "cl_shownet", "0", CVAR_TEMP );
	cl_shownuments = Cvar_Get( "cl_shownuments", "0", CVAR_TEMP );
	cl_showServerCommands = Cvar_Get( "cl_showServerCommands", "0", 0 );
	cl_showSend = Cvar_Get( "cl_showSend", "0", CVAR_TEMP );
	cl_showTimeDelta = Cvar_Get( "cl_showTimeDelta", "0", CVAR_TEMP );
	cl_activeAction = Cvar_Get( "activeAction", "", CVAR_TEMP );
	cl_autorecord = Cvar_Get( "cl_autorecord", "0", CVAR_TEMP );

	cl_aviFrameRate = Cvar_Get( "cl_aviFrameRate", "25", 0 );

	// XreaL BEGIN
	cl_aviMotionJpeg = Cvar_Get( "cl_aviMotionJpeg", "1", 0 );
	// XreaL END

	cl_yawspeed = Cvar_Get( "cl_yawspeed", "140", 0 );
	cl_pitchspeed = Cvar_Get( "cl_pitchspeed", "140", 0 );
	cl_anglespeedkey = Cvar_Get( "cl_anglespeedkey", "1.5", 0 );

	cl_maxpackets = Cvar_Get( "cl_maxpackets", "125", 0 );
	cl_packetdup = Cvar_Get( "cl_packetdup", "1", 0 );

	cl_run = Cvar_Get( "cl_run", "1", 0 );
	cl_sensitivity = Cvar_Get( "sensitivity", "5", CVAR_ARCHIVE );
	cl_mouseAccel = Cvar_Get( "cl_mouseAccel", "0", 0 );
	cl_freelook = Cvar_Get( "cl_freelook", "1", CVAR_ARCHIVE );

	cl_gameControllerAvailable = Cvar_Get( "in_gameControllerAvailable", "0", CVAR_ROM );

	// 0: legacy mouse acceleration
	// 1: new implementation

	cl_mouseAccelStyle = Cvar_Get( "cl_mouseAccelStyle", "0", 0 );
	// offset for the power function (for style 1, ignored otherwise)
	// this should be set to the max rate value
	cl_mouseAccelOffset = Cvar_Get( "cl_mouseAccelOffset", "5", 0 );

	cl_showMouseRate = Cvar_Get( "cl_showmouserate", "0", 0 );

	cl_allowDownload = Cvar_Get( "cl_allowDownload", "1", 0 );

	cl_inGameVideo = Cvar_Get( "r_inGameVideo", "1", 0 );

	cl_serverStatusResendTime = Cvar_Get( "cl_serverStatusResendTime", "750", 0 );

	cl_doubletapdelay = Cvar_Get( "cl_doubletapdelay", "250", 0 );  // Arnout: double tap
	m_pitch = Cvar_Get( "m_pitch", "0.022", CVAR_ARCHIVE );
	m_yaw = Cvar_Get( "m_yaw", "0.022", 0 );
	m_forward = Cvar_Get( "m_forward", "0.25", 0 );
	m_side = Cvar_Get( "m_side", "0.25", 0 );
	m_filter = Cvar_Get( "m_filter", "0", CVAR_ARCHIVE );

	j_pitch = Cvar_Get( "j_pitch", "0.022", 0 );
	j_yaw = Cvar_Get( "j_yaw", "-0.022", 0 );
	j_forward = Cvar_Get( "j_forward", "-0.25", 0 );
	j_side = Cvar_Get( "j_side", "0.25", 0 );
	j_up = Cvar_Get ("j_up", "1", 0);

	j_pitch_axis = Cvar_Get( "j_pitch_axis", "3", 0 );
	j_yaw_axis = Cvar_Get( "j_yaw_axis", "4", 0 );
	j_forward_axis = Cvar_Get( "j_forward_axis", "1", 0 );
	j_side_axis = Cvar_Get( "j_side_axis", "0", 0 );
	j_up_axis = Cvar_Get( "j_up_axis", "2", 0 );

	cl_consoleFontKerning = Cvar_Get( "cl_consoleFontKerning", "0", 0 );

	cl_consoleCommand = Cvar_Get( "cl_consoleCommand", "say", 0 );

	cl_logs = Cvar_Get ("cl_logs", "0", 0);

	p_team = Cvar_Get("p_team", "0", CVAR_ROM );

	cl_gamename = Cvar_Get( "cl_gamename", GAMENAME_FOR_MASTER, CVAR_TEMP );
	cl_altTab = Cvar_Get( "cl_altTab", "1", 0 );

	//bani
	cl_packetloss = Cvar_Get( "cl_packetloss", "0", CVAR_CHEAT );
	cl_packetdelay = Cvar_Get( "cl_packetdelay", "0", CVAR_CHEAT );

	Cvar_Get( "cl_maxPing", "800", 0 );
	// userinfo
	Cvar_Get( "name", UNNAMED_PLAYER, CVAR_USERINFO | CVAR_ARCHIVE );
	cl_rate = Cvar_Get( "rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get( "snaps", "40", CVAR_USERINFO  );

	Cvar_Get( "password", "", CVAR_USERINFO );

	cl_useMumble = Cvar_Get( "cl_useMumble", "0",  CVAR_LATCH );
	cl_mumbleScale = Cvar_Get( "cl_mumbleScale", "0.0254", 0 );

	// cgame might not be initialized before menu is used
	Cvar_Get( "cg_viewsize", "100", 0 );

	cl_cgameSyscallStats = Cvar_Get( "cl_cgameSyscallStats", "0", 0 );

	//
	// register our commands
	//
	Cmd_AddCommand( "cmd", CL_ForwardToServer_f );
	Cmd_AddCommand( "configstrings", CL_Configstrings_f );
	Cmd_AddCommand( "clientinfo", CL_Clientinfo_f );
	Cmd_AddCommand( "snd_restart", CL_Snd_Restart_f );
	Cmd_AddCommand( "vid_restart", CL_Vid_Restart_f );
	Cmd_AddCommand( "disconnect", CL_Disconnect_f );
	Cmd_AddCommand( "connect", CL_Connect_f );
	Cmd_AddCommand( "reconnect", CL_Reconnect_f );
	Cmd_AddCommand( "localservers", CL_LocalServers_f );
	Cmd_AddCommand( "globalservers", CL_GlobalServers_f );

	Cmd_AddCommand( "ping", CL_Ping_f );
	Cmd_AddCommand( "serverstatus", CL_ServerStatus_f );
	Cmd_AddCommand( "showip", CL_ShowIP_f );

	Cmd_AddCommand( "updatescreen", SCR_UpdateScreen );
	// done.

	Cmd_AddCommand( "setRecommended", CL_SetRecommended_f );

	SCR_Init();

	Cmd::ExecuteCommandBuffer();

	Cvar_Set( "cl_running", "1" );

	PrintBanner( "Client Initialization Complete" )
}

/*
===============
CL_Shutdown

===============
*/
void CL_Shutdown()
{
	static bool recursive = false;

	// check whether the client is running at all.
	if ( !( com_cl_running && com_cl_running->integer ) )
	{
		return;
	}

	Log::Debug( "----- CL_Shutdown -----" );
	if ( recursive )
	{
		printf( "recursive shutdown\n" );
		return;
	}

	recursive = true;

	CL_Disconnect( true );

	CL_ShutdownCGame();

	Audio::Shutdown();
	DL_Shutdown();

	if ( re.UnregisterFont )
	{
		re.UnregisterFont( nullptr );
		cls.consoleFont = nullptr;
	}

	CL_ShutdownRef();

	Cmd_RemoveCommand( "cmd" );
	Cmd_RemoveCommand( "configstrings" );
	Cmd_RemoveCommand( "userinfo" );
	Cmd_RemoveCommand( "snd_restart" );
	Cmd_RemoveCommand( "vid_restart" );
	Cmd_RemoveCommand( "disconnect" );
	Cmd_RemoveCommand( "connect" );
	Cmd_RemoveCommand( "localservers" );
	Cmd_RemoveCommand( "globalservers" );
	Cmd_RemoveCommand( "rcon" );
	Cmd_RemoveCommand( "ping" );
	Cmd_RemoveCommand( "serverstatus" );
	Cmd_RemoveCommand( "showip" );
	Cmd_RemoveCommand( "model" );

	CL_ClearKeyBinding();
	CL_ClearInput();

	CL_ClearRSAKeys();

	// done.

	Cvar_Set( "cl_running", "0" );

	recursive = false;

	memset( &cls, 0, sizeof( cls ) );

	Log::Debug( "-----------------------" );

}

static void CL_SetServerInfo( serverInfo_t *server, const char *info, int ping )
{
	if ( server )
	{
		if ( info )
		{
			server->clients = atoi( Info_ValueForKey( info, "clients" ) );
			server->bots = atoi( Info_ValueForKey( info, "bots" ) );
			Q_strncpyz( server->hostName, Info_ValueForKey( info, "hostname" ), MAX_NAME_LENGTH );
			server->load = atoi( Info_ValueForKey( info, "serverload" ) );
			Q_strncpyz( server->mapName, Info_ValueForKey( info, "mapname" ), MAX_NAME_LENGTH );
			server->maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
			Q_strncpyz( server->game, Info_ValueForKey( info, "game" ), MAX_NAME_LENGTH );
			server->netType = Util::enum_cast<netadrtype_t>(atoi(Info_ValueForKey(info, "nettype")));
			server->minPing = atoi( Info_ValueForKey( info, "minping" ) );
			server->maxPing = atoi( Info_ValueForKey( info, "maxping" ) );
			server->friendlyFire = atoi( Info_ValueForKey( info, "g_friendlyFire" ) );   // NERVE - SMF
			server->needpass = atoi( Info_ValueForKey( info, "g_needpass" ) );   // NERVE - SMF
			Q_strncpyz( server->gameName, Info_ValueForKey( info, "gamename" ), MAX_NAME_LENGTH );   // Arnout
		}

		server->ping = ping;
	}
}

static void CL_SetServerInfoByAddress( const netadr_t& from, const char *info, int ping )
{
	int i;

	for ( i = 0; i < MAX_OTHER_SERVERS; i++ )
	{
		if ( NET_CompareAdr( from, cls.localServers[ i ].adr ) )
		{
			CL_SetServerInfo( &cls.localServers[ i ], info, ping );
		}
	}

	for ( i = 0; i < MAX_GLOBAL_SERVERS; i++ )
	{
		if ( NET_CompareAdr( from, cls.globalServers[ i ].adr ) )
		{
			CL_SetServerInfo( &cls.globalServers[ i ], info, ping );
		}
	}

	for ( i = 0; i < MAX_OTHER_SERVERS; i++ )
	{
		if ( NET_CompareAdr( from, cls.favoriteServers[ i ].adr ) )
		{
			CL_SetServerInfo( &cls.favoriteServers[ i ], info, ping );
		}
	}
}

/*
===================
CL_ServerInfoPacket
===================
*/
void CL_ServerInfoPacket( const netadr_t& from, msg_t *msg )
{
	int  i, type;
	char info[ MAX_INFO_STRING ];
//	char*   str;
	char *infoString;
	int  prot;
	const char *gameName;

	infoString = MSG_ReadString( msg );

	// if this isn't the correct protocol version, ignore it
	prot = atoi( Info_ValueForKey( infoString, "protocol" ) );

	if ( prot != PROTOCOL_VERSION )
	{
		Log::Debug( "Different protocol info packet: %s", infoString );
		return;
	}

	// Arnout: if this isn't the correct game, ignore it
	gameName = Info_ValueForKey( infoString, "gamename" );

	if ( !gameName[ 0 ] || Q_stricmp( gameName, GAMENAME_STRING ) )
	{
		Log::Debug( "Different game info packet: %s", infoString );
		return;
	}

	// iterate servers waiting for ping response
	for ( i = 0; i < MAX_PINGREQUESTS; i++ )
	{
		if ( cl_pinglist[ i ].adr.port && !cl_pinglist[ i ].time && NET_CompareAdr( from, cl_pinglist[ i ].adr ) )
		{
			// calc ping time
			cl_pinglist[ i ].time = Sys_Milliseconds() - cl_pinglist[ i ].start;

			Log::Debug( "ping time %dms from %s", cl_pinglist[ i ].time, NET_AdrToString( from ) );

			// save of info
			Q_strncpyz( cl_pinglist[ i ].info, infoString, sizeof( cl_pinglist[ i ].info ) );

			// tack on the net type
			// NOTE: make sure these types are in sync with the netnames strings in the UI
			switch ( from.type )
			{
				case netadrtype_t::NA_BROADCAST:
				case netadrtype_t::NA_IP:
					//str = "udp";
					type = 1;
					break;

				case netadrtype_t::NA_IP6:
					type = 2;
					break;

				default:
					//str = "???";
					type = 0;
					break;
			}

			Info_SetValueForKey( cl_pinglist[ i ].info, "nettype", va( "%d", type ), false );
			CL_SetServerInfoByAddress( from, infoString, cl_pinglist[ i ].time );

			return;
		}
	}

	// if not just sent a local broadcast or pinging local servers
	if ( cls.pingUpdateSource != AS_LOCAL )
	{
		return;
	}

	for ( i = 0; i < MAX_OTHER_SERVERS; i++ )
	{
		// empty slot
		if ( cls.localServers[ i ].adr.port == 0 )
		{
			break;
		}

		// avoid duplicate
		if ( NET_CompareAdr( from, cls.localServers[ i ].adr ) )
		{
			return;
		}
	}

	if ( i == MAX_OTHER_SERVERS )
	{
		Log::Debug("MAX_OTHER_SERVERS hit, dropping infoResponse" );
		return;
	}

	// add this to the list
	cls.numlocalservers = i + 1;
	cls.localServers[ i ].adr = from;
	cls.localServers[ i ].clients = 0;
	cls.localServers[ i ].hostName[ 0 ] = '\0';
	cls.localServers[ i ].load = -1;
	cls.localServers[ i ].mapName[ 0 ] = '\0';
	cls.localServers[ i ].maxClients = 0;
	cls.localServers[ i ].maxPing = 0;
	cls.localServers[ i ].minPing = 0;
	cls.localServers[ i ].ping = -1;
	cls.localServers[ i ].game[ 0 ] = '\0';
	cls.localServers[ i ].netType = from.type;
	cls.localServers[ i ].friendlyFire = 0; // NERVE - SMF
	cls.localServers[ i ].needpass = 0;
	cls.localServers[ i ].gameName[ 0 ] = '\0'; // Arnout

	Q_strncpyz( info, MSG_ReadString( msg ), MAX_INFO_STRING );

	if ( info[ 0 ] )
	{
		if ( info[ strlen( info ) - 1 ] == '\n' )
			Log::Notice( "%s: %s", NET_AdrToStringwPort( from ), info );
		else
			Log::Notice( "%s: %s\n", NET_AdrToStringwPort( from ), info );
	}
}

/*
===================
CL_GetServerStatus
===================
*/
static serverStatus_t *CL_GetServerStatus( const netadr_t& from )
{
//	serverStatus_t *serverStatus;
	int i, oldest, oldestTime;

//	serverStatus = nullptr;
	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( NET_CompareAdr( from, cl_serverStatusList[ i ].address ) )
		{
			return &cl_serverStatusList[ i ];
		}
	}

	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( cl_serverStatusList[ i ].retrieved )
		{
			return &cl_serverStatusList[ i ];
		}
	}

	oldest = -1;
	oldestTime = 0;

	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( oldest == -1 || cl_serverStatusList[ i ].startTime < oldestTime )
		{
			oldest = i;
			oldestTime = cl_serverStatusList[ i ].startTime;
		}
	}

	if ( oldest != -1 )
	{
		return &cl_serverStatusList[ oldest ];
	}

	serverStatusCount++;
	return &cl_serverStatusList[ serverStatusCount & ( MAX_SERVERSTATUSREQUESTS - 1 ) ];
}

/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen )
{
	int            i;
	netadr_t       to;
	serverStatus_t *serverStatus;

	// if no server address then reset all server status requests
	if ( !serverAddress )
	{
		for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
		{
			cl_serverStatusList[ i ].address.port = 0;
			cl_serverStatusList[ i ].retrieved = true;
		}

		return false;
	}

	// get the address
	if ( !NET_StringToAdr( serverAddress, &to, netadrtype_t::NA_UNSPEC ) )
	{
		return false;
	}

	serverStatus = CL_GetServerStatus( to );

	// if no server status string then reset the server status request for this address
	if ( !serverStatusString )
	{
		serverStatus->retrieved = true;
		return false;
	}

	// if this server status request has the same address
	if ( NET_CompareAdr( to, serverStatus->address ) )
	{
		// if we received a response for this server status request
		if ( !serverStatus->pending )
		{
			Q_strncpyz( serverStatusString, serverStatus->string, maxLen );
			serverStatus->retrieved = true;
			serverStatus->startTime = 0;
			return true;
		}
		// resend the request regularly
		else if ( serverStatus->startTime < Sys_Milliseconds() - cl_serverStatusResendTime->integer )
		{
			serverStatus->print = false;
			serverStatus->pending = true;
			serverStatus->retrieved = false;
			serverStatus->time = 0;
			serverStatus->startTime = Sys_Milliseconds();
			Net::OutOfBandPrint( netsrc_t::NS_CLIENT, to, "getstatus" );
			return false;
		}
	}
	// if retrieved
	else if ( serverStatus->retrieved )
	{
		serverStatus->address = to;
		serverStatus->print = false;
		serverStatus->pending = true;
		serverStatus->retrieved = false;
		serverStatus->startTime = Sys_Milliseconds();
		serverStatus->time = 0;
		Net::OutOfBandPrint( netsrc_t::NS_CLIENT, to, "getstatus" );
		return false;
	}

	return false;
}

/*
===================
CL_ServerStatusResponse
===================
*/
void CL_ServerStatusResponse( const netadr_t& from, msg_t *msg )
{
	const char           *s;
	int            i, score, ping;
	int            len;
	serverStatus_t *serverStatus;

	serverStatus = nullptr;

	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( NET_CompareAdr( from, cl_serverStatusList[ i ].address ) )
		{
			serverStatus = &cl_serverStatusList[ i ];
			break;
		}
	}

	// if we didn't request this server status
	if ( !serverStatus )
	{
		return;
	}

	s = MSG_ReadStringLine( msg );

	len = 0;
	Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "%s", s );

	if ( serverStatus->print )
	{
		Log::CommandInteractionMessage("Server settings:" );
		// print cvars
		for (const auto& kv: InfoStringToMap(s)) {
			Log::CommandInteractionMessage(Str::Format("%-24s%s", kv.first, kv.second));
		}
	}

	len = strlen( serverStatus->string );
	Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "\\" );

	if ( serverStatus->print )
	{
		Log::CommandInteractionMessage( "\nPlayers:" );
		Log::CommandInteractionMessage( "num: score: ping: name:" );
	}

	for ( i = 0, s = MSG_ReadStringLine( msg ); *s; s = MSG_ReadStringLine( msg ), i++ )
	{
		len = strlen( serverStatus->string );
		Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "\\%s", s );

		if ( serverStatus->print )
		{
			score = ping = 0;
			sscanf( s, "%d %d", &score, &ping );
			s = strchr( s, ' ' );

			if ( s )
			{
				s = strchr( s + 1, ' ' );
			}

			if ( s )
			{
				s++;
			}
			else
			{
				s = "unknown";
			}

			Log::CommandInteractionMessage(Str::Format("%-2d   %-3d    %-3d   %s", i, score, ping, s));
		}
	}

	len = strlen( serverStatus->string );
	Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "\\" );

	serverStatus->time = Sys_Milliseconds();
	serverStatus->address = from;
	serverStatus->pending = false;

	if ( serverStatus->print )
	{
		serverStatus->retrieved = true;
	}
}

/*
==================
CL_LocalServers_f
==================
*/
void CL_LocalServers_f()
{
	const char *message;
	int      i, j;
	netadr_t to;

	Log::Debug( "Scanning for servers on the local network…" );

	// reset the list, waiting for response
	cls.numlocalservers = 0;
	cls.pingUpdateSource = AS_LOCAL;

	for ( i = 0; i < MAX_OTHER_SERVERS; i++ )
	{
		bool b = cls.localServers[ i ].visible;
		Com_Memset( &cls.localServers[ i ], 0, sizeof( cls.localServers[ i ] ) );
		cls.localServers[ i ].visible = b;
	}

	Com_Memset( &to, 0, sizeof( to ) );

	// The 'xxx' in the message is a challenge that will be echoed back
	// by the server.  We don't care about that here, but master servers
	// can use that to prevent spoofed server responses from invalid IP addresses
	message = "\377\377\377\377getinfo xxx";
	int messageLen = strlen(message);

	// send each message twice in case one is dropped
	for ( i = 0; i < 2; i++ )
	{
		// send a broadcast packet on each server port
		// we support multiple server ports so a single machine
		// can nicely run multiple servers
		for ( j = 0; j < NUM_SERVER_PORTS; j++ )
		{
			to.port = BigShort( ( short )( PORT_SERVER + j ) );

			to.type = netadrtype_t::NA_BROADCAST;
			NET_SendPacket( netsrc_t::NS_CLIENT, messageLen, message, to );

			to.type = netadrtype_t::NA_MULTICAST6;
			NET_SendPacket( netsrc_t::NS_CLIENT, messageLen, message, to );
		}
	}
}

/*
==================
CL_GlobalServers_f
==================
*/
void CL_GlobalServers_f()
{
	netadr_t to;
	int      count, i, masterNum, protocol;
	char     command[ 1024 ], *masteraddress;
	bool     wildcard = false;
	bool     active_master_servers[MAX_MASTER_SERVERS] = { false, false, false, false, false };

	count = Cmd_Argc();
	protocol = atoi( Cmd_Argv( 2 ) );   // Do this right away, otherwise weird things happen when you use the ingame "Get New Servers" button.

	if ( ! strcmp( Cmd_Argv( 1 ), "*" ) )
	{
		wildcard = true;
		for ( masterNum = 0; masterNum < MAX_MASTER_SERVERS; masterNum++ )
		{
			active_master_servers[ masterNum ] = true;
		}
	}
	else {
		masterNum = atoi( Cmd_Argv( 1 ) );
		active_master_servers[ masterNum ] = true;
	}

	if ( count < 2 || ( ( masterNum < 0 || masterNum > MAX_MASTER_SERVERS - 1 ) && !wildcard ) )
	{
		Cmd_PrintUsage("(<master# 0-" XSTRING(MAX_MASTER_SERVERS - 1) "> | *) [<protocol>] [<keywords>]", nullptr);
		return;
	}

	for ( masterNum = 0; masterNum < MAX_MASTER_SERVERS; masterNum++ )
	{
		if ( !active_master_servers[ masterNum ] ) {
			continue;
		}

		sprintf( command, "sv_master%d", masterNum + 1 );
		masteraddress = Cvar_VariableString( command );

		if ( !*masteraddress )
		{
			if ( !wildcard )
			{
				Log::Warn( "CL_GlobalServers_f: No master server address given.\n" );
			}
			continue;
		}

		Log::Debug( "CL_GlobalServers_f: Resolving %s", masteraddress );

		// reset the list, waiting for response
		// -1 is used to distinguish a "no response"

		i = NET_StringToAdr( masteraddress, &to, netadrtype_t::NA_UNSPEC );

		if ( !i )
		{
			Log::Warn( "CL_GlobalServers_f: Could not resolve address of master %s\n", masteraddress );
			continue;
		}
		else if ( i == 2 )
		{
			to.port = BigShort( PORT_MASTER );
		}

		Log::Debug( "CL_GlobalServers_f: %s resolved to %s", masteraddress,
		             NET_AdrToStringwPort( to ) );

		Log::Debug( "CL_GlobalServers_f: Requesting servers from master %s…", masteraddress );

		cls.numglobalservers = -1;
		cls.numserverLinks = 0;
		cls.pingUpdateSource = AS_GLOBAL;

		Com_sprintf( command, sizeof( command ), "getserversExt %s %d dual",
		             cl_gamename->string, protocol );

		// TODO: test if we only have IPv4/IPv6, if so request only the relevant
		// servers with getserversExt %s %d ipvX
		// not that big a deal since the extra servers won't respond to getinfo
		// anyway.

		for ( i = 3; i < count; i++ )
		{
			Q_strcat( command, sizeof( command ), " " );
			Q_strcat( command, sizeof( command ), Cmd_Argv( i ) );
		}

		Net::OutOfBandPrint( netsrc_t::NS_SERVER, to, "%s", command );
	}
}

/*
==================
CL_GetPing
==================
*/
void CL_GetPing( int n, char *buf, int buflen, int *pingtime )
{
	const char *str;
	int        time;
	int        maxPing;

	if ( n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[ n ].adr.port )
	{
		// invalid or empty slot
		buf[ 0 ] = '\0';
		*pingtime = 0;
		return;
	}

	str = NET_AdrToStringwPort( cl_pinglist[ n ].adr );
	Q_strncpyz( buf, str, buflen );

	time = cl_pinglist[ n ].time;

	if ( !time )
	{
		// check for timeout
		time = Sys_Milliseconds() - cl_pinglist[ n ].start;
		maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );

		if ( maxPing < 100 )
		{
			maxPing = 100;
		}

		if ( time < maxPing )
		{
			// not timed out yet
			time = 0;
		}
	}

	CL_SetServerInfoByAddress( cl_pinglist[ n ].adr, cl_pinglist[ n ].info, cl_pinglist[ n ].time );

	*pingtime = time;
}

/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing( int n )
{
	if ( n < 0 || n >= MAX_PINGREQUESTS )
	{
		return;
	}

	cl_pinglist[ n ].adr.port = 0;
}

/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount()
{
	int    i;
	int    count;
	ping_t *pingptr;

	count = 0;
	pingptr = cl_pinglist;

	for ( i = 0; i < MAX_PINGREQUESTS; i++, pingptr++ )
	{
		if ( pingptr->adr.port )
		{
			count++;
		}
	}

	return ( count );
}

/*
==================
CL_GetFreePing
==================
*/
ping_t         *CL_GetFreePing()
{
	ping_t *pingptr;
	ping_t *best;
	int    oldest;
	int    i;
	int    time;

	pingptr = cl_pinglist;

	for ( i = 0; i < MAX_PINGREQUESTS; i++, pingptr++ )
	{
		// find free ping slot
		if ( pingptr->adr.port )
		{
			if ( !pingptr->time )
			{
				if ( Sys_Milliseconds() - pingptr->start < 500 )
				{
					// still waiting for response
					continue;
				}
			}
			else if ( pingptr->time < 500 )
			{
				// results have not been queried
				continue;
			}
		}

		// clear it
		pingptr->adr.port = 0;
		return ( pingptr );
	}

	// use oldest entry
	pingptr = cl_pinglist;
	best = cl_pinglist;
	oldest = INT_MIN;

	for ( i = 0; i < MAX_PINGREQUESTS; i++, pingptr++ )
	{
		// scan for oldest
		time = Sys_Milliseconds() - pingptr->start;

		if ( time > oldest )
		{
			oldest = time;
			best = pingptr;
		}
	}

	return ( best );
}

/*
==================
CL_Ping_f
==================
*/
void CL_Ping_f()
{
	netadr_t     to;
	ping_t        *pingptr;
	const char   *server;
	int          argc;
	netadrtype_t family = netadrtype_t::NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 )
	{
		Cmd_PrintUsage("[-4|-6] <server>", nullptr);
		return;
	}

	if ( argc == 2 )
	{
		server = Cmd_Argv( 1 );
	}
	else
	{
		if ( !strcmp( Cmd_Argv( 1 ), "-4" ) )
		{
			family = netadrtype_t::NA_IP;
		}
		else if ( !strcmp( Cmd_Argv( 1 ), "-6" ) )
		{
			family = netadrtype_t::NA_IP6;
		}
		else
		{
			Log::Warn("only -4 or -6 as address type understood." );
		}

		server = Cmd_Argv( 2 );
	}

	Com_Memset( &to, 0, sizeof( netadr_t ) );

	if ( !NET_StringToAdr( server, &to, family ) )
	{
		return;
	}

	pingptr = CL_GetFreePing();

	memcpy( &pingptr->adr, &to, sizeof( netadr_t ) );
	pingptr->start = Sys_Milliseconds();
	pingptr->time = 0;

	CL_SetServerInfoByAddress( pingptr->adr, nullptr, 0 );

	Net::OutOfBandPrint( netsrc_t::NS_CLIENT, to, "getinfo xxx" );
}

/*
==================
CL_UpdateVisiblePings_f
==================
*/
bool CL_UpdateVisiblePings_f( int source )
{
	int      slots, i;
	char     buff[ MAX_STRING_CHARS ];
	int      pingTime;
	int      max;
	bool status = false;

	if ( source < 0 || source > AS_FAVORITES )
	{
		return false;
	}

	cls.pingUpdateSource = source;

	slots = CL_GetPingQueueCount();

	if ( slots < MAX_PINGREQUESTS )
	{
		serverInfo_t *server = nullptr;

		max = ( source == AS_GLOBAL ) ? MAX_GLOBAL_SERVERS : MAX_OTHER_SERVERS;

		switch ( source )
		{
			case AS_LOCAL:
				server = &cls.localServers[ 0 ];
				max = cls.numlocalservers;
				break;

			case AS_GLOBAL:
				server = &cls.globalServers[ 0 ];
				max = cls.numglobalservers;
				break;

			case AS_FAVORITES:
				server = &cls.favoriteServers[ 0 ];
				max = cls.numfavoriteservers;
				break;
		}

		for ( i = 0; i < max; i++ )
		{
			if ( server[ i ].visible )
			{
				if ( server[ i ].ping == -1 )
				{
					int j;

					if ( slots >= MAX_PINGREQUESTS )
					{
						break;
					}

					for ( j = 0; j < MAX_PINGREQUESTS; j++ )
					{
						if ( !cl_pinglist[ j ].adr.port )
						{
							continue;
						}

						if ( NET_CompareAdr( cl_pinglist[ j ].adr, server[ i ].adr ) )
						{
							// already on the list
							break;
						}
					}

					// Not in the list, so find and use a free slot.
					// If all slots are full, the server won't be pinged.
					if ( j >= MAX_PINGREQUESTS )
					{
						status = true;

						for ( j = 0; j < MAX_PINGREQUESTS; j++ )
						{
							if ( !cl_pinglist[ j ].adr.port )
							{
								memcpy( &cl_pinglist[ j ].adr, &server[ i ].adr, sizeof( netadr_t ) );
								cl_pinglist[ j ].start = Sys_Milliseconds();
								cl_pinglist[ j ].time = 0;
								Net::OutOfBandPrint( netsrc_t::NS_CLIENT, cl_pinglist[ j ].adr, "getinfo xxx" );
								slots++;
								break;
							}
						}
					}
				}
				// if the server has a ping higher than cl_maxPing or
				// the ping packet got lost
				else if ( server[ i ].ping == 0 )
				{
					// if we are updating global servers
					if ( source == AS_GLOBAL )
					{
						//
						if ( cls.numGlobalServerAddresses > 0 )
						{
							// overwrite this server with one from the additional global servers
							cls.numGlobalServerAddresses--;
							CL_InitServerInfo( &server[ i ], &cls.globalServerAddresses[ cls.numGlobalServerAddresses ] );
							// NOTE: the server[i].visible flag stays untouched
						}
					}
				}
			}
		}
	}

	if ( slots )
	{
		status = true;
	}

	for ( i = 0; i < MAX_PINGREQUESTS; i++ )
	{
		if ( !cl_pinglist[ i ].adr.port )
		{
			continue;
		}

		CL_GetPing( i, buff, MAX_STRING_CHARS, &pingTime );

		if ( pingTime != 0 )
		{
			CL_ClearPing( i );
			status = true;
		}
	}

	return status;
}

/*
==================
CL_ServerStatus_f
==================
*/
void CL_ServerStatus_f()
{
	netadr_t       to, *toptr = nullptr;
	const char     *server;
	serverStatus_t *serverStatus;
	int            argc;
	netadrtype_t   family = netadrtype_t::NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 )
	{
		if ( cls.state != connstate_t::CA_ACTIVE || clc.demoplaying )
		{
			Log::Notice( "Not connected to a server.\n" );
			Cmd_PrintUsage("[-4|-6] <server>", nullptr);
			return;
		}

		toptr = &clc.serverAddress;
	}

	if ( !toptr )
	{
		Com_Memset( &to, 0, sizeof( netadr_t ) );

		if ( argc == 2 )
		{
			server = Cmd_Argv( 1 );
		}
		else
		{
			if ( !strcmp( Cmd_Argv( 1 ), "-4" ) )
			{
				family = netadrtype_t::NA_IP;
			}
			else if ( !strcmp( Cmd_Argv( 1 ), "-6" ) )
			{
				family = netadrtype_t::NA_IP6;
			}
			else
			{
				Log::Warn( "only -4 or -6 as address type understood." );
			}

			server = Cmd_Argv( 2 );
		}

		toptr = &to;

		if ( !NET_StringToAdr( server, toptr, family ) )
		{
			return;
		}
	}

	Net::OutOfBandPrint( netsrc_t::NS_CLIENT, *toptr, "getstatus" );

	serverStatus = CL_GetServerStatus( *toptr );
	serverStatus->address = *toptr;
	serverStatus->print = true;
	serverStatus->pending = true;
}

/*
==================
CL_ShowIP_f
==================
*/
void CL_ShowIP_f()
{
	Sys_ShowIP();
}

/*
====================
CL_GetClipboardData
====================
*/
#ifdef BUILD_GRAPHICAL_CLIENT
void CL_GetClipboardData( char *buf, int buflen )
{
	int         i, j;
	char       *cbd = SDL_GetClipboardText();
	const char *clean;

	if ( !cbd )
	{
		*buf = 0;
		return;
	}

	clean = Com_ClearForeignCharacters( cbd ); // yes, I know
	SDL_free( cbd );

	i = j = 0;
	while ( clean[ i ] )
	{
		if ( (unsigned char)clean[ i ] < ' ' || clean[ i ] == 0x7F )
		{
			if ( j + 1 >= buflen )
			{
				break;
			}

			i++;
			buf[ j++ ] = ' ';
		}
		else
		{
			int w = Q_UTF8_Width( clean + i );

			if ( j + w >= buflen )
			{
				break;
			}

			switch ( w )
			{
			case 4: buf[ j++ ] = clean[ i++ ]; DAEMON_FALLTHROUGH;
			case 3: buf[ j++ ] = clean[ i++ ]; DAEMON_FALLTHROUGH;
			case 2: buf[ j++ ] = clean[ i++ ]; DAEMON_FALLTHROUGH;
			case 1: buf[ j++ ] = clean[ i++ ];
			}
		}
	}

	buf[ j ] = '\0';
}
#endif
