//SeokkguKim's ShortSwordSlicer
//Unpack and pack .mod
#include "ConcatenatedHeader.h"
using namespace std;
using namespace rapidjson;

vector<string> argsStack;
set<string> discoveredEntries;
const string initError = "Error occured while initializing. ";
const string unpackError = "Error occured while unpacking. ";
const string packError = "Error occured while packing. ";
const string vaildateError = "Error occured while validating. ";

int extensioncheck(string myPath, string& outPath) {
	auto curInput = filesystem::directory_entry(myPath);

	if (curInput.is_directory()) {
		outPath = myPath;
		while (filesystem::exists(outPath + ".mod")) outPath += "_new";
		outPath += ".mod";
		return 2;
	}

	if (curInput.path().extension().string() == ".mod") {
		outPath = curInput.path().parent_path().string();
		if (outPath.back() != '/' && outPath.back() != '\\') outPath += "/";
		outPath += curInput.path().stem().string();
		while (filesystem::exists(outPath)) outPath += "_new";
		return 1;
	}

	return 0;
}

size_t getLength(unsigned char*& data, size_t& idx) {
	size_t partLen = 0, f = 1;

	while (data[++idx] & 128) {
		partLen += (data[idx] & 127) * f;
		f *= 128;
	}
	partLen += (data[idx++] & 127) * f;

	return partLen;
}

string tabLine(size_t n) {
	string str(n, '\t');
	return str;
}

void truncateRest(string& str) {
	if (str.length() > 1 && str[str.length() - 2] == ',') {
		str.pop_back();
		str.pop_back();
		str += "\n";
	}
}

string stringify(const Value& v) {
	if (v.IsString())
		return { v.GetString(), v.GetStringLength() };
	else {
		StringBuffer strbuf;
		Writer<StringBuffer> writer(strbuf);
		v.Accept(writer);
		return strbuf.GetString();
	}
}

void jsonRecursiveLua(const Value& curObj, string& resString, size_t depth, string prevName) {
	if (curObj.IsArray()) {
		auto items = curObj.GetArray();
		for (auto& item : items) {
			auto curType = item.GetType();
			if (curType == Type::kArrayType || curType == Type::kObjectType) {
				resString += tabLine(depth) + "{\n";
				jsonRecursiveLua(item, resString, depth + 1, prevName);
				resString += tabLine(depth) + "},\n";
			}
			else if (curType == Type::kStringType){
				string cur = item.GetString();
				if (cur.size() > 1 && cur[0] == '\"' && cur.back() == '\"') cur = cur.substr(1), cur.pop_back();
				resString += "\"" + cur + "\",\n";
			}
			else if (curType == Type::kNullType) {
				resString += "nil,\n";
			}
			else {
				string cur = stringify(item);
				resString += cur + ",\n";
			}
		}
	}
	else if (curObj.IsObject()) {
		auto groups = curObj.GetObject();
		for (auto& curGroup : groups) {
			resString += tabLine(depth) + curGroup.name.GetString() + " = ";
			string curName = curGroup.name.GetString();
			if (curName == "Code") {
				string curArgs = "";
				for (auto& arg : argsStack) {
					curArgs += (curArgs.length() ? ", " : "") + arg;
				}
				argsStack.clear();
				resString += "function(" + curArgs + ")\n";
				string cur = curGroup.value.GetString();
				regex newline("\n");
				resString += tabLine(depth + 1) + regex_replace(cur, newline, "\n" + tabLine(depth + 1));
				resString += "\n" + tabLine(depth) + "end,\n";
				continue;
			}

			auto curType = curGroup.value.GetType();
			if (curType == Type::kArrayType || curType == Type::kObjectType) {
				resString += "{\n";
				jsonRecursiveLua(curGroup.value, resString, depth + 1, curName);
				resString += tabLine(depth) + "},\n";
			}
			else if (curType == Type::kStringType){
				string cur = curGroup.value.GetString();
				if (cur.size() > 1 && cur[0] == '\"' && cur.back() == '\"') cur = cur.substr(1), cur.pop_back();
				resString += "\"" + cur + "\",\n";
				if (prevName == "Arguments" && curName == "Name") argsStack.push_back(cur);
			}
			else if (curType == Type::kNullType) {
				resString += "nil,\n";
			}
			else {
				string cur = stringify(curGroup.value);
				resString += cur + ",\n";
			}
		}
	}
	truncateRest(resString);
}

