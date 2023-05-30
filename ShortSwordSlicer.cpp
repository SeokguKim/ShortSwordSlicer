#include "ConcatenatedHeader.h"
using namespace std;
using namespace rapidjson;

vector<string> argsStack;

int extensioncheck(string mypath, string& outpath) {
	int idx = mypath.find_last_of(".");
	if (idx == -1) {
		if (!filesystem::exists(mypath)) return 0;
		outpath = mypath + ".mod";
		if (filesystem::exists(outpath)) outpath += mypath + "_new.mod";
		return 2;
	}

	string extension = mypath.substr(idx + 1), base = mypath.substr(0, idx);
	if (extension == "mod") {
		outpath = base;
		if (filesystem::exists(outpath)) outpath += "_new";
		return 1;
	}
	return 0;
}

string tabline(int n) {
	string str(n, '\t');
	return str;
}

void truncaterest(string& str) {
	if (str[str.size() - 2] == ',') {
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

void jsonRecursiveLua(const Value& curObj, string& resString, int depth, string prevName) {
	if (curObj.IsArray()) {
		auto items = curObj.GetArray();
		for (auto& item : items) {
			auto curType = item.GetType();
			if (curType == Type::kArrayType || curType == Type::kObjectType) {
				resString += tabline(depth) + "{\n";
				jsonRecursiveLua(item, resString, depth + 1, prevName);
				resString += tabline(depth) + "},\n";
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
			resString += tabline(depth) + curGroup.name.GetString() + " = ";
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
				resString += tabline(depth + 1) + regex_replace(cur, newline, "\n" + tabline(depth + 1));
				resString += "\n" + tabline(depth) + "end,\n";
				continue;
			}

			auto curType = curGroup.value.GetType();
			if (curType == Type::kArrayType || curType == Type::kObjectType) {
				resString += "{\n";
				jsonRecursiveLua(curGroup.value, resString, depth + 1, curName);
				resString += tabline(depth) + "},\n";
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
	truncaterest(resString);
}

string modextract(string mypath, string outpath) {
	ifstream fin(mypath, ios_base::in | ios_base::binary);
	if (!fin) return "Error occured: Invalid .mod file: " + mypath;

	fin.seekg(0, fin.end);
	int len = (int)fin.tellg();
	fin.seekg(0, fin.beg);

	unsigned char* data = (unsigned char*)malloc(len);
	fin.read((char*)data, len);
	fin.close();
	
	string strdata = "";
	for (int i = 2; i < len; i++) strdata += data[i];
	
	regex block("\\s([\\s\\S]+?)(X\x01|\\}X)", regex::optimize);
	regex hashInfo("\\s(\\w+)");
	regex idInfo("\x1A[\\s\\S]+?(\\w+):\\/\\/([^\"]+)");
	regex jsonInfo("\\{[\\s\\S]+");
	regex subInfo("\x1A[\\s\\S]+?\n\\$((?:\\w{8}-\\w{4}-\\w{4}-\\w{4}-\\w{12}))\x12[\\s\\S]+?(?:(\\/[\\w\\/]+))\x1A[\\s\\S]+?(?:(\\{[^\x1A]+\\}))(\"[\\s\\S]+?((?:[\\x00-\\x7F])([\\w\\.\\,]+)))?");
	regex miscslicer("[\\w\\.]+");


	auto regexs = sregex_iterator(strdata.begin(), strdata.end() , block);
	auto regexe = sregex_iterator();
	
	for (sregex_iterator it = regexs; it != regexe; it++) {
		smatch matchstr = *it;
		string curstr = matchstr[0].str();
		while (curstr.back() != 'X') curstr.pop_back();
		curstr.pop_back();
		
		smatch m;
		regex_search(curstr, m, hashInfo);
		string uniqueIdentifier = m[1];
		curstr = m.suffix();
		regex_search(curstr, m, hashInfo);
		string bundleIdentifier = m[1];
		curstr = m.suffix();

		regex_search(curstr, m, idInfo);
		string category = m[1], entryId = m[2];
		curstr = m.suffix();
		string newpath = outpath + "\\" + category;
		filesystem::create_directories(newpath);

		string scriptName = entryId;
		string resString = "";
		if (category == "codeblock") {
			resString += "local unpackedContents = {\n";
			resString += tabline(1) + "uniqueIdentifier = \"" + uniqueIdentifier + "\",\n";
			resString += tabline(1) + "bundleIdentifier = \"" + bundleIdentifier + "\",\n";
			resString += tabline(1) + "category = \"" + category + "\",\n";
			resString += tabline(1) + "entryId = \"" + entryId + "\",\n";
			resString += tabline(1) + "contents = {\n";

			regex_search(curstr, m, jsonInfo);
			string fullString = m[0].str();
		
			const char* fullJson = fullString.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);
			if (jsonObj.HasParseError()) return "Error occured: Invalid json inside: " + category + ", " + entryId;

			if (jsonObj.HasMember("Name")) scriptName = jsonObj["Name"].GetString();
			else if (jsonObj.HasMember("name")) scriptName = jsonObj["name"].GetString();

			jsonRecursiveLua(jsonObj, resString, 2, "contentRoot");
			resString += tabline(1) + "}\n";
			resString += "}\n\nreturn unpackedContents";
		}
		else {
			resString += "{\n";
			resString += tabline(1) + "\"uniqueIdentifier\": \"" + uniqueIdentifier + "\",\n";
			resString += tabline(1) + "\"bundleIdentifier\": \"" + bundleIdentifier + "\",\n";
			resString += tabline(1) + "\"category\": \"" + category + "\",\n";
			resString += tabline(1) + "\"entryId\": \"" + entryId + "\",\n";
			resString += tabline(1) + "\"contents\": [\n";

			if (category == "gamelogic" || category == "map" || category == "ui") {
				auto subs = sregex_iterator(curstr.begin(), curstr.end() , subInfo);
				auto sube = sregex_iterator();

				for (auto sit = subs; sit != sube; sit++) {
					smatch cursub = *sit;
					resString += tabline(2) + "{\n";
					resString += tabline(3) + "\"entryId\": \"" + cursub[1].str() + "\",\n";
					resString += tabline(3) + "\"entryPath\": \"" + cursub[2].str() + "\",\n";
					resString += tabline(3) + "\"contents\": [\n";
					
					if (category == "ui" && sit == subs) scriptName = cursub[2].str().substr(4);
					string fullString = cursub[3].str();

					const char* fullJson = fullString.c_str();
					Document jsonObj;
					jsonObj.Parse(fullJson);
					if (jsonObj.HasParseError()) return "Error occured: Invalid json inside: " + category + ", " + entryId;

					StringBuffer buffer;
					PrettyWriter<StringBuffer> writer(buffer);
					jsonObj.Accept(writer);

					string cur = buffer.GetString();
					regex newline("\n");
					resString += tabline(4) + regex_replace(cur, newline, "\n" + tabline(4));

					resString += "\n" + tabline(3) + "],\n";
					resString += tabline(3) + "\"miscs\": [\n";
					if (cursub[5].str().size()) {
						string miscs = cursub[5].str();
						auto ms = sregex_iterator(miscs.begin(), miscs.end() , miscslicer);
						auto me = sregex_iterator();
						
						for (auto mit = ms; mit != me; mit++) {
							smatch curmisc = *mit;
							resString += tabline(4) + "\"" + curmisc.str() + "\",\n";
						}
						truncaterest(resString);
					}
					resString += tabline(3) + "]\n";
					resString += tabline(2) + "},\n";
				}
				truncaterest(resString);
				if (category == "gamelogic") scriptName = "common";
			}
			else {
				regex_search(curstr, m, jsonInfo);
				string fullString = m[0].str();

				const char* fullJson = fullString.c_str();
				Document jsonObj;
				jsonObj.Parse(fullJson);
				if (jsonObj.HasParseError()) return "Error occured: Invalid json inside: " + category + ", " + entryId;

				StringBuffer buffer;
				PrettyWriter<StringBuffer> writer(buffer);
				jsonObj.Accept(writer);

				string cur = buffer.GetString();
				regex newline("\n");
				resString += tabline(2) + regex_replace(cur, newline, "\n" + tabline(2)) + "\n";

				if (jsonObj.HasMember("Name")) scriptName = jsonObj["Name"].GetString();
				else if (jsonObj.HasMember("name")) scriptName = jsonObj["name"].GetString();
			}
			resString += tabline(1) + "]\n}";
		}

		string newfile = newpath + "\\" + category + "-" + scriptName + (category == "codeblock" ? ".lua" : ".json");
		ofstream fout(newfile, ios::binary | std::ios::trunc);
		fout << resString;
		fout.close();
	}
	return "Succefully unpacked your files in.." + outpath;
}

string getbyte(int len) {
	string res = "";

	while (len) {
		unsigned char cur = len % 128;
		len /= 128;
		if (len) cur |= 128;
		res += cur;
	}

	return res;
}

string blockconcat(vector<string>& v) {
	string res = v[0];
	res += (unsigned char)18;
	res +=(unsigned char)32;
	res += v[1];
	res += (unsigned char)26;
	res += getbyte(v[2].size());
	res += v[2];
	res += (unsigned char)34;
	res += getbyte(v[3].size());
	res += v[3];
	
	string inner = "";
	inner += (unsigned char)18;
	inner += getbyte(v[4].size());
	inner += v[4];
	inner = '2' + getbyte(inner.size()) + inner;

	res += inner + v[5];
	res = getbyte(res.size()) + res;
	res.insert(0, 1, (unsigned char)18);
	res.back() = (unsigned char)1;

	return res;
}

string specialconcat(vector<string>& v) {
	string res = v[0];
	res += (unsigned char)18;
	res += (unsigned char)32;
	res += v[1];
	res += (unsigned char)26;
	res += getbyte(v[2].size());
	res += v[2];
	res += (unsigned char)34;
	res += getbyte(v[3].size());
	res += v[3];

	int idx = 4;
	string inner = "";
	inner += (unsigned char)8;
	inner += (unsigned char)1;
	while (idx < v.size() - 1) {
		string tmp = v[idx++];
		tmp += (unsigned char)18;
		tmp += getbyte(v[idx].size());
		tmp += v[idx++];
		tmp += (unsigned char)26;
		tmp += getbyte(v[idx].size());
		tmp += v[idx++];
		if (v[idx][0] != 26 && v[idx][0] != 88 && v[idx][1] != 1) {
			tmp += (unsigned char)34;
			tmp += getbyte(v[idx].size());
			tmp += v[idx++];
		}
		tmp = getbyte(tmp.size()) + tmp;
		tmp.insert(0, 1, (unsigned char)26);
		inner += tmp;
	}

	inner = '2' + getbyte(inner.size()) + inner;

	res += inner + v.back();
	res = getbyte(res.size()) + res;
	res.insert(0, 1, (unsigned char)18);
	res += "X\x01";
	
	return res;
}

string modpack(string mypath, string outpath) {
	vector<pair<string, string>> sorted;
	map<string, int> dirNode;
	vector<vector<int>> dirTree;

	regex cbHead("Code\\s*=\\s*function\\s*\\(\\s*(\\w+\\,\\s*)*\\w*\\s*\\)");
	regex cbTail("end\\,(\\s*Scope\\s*=\\s*\\d+\\s*\\,[\\s\n]*ExecSpace\\s*=\\s*\\d+)");
	regex cbTabs("(^|\n)\t{4,5}");

	for (const auto& file : filesystem::recursive_directory_iterator(mypath)) {
		if (file.is_directory()) continue;
		string pn = file.path().string();
		string originfn = file.path().filename().string();
		string pathstr = file.path().parent_path().string();
		string extension = file.path().extension().string();
		string category = pathstr.substr(pathstr.rfind("\\") + 1);

		ifstream fin(pn);
		if (!fin) return "Error occured: Unable to read file: " + originfn + "";

		stringstream ss;
    	ss << fin.rdbuf();
		fin.close();

		string strdata = ss.str();
		vector<string> v;

		string fullId = "";
		string fullCategory = "";
		if (category == "codeblock") {
			if (extension != ".lua") return "Error occured: There is codeblock with invalid extension: " + originfn;
			queue<string> codeblocks;
			smatch sit;
			while (regex_search(strdata, sit, cbHead)) {
				smatch eit;
				if (!regex_search(strdata, eit, cbTail)) return "Error occured: Invalid codeblock in: " + originfn;

				int cs = sit.position() + sit.length(), ce = eit.position();
				string curCode = strdata.substr(cs, ce - cs);
				curCode = regex_replace(curCode, cbTabs, "$1");
				if (curCode[0] == '\n') curCode.assign(curCode.begin() + 1, curCode.end());
				if (curCode.back() == '\n') curCode.pop_back();
				codeblocks.push(curCode);
				strdata.replace(strdata.begin() + cs, strdata.begin() + ce, "\"\"");
				strdata = regex_replace(strdata, cbHead, "Code = ", regex_constants::format_first_only);
				strdata = regex_replace(strdata, cbTail, ",$1", regex_constants::format_first_only);
			}
			cout << strdata;
		}
		else {
			const char* fullJson = strdata.c_str();
			Document jsonObj;
			jsonObj.Parse(fullJson);

			fullId = string(jsonObj["catregory"].GetString()) + "://" + string(jsonObj["entryId"].GetString());
			fullCategory = "x-mod/" + string(jsonObj["catregory"].GetString());

			v.push_back("\n " + string(jsonObj["uniqueIdentifier"].GetString()));
			v.push_back(jsonObj["bundleIdentifier"].GetString());
			v.push_back(fullId);
			v.push_back(fullCategory);

			if (category == "directory") {
				if (!dirNode.count(fullId)) {
					dirNode[fullId] = dirNode.size();
					dirTree.push_back({});
				}
				auto children = jsonObj["contents"][0]["child_list"].GetArray();
				for (auto& child : children) {
					string curChild = child.GetString();
					if (!dirNode.count(curChild)) {
						dirNode[curChild] = dirNode.size();
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

		if (category == "gamelogic" || category == "map" || category == "ui") {
			sorted.push_back({fullId, specialconcat(v)});
		}
		else {
			sorted.push_back({fullId, blockconcat(v)});
		}
	}

	ofstream fout(outpath, ios::binary);
	fout << "\n\0";
	std::sort(sorted.begin(), sorted.end());
	for (auto& block : sorted) fout << block.second;
	fout.close();

	return "Succefully packed your files into..." + outpath;
}

int main(int argc, char** argv) {
	string mypath, outpath;

	int cnt = 0;
	while (++cnt) {
		if (cnt == 1 && argc >= 2) mypath = argv[1];
		else {
			cout << "Input path to .mod or directory... X to exit...\n";
			getline(cin, mypath);
		}
		if (mypath == "X") return 0 ;

		int ext = extensioncheck(mypath, outpath);
		if (!ext) {
			cout << "Error occured: Not a valid .mod file or path.\n";
			continue;
		}

		if (ext == 1) cout << modextract(mypath, outpath) << "\n";
		else cout << modpack(mypath, outpath) << "\n";
	}
}
