#ifndef LITTLEFS_MANAGER_H
#define LITTLEFS_MANAGER_H

#include "LittleFS.h"

// Initialize LittleFS
void initLittleFS();

// Read File from LittleFS
String readFile(fs::FS &fs, const char * path);

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message);

#endif
