#include "log.h"

#include <gtest/gtest.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace sh_renderer {
namespace {

struct Captured {
  LogSeverity severity;
  std::string file;
  int line;
  std::string message;
};

class LogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Other tests in this binary create GL contexts (and driver threads), so
    // death tests re-execute the binary instead of forking it.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    previous_ = SetLogSink(LogSink{});
  }
  void TearDown() override { SetLogSink(previous_); }

  // Installs a sink that records INFO/WARNING/ERROR messages.
  void InstallCapturingSink() {
    LogSink sink;
    sink.message = [this](LogSeverity severity, const char* file, int line,
                          const std::string& message) {
      captured_.push_back({severity, file, line, message});
    };
    SetLogSink(sink);
  }

  std::vector<Captured> captured_;

 private:
  LogSink previous_;
};

TEST_F(LogTest, MessagesReachInstalledSinkWithSeverityFileAndLine) {
  InstallCapturingSink();
  const int info_line = __LINE__ + 1;
  LOG(INFO) << "info " << 1;
  const int warning_line = __LINE__ + 1;
  LOG(WARNING) << "warning " << 2.5f;
  const int error_line = __LINE__ + 1;
  LOG(ERROR) << "error " << std::string("three");

  ASSERT_EQ(captured_.size(), 3u);
  EXPECT_EQ(captured_[0].severity, LogSeverity::kInfo);
  EXPECT_EQ(captured_[0].message, "info 1");
  EXPECT_EQ(captured_[0].line, info_line);
  EXPECT_EQ(captured_[1].severity, LogSeverity::kWarning);
  EXPECT_EQ(captured_[1].message, "warning 2.5");
  EXPECT_EQ(captured_[1].line, warning_line);
  EXPECT_EQ(captured_[2].severity, LogSeverity::kError);
  EXPECT_EQ(captured_[2].message, "error three");
  EXPECT_EQ(captured_[2].line, error_line);
  for (const Captured& c : captured_) EXPECT_EQ(c.file, "log_test.cpp");
}

TEST_F(LogTest, PassingChecksDoNotLog) {
  InstallCapturingSink();
  int evaluations = 0;
  CHECK(++evaluations == 1) << "not streamed";
  CHECK_EQ(++evaluations, 2);
  CHECK_NE(1, 2);
  CHECK_LT(1, 2);
  CHECK_LE(2, 2);
  CHECK_GT(3, 2);
  CHECK_GE(3, 3);
  EXPECT_EQ(evaluations, 2);
  EXPECT_TRUE(captured_.empty());
}

// Installs a fatal handler that reports and exits the way ri.Error does.
void InstallExitingFatalHandler() {
  LogSink sink;
  sink.fatal = [](const char* file, int line, const std::string& message) {
    std::fprintf(stderr, "fatal handler: %s:%d: %s\n", file, line,
                 message.c_str());
    std::fflush(stderr);
    std::_Exit(3);
  };
  SetLogSink(sink);
}

TEST_F(LogTest, CheckFailureCallsFatalHandler) {
  EXPECT_EXIT(
      {
        InstallExitingFatalHandler();
        CHECK(1 + 1 == 3) << "context " << 42;
      },
      ::testing::ExitedWithCode(3),
      "fatal handler: log_test.cpp:[0-9]+: Check failed: 1 \\+ 1 == 3 "
      "context 42");
}

TEST_F(LogTest, CheckOpFailureReportsBothValues) {
  EXPECT_EXIT(
      {
        InstallExitingFatalHandler();
        int width = 0;
        CHECK_GT(width, 0) << "width";
      },
      ::testing::ExitedWithCode(3),
      "fatal handler: log_test.cpp:[0-9]+: Check failed: width > 0 \\(0 vs. "
      "0\\) width");
}

TEST_F(LogTest, LogFatalCallsFatalHandler) {
  EXPECT_EXIT(
      {
        InstallExitingFatalHandler();
        LOG(FATAL) << "fatal " << 7;
      },
      ::testing::ExitedWithCode(3),
      "fatal handler: log_test.cpp:[0-9]+: fatal 7");
}

TEST_F(LogTest, FatalNeverReturnsWhenHandlerReturns) {
  // The handler returns, so the shim must abort rather than continue past the
  // failed check.
  EXPECT_EXIT(
      {
        LogSink sink;
        sink.fatal = [](const char*, int, const std::string& message) {
          std::fprintf(stderr, "handler returned: %s\n", message.c_str());
        };
        SetLogSink(sink);
        CHECK(false) << "boom";
        std::fprintf(stderr, "continued past the check\n");
      },
      ::testing::KilledBySignal(SIGABRT), "handler returned: .*boom");
}

