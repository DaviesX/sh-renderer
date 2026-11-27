#include "log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <utility>

namespace sh_renderer {
namespace {

std::mutex& SinkMutex() {
  static std::mutex mutex;
  return mutex;
}

LogSink& InstalledSink() {
  static LogSink sink;
  return sink;
}

LogSink CurrentSink() {
  std::lock_guard<std::mutex> lock(SinkMutex());
  return InstalledSink();
}

const char* BaseName(const char* path) {
  const char* slash = std::strrchr(path, '/');
  return slash != nullptr ? slash + 1 : path;
}

char SeverityLetter(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kInfo:
      return 'I';
    case LogSeverity::kWarning:
      return 'W';
    case LogSeverity::kError:
      return 'E';
    case LogSeverity::kFatal:
      return 'F';
  }
  return '?';
}

void WriteToStderr(LogSeverity severity, const char* file, int line,
                   const std::string& message) {
  std::fprintf(stderr, "%c %s:%d] %s\n", SeverityLetter(severity), file, line,
               message.c_str());
  std::fflush(stderr);
}

}  // namespace

LogSink SetLogSink(LogSink sink) {
  std::lock_guard<std::mutex> lock(SinkMutex());
  return std::exchange(InstalledSink(), std::move(sink));
}

namespace log_internal {

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : file_(BaseName(file)), line_(line), severity_(severity) {}

LogMessage::~LogMessage() {
  const LogSink sink = CurrentSink();
  if (sink.message) {
    sink.message(severity_, file_, line_, stream_.str());
  } else {
    WriteToStderr(severity_, file_, line_, stream_.str());
  }
}

LogMessageFatal::LogMessageFatal(const char* file, int line)
    : file_(BaseName(file)), line_(line) {}

LogMessageFatal::LogMessageFatal(const char* file, int line,
                                 const std::string& prefix)
    : LogMessageFatal(file, line) {
  stream_ << prefix;
}

LogMessageFatal::~LogMessageFatal() {
  const LogSink sink = CurrentSink();
  if (sink.fatal) {
    sink.fatal(file_, line_, stream_.str());
  } else {
    WriteToStderr(LogSeverity::kFatal, file_, line_, stream_.str());
  }
  std::abort();
}

}  // namespace log_internal
}  // namespace sh_renderer
