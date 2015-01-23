#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <ctime>

enum LogLevel
{
  LOG_NONE   = 0, // Log nothing
  LOG_ERROR  = 1, // Log errors only
  LOG_DEBUG  = 2, // Log all (debug + error)
  LOG_DEBUG2 = 3
};

inline const char* toString(LogLevel l)
{
  switch (l)
  {
	case LOG_ERROR:	return "ERROR";
	default:		return "DEBUG";
  }
}

class Output2FILE // implementation of OutputPolicy
{
public:
  static FILE*& stream();
  static void output(const std::string& msg);
};
inline FILE*& Output2FILE::stream()
{
  static FILE* pStream = stderr;
  return pStream;
}
inline void Output2FILE::output(const std::string& msg)
{
  FILE* pStream = stream();
  if (!pStream)
    return;

  fprintf(pStream, "%s", msg.c_str());
  fflush(pStream);
}

template <typename OutputPolicy>
class Log
{
public:
  Log()
  {
  }
  
  virtual ~Log()
  {
	OutputPolicy::output(os.str());
  }
  
  std::ostringstream& get(LogLevel level = LOG_DEBUG)
  {
	std::time_t time = std::time(nullptr);  
	os << "- " << time;
	os << " " << toString(level) << ": ";
	
	os << std::string(level > LOG_DEBUG ? level - LOG_DEBUG : 0, '\t');
	logLevel_ = level;
	return os;
  }
public:
  static LogLevel& reportingLevel()
  {
    static LogLevel level = LOG_DEBUG;
    return level;
  }
protected:
  std::ostringstream os;
private:
  Log(const Log&);
  Log& operator =(const Log&);
private:
  LogLevel logLevel_;
};

#define FILE_LOG(level) \
if (level > Log<Output2FILE>::reportingLevel() || !Output2FILE::stream()) ; \
else Log<Output2FILE>().get(level)

#endif