string unpackMod(string myPath, string outPath) {
	ifstream fin(myPath, ios_base::in | ios_base::binary);
	if (!fin) return unpackError + "Unable to read .mod file: " + myPath;

	fin.seekg(0, fin.end);
	size_t len = (size_t)fin.tellg();
	fin.seekg(0, fin.beg);

	unsigned char* data = (unsigned char*)malloc(len);
	fin.read((char*)data, len);
	fin.close();
	
	regex hashInfo("\\s(\\w+)");
	regex idInfo("\x1A[\\s\\S]+?(\\w+):\\/\\/([^\"]+)");
	regex jsonInfo("\\{[\\s\\S]+\\}");
	regex subInfo("\x1A[\\s\\S]+?\n\\$((?:\\w{8}-\\w{4}-\\w{4}-\\w{4}-\\w{12}))\x12[\\s\\S]+?(?:(\\/[\\w\\/]+))\x1A[\\s\\S]+?(?:(\\{[^\x1A]+\\}))(\"[\\s\\S]+?((?:[\\x00-\\x7F])([\\w\\.\\,]+)))?");
	regex miscslicer("[\\w\\.]+");

	size_t idx = 2;
	
	while (idx < len) {
		size_t curBlockLen = getLength(data, idx);
		string curStr(data + idx, data + idx + curBlockLen);
		idx += curBlockLen;

		smatch m;
		regex_search(curStr, m, hashInfo);
		string uniqueIdentifier = m[1];
		curStr = m.suffix();
		regex_search(curStr, m, hashInfo);
		string bundleIdentifier = m[1];
		curStr = m.suffix();

		regex_search(curStr, m, idInfo);
		string category = m[1], entryId = m[2];
		curStr = m.suffix();
		string newpath = outPath + "\\" + category;
		filesystem::create_directories(newpath);

		string scriptName = entryId;
		string resString = "";
		if (category == "codeblock") {
			resString += "unpackedContents = {\n";
			resString += tabLine(1) + "uniqueIdentifier = \"" + uniqueIdentifier + "\",\n";
			resString += tabLine(1) + "bundleIdentifier = \"" + bundleIdentifier + "\",\n";
			resString += tabLine(1) + "category = \"" + category + "\",\n";
			resString += tabLine(1) + "entryId = \"" + entryId + "\",\n";
			resString += tabLine(1) + "contents = {\n";

			regex_search(curStr, m, jsonInfo);
			string fullString = m[0].str();
		
			const char* fullJson = fullString.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);
			if (jsonObj.HasParseError()) return "Error occured: Invalid json inside: " + category + ", " + entryId;

			if (jsonObj.HasMember("Name")) scriptName = jsonObj["Name"].GetString();
			else if (jsonObj.HasMember("name")) scriptName = jsonObj["name"].GetString();

			jsonRecursiveLua(jsonObj, resString, 2, "contentRoot");
			resString += tabLine(1) + "}\n";
			resString += "}\n\nreturn unpackedContents";
		}
		else {
			resString += "{\n";
			resString += tabLine(1) + "\"uniqueIdentifier\": \"" + uniqueIdentifier + "\",\n";
			resString += tabLine(1) + "\"bundleIdentifier\": \"" + bundleIdentifier + "\",\n";
			resString += tabLine(1) + "\"category\": \"" + category + "\",\n";
			resString += tabLine(1) + "\"entryId\": \"" + entryId + "\",\n";
			resString += tabLine(1) + "\"contents\": [\n";

			if (category == "gamelogic" || category == "map" || category == "ui") {
				auto subs = sregex_iterator(curStr.begin(), curStr.end() , subInfo);
				auto sube = sregex_iterator();

				for (auto sit = subs; sit != sube; sit++) {
					smatch cursub = *sit;
					resString += tabLine(2) + "{\n";
					resString += tabLine(3) + "\"entryId\": \"" + cursub[1].str() + "\",\n";
					resString += tabLine(3) + "\"entryPath\": \"" + cursub[2].str() + "\",\n";
					resString += tabLine(3) + "\"contents\": [\n";
					
					if (category == "ui" && sit == subs) scriptName = cursub[2].str().substr(4);
					string fullString = cursub[3].str();

					const char* fullJson = fullString.c_str();
					Document jsonObj;
					jsonObj.Parse(fullJson);
					if (jsonObj.HasParseError()) return unpackError + "Invalid json inside: " + category + ", " + entryId;

					StringBuffer buffer;
					PrettyWriter<StringBuffer> writer(buffer);
					jsonObj.Accept(writer);

					string cur = buffer.GetString();
					regex newline("\n");
					resString += tabLine(4) + regex_replace(cur, newline, "\n" + tabLine(4));

					resString += "\n" + tabLine(3) + "],\n";
					resString += tabLine(3) + "\"miscs\": [\n";
					if (cursub[5].str().size()) {
						string miscs = cursub[5].str();
						if (miscs.back() == 'X') miscs.pop_back();
						auto ms = sregex_iterator(miscs.begin(), miscs.end() , miscslicer);
						auto me = sregex_iterator();
						
						for (auto mit = ms; mit != me; mit++) {
							smatch curmisc = *mit;
							resString += tabLine(4) + "\"" + curmisc.str() + "\",\n";
						}
						truncateRest(resString);
					}
					resString += tabLine(3) + "]\n";
					resString += tabLine(2) + "},\n";
				}
				truncateRest(resString);
				if (category == "gamelogic") scriptName = "common";
			}
			else {
				regex_search(curStr, m, jsonInfo);
				string fullString = m[0].str();

				const char* fullJson = fullString.c_str();
				Document jsonObj;
				jsonObj.Parse(fullJson);
				if (jsonObj.HasParseError()) return unpackError + "Invalid json inside: " + category + ", " + entryId;

				StringBuffer buffer;
				PrettyWriter<StringBuffer> writer(buffer);
				jsonObj.Accept(writer);

				string cur = buffer.GetString();
				regex newline("\n");
				resString += tabLine(2) + regex_replace(cur, newline, "\n" + tabLine(2)) + "\n";

				if (jsonObj.HasMember("Name")) scriptName = jsonObj["Name"].GetString();
				else if (jsonObj.HasMember("name")) scriptName = jsonObj["name"].GetString();
			}
			resString += tabLine(1) + "]\n}";
		}

		string newfile = newpath + "\\" + category + "-" + scriptName + (category == "codeblock" ? ".lua" : ".json");
		ofstream fout(newfile, ios::binary | std::ios::trunc);
		fout << resString;
		fout.close();
	}
	return "Succefully unpacked your files in.." + outPath;
}