TEST_F(LogTest, DefaultSinkWritesToStderrAndAborts) {
  EXPECT_EXIT(
      {
        LOG(WARNING) << "first a warning";
        CHECK(false) << "then a failed check";
      },
      ::testing::KilledBySignal(SIGABRT),
      "W log_test.cpp:[0-9]+\\] first a warning.*"
      "F log_test.cpp:[0-9]+\\] Check failed: false then a failed check");
}

TEST_F(LogTest, LogEveryNFiresOnFirstAndEveryNthPlusOneCall) {
  InstallCapturingSink();
  for (int i = 1; i <= 10; ++i) {
    LOG_EVERY_N(ERROR, 4) << "call " << i;
  }
  ASSERT_EQ(captured_.size(), 3u);
  EXPECT_EQ(captured_[0].message, "call 1");
  EXPECT_EQ(captured_[1].message, "call 5");
  EXPECT_EQ(captured_[2].message, "call 9");
  EXPECT_EQ(captured_[0].severity, LogSeverity::kError);
}

TEST_F(LogTest, CheckOpEvaluatesEachOperandOnceWhenPassing) {
  InstallCapturingSink();
  int a_evaluations = 0;
  int b_evaluations = 0;
  auto a = [&] { return ++a_evaluations; };
  auto b = [&] { return ++b_evaluations; };
  CHECK_EQ(a(), b());
  CHECK_NE(a(), b() + 10);
  CHECK_LT(a(), b() + 1);
  CHECK_LE(a(), b());
  CHECK_GT(a() + 1, b());
  CHECK_GE(a(), b());
  EXPECT_EQ(a_evaluations, 6);
  EXPECT_EQ(b_evaluations, 6);
  EXPECT_TRUE(captured_.empty());
}

TEST_F(LogTest, CheckOpEvaluatesEachOperandOnceWhenFailing) {
  // The failure message prints the values computed for the comparison, so a
  // second evaluation would show up as 2 (or 4) in the message.
  EXPECT_EXIT(
      {
        InstallExitingFatalHandler();
        int a_evaluations = 0;
        int b_evaluations = 0;
        CHECK_EQ(++a_evaluations, (b_evaluations += 3))
            << "a=" << a_evaluations << " b=" << b_evaluations;
      },
      ::testing::ExitedWithCode(3),
      "Check failed: \\+\\+a_evaluations == \\(b_evaluations \\+= 3\\) "
      "\\(1 vs. 3\\) a=1 b=3");
}

TEST_F(LogTest, CheckIsDanglingElseSafe) {
  InstallCapturingSink();
  // Each `else` must bind to the `if` written here, not to a statement inside
  // the macro. With a bare `if` in the macro, the passing branch would skip
  // the else branch (or the failing CHECK would run it).
  int then_branch = 0;
  int else_branch = 0;
  for (int i = 0; i < 2; ++i) {
    if (i == 0)
      CHECK(true) << "never streamed";
    else
      ++else_branch;
    if (i == 1)
      CHECK_EQ(i, 1) << "never streamed";
    else
      ++then_branch;
  }
  EXPECT_EQ(else_branch, 1);
  EXPECT_EQ(then_branch, 1);
  EXPECT_TRUE(captured_.empty());
}

TEST_F(LogTest, LogEveryNIsDanglingElseSafe) {
  InstallCapturingSink();
  int else_branch = 0;
  for (int i = 0; i < 4; ++i) {
    if (i % 2 == 0)
      LOG_EVERY_N(WARNING, 1) << "even " << i;
    else
      ++else_branch;
  }
  EXPECT_EQ(else_branch, 2);
  ASSERT_EQ(captured_.size(), 2u);
  EXPECT_EQ(captured_[0].message, "even 0");
  EXPECT_EQ(captured_[1].message, "even 2");
}

TEST_F(LogTest, DcheckEvaluatesOnlyInDebugBuilds) {
  InstallCapturingSink();
  int evaluations = 0;
  DCHECK(++evaluations > 0);
#ifdef NDEBUG
  EXPECT_EQ(evaluations, 0);
#else
  EXPECT_EQ(evaluations, 1);
#endif
}

TEST_F(LogTest, DlogEvaluatesOnlyInDebugBuilds) {
  InstallCapturingSink();
  int evaluations = 0;
  DLOG(WARNING) << "debug only " << ++evaluations;
#ifdef NDEBUG
  EXPECT_EQ(evaluations, 0);
  EXPECT_TRUE(captured_.empty());
#else
  EXPECT_EQ(evaluations, 1);
  ASSERT_EQ(captured_.size(), 1u);
  EXPECT_EQ(captured_[0].message, "debug only 1");
#endif
}

}  // namespace
}  // namespace sh_renderer
