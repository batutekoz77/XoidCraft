#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <filesystem>
#include <string>


namespace {
	inline std::string OUTPUT_DIRECTORY() {
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length == 0) return {};
        return std::filesystem::path(std::string(buffer, length)).parent_path().string();
    }
}

namespace SERVER {
	inline const char*		IP					=		"127.0.0.1";
	inline unsigned short	PORT				=		25565;

	inline int				MAX_PLAYER			=		20;
	inline int				PROTOCOL			=		47;

	inline std::string		NAME				=		"XoidCraft";
	inline std::string		TAG					=		"Network";
	inline std::string		DESCRIPTION			=		"Development Server";
	inline std::string		VERSION				=		"1.8.9";
}