string getByte(size_t partLen) {
	string res = "";

	while (partLen) {
		unsigned char cur = partLen % 128;
		partLen /= 128;
		if (partLen) cur |= 128;
		res += cur;
	}

	return res;
}

string blockConcat(vector<string>& v) {
	if (v.size() < 5) return packError + "Invalid sequence while concatenating normal block";

	string res = v[0];
	res += "\x12\x20" + v[1];
	res += "\x1A" + getByte(v[2].length()) + v[2];
	res += "\x22" + getByte(v[3].size()) + v[3];
	
	string innerSide = "\x12";
	innerSide += getByte(v[4].length());
	innerSide += v[4];
	innerSide = "2" + getByte(innerSide.length()) + innerSide;

	res += innerSide + "X\x01";
	res = getByte(res.length()) + res;
	res = "\x12" + res;

	return res;
}

string hierarchyConcat(vector<string>& v) {
	if (v.size() < 5) return packError + "Invalid sequence while concatenating hierarchy block";

	string res = v[0];
	res += "\x12\x20" + v[1];
	res += "\x1A" + getByte(v[2].length()) + v[2];
	res += "\x22" + getByte(v[3].length()) + v[3];

	size_t idx = 4;
	string innerSide = "\x08\x01";

	while (idx < v.size()) {
		string tmp = v[idx++];
		tmp += "\x12" + getByte(v[idx].length());
		tmp += v[idx++];
		tmp += "\x1A" + getByte(v[idx].length());
		tmp += v[idx++];
		if (v[idx] != "") {
			tmp += "\x22" + getByte(v[idx].length());
			tmp += v[idx++];
		}
		else idx++;
		tmp = getByte(tmp.length()) + tmp;
		tmp = "\x1A" + tmp;
		innerSide += tmp;
	}

	innerSide = "2" + getByte(innerSide.length()) + innerSide;

	res += innerSide + "X\x01";
	res = getByte(res.length()) + res;
	res = "\x12" + res;
	
	return res;
}

