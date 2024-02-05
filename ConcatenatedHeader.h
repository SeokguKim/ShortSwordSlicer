// Maybe you need MinGW to build.
#pragma once

#include <vector>
#include <queue>
#include <set>
#include <map>
#include <regex>
#include <variant>
#include <fstream>
#include <locale>
#include <codecvt>
#include <iostream>
#include <filesystem>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <lua.hpp>
#include <algorithm>
#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
    #include <Windows.h>
    #undef GetObject
#endif