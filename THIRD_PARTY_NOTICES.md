# Third-Party Notices

This application relies on the following open-source libraries:

## SQLite
- **URL**: https://www.sqlite.org/
- **License**: Public Domain
- **Description**: A C-language library that implements a small, fast, self-contained, high-reliability, full-featured, SQL database engine.
- **Usage**: Used for storing local Dhikr history and statistics.

## nlohmann/json
- **URL**: https://github.com/nlohmann/json
- **License**: MIT License
- **Description**: JSON for Modern C++
- **Usage**: Used for parsing the embedded content library and user-provided custom Dhikr JSON files.

### nlohmann/json License Text
```
MIT License 

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## miniaudio

The Qt frontend uses the bundled `third_party/miniaudio.h` (0.11.25) for local audio. The header includes its license options: public domain/Unlicense or MIT No Attribution. Source and license text: https://github.com/mackron/miniaudio.

## Qt 6

The Linux/macOS frontend links dynamically to Qt Core, Gui, Widgets, Network and, on Linux, DBus. The tests also use Qt Test; that module is not linked into the user executable. Linux candidate packaging uses the distribution's Qt 6.2.4, and macOS CI selects the official Qt 6.8.3 SDK.

Qt library license notices and the LGPL v3/GPL v3 texts accompany Qt candidate packages. Primary source and licensing information: https://code.qt.io/cgit/qt/qtbase.git/ and https://doc.qt.io/qt-6/licensing.html. The packaging script copies Debian copyright notices for bundled Linux libraries, including Qt's third-party dependencies. The project MIT license does not replace these licenses.

Qt is dynamically linked. An AppImage can be extracted with `--appimage-extract`; its libraries are in `squashfs-root/usr/lib`. A compatible replacement library can be used in an extracted bundle. macOS libraries are under the app's `Contents/Frameworks`; modified signed bundles may need a new signature to run.

## Cairo font

`assets/fonts/cairo.ttf` is the Cairo font, distributed under the SIL Open Font License 1.1. The copyright notice and complete license are copied into `assets/licenses/Cairo-OFL.txt` and shipped with installers/bundles. Upstream: https://github.com/Gue3bara/Cairo and https://github.com/google/fonts/tree/main/ofl/cairo.

## Bundled content and audio

The repository contains dhikr text and `assets/audio/diode_matrix.mp3`. Their presence does not establish authorship, redistribution permission or religious accuracy. Confirm their provenance and references before public distribution; that review has not been completed here.