bool dirDFS(int dirNum, vector<vector<int>>& dirTree, vector<string>& dirIndex, vector<bool>& visited) {
	if (visited[dirNum]) return 0;
	if (dirIndex[dirNum].find("component://") == -1 && dirIndex[dirNum].find("gamelogic://") == -1 && !discoveredEntries.count(dirIndex[dirNum]))
		return 0;
	visited[dirNum] = 1;

	bool res = 1;
	for (int& dir : dirTree[dirNum]) 
		res &= dirDFS(dir, dirTree, dirIndex, visited);

	return res;
}

variant<string, int> luaGetfromKey(lua_State *L, string key) {
	lua_pushstring(L, key.c_str());
	int err = lua_gettable(L, -2);
	
	if (lua_isinteger(L, -1)) {
		int resInt = lua_tointeger(L, -1);
		lua_pop(L, 1);
		return resInt;
	}
	else if (lua_isstring(L, -1)) {
		string resString = lua_tostring(L, -1);
		lua_pop(L, 1);
		return resString;
	}

	lua_pop(L, 1);

	return "\v";
}

void propertyIterate(lua_State *L, Value& dest, rapidjson::MemoryPoolAllocator<>& alloc) {
	while(lua_next(L, -2)) {
		string ptype = get<string>(luaGetfromKey(L, "Type"));
		string defaultValue = get<string>(luaGetfromKey(L, "DefaultValue"));
		int syncDirection = get<int>(luaGetfromKey(L, "SyncDirection"));
		string name = get<string>(luaGetfromKey(L, "Name"));
		if (ptype == "string") defaultValue = "\"" + defaultValue + "\"";

		Value curProperty;
		curProperty.SetObject();
		curProperty.AddMember("Type", Value().SetString(ptype.c_str(), alloc), alloc);
		curProperty.AddMember("DefaultValue", (defaultValue == "\v" ? Value().SetNull() : Value().SetString(defaultValue.c_str(), alloc)), alloc);
		curProperty.AddMember("SyncDirection", Value(syncDirection), alloc);
		curProperty.AddMember("Attributes", Value().SetArray(), alloc);
		curProperty.AddMember("Name", (name == "\v" ? Value().SetNull() : Value().SetString(name.c_str(), alloc)), alloc);
		
		dest.PushBack(curProperty, alloc);

		lua_pop(L, 1);
	}
}

void methodIterate(lua_State *L, Value& dest, rapidjson::MemoryPoolAllocator<>& alloc, queue<string>& codeblocks) {
	while(lua_next(L, -2)) {
		Value curMethod;
		curMethod.SetObject();

		Value ret;
		ret.SetObject();
		lua_pushstring(L, "Return");
		lua_gettable(L, -2);

		string rtype = get<string>(luaGetfromKey(L, "Type"));
		string defaultValue = get<string>(luaGetfromKey(L, "DefaultValue"));
		int syncDirection = get<int>(luaGetfromKey(L, "SyncDirection"));
		string rname = get<string>(luaGetfromKey(L, "Name"));
		if (rtype == "string") defaultValue = "\"" + defaultValue + "\"";
		
		
		ret.AddMember("Type", Value().SetString(rtype.c_str(), alloc), alloc);
		ret.AddMember("DefaultValue", (defaultValue == "\v" ? Value().SetNull() : Value().SetString(defaultValue.c_str(), alloc)), alloc);
		ret.AddMember("SyncDirection", Value(syncDirection), alloc);
		ret.AddMember("Attributes", Value().SetArray(), alloc);
		ret.AddMember("Name", (rname == "\v" ? Value().SetNull() : Value().SetString(rname.c_str(), alloc)), alloc);
		curMethod.AddMember("Return", ret, alloc);
		lua_pop(L, 1);

		Value args;
		args.SetArray();
		lua_pushstring(L, "Arguments");
		lua_gettable(L, -2);
		if (lua_isnil(L, -1)) curMethod.AddMember("Arguments", Value().SetNull(), alloc);
		else {
			lua_pushnil(L);
			propertyIterate(L, args, alloc);
			curMethod.AddMember("Arguments", args, alloc);
		}
		lua_pop(L, 1);

		string curCode = codeblocks.front();
		codeblocks.pop();
		int scope = get<int>(luaGetfromKey(L, "Scope"));
		int_least64_t execspace = get<int>(luaGetfromKey(L, "ExecSpace"));
		string mname = get<string>(luaGetfromKey(L, "Name"));
		curMethod.AddMember("Code", Value().SetString(curCode.c_str(), alloc), alloc);
		curMethod.AddMember("Scope", Value(scope), alloc);
		curMethod.AddMember("ExecSpace", Value(execspace), alloc);
		curMethod.AddMember("Attributes", Value().SetArray(), alloc);
		curMethod.AddMember("Name", Value().SetString(mname.c_str(), alloc), alloc);
		
		dest.PushBack(curMethod, alloc);

		lua_pop(L, 1);
	}
}

