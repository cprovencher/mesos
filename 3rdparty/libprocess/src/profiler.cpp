// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#include <stdio.h>
#include <string>
#include <unistd.h>

#include <glog/logging.h>

#ifdef ENABLE_GPERFTOOLS
#include <gperftools/profiler.h>
#endif

#include "process/future.hpp"
#include "process/help.hpp"
#include "process/http.hpp"
#include "process/profiler.hpp"

#include "stout/format.hpp"
#include "stout/option.hpp"
#include "stout/os.hpp"
#include "stout/os/strerror.hpp"

namespace process {

#ifdef ENABLE_GPERFTOOLS
namespace {
constexpr char PROFILE_FILE[] = "perftools.out";
} // namespace {
#endif

const std::string Profiler::START_HELP()
{
  return HELP(
    TLDR(
        "Starts profiling."),
    DESCRIPTION(
        "Starts profiling the current process by using 'perf record'."),
    AUTHENTICATION(true));
}


const std::string Profiler::STOP_HELP()
{
  return HELP(
    TLDR(
        "Stops profiling."),
    DESCRIPTION(
        "Stops perf recording and returns perf.data."),
    AUTHENTICATION(true));
}

Future<http::Response> Profiler::start(
    const http::Request& request,
    const Option<http::authentication::Principal>&)
{
#ifdef ENABLE_GPERFTOOLS
  const Option<std::string>
    enableProfiler = os::getenv("LIBPROCESS_ENABLE_PROFILER");
  if (enableProfiler.isNone() || enableProfiler.get() != "1") {
    return http::BadRequest(
        "The profiler is not enabled. To enable the profiler, libprocess "
        "must be started with LIBPROCESS_ENABLE_PROFILER=1 in the "
        "environment.\n");
  }

  if (started) {
    return http::BadRequest("Profiler already started.\n");
  }

  LOG(INFO) << "Starting Profiler";

  // get frequency from parameter in http request, default to 49 otherwise
  string frequency = "49";
  string pid = std::to_string(getpid());
  // don't use popen here, find another implementation that allows to kill it
  FILE* pipe = popen("perf record -a -p " + pid + " -F " + frequency, "r");

  if (!ProfilerStart(PROFILE_FILE)) {
    Try<std::string> error =
      strings::format("Failed to start profiler: %s", os::strerror(errno));
    LOG(ERROR) << error.get();
    return http::InternalServerError(error.get());
  }

  started = true;
  return http::OK("Profiler started.\n");
#else
  return http::BadRequest(
      "Perftools is disabled. To enable perftools, "
      "configure libprocess with --enable-perftools.\n");
#endif
}


Future<http::Response> Profiler::stop(
    const http::Request& request,
    const Option<http::authentication::Principal>&)
{
#ifdef ENABLE_GPERFTOOLS
  if (!started) {
    return http::BadRequest("Profiler not running.\n");
  }

  LOG(INFO) << "Stopping Profiler";

  // kill process started higher
  // return perf.data in http response by runnign perf script
  ProfilerStop();

  http::OK response;
  response.type = response.PATH;
  response.path = "perftools.out";
  response.headers["Content-Type"] = "application/octet-stream";
  response.headers["Content-Disposition"] =
    strings::format("attachment; filename=%s", PROFILE_FILE).get();

  started = false;
  return response;
#else
  return http::BadRequest(
      "Perftools is disabled. To enable perftools, "
      "configure libprocess with --enable-perftools.\n");
#endif
}

} // namespace process {
