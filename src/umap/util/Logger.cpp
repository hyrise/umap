//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/util/Logger.hpp"

#include <stdlib.h>   // for getenv()
#include <strings.h>  // for strcasecmp()
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>

namespace Umap {

static const char* env_name = "UMAP_LOG_LEVEL";
static const char* env_name_no_timestamp = "UMAP_LOG_NO_TIMESTAMP_LEVEL";
static message::Level defaultLevel = message::Info;
Logger* Logger::s_Logger = nullptr;

static const std::string MessageLevelName[ message::Num_Levels ] = {
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG"
};

Logger::Logger(bool log_with_timestamp) noexcept
  : m_log_timestamp(log_with_timestamp), m_out(&std::cout)
{
  // by default, all message streams are disabled
  for ( int i=0 ; i < message::Num_Levels ; ++i )
    m_isEnabled[ i ] = false;
}

Logger::~Logger() noexcept
{
}

void Logger::setOutputStream(std::ostream* output_stream) noexcept
{
  m_out = output_stream;
}

void Logger::setLoggingMsgLevel( message::Level level ) noexcept
{
  for ( int i=0 ; i < message::Num_Levels ; ++i )
    m_isEnabled[ i ] = (i<= level) ? true : false;
}

void Logger::logMessage( message::Level level,
                         const std::string& message,
                         const std::string& fileName,
                         int line ) noexcept
{
  if ( !logLevelEnabled( level ) )
    return;   /* short-circuit */

  if (!m_log_timestamp) {
    std::lock_guard<std::mutex> guard(g_logging_mutex);
    *m_out
      << message
      << std::endl;
    return;
  }

  std::ostringstream ss;
  ss << std::fixed << std::setprecision(3) << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
  ss << ":" << getpid() << ":"
     << syscall(__NR_gettid) << " "
     << "[" << MessageLevelName[ level ] << "]"
     << "[" << fileName  << ":" << line << "]:"
     << message;
  std::lock_guard<std::mutex> guard(g_logging_mutex);
  *m_out << ss.str() << std::endl;;
}

void Logger::initialize()
{
  if ( s_Logger != nullptr )
    return;

  message::Level level = defaultLevel;
  char* enval = getenv(env_name);
  char* enval_no_timestamp = getenv(env_name_no_timestamp);
  bool log_with_timestamp = true;

  if ( enval != NULL || enval_no_timestamp != NULL ) {

    if (enval_no_timestamp != NULL) {
      enval = enval_no_timestamp;
      log_with_timestamp = false;
    }

    bool level_found = false;
    for ( int i = 0; i < message::Num_Levels; ++i ) {
      if ( strcasecmp( enval, MessageLevelName[ i ].c_str() ) == 0 ) {
        level_found = true;
        level = (message::Level)i;
        break;
      }
    }
    if (! level_found ) {
      std::cerr << "No matching logging levels for: " << enval << "\n";
      std::cerr << "Available levels are:\n";
      for ( int i = 0; i < message::Num_Levels; ++i ) {
        std::cerr << "\t" << MessageLevelName[ i ] << "\n";
      }
    }
  }

  s_Logger = new Logger(log_with_timestamp);
  s_Logger->setLoggingMsgLevel(level);
}

void Logger::finalize()
{
  delete s_Logger;
  s_Logger = nullptr;
}

Logger* Logger::getActiveLogger()
{
  if ( s_Logger == nullptr )
    Logger::initialize();

  return s_Logger;
}

} /* namespace umap */