void eventIterate(lua_State *L, Value& dest, rapidjson::MemoryPoolAllocator<>& alloc, queue<string>& codeblocks) {
	while(lua_next(L, -2)) {
		Value curEvent;
		curEvent.SetObject();
		string ehname = get<string>(luaGetfromKey(L, "Name"));
		string eventname = get<string>(luaGetfromKey(L, "EventName"));
		string target = get<string>(luaGetfromKey(L, "Target"));
		string curCode = codeblocks.front();
		codeblocks.pop();
		int scope = get<int>(luaGetfromKey(L, "Scope"));
		int execspace = get<int>(luaGetfromKey(L, "ExecSpace"));

		curEvent.AddMember("Name", Value().SetString(ehname.c_str(), alloc), alloc);
		curEvent.AddMember("EventName", Value().SetString(eventname.c_str(), alloc), alloc);
		curEvent.AddMember("Target", (target == "\v" ? Value().SetNull() : Value().SetString(target.c_str(), alloc)), alloc);
		curEvent.AddMember("Code", Value().SetString(curCode.c_str(), alloc), alloc);
		curEvent.AddMember("Scope", Value(scope), alloc);
		curEvent.AddMember("ExecSpace", Value(execspace), alloc);

		dest.PushBack(curEvent, alloc);

		lua_pop(L, 1);
	}
}

int luaTableDecode(string& stringData, vector<string>& v, string& destId, queue<string>& codeblocks) {
	lua_State *L = luaL_newstate();
	int err = luaL_dostring(L, stringData.c_str());
	if (err) return 1;

	lua_getglobal(L, "unpackedContents");
	if (!lua_istable(L, -1)) return 1;

	string uniqueIdentifier = get<string>(luaGetfromKey(L, "uniqueIdentifier"));
	string bundleIdentifier = get<string>(luaGetfromKey(L, "bundleIdentifier"));
	string category = get<string>(luaGetfromKey(L, "category"));
	string entryId = get<string>(luaGetfromKey(L, "entryId"));

	string fullId = category + "://" + entryId;
	destId = fullId;
	string fullCategory = "x-mod/" + category;

	v.push_back("\n " + uniqueIdentifier);
	v.push_back(bundleIdentifier);
	v.push_back(fullId);
	v.push_back(fullCategory);

	lua_pushstring(L, "contents");
	lua_gettable(L, -2);

	Document jsonObj;
	jsonObj.SetObject();
	auto& alloc = jsonObj.GetAllocator();

	lua_pushstring(L, "CoreVersion");
	lua_gettable(L, -2);
	Value coreVersion;
	coreVersion.SetObject();
	int cmajor = get<int>(luaGetfromKey(L, "Major")), cminor = get<int>(luaGetfromKey(L, "Minor"));
	coreVersion.AddMember("Major", Value(cmajor), alloc);
	coreVersion.AddMember("Minor", Value(cminor), alloc);
	jsonObj.AddMember("CoreVersion", coreVersion, alloc);
	lua_pop(L, 1);

	lua_pushstring(L, "ScriptVersion");
	lua_gettable(L, -2);
	Value scriptVersion;
	scriptVersion.SetObject();
	int smajor = get<int>(luaGetfromKey(L, "Major")), sminor = get<int>(luaGetfromKey(L, "Minor"));
	scriptVersion.AddMember("Major", Value(smajor), alloc);
	scriptVersion.AddMember("Minor", Value(sminor), alloc);
	jsonObj.AddMember("ScriptVersion", scriptVersion, alloc);
	lua_pop(L, 1);

	string description = get<string>(luaGetfromKey(L, "Description"));
	string id = get<string>(luaGetfromKey(L, "Id"));
	int language = get<int>(luaGetfromKey(L, "Language"));
	string name = get<string>(luaGetfromKey(L, "Name"));
	int stype = get<int>(luaGetfromKey(L, "Type"));
	int ssource = get<int>(luaGetfromKey(L, "Source"));
	string target = get<string>(luaGetfromKey(L, "Target"));
	string modifytime = get<string>(luaGetfromKey(L, "ModifyTime"));

	jsonObj.AddMember("Description", Value().SetString(description.c_str(), alloc), alloc);
	jsonObj.AddMember("Id", Value().SetString(id.c_str(), alloc), alloc);
	jsonObj.AddMember("Language", Value(language), alloc);
	jsonObj.AddMember("Name", Value().SetString(name.c_str(), alloc), alloc);
	jsonObj.AddMember("Type", Value(stype), alloc);
	jsonObj.AddMember("Source", Value(ssource), alloc);
	jsonObj.AddMember("Target", (target == "\v" ? Value().SetNull() : Value().SetString(target.c_str(), alloc)), alloc);
	jsonObj.AddMember("ModifyTime", Value().SetString(modifytime.c_str(), alloc), alloc);

	lua_pushstring(L, "Properties");
	lua_gettable(L, -2);
	lua_pushnil(L);
	Value properties;
	properties.SetArray();
	propertyIterate(L, properties, alloc);
	jsonObj.AddMember("Properties", properties, alloc);
	lua_pop(L, 1);

	lua_pushstring(L, "Methods");
	lua_gettable(L, -2);
	lua_pushnil(L);
	Value methods;
	methods.SetArray();
	methodIterate(L, methods, alloc, codeblocks);
	jsonObj.AddMember("Methods", methods, alloc);
	lua_pop(L, 1);

	lua_pushstring(L, "EntityEventHandlers");
	lua_gettable(L, -2);
	lua_pushnil(L);
	Value events;
	events.SetArray();
	eventIterate(L, events, alloc, codeblocks);
	jsonObj.AddMember("EntityEventHandlers", events, alloc);
	lua_pop(L, 1);

	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	jsonObj.Accept(writer);
	string convertedString = buffer.GetString();
	v.push_back(convertedString);

	return 0;
}

