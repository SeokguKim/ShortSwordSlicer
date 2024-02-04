//SeokkguKim's ShortSwordSlicer
//Unpack and pack .mod
//     ......                                                        ...........   
//    :-------------::..      ..:::----------------::..      .::----------------   
//    ---------------------:------------------------------:---------------------   
//    --------------------------------------------------------------------------   
//    --------------------------------------------------------------------------.  
//   .--------------------------------------------------------------------------.  
//   .--------------------------------------------------------------------------.  
//   .--------------------------------------------------------------------------   
//   .--------------------------------------------------------------------------   
//    -------------------------------------------------------------------------:   
//    -------------------------------------------------------------------------.   
//    .------------------------------------------------------------------------    
//     -----------------------------------------------------------------------:    
//     .----------------------------------------------------------------------.    
//     :----------------------------------------------------------------------.    
//     ------------------------------------------------------------------------    
//     ------------------------------------------------------------------------.   
//     ------------------------------------------------------------------------.   
//     ------------------------------------------------------------------------    
//     ------------------------------------------------------------------------    
//     .----------------------------------------------------------------------.    
//      ----------------------------------------------------------------------     
//      .--------------------------------------------------------------------      
//       :------------------------------------------------------------------.      
//        :-------------------::..                  ..::-------------------.       
//         :-------------:..                               .::------------.        
//          .--------:.                                         .:-------          
//            :---.                                                .:--:           
#include "ConcatenatedHeader.h"
using namespace std;
using namespace rapidjson;

const wstring initError = L"Error occured while initializing. ";
const wstring unpackError = L"Error occured while unpacking. ";
const wstring packError = L"Error occured while packing. ";
const wstring vaildateError = L"Error occured while validating. ";

