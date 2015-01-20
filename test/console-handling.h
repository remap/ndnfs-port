#ifndef CONSOLE_HANDLING_H
#define CONSOLE_HANDLING_H

#include <unistd.h>

#include <poll.h>
#include <sstream>
#include <vector>
#include <string>

#define INPUT_BUFFER_SIZE 1000

// polling functions copied from test-chrono-chat.cpp
/**
 * Read a line from from stdin and return a trimmed string.  (We don't use
 * cin because it ignores a blank line.)
 */
static const char *WHITESPACE_CHARS = " \n\r\t";

/**
 * Modify str in place to erase whitespace on the left.
 * @param str
 */
static inline void
trimLeft(std::string& str)
{
  size_t found = str.find_first_not_of(WHITESPACE_CHARS);
  if (found != std::string::npos) {
    if (found > 0)
      str.erase(0, found);
  }
  else
    // All whitespace
    str.clear();
}

/**
 * Modify str in place to erase whitespace on the right.
 * @param str
 */
static inline void
trimRight(std::string& str)
{
  size_t found = str.find_last_not_of(WHITESPACE_CHARS);
  if (found != std::string::npos) {
    if (found + 1 < str.size())
      str.erase(found + 1);
  }
  else
    // All whitespace
    str.clear();
}

/**
 * Modify str in place to erase whitespace on the left and right.
 * @param str
 */
static void
trim(std::string& str)
{
  trimLeft(str);
  trimRight(str);
}
 
static std::string
stdinReadLine()
{
  char inputBuffer[INPUT_BUFFER_SIZE];
  ssize_t nBytes = ::read(STDIN_FILENO, inputBuffer, sizeof(inputBuffer) - 1);
    
  inputBuffer[nBytes] = 0;
  std::string input(inputBuffer);
  trim(input);

  return input;
}

/**
 * Poll stdin and return true if it is ready to ready (e.g. from stdinReadLine).
 */
static bool
isStdinReady()
{
  struct pollfd pollInfo;
  pollInfo.fd = STDIN_FILENO;
  pollInfo.events = POLLIN;

  return poll(&pollInfo, 1, 0) > 0;
}

std::vector<std::string> 
&split(const std::string &s, char delim, std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
	  elems.push_back(item);
  }
  return elems;
}

std::vector<std::string> 
split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, elems);
  return elems;
}

#endif