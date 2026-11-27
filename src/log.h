#pragma once

// A small logging shim with the usual LOG/CHECK macro syntax, so the renderer
// core links no logging library and the embedding program decides where
// messages go.
//
//   LOG(INFO) << "text";            // also WARNING, ERROR, FATAL
//   LOG_EVERY_N(ERROR, 100) << ...;  // fires on the 1st, N+1th, 2N+1th call
//   CHECK(cond) << "context";        // also CHECK_EQ/NE/LT/LE/GT/GE
//   DLOG(INFO) << ...;               // DLOG and DCHECK are compiled out (not
//   DCHECK(cond);                    // evaluated) under NDEBUG
//
// Messages go to the installed LogSink. A failed CHECK or LOG(FATAL) calls the
// sink's fatal handler and never returns: if the handler returns, the process
// aborts. With no sink installed (or an empty callback), messages go to stderr
// as "<I|W|E|F> file:line] message" and fatal aborts.
//
// The macros are global; the implementation is in namespace sh_renderer.

#include <atomic>
#include <functional>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace sh_renderer {

enum class LogSeverity { kInfo, kWarning, kError, kFatal };

// Where log messages go. `file` is the source file's base name.
struct LogSink {
  // Receives INFO, WARNING and ERROR messages.
  std::function<void(LogSeverity severity, const char* file, int line,
                     const std::string& message)>
      message;
  // Receives FATAL messages (failed checks included). It should not return;
  // if it does, the process aborts.
  std::function<void(const char* file, int line, const std::string& message)>
      fatal;
};

// Installs `sink` for all subsequent messages and returns the previous one.
// An empty callback selects the default stderr behaviour for that part.
LogSink SetLogSink(LogSink sink);

namespace log_internal {

// Builds one INFO/WARNING/ERROR message and hands it to the sink when
// destroyed.
class LogMessage {
 public:
  LogMessage(const char* file, int line, LogSeverity severity);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  ~LogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  int line_;
  LogSeverity severity_;
  std::ostringstream stream_;
};

// Builds one FATAL message; its destructor calls the fatal handler and never
// returns.
class LogMessageFatal {
 public:
  LogMessageFatal(const char* file, int line);
  LogMessageFatal(const char* file, int line, const std::string& prefix);
  LogMessageFatal(const LogMessageFatal&) = delete;
  LogMessageFatal& operator=(const LogMessageFatal&) = delete;
  [[noreturn]] ~LogMessageFatal();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  int line_;
  std::ostringstream stream_;
};

// Counts the calls of one LOG_EVERY_N site.
class EveryN {
 public:
  // True on the 1st, n+1th, 2n+1th, ... call.
  bool Tick(int n) {
    const int count = count_.fetch_add(1, std::memory_order_relaxed);
    return n <= 1 || count % n == 0;
  }

 private:
  std::atomic<int> count_{0};
};

template <typename A, typename B>
std::unique_ptr<std::string> MakeCheckOpString(const A& a, const B& b,
                                               const char* expression) {
  std::ostringstream os;
  os << "Check failed: " << expression << " (" << a << " vs. " << b << ") ";
  return std::make_unique<std::string>(os.str());
}

#define SH_LOG_DEFINE_CHECK_OP_IMPL(name, op)                              \
  template <typename A, typename B>                                        \
  std::unique_ptr<std::string> Check##name##Impl(const A& a, const B& b,   \
                                                 const char* expression) { \
    if (a op b) [[likely]]                                                 \
      return nullptr;                                                      \
    return MakeCheckOpString(a, b, expression);                            \
  }
SH_LOG_DEFINE_CHECK_OP_IMPL(EQ, ==)
SH_LOG_DEFINE_CHECK_OP_IMPL(NE, !=)
SH_LOG_DEFINE_CHECK_OP_IMPL(LT, <)
SH_LOG_DEFINE_CHECK_OP_IMPL(LE, <=)
SH_LOG_DEFINE_CHECK_OP_IMPL(GT, >)
SH_LOG_DEFINE_CHECK_OP_IMPL(GE, >=)
#undef SH_LOG_DEFINE_CHECK_OP_IMPL

}  // namespace log_internal
}  // namespace sh_renderer

#define SH_LOG_INFO                                           \
  ::sh_renderer::log_internal::LogMessage(__FILE__, __LINE__, \
                                          ::sh_renderer::LogSeverity::kInfo)
#define SH_LOG_WARNING                     \
  ::sh_renderer::log_internal::LogMessage( \
      __FILE__, __LINE__, ::sh_renderer::LogSeverity::kWarning)
#define SH_LOG_ERROR                                          \
  ::sh_renderer::log_internal::LogMessage(__FILE__, __LINE__, \
                                          ::sh_renderer::LogSeverity::kError)
#define SH_LOG_FATAL \
  ::sh_renderer::log_internal::LogMessageFatal(__FILE__, __LINE__)

#define LOG(severity) SH_LOG_##severity.stream()

#define SH_LOG_CONCAT_INNER(a, b) a##b
#define SH_LOG_CONCAT(a, b) SH_LOG_CONCAT_INNER(a, b)

#define LOG_EVERY_N(severity, n)                                \
  if (static ::sh_renderer::log_internal::EveryN SH_LOG_CONCAT( \
          sh_log_every_n_, __LINE__);                           \
      !SH_LOG_CONCAT(sh_log_every_n_, __LINE__).Tick(n)) {      \
  } else                                                        \
    LOG(severity)

#define CHECK(condition)                                   \
  while (!(condition))                                     \
  ::sh_renderer::log_internal::LogMessageFatal(            \
      __FILE__, __LINE__, "Check failed: " #condition " ") \
      .stream()

#define SH_CHECK_OP(name, op, a, b)                                  \
  while (std::unique_ptr<std::string> sh_check_op_result_ =          \
             ::sh_renderer::log_internal::Check##name##Impl(         \
                 (a), (b), #a " " #op " " #b))                       \
  ::sh_renderer::log_internal::LogMessageFatal(__FILE__, __LINE__,   \
                                               *sh_check_op_result_) \
      .stream()

#define CHECK_EQ(a, b) SH_CHECK_OP(EQ, ==, a, b)
#define CHECK_NE(a, b) SH_CHECK_OP(NE, !=, a, b)
#define CHECK_LT(a, b) SH_CHECK_OP(LT, <, a, b)
#define CHECK_LE(a, b) SH_CHECK_OP(LE, <=, a, b)
#define CHECK_GT(a, b) SH_CHECK_OP(GT, >, a, b)
#define CHECK_GE(a, b) SH_CHECK_OP(GE, >=, a, b)

#ifdef NDEBUG
// Compiled but never evaluated.
#define DLOG(severity) \
  while (false) LOG(severity)
#define DCHECK(condition) \
  while (false) CHECK(condition)
#else
#define DLOG(severity) LOG(severity)
#define DCHECK(condition) CHECK(condition)
#endif
