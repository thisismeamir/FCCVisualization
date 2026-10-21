// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.5: help generated from the command table and the registries.
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "BackendRegistry.h"
#include "GoldenText.h"
#include "fccvis/api/Expression.h"
#include "fccvis/api/Interpreter.h"

namespace api = fccvis::api;
using fccvis::session::Session;
using namespace fccvis::test;

TEST_CASE("Help is generated from the command table", "[api][help]") {
  Session s("help");
  api::Interpreter in(s);
  const auto overview = in.Help();
  for (const auto& f : api::Interpreter::Forms()) {
    INFO(f.usage);
    CHECK(overview.find(f.usage.substr(0, 40)) != std::string::npos);
    CHECK(overview.find(f.summary) != std::string::npos);
    CHECK(in.Help(f.words[0]).find(f.usage) != std::string::npos);
  }
  const auto golden = CompareGoldenText(overview, "shell_help.txt");
  INFO(golden.message);
  CHECK(golden.ok);

  // help through the interpreter prints the same text
  const auto reply = in.Execute("help scene");
  std::string text;
  for (const auto& l : reply.lines) text += l + "\n";
  CHECK(text == in.Help("scene"));
  CHECK(in.Help("bogus").rfind("unknown help topic 'bogus'", 0) == 0);
}

TEST_CASE("Adapter and backend help comes from their capabilities", "[api][help]") {
  Session s("help");
  api::Interpreter in(s);
  for (const auto& n : fccvis::adapters::Registry::Global().Names()) {
    INFO(n);
    CHECK(in.Help("adapter " + n) == fccvis::adapters::Registry::Global().Help(n));
    CHECK(in.Help("adapters").find(fccvis::adapters::Registry::Global().Help(n)) != std::string::npos);
  }
  for (const auto& n : fccvis::backend::Registry::Global().Names()) {
    if (n.rfind("test-", 0) == 0) continue;
    INFO(n);
    const auto h = fccvis::backend::Registry::Global().Help(n);
    CHECK(in.Help("backend " + n) == h);
    CHECK(in.Help("backends").find(h) != std::string::npos);
    const auto caps = fccvis::backend::Registry::Global().Create(n)->GetCapabilities();
    const auto ref = api::Interpreter::Reference("backends");
    if (!fccvis::backend::Registry::Global().AliasOf(n).empty()) {
      CHECK(ref.find("Also registered as `" + n + "`") != std::string::npos);
      continue;
    }
    CHECK(ref.find("### " + n + "\n\n" + caps.description + "\n") != std::string::npos);
    for (const auto& [o, d] : caps.options) CHECK(ref.find("| `" + o + "` |") != std::string::npos);
  }
  CHECK(in.Help("adapter nope").find("unknown adapter 'nope'") == 0);
  for (const char* kind : {"point", "line", "surface", "environment"})
    for (const auto& key : api::StyleKeys(kind)) CHECK(in.Help("styles").find(key) != std::string::npos);
}

TEST_CASE("The Markdown reference lists every command", "[api][help]") {
  const auto ref = api::Interpreter::Reference("commands");
  for (const auto& f : api::Interpreter::Forms()) {
    std::string escaped;
    for (char c : f.usage) escaped += c == '|' ? std::string("\\|") : std::string(1, c);
    INFO(f.usage);
    CHECK(ref.find("| `" + escaped + "` |") != std::string::npos);
  }
  CHECK(api::Interpreter::Reference().find("## Adapters") != std::string::npos);
  CHECK(ref.find("## Expression languages") != std::string::npos);
  for (const auto& n : fccvis::adapters::Registry::Global().Names()) {
    const auto caps = fccvis::adapters::Registry::Global().Create(n)->GetCapabilities();
    for (const auto& o : caps.options)
      CHECK(api::Interpreter::Reference("adapters").find("| `" + o.name + "` |") != std::string::npos);
  }
}
