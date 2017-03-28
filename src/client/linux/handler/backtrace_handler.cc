#include <atomic>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client/linux/handler/exception_handler.h"
#include "common/linux/libcurl_wrapper.h"
#include "common/scoped_ptr.h"

#include "backtrace_handler.h"

namespace google_breakpad {

namespace {
const string kDefaultDescriptorPath = "/tmp";
};

class BacktraceHandlerContext {
 public:
  BacktraceHandlerContext(const string& url, const string& token,
                          const std::map<string, string>& attributes);

  string url_;
  string token_;
  std::map<string, string> attributes_;

  scoped_ptr<LibcurlWrapper> http_layer_;

  MinidumpDescriptor descriptor_;
  ExceptionHandler handler_;

  static bool MinidumpCallback(
      const google_breakpad::MinidumpDescriptor& descriptor, void* context,
      bool succeeded);
};

/* Global shared context */
/* FIXME: protect with mutex */
BacktraceHandlerContext* ctx_ = nullptr;

BacktraceHandlerContext::BacktraceHandlerContext(
    const string& url, const string& token,
    const std::map<string, string>& attributes)
    : url_(url),
      token_(token),
      attributes_(attributes),
      http_layer_(new LibcurlWrapper()),
      descriptor_(kDefaultDescriptorPath),
      handler_(descriptor_, NULL, MinidumpCallback, NULL, true, -1) {}

bool isSuccessfulHttpCode(int code) { return (200 <= code && code < 300); }

bool BacktraceHandlerContext::MinidumpCallback(
    const google_breakpad::MinidumpDescriptor& descriptor, void* context,
    bool succeeded) {
  if (ctx_ == nullptr) return false;

  if (succeeded) {
    /* FIXME: http_layer calls dlopen("curl.so"), curl is a hidden dep. */
    if (!ctx_->http_layer_->Init()) std::cerr << "http layer init failed\n";

    string minidump_pathname = descriptor.path();
    struct stat st;
    if (stat(minidump_pathname.c_str(), &st)) {
      std::cerr << minidump_pathname << " could not be found";
      return false;
    }

    /* FIXME: properly parse url and adjust query string sanely */
    /* FIXME: attributes are sent via form-data, coronerd handle this? */
    std::string url =
        ctx_->url_ + "/post?format=minidump&token=" + ctx_->token_;

    if (!ctx_->http_layer_->AddFile(minidump_pathname,
                                    "upload_file_minidump")) {
      return false;
    }

    int http_status_code;
    string http_response_header;
    string http_response_body;
    bool send_success = ctx_->http_layer_->SendRequest(
        url, ctx_->attributes_, &http_status_code, &http_response_header,
        &http_response_body);

    if (!send_success || !isSuccessfulHttpCode(http_status_code)) {
      std::cerr << "Failed to send dump to " << url << "\n Received error code "
                << http_status_code << " with request:\n\n"
                << http_response_header << "\n"
                << http_response_body;
    }

    return send_success;
  }

  return succeeded;
}

bool BacktraceHandler::Init(const string& url, const string& token,
                            const std::map<string, string>& attributes) {
  if (ctx_ != nullptr) return false;

  ctx_ = new BacktraceHandlerContext(url, token, attributes);

  return true;
}

/* FIXME: implement SetOrReplaceAttribute and RemoveAttribute */
}
