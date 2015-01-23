#ifndef MIME_INFERENCE_H
#define MIME_INFERENCE_H

#include <map>
#include <iostream>
#include <string>
#include <cstring>

#include <unistd.h>

#include "ndnfs.h"

// pass the comparison method to map, since we are storing char *, the default
// comparison would be comparing two pointers.
struct str_cmp
{
  bool operator()(const char *a, const char *b) const
  {
    // comparison function expects a true on less than, and false if otherwise
    return std::strcmp(a, b) < 0;
  }
};

extern std::map<const char *, const char *, str_cmp> ext_mime_map;

// store a map between ext and mime_type in memory when fs launches
int initialize_ext_mime_map();

// inference function based on in-memory map
int mime_infer(char *mime_type, const char *path);

#endif