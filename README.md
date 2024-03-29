# :dagger: ShortSwordSlicer
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

![sss](sss.ico)

MapleStory Wolrds .mod file packing/unpacking tools with visiblity.  
시인성을 가진 메이플스토리 월드 .mod 파일 패킹/언패킹 도구

## Features
- Unpack .mod file to distinct .lua and .json files.  
- .mod 파일을 각각의 고유한 .lua 파일들과 .json 파일들로 언패킹합니다.

- Repack unfolded .lua and .json files to .mod file.  
- 언패킹된 .lua 파일들과 .json 파일들로부터 .mod 파일을 패킹합니다.

- Valid packing with files written in different worlds.. as long as they are sliced with ShortSwordSlicer.
- 각기 다른 월드에서 작성된 파일들을 ShortSwordSlicer로 분해하고 합쳤을 때 유효하게 패킹됩니다.

## Quick Guide
1. Clone the repository and build your own with CMake or download released version of `ShortSwordSlicer.exe`.
2. Execute your `ShortSwordSlicer.exe` program.
3. Input .mod file path or directory path contains vaild sources.(Input X to exit.)
4. Done.
---
1. 리포지토리를 복제하여 CMake로 직접 빌드하거나 릴리즈된 버전의 `ShortSwordSlicer.exe`를 다운로드합니다. 
2. `ShortSwordSlicer.exe` 프로그램을 실행합니다.
3. .mod나 유효한 파일들을 포함하는 디렉토리 경로를 입력합니다.(X를 입력해 종료합니다.)
4. 끝.
---

## Dependencies
- [Lua 5.4+](http://www.lua.org/)

## Bulid Dependencies
- [GNU Toolchain 12.0+](https://www.gnu.org/)
- [Lua 5.4+](http://www.lua.org/)
- [rapidjson 1.1+](http://rapidjson.org/)
- [CMake 3.26+](https://cmake.org/)

## Contact
SeokguKim <rokja97@naver.com>

This project is licensed under the terms of the MIT license.
