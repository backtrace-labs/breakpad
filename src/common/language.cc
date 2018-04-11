// Copyright (c) 2010 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// language.cc: Subclasses and singletons for google_breakpad::Language.
// See language.h for details.

#include "common/language.h"

#include <stdlib.h>

#if !defined(__ANDROID__)
#include <cxxabi.h>
#endif

#if defined(HAVE_RUST_DEMANGLE)
#include <rust_demangle.h>
#endif

#include <limits>
#include <regex>
#include <unordered_map>
#include <sstream>

namespace {

string MakeQualifiedNameWithSeparator(const string& parent_name,
                                      const char* separator,
                                      const string& name) {
  if (parent_name.empty()) {
    return name;
  }

  return parent_name + separator + name;
}

}  // namespace

namespace google_breakpad {

// C++ language-specific operations.
class CPPLanguage: public Language {
 public:
  CPPLanguage() {}

  string MakeQualifiedName(const string &parent_name,
                           const string &name) const {
    return MakeQualifiedNameWithSeparator(parent_name, "::", name);
  }

  virtual DemangleResult DemangleName(const string& mangled,
                                      string* demangled) const {
#if defined(__ANDROID__)
    // Android NDK doesn't provide abi::__cxa_demangle.
    demangled->clear();
    return kDontDemangle;
#else
    int status;
    char* demangled_c =
        abi::__cxa_demangle(mangled.c_str(), NULL, NULL, &status);

    DemangleResult result;
    if (status == 0) {
      result = kDemangleSuccess;
      demangled->assign(demangled_c);
    } else {
      result = kDemangleFailure;
      demangled->clear();
    }

    if (demangled_c) {
      free(reinterpret_cast<void*>(demangled_c));
    }

    return result;
#endif
  }
};

CPPLanguage CPPLanguageSingleton;

// Java language-specific operations.
class JavaLanguage: public Language {
 public:
  JavaLanguage() {}

  string MakeQualifiedName(const string &parent_name,
                           const string &name) const {
    return MakeQualifiedNameWithSeparator(parent_name, ".", name);
  }
};

JavaLanguage JavaLanguageSingleton;

// Swift language-specific operations.
class SwiftLanguage: public Language {
 public:
  SwiftLanguage() {}

  string MakeQualifiedName(const string &parent_name,
                           const string &name) const {
    return MakeQualifiedNameWithSeparator(parent_name, ".", name);
  }

  virtual DemangleResult DemangleName(const string& mangled,
                                      string* demangled) const {
    // There is no programmatic interface to a Swift demangler. Pass through the
    // mangled form because it encodes more information than the qualified name
    // that would have been built by MakeQualifiedName(). The output can be
    // post-processed by xcrun swift-demangle to transform mangled Swift names
    // into something more readable.
    demangled->assign(mangled);
    return kDemangleSuccess;
  }
};

SwiftLanguage SwiftLanguageSingleton;

static bool rust_replace_dollar(string::const_iterator it, const string::const_iterator eit,
				std::ostringstream& o) {
  static const std::unordered_map<string, char> map{
    { "C",   ',' },

    { "SP",  '@'  },
    { "BP",  '*'  },
    { "RF",  '&'  },
    { "LT",  '<'  },
    { "GT",  '>'  },
    { "LP",  '('  },
    { "RP",  ')'  },

    { "u20", ' '  },
    { "u22", '\\' },
    { "u27", '\'' },
    { "u2b", '+'  },

    { "u3b", ';'  },

    { "u5b", '['  },
    { "u5d", ']'  },

    { "u7b", '{'  },
    { "u7d", '}'  },
    { "u7e", '~'  },
  };

  const string::const_iterator e = std::find(it, eit, '$');
  if (e == eit)
    return false;

  string l{it, e};
  auto c = map.find(l);
  if (c == map.end())
    return false;

  o << c->second;

  return true;
}

static bool rust_scan_replace(const string& s, std::ostringstream& o) {
  string::const_iterator it = begin(s);
  const string::const_iterator eit = end(s);

  while (it != eit) {
    switch (*it) {
    case '_':
      // TODO
      break;
    case '$':
      advance(it, 1);
      if (it == eit)
	goto fail;
      if (!rust_replace_dollar(it, eit, o))
	goto fail;
      break;
    default:
      o << *it;
    }
    advance(it, 1);
  }
  return true;
fail:
  return false;
}

// Rust language-specific operations.
class RustLanguage: public Language {
 public:
  RustLanguage() {}

  string MakeQualifiedName(const string &parent_name,
                           const string &name) const {
    return MakeQualifiedNameWithSeparator(parent_name, ".", name);
  }

  virtual DemangleResult DemangleName(const string& mangled,
                                      string* demangled) const {
    // Rust names use GCC C++ name mangling, but demangling them with
    // abi_demangle doesn't produce stellar results due to them having
    // another layer of encoding.
    // If callers provide rustc-demangle, use that.
#if defined(HAVE_RUST_DEMANGLE)
    char* rust_demangled = rust_demangle(mangled.c_str());
    if (rust_demangled == nullptr) {
      return kDemangleFailure;
    }
    demangled->assign(rust_demangled);
    free_rust_demangled_name(rust_demangled);
    return kDemangleSuccess;
#else
    static std::regex re{"(^[a-zA-Z0-9_.:$]+)::h([a-f0-9]{16})$",
		         std::regex_constants::ECMAScript|
		         std::regex_constants::optimize};
    std::smatch sm;

    demangled->clear();
    int status;
    char *cpp_demangled = abi::__cxa_demangle(mangled.c_str(), NULL, NULL, &status);
    if (status != 0)
	    return kDemangleFailure;
    string rd{cpp_demangled};
    free(cpp_demangled);
    cpp_demangled = nullptr;

    if (!regex_match(rd, sm, re))
	    return kDemangleFailure;

    std::ostringstream o;
    if (!rust_scan_replace(sm.str(1), o))
	    return kDemangleFailure;

    *demangled = o.str();
    return kDemangleSuccess;
#endif
  }
};

RustLanguage RustLanguageSingleton;

// Assembler language-specific operations.
class AssemblerLanguage: public Language {
 public:
  AssemblerLanguage() {}

  bool HasFunctions() const { return false; }
  string MakeQualifiedName(const string &parent_name,
                           const string &name) const {
    return name;
  }
};

AssemblerLanguage AssemblerLanguageSingleton;

const Language * const Language::CPlusPlus = &CPPLanguageSingleton;
const Language * const Language::Java = &JavaLanguageSingleton;
const Language * const Language::Swift = &SwiftLanguageSingleton;
const Language * const Language::Rust = &RustLanguageSingleton;
const Language * const Language::Assembler = &AssemblerLanguageSingleton;

} // namespace google_breakpad
