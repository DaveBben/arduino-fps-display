// fps_server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include "Include/stdafx.h"
#include "fps_server.h"
#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "Include/RTSSSharedMemory.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <fstream>
#include <set>
#include <streambuf>
#include <string>

typedef websocketpp::connection_hdl connection_hdl;
typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::server<websocketpp::config::asio> server;
typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

server m_endpoint;
con_list m_connections;
server::timer_ptr m_timer;

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

uint16_t getFps() {
	DWORD dwClients = 0;
	/// <summary>
	/// Grabs the FPS of the currently open program that is using any graphics
	//  Check out RTSS SDK for API
	// Adapted from: https://github.com/Kaldaien/BMF/blob/master/BMF/osd.cpp
	/// </summary>
	/// <returns></returns>
	
	// RiverTune statistics must be open and running else no value will be returned
	HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "RTSSSharedMemoryV2");
    uint16_t fps = 0;

	if (hMapFile)
	{
		LPVOID pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		LPRTSS_SHARED_MEMORY pMem = (LPRTSS_SHARED_MEMORY)pMapAddr;

		if (pMem)
		{
				if ((pMem->dwSignature == 'RTSS') &&
					(pMem->dwVersion >= 0x00020000))
				{
					for (DWORD dwEntry = 0; dwEntry < pMem->dwAppArrSize; dwEntry++)
					{
						RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY)((LPBYTE)pMem + pMem->dwAppArrOffset + dwEntry * pMem->dwAppEntrySize);
						if (pEntry->dwProcessID != 0 && pEntry->dwFrames > 0) {
							fps = 1000.0f * pEntry->dwFrames / (pEntry->dwTime1 - pEntry->dwTime0);
							std::cout << fps << "FPS\n";
                            return fps;
                            
						}
					}
				}

				//Sleep(2000);
               
			UnmapViewOfFile(pMapAddr);
		}
		CloseHandle(hMapFile);
      
	}
    return fps;

}


void on_open(server* server, connection_hdl hdl) {
	m_connections.insert(hdl);
}

void on_close(server* server, connection_hdl hdl) {
	m_connections.erase(hdl);
}


void on_timer(server* server, websocketpp::lib::error_code const& ec) {
	if (ec) {
		// there was an error, stop telemetry
		server->get_alog().write(websocketpp::log::alevel::app,
			"Timer Error: " + ec.message());
		return;
	}

	//Get the FPS
	long fps = getFps();
	std::string fps_str = std::to_string(fps);

	// Broadcast count to all connections
	con_list::iterator it;
	for (it = m_connections.begin(); it != m_connections.end(); ++it) {
		server->send(*it, fps_str, websocketpp::frame::opcode::text);

	}

	// set timer for next telemetry check
	server->set_timer(
		1000,
		bind(
			&on_timer,
			server,
			::_1
		)
	);
}

int main()
{
	// Create a server endpoint
	server echo_server;

	// Adapted from Websocketcpp Telemetry example
	// https://github.com/zaphoyd/websocketpp/blob/master/examples/telemetry_server/telemetry_server.cpp

	try {
		// Set logging settings
		echo_server.set_access_channels(websocketpp::log::alevel::all);
		echo_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

		// Initialize Asio
		echo_server.init_asio();

		// Register our message handler
		echo_server.set_open_handler(bind(&on_open,&echo_server, ::_1));
		echo_server.set_close_handler(bind(&on_close, &echo_server, ::_1));

		// Listen on port 9002
		echo_server.listen(9002);

		// Start the server accept loop
		echo_server.start_accept();

		m_timer = echo_server.set_timer(
			1000,
			bind(
				&on_timer,
				&echo_server,
				::_1
			)
		);

		// Start the ASIO io_service run loop
		echo_server.run();
	}
	catch (websocketpp::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	catch (...) {
		std::cout << "other exception" << std::endl;
	}


	
	

	}