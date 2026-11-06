#include "global.h"
#include "DsfUpdate.h"
#include "RageLog.h"
#include "RageUtil.h"

#if defined(WIN32)
#include <windows.h>

namespace
{
	volatile LONG g_iState = 0;	/* 0 not checked, 1 checking, 2 done */
	char g_szVersion[64];		/* the newer version, or "" */
	bool g_bOffered = false;

	CString UpdaterPath()
	{
		char sz[MAX_PATH];
		GetModuleFileNameA( NULL, sz, sizeof sz );
		char *p = strrchr( sz, '\\' );
		if( p ) *p = 0;
		return CString( sz ) + "\\Updater.exe";
	}

	DWORD WINAPI CheckThread( void * )
	{
		g_szVersion[0] = 0;
		const CString sExe = UpdaterPath();
		SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
		HANDLE hRead = NULL, hWrite = NULL;
		if( GetFileAttributesA(sExe.c_str()) != INVALID_FILE_ATTRIBUTES && CreatePipe(&hRead, &hWrite, &sa, 0) )
		{
			SetHandleInformation( hRead, HANDLE_FLAG_INHERIT, 0 );
			STARTUPINFOA si;
			ZERO( si );
			si.cb = sizeof si;
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = hWrite;
			si.hStdError = hWrite;
			si.hStdInput = GetStdHandle( STD_INPUT_HANDLE );
			PROCESS_INFORMATION pi;
			CString sCmd = "\"" + sExe + "\" --check";
			char szCmd[MAX_PATH * 2];
			strncpy( szCmd, sCmd.c_str(), sizeof szCmd - 1 );
			szCmd[sizeof szCmd - 1] = 0;
			if( CreateProcessA(NULL, szCmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi) )
			{
				CloseHandle( hWrite ); hWrite = NULL;
				char buf[512];
				DWORD n, got = 0;
				while( got < sizeof buf - 1 && ReadFile(hRead, buf + got, sizeof buf - 1 - got, &n, NULL) && n )
					got += n;
				buf[got] = 0;
				WaitForSingleObject( pi.hProcess, 30000 );
				CloseHandle( pi.hThread );
				CloseHandle( pi.hProcess );
				CString sOut = buf;
				TrimRight( sOut );
				LOG->Info( "DsfUpdate: %s", sOut.c_str() );
				if( sOut.compare(0, 7, "UPDATE ") == 0 )
				{
					strncpy( g_szVersion, sOut.c_str() + 7, sizeof g_szVersion - 1 );
					g_szVersion[sizeof g_szVersion - 1] = 0;
				}
			}
		}
		if( hWrite ) CloseHandle( hWrite );
		if( hRead ) CloseHandle( hRead );
		InterlockedExchange( &g_iState, 2 );
		return 0;
	}
}

void DsfUpdate::StartCheck()
{
	if( InterlockedCompareExchange(&g_iState, 1, 0) != 0 )
		return;
	HANDLE h = CreateThread( NULL, 0, CheckThread, NULL, 0, NULL );
	if( h ) CloseHandle( h );
	else InterlockedExchange( &g_iState, 2 );
}

bool DsfUpdate::TakeOffer( CString &sVersion )
{
	if( g_bOffered || g_iState != 2 || g_szVersion[0] == 0 )
		return false;
	g_bOffered = true;
	sVersion = g_szVersion;
	return true;
}

bool DsfUpdate::Launch()
{
	const CString sExe = UpdaterPath();
	char szCmd[MAX_PATH * 2];
	snprintf( szCmd, sizeof szCmd, "\"%s\" --yes --restart", sExe.c_str() );
	STARTUPINFOA si;
	ZERO( si );
	si.cb = sizeof si;
	PROCESS_INFORMATION pi;
	if( !CreateProcessA(NULL, szCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) )
		return false;
	CloseHandle( pi.hThread );
	CloseHandle( pi.hProcess );
	return true;
}

#else
void DsfUpdate::StartCheck() {}
bool DsfUpdate::TakeOffer( CString & ) { return false; }
bool DsfUpdate::Launch() { return false; }
#endif