int extensioncheck(wstring& myPath) {
	auto curInput = filesystem::directory_entry(myPath);

	if (curInput.is_directory()) return 2;
	if (curInput.path().extension().string() == ".mod")	return 1;

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

size_t getLength(string& data, size_t& idx) {
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

void jsonRecursiveLua(const Value& curObj, string& resString, size_t depth, string prevName, vector<string> &argsStack) {
	if (curObj.IsArray()) {
		auto items = curObj.GetArray();
		for (auto& item : items) {
			auto curType = item.GetType();
			if (curType == Type::kArrayType || curType == Type::kObjectType) {
				resString += tabLine(depth) + "{\n";
				jsonRecursiveLua(item, resString, depth + 1, prevName, argsStack);
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
				if (prevName != "Properties" || curName != "Attributes") {
					jsonRecursiveLua(curGroup.value, resString, depth + 1, curName, argsStack);
				}
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

wstring unpackMod(wstring& myPath) {
	filesystem::path mymyPath(myPath);
	ifstream fin(mymyPath, ios_base::in | ios_base::binary);
	if (!fin) return unpackError + L"Unable to read .mod file: " + myPath;

	fin.seekg(0, fin.end);
	size_t len = (size_t)fin.tellg();
	fin.seekg(0, fin.beg);

	unsigned char* data = (unsigned char*)malloc(len);
	fin.read((char*)data, len);
	fin.close();
	
	regex miscslicer("[\\w\\d\\_\\.]+");

	size_t idx = 2;

	auto nd = myPath;
	for (int i = 0; i < 4; i++) nd.pop_back();
	while (filesystem::exists(nd)) nd += L"_new";

	while (idx < len) {
		size_t curBlockLen = getLength(data, idx);
		string curStr(data + idx, data + idx + curBlockLen);
		idx += curBlockLen;
		
		size_t subIdx = 0;

		size_t uniqueIdLen = getLength(curStr, subIdx);
		string uniqueIdentifier = curStr.substr(subIdx, uniqueIdLen);
		subIdx += uniqueIdLen;

		size_t bundleIdLen = getLength(curStr, subIdx);
		string bundleIdentifier = curStr.substr(subIdx, bundleIdLen);
		subIdx += bundleIdLen;

		size_t categoryLen = getLength(curStr, subIdx);
		string categoryId = curStr.substr(subIdx, categoryLen);
		subIdx += categoryLen;
		string category = categoryId.substr(0, categoryId.find(":"));
		string entryId = categoryId.substr(categoryId.find("://") + 3);
		
		size_t entryIdLen = getLength(curStr, subIdx);
		subIdx += entryIdLen;

		size_t contentLen = getLength(curStr, subIdx);

		
		auto newpath = nd + L"\\" + wstring(category.begin(), category.end());
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

			size_t jsonLen = getLength(curStr, subIdx);
			string fullString =  curStr.substr(subIdx, jsonLen);	
		
			const char* fullJson = fullString.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);
			if (jsonObj.HasParseError()) return L"Error occured: Invalid json inside: " + wstring(category.begin(), category.end()) + L", " + wstring(entryId.begin(), entryId.end());

			if (jsonObj.HasMember("Name")) scriptName = jsonObj["Name"].GetString();
			else if (jsonObj.HasMember("name")) scriptName = jsonObj["name"].GetString();

			vector<string> argsStack;
			jsonRecursiveLua(jsonObj, resString, 2, "contentRoot", argsStack);
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
				curStr = curStr.substr(subIdx, contentLen);
				subIdx = 2;
				size_t subLen = curStr.length();
				while (subIdx < subLen) {
					size_t curSubLen = getLength(curStr, subIdx);
					string curSub = curStr.substr(subIdx, curSubLen);

					size_t curSubIdx = 0;
					
					resString += tabLine(2) + "{\n";
	
					size_t curSubIdLen = getLength(curSub, curSubIdx);
					string subId = curSub.substr(curSubIdx, curSubIdLen);
					resString += tabLine(3) + "\"entryId\": \"" + subId + "\",\n";
					curSubIdx += curSubIdLen;
					
					size_t curSub_catLen = getLength(curSub, curSubIdx);
					string sub_cat = curSub.substr(curSubIdx, curSub_catLen);
					resString += tabLine(3) + "\"entryPath\": \"" + sub_cat + "\",\n";
					curSubIdx += curSub_catLen;

					resString += tabLine(3) + "\"contents\": [\n";
					size_t curSubMainLen = getLength(curSub, curSubIdx);
					string subMain = curSub.substr(curSubIdx, curSubMainLen);
					curSubIdx += curSubMainLen;

					const char* fullJson = subMain.c_str();
					Document jsonObj;
					jsonObj.Parse(fullJson);
					if (jsonObj.HasParseError()) return unpackError + L"Invalid json inside: " + wstring(category.begin(), category.end()) + L", " + wstring(entryId.begin(), entryId.end());
					
					StringBuffer buffer;
					PrettyWriter<StringBuffer> writer(buffer);
					jsonObj.Accept(writer);

					string cur = buffer.GetString();
					regex newline("\n");
					resString += tabLine(4) + regex_replace(cur, newline, "\n" + tabLine(4));

					resString += "\n" + tabLine(3) + "],\n";
					resString += tabLine(3) + "\"miscs\": [\n";

					if (curSub[curSubIdx] == '\"') {
						size_t curSub_miscsLen = getLength(curSub, curSubIdx);
						string miscs = curSub.substr(curSubIdx, curSub_miscsLen);
						curSubIdx += curSub_miscsLen;
						
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
					subIdx += curSubLen;		
				}
				truncateRest(resString);
				if (category == "gamelogic") scriptName = "common";
			}
			else {
				size_t jsonLen = getLength(curStr, subIdx);
				string fullString =  curStr.substr(subIdx, jsonLen);

				const char* fullJson = fullString.c_str();
				Document jsonObj;
				jsonObj.Parse(fullJson);
				if (jsonObj.HasParseError()) return unpackError + L"Invalid json inside: " + wstring(category.begin(), category.end()) + L", " + wstring(entryId.begin(), entryId.end());

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

		auto newfile = newpath + L"\\" + wstring(category.begin(), category.end()) + L"-" + wstring(scriptName.begin(), scriptName.end()) + (category == "codeblock" ? L".lua" : L".json");
		filesystem::path mynewFile(newfile);
		ofstream fout(mynewFile, ios::binary | std::ios::trunc);
		fout << resString;
		fout.close();
	}
	return L"Succefully unpacked your files in... " + nd;
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
	if (v.size() < 5) return "Invalid sequence while concatenating normal block";

	string res = "\n" + getByte(v[0].length()) + v[0];
	res += "\x12" + getByte(v[1].length()) + v[1];
	res += "\x1A" + getByte(v[2].length()) + v[2];
	res += "\x22" + getByte(v[3].length()) + v[3];
	
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
	if (v.size() < 5) return "Invalid sequence while concatenating hierarchy block";

	string res = "\n" + getByte(v[0].length()) + v[0];
	res += "\x12" + getByte(v[1].length()) + v[1];
	res += "\x1A" + getByte(v[2].length()) + v[2];
	res += "\x22" + getByte(v[3].length()) + v[3];

	size_t idx = 4;
	string innerSide = "\x08\x01";

	while (idx < v.size()) {
		string tmp = "\n" + getByte(v[idx].length());
		tmp += v[idx++];
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

int luaTableDecode(string& stringData, vector<string>& v, string& destId, queue<string>& codeblocks, string& scriptName) {
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

	v.push_back(uniqueIdentifier);
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
	scriptName = name;
	for (int i = 0; i < scriptName.length(); i++) {
		if ('A' <= scriptName[i] && scriptName[i] <= 'Z') scriptName[i] += 32;
	}

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

wstring packMod(wstring& myPath) {
	vector<pair<string, string>> sorted;

	set<string> missingCheck, duplicatedCheck;
	regex cbHead("Code\\s*=\\s*function\\s*\\(\\s*(\\w+\\,\\s*)*\\w*\\s*\\)");
	regex cbTail("end\\,(\\s*Scope\\s*=\\s*\\d+\\s*\\,[\\s\n]*ExecSpace\\s*=\\s*\\d+)");
	regex cbTabs("(^|\n)\t{4,5}");

	for (const auto& file : filesystem::recursive_directory_iterator(myPath)) {
		if (file.is_directory()) continue;
		string originalFilename = file.path().filename().string();
		string parentPath = file.path().parent_path().string();
		string extension = file.path().extension().string();
		string category = parentPath.substr(parentPath.rfind("\\") + 1);

		ifstream fin(file.path());
		if (!fin) return packError + L"Unable to read file: " + wstring(originalFilename.begin(), originalFilename.end());

		stringstream ss;
    	ss << fin.rdbuf();
		fin.close();

		string stringData = ss.str();
		vector<string> v;

		string fullId = "";
		string fullCategory = "";
		if (category == "codeblock") {
			if (extension != ".lua") return packError + L"There is a codeblock entry with invalid extension: " + wstring(originalFilename.begin(), originalFilename.end());
			queue<string> codeblocks;
			smatch sit;
			while (regex_search(stringData, sit, cbHead)) {
				smatch eit;
				if (!regex_search(stringData, eit, cbTail)) return packError + L"Invalid codeblock in: " + wstring(originalFilename.begin(), originalFilename.end());

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
			string scriptName = "";
			int luaDecodeResult = luaTableDecode(stringData, v, fullId, codeblocks, scriptName);
			if (luaDecodeResult) return packError + L"Invalid lua chunk in:" + wstring(originalFilename.begin(), originalFilename.end());
			if (duplicatedCheck.count(scriptName) || duplicatedCheck.count(fullId.substr(fullId.find("://") + 3))) return packError + L"There are duplicated entries with: " + wstring(originalFilename.begin(), originalFilename.end());
			duplicatedCheck.insert(scriptName);
			duplicatedCheck.insert(fullId.substr(fullId.find("://") + 3));
		}	
		else {
			if (extension != ".json") return packError + L"There is a non-codeblock entry with invalid extension: " + wstring(originalFilename.begin(), originalFilename.end());
			const char* fullJson = stringData.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);
			if (jsonObj.HasParseError()) return packError + L"Invalid json inside: " + wstring(originalFilename.begin(), originalFilename.end());

			fullId = string(jsonObj["category"].GetString()) + "://" + string(jsonObj["entryId"].GetString());
			fullCategory = "x-mod/" + string(jsonObj["category"].GetString());

			v.push_back(string(jsonObj["uniqueIdentifier"].GetString()));
			v.push_back(jsonObj["bundleIdentifier"].GetString());
			v.push_back(fullId);
			v.push_back(fullCategory);

			if (category == "directory") {
				string cd = jsonObj["contents"][0]["name"].GetString();
				auto children = jsonObj["contents"][0]["child_list"].GetArray();
				for (auto& child : children) {
					string curChild = child.GetString();
					missingCheck.insert(curChild);
				}
			}

			if (category == "gamelogic" || category == "map" || category == "ui") {
				auto contents = jsonObj["contents"].GetArray();
				for (auto& content : contents) {
					v.push_back(string(content["entryId"].GetString()));
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

			if (duplicatedCheck.count(fullId.substr(fullId.find("://") + 3))) return packError + L"There are duplicated entries with: " + wstring(originalFilename.begin(), originalFilename.end());
			duplicatedCheck.insert(fullId.substr(fullId.find("://") + 3));
		}

		if (category == "gamelogic" || category == "map" || category == "ui") {
			sorted.push_back({fullId, hierarchyConcat(v)});
		}
		else {
			sorted.push_back({fullId, blockConcat(v)});
		}
	}

	for (auto entry : missingCheck) {
		if (!duplicatedCheck.count(entry.substr(entry.find("://") + 3))) {
			string cur = entry.substr(entry.find("://") + 3);
			return packError + L"There is no entry with Id or Name: " + wstring(cur.begin(), cur.end());
		}
	}
	
	wstring outPath = myPath;
	while (filesystem::exists(outPath + L".mod")) outPath += L"_new";
	filesystem::path myoutPath(outPath + L".mod");
	ofstream fout(myoutPath, ios::binary);
	fout << '\n' << '\0';
	std::sort(sorted.begin(), sorted.end());
	for (auto& block : sorted) fout << block.second;
	fout.close();

	return L"Succefully packed your files into... " + outPath + L".mod";
}

int main(int argc, char** argv) {
	#ifdef _WIN32
		SetConsoleOutputCP(CP_UTF8);
		_setmode(_fileno(stdin), _O_U16TEXT);
	#else
		setlocale(LC_ALL, "");
	#endif

	wstring myPath; 
	
	
	int cnt = 0;
	while (++cnt) {
		
		if (cnt == 1 && argc >= 2) {
			wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			myPath = converter.from_bytes(argv[1]);
		}
		else {
			wcout << L"Input path to .mod or directory... X or Q to exit..." << "\n";
			getline(wcin, myPath);
			std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
		}
		if (myPath == L"X" || myPath == L"x" || myPath == L"Q" || myPath == L"q") return 0 ;

		int ext = extensioncheck(myPath);
		if (!ext) {
			wcerr << initError + L"Not a valid .mod file or path." << "\n";
			continue;
		}

		if (ext == 1) wcout << unpackMod(myPath) << "\n";
		else wcout << packMod(myPath) << "\n";
	}
}