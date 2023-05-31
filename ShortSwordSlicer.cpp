#include "ConcatenatedHeader.h"
using namespace std;
using namespace rapidjson;

vector<string> argsStack;
const string initError = "Error occured while initializing: ";
const string unpackError = "Error occured while unpacking: ";
const string packError = "Error occured while packing: ";
const string vaildateError = "Error occured while validating: ";

int extensioncheck(string myPath, string& outPath) {
	auto curInput = filesystem::directory_entry(myPath);

	if (curInput.is_directory()) {
		outPath = myPath + ".mod";
		if (filesystem::exists(outPath)) outPath += myPath + "_new.mod";
		return 2;
	}

	if (curInput.path().extension().string() == ".mod") {
		outPath = curInput.path().parent_path().string();
		if (outPath.back() != '/' && outPath.back() != '\\') outPath += "/";
		outPath += curInput.path().stem().string();
		if (filesystem::exists(outPath)) outPath += "_new";
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
				if (cur == "\"\"") cur = "";
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
				if (cur == "\"\"") cur = "";
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
			resString += "local unpackedContents = {\n";
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
	res += "\x22" + getByte(v[3].size()) + v[3];

	size_t idx = 4;
	string innerSide = "\x08\x01";

	while (idx < v.size()) {
		string tmp = v[idx++];
		tmp += (unsigned char)18;
		tmp += getByte(v[idx].size());
		tmp += v[idx++];
		tmp += (unsigned char)26;
		tmp += getByte(v[idx].size());
		tmp += v[idx++];
		if (v[idx][0] != 26 && v[idx][0] != 88 && v[idx][1] != 1) {
			tmp += (unsigned char)34;
			tmp += getByte(v[idx].size());
			tmp += v[idx++];
		}
		tmp = getByte(tmp.size()) + tmp;
		tmp.insert(0, 1, (unsigned char)26);
		innerSide += tmp;
	}

	innerSide = "2" + getByte(innerSide.length()) + innerSide;

	res += innerSide + "X\x01";
	res = getByte(res.length()) + res;
	res = "\x12" + res;
	
	return res;
}

bool dirDFS(int dirNum, set<string>& discoveredEntries, vector<vector<int>>& dirTree, vector<string>& dirIndex, vector<bool>& visited) {
	if (visited[dirNum] || !discoveredEntries.count(dirIndex[dirNum])) return 0;
	visited[dirNum] = 1;

	bool res = 1;
	for (int& dir : dirTree[dirNum]) res &= dirDFS(dir, discoveredEntries, dirTree, dirIndex, visited);
	return res;
}

string packMod(string myPath, string outPath) {
	vector<pair<string, string>> sorted;
	set<string> discoveredEntries;
	map<string, int> dirNode;
	vector<string> dirIndex;
	vector<vector<int>> dirTree;
	int rootDirNum = -1;

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
			lua_State *L = luaL_newstate();
		}
		else {
			if (extension != ".json") return packError + "There is a non-codeblock entry with invalid extension: " + originalFilename;
			const char* fullJson = stringData.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);
			if (jsonObj.HasParseError()) return packError + "Invalid json inside: " + originalFilename;

			fullId = string(jsonObj["catregory"].GetString()) + "://" + string(jsonObj["entryId"].GetString());
			fullCategory = "x-mod/" + string(jsonObj["catregory"].GetString());

			v.push_back("\n " + string(jsonObj["uniqueIdentifier"].GetString()));
			v.push_back(jsonObj["bundleIdentifier"].GetString());
			v.push_back(fullId);
			v.push_back(fullCategory);

			if (category == "directory") {
				if (!dirNode.count(fullId)) {
					dirNode[fullId] = dirNode.size();
					dirIndex.push_back(fullId);
					dirTree.push_back({});
				}
				if (jsonObj["contents"][0]["name"] == "RookDesk") rootDirNum = dirNode[fullId];

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

			auto contents = jsonObj["contents"].GetArray();
			for (auto& content : contents) {
				v.push_back("\n$" + string(content["entryId"].GetString()));
				v.push_back(content["entryPath"].GetString());

				StringBuffer buffer;
				Writer<StringBuffer> writer(buffer);
				content["contents"][0].Accept(writer);
				string convertedString = buffer.GetString();
				v.push_back(convertedString);

				string miscString = "";
				auto miscs = content["miscs"].GetArray();
				for (auto& misc : miscs) {
					miscString += (miscString.size() ? "," : "") + string(misc.GetString());
				}
				v.push_back(miscString);
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
	if (!dirDFS(rootDirNum, discoveredEntries, dirTree, dirIndex, visited)) return vaildateError + "Invalid directory structure";

	ofstream fout(outPath, ios::binary);
	fout << "\n\0";
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
	}
}