string packMod(string myPath, string outPath) {
	vector<pair<string, string>> sorted;
	map<string, int> dirNode;
	vector<string> dirIndex;
	vector<vector<int>> dirTree;
	int rootDirNum = -1, dirCnt = 0;

	regex cbHead("Code\\s*=\\s*function\\s*\\(\\s*(\\w+\\,\\s*)*\\w*\\s*\\)");
	regex cbTail("end\\,(\\s*Scope\\s*=\\s*\\d+\\s*\\,[\\s\n]*ExecSpace\\s*=\\s*\\d+)");
	regex cbTabs("(^|\n)\t{4,5}");

	for (const auto& file : filesystem::recursive_directory_iterator(myPath)) {
		if (file.is_directory()) continue;
		string filePath = file.path().string();
		string originalFilename = file.path().filename().string();
		string parentPath = file.path().parent_path().string();
		string extension = file.path().extension().string();
		string category = parentPath.substr(parentPath.rfind("\\") + 1);

		ifstream fin(filePath);
		if (!fin) return packError + "Unable to read file: " + originalFilename;

		stringstream ss;
    	ss << fin.rdbuf();
		fin.close();

		string stringData = ss.str();
		vector<string> v;

		string fullId = "";
		string fullCategory = "";
		if (category == "codeblock") {
			if (extension != ".lua") return packError + "There is a codeblock entry with invalid extension: " + originalFilename;
			queue<string> codeblocks;
			smatch sit;
			while (regex_search(stringData, sit, cbHead)) {
				smatch eit;
				if (!regex_search(stringData, eit, cbTail)) return packError + "Invalid codeblock in: " + originalFilename;

				size_t cs = sit.position() + sit.length(), ce = eit.position();
				string curCode = stringData.substr(cs, ce - cs);
				curCode = regex_replace(curCode, cbTabs, "$1");
				if (curCode[0] == '\n') curCode.assign(curCode.begin() + 1, curCode.end());
				if (curCode.back() == '\n') curCode.pop_back();
				codeblocks.push(curCode);
				stringData.replace(stringData.begin() + cs, stringData.begin() + ce, "\"\"");
				stringData = regex_replace(stringData, cbHead, "Code = ", regex_constants::format_first_only);
				stringData = regex_replace(stringData, cbTail, ",$1", regex_constants::format_first_only);
			}
			int luaDecodeResult = luaTableDecode(stringData, v, fullId, codeblocks);
			if (luaDecodeResult) return packError + "Invalid lua chunk in:" + originalFilename;
		}	
		else {
			if (extension != ".json") return packError + "There is a non-codeblock entry with invalid extension: " + originalFilename;
			const char* fullJson = stringData.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);
			if (jsonObj.HasParseError()) return packError + "Invalid json inside: " + originalFilename;

			fullId = string(jsonObj["category"].GetString()) + "://" + string(jsonObj["entryId"].GetString());
			fullCategory = "x-mod/" + string(jsonObj["category"].GetString());

			v.push_back("\n " + string(jsonObj["uniqueIdentifier"].GetString()));
			v.push_back(jsonObj["bundleIdentifier"].GetString());
			v.push_back(fullId);
			v.push_back(fullCategory);

			if (category == "directory") {
				dirCnt++;

				if (!dirNode.count(fullId)) {
					dirNode[fullId] = dirNode.size();
					dirIndex.push_back(fullId);
					dirTree.push_back({});
				}
				string cd = jsonObj["contents"][0]["name"].GetString();
				if (cd == "RootDesk") rootDirNum = dirNode[fullId];

				auto children = jsonObj["contents"][0]["child_list"].GetArray();
				for (auto& child : children) {
					string curChild = child.GetString();
					if (!dirNode.count(curChild)) {
						dirNode[curChild] = dirNode.size();
						dirIndex.push_back(curChild);
						dirTree.push_back({});
					}
					dirTree[dirNode[fullId]].push_back(dirNode[curChild]);
				}
			}

			if (category == "gamelogic" || category == "map" || category == "ui") {
				auto contents = jsonObj["contents"].GetArray();
				for (auto& content : contents) {
					v.push_back("\n$" + string(content["entryId"].GetString()));
					v.push_back(content["entryPath"].GetString());

					StringBuffer buffer;
					Writer<StringBuffer> writer(buffer);
					content["contents"][0].Accept(writer);
					string convertedString = buffer.GetString();
					v.push_back(convertedString);

					if (content.HasMember("miscs")){
						string miscString = "";
						auto miscs = content["miscs"].GetArray();
						for (auto& misc : miscs) {
							miscString += (miscString.size() ? "," : "") + string(misc.GetString());
						}
						v.push_back(miscString);
					}
				}
			}
			else {
				StringBuffer buffer;
				Writer<StringBuffer> writer(buffer);
				jsonObj["contents"][0].Accept(writer);
				string convertedString = buffer.GetString();
				v.push_back(convertedString);
			}
		}
		if (discoveredEntries.count(fullId)) return vaildateError + "There a pair of duplicated id: " + fullId;
		else discoveredEntries.insert(fullId);

		if (category == "gamelogic" || category == "map" || category == "ui") {
			sorted.push_back({fullId, hierarchyConcat(v)});
		}
		else {
			sorted.push_back({fullId, blockConcat(v)});
		}
	}

	vector<bool> visited(dirNode.size());
	if (rootDirNum == -1 && dirCnt) return vaildateError + "Invalid directory structure";
	if (rootDirNum != -1 && !dirDFS(rootDirNum, dirTree, dirIndex, visited)) return vaildateError + "Invalid directory structure";

	ofstream fout(outPath, ios::binary);
	fout << '\n' << '\0';
	std::sort(sorted.begin(), sorted.end());
	for (auto& block : sorted) fout << block.second;
	fout.close();

	return "Succefully packed your files into..." + outPath;
}

int main(int argc, char** argv) {
	string myPath, outPath;

	int cnt = 0;
	while (++cnt) {
		if (cnt == 1 && argc >= 2) myPath = argv[1];
		else {
			cout << "Input path to .mod or directory... X to exit...\n";
			getline(cin, myPath);
		}
		if (myPath == "X") return 0 ;

		int ext = extensioncheck(myPath, outPath);
		if (!ext) {
			cout << initError + "Not a valid .mod file or path.\n";
			continue;
		}

		if (ext == 1) cout << unpackMod(myPath, outPath) << "\n";
		else cout << packMod(myPath, outPath) << "\n";
		
		argsStack.clear();
		discoveredEntries.clear();
	}
}