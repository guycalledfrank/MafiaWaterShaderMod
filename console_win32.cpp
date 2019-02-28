//#include "platform.h"

//#ifdef IPLATFORM_WIN32

#include <windows.h>

#include <stdio.h>

#include <fcntl.h>

#include <io.h>

#include <iostream>

#include <fstream>

#ifndef _USE_OLD_IOSTREAMS

using namespace std;

#endif

// maximum mumber of lines the output console should have

static const WORD MAX_CONSOLE_LINES = 2500;

//#ifdef _DEBUG
	MSG msg;
	char curdir[2048];
	signed int dirsize;

void RedirectIOToConsole()

{

	int hConHandle;

	long lStdHandle;

	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	FILE *fp;

	// allocate a console for this app

	AllocConsole();
	int consoleTitleSize = GetConsoleTitle(NULL,0);
	char* consoleTitle = new char[consoleTitleSize+1];
	GetConsoleTitle(consoleTitle,consoleTitleSize);
	consoleTitle[consoleTitleSize] = 0;
	HWND consoleHWND = FindWindow(NULL,consoleTitle);
	delete[] consoleTitle;

	// set the screen buffer to be big enough to let us scroll text

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), 

		&coninfo);

	coninfo.dwSize.Y = MAX_CONSOLE_LINES;

	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), 

		coninfo.dwSize);

	// redirect unbuffered STDOUT to the console

	lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE consoleStdOut = (HANDLE)lStdHandle;

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen( hConHandle, "w" );

	*stdout = *fp;

	setvbuf( stdout, NULL, _IONBF, 0 );

	// redirect unbuffered STDIN to the console

	lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen( hConHandle, "r" );

	*stdin = *fp;

	setvbuf( stdin, NULL, _IONBF, 0 );

	// redirect unbuffered STDERR to the console

	lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen( hConHandle, "w" );

	*stderr = *fp;

	setvbuf( stderr, NULL, _IONBF, 0 );


	// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog 

	// point to console as well

	ios::sync_with_stdio();

}

//#endif

//End of File

//#endif