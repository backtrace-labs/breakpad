#ifndef CLIENT_LINUX_HANDLER_BACKTRACE_HANDLER_H_
#define CLIENT_LINUX_HANDLER_BACKTRACE_HANDLER_H_

#include <map>
#include <string>

#include "common/using_std_string.h"

namespace google_breakpad {

class BacktraceHandler {
 public:
  // Initialize BacktraceHandler.
  static bool Init(const string& url, const string& token,
                   const std::map<string, string>& attributes);

  // Add or Replace an attribute.
  //
  // Return true if attribute was added, otherwise false.
  static bool SetOrReplaceAttribute(const string& key, const string& val);

  // Remove an attribute.
  //
  // Return true if attribute was found and removed, otherwise false.
  static bool RemoveAttribute(const string& key);
};

}  // namespace google_breakpad

#endif  // CLIENT_LINUX_HANDLER_BACKTRACE_HANDLER_H_
