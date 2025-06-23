// From prunefl

// MIT License

// Copyright (c) 2025 Silimate Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "file.hh"

#include <access_private.hpp>
#include <fmt/format.h>

#include <iostream>
#include <slang/driver/Driver.h>
#include <slang/parsing/Preprocessor.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxKind.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>

using namespace slang;

namespace access_private {
	// Expose private member
	template struct access<&parsing::Preprocessor::nextRaw>;
} // namespace access_private

prunefl::SourceNode::SourceNode(
	driver::Driver *driver, SourceBuffer &buffer, size_t load_order
)
	: driver(driver), buffer(buffer), load_order(load_order) {}

void prunefl::SourceNode::output(FILE *f) const {
	fmt::println(f, "{}:", get_path().c_str());
	fmt::println(f, "  exported_macros:");
	for (auto &macro : exported_macro_locations) {
		fmt::println(f, "    - {}", macro.first);
	}
	fmt::println(f, "  unresolved_macros:");
	for (auto &macro : unresolved_macro_locations) {
		fmt::println(f, "    - {}", macro.first);
	}
	fmt::println(f, "  resolved_dependencies:");
	for (auto &dep : dependencies) {
		fmt::println(f, "    - {}", dep.c_str());
	}
	fflush(f);
}

void prunefl::SourceNode::process_directives(
	prunefl::SourceBufferCallback source_buffer_cb
) {
	Diagnostics diagnostics;
	BumpAllocator alloc;
	parsing::Preprocessor preprocessor(
		driver->sourceManager, alloc, diagnostics, {}, {}
	);
	preprocessor.pushSource(buffer);

	auto token = preprocessor.next();
	while (token.kind != parsing::TokenKind::EndOfFile) {
		token = preprocessor.next();
	}

	auto include_directives = preprocessor.getIncludeDirectives();
	for (auto &include : include_directives) {
		auto include_path =
			driver->sourceManager.getFullPath(include.buffer.id);
		auto include_directive_location = include.syntax->sourceRange().start();
		assert(include_directive_location.buffer() == buffer.id);
		includes[include_path] = include_directive_location;
		dependencies.insert(include_path);
		source_buffer_cb(include.buffer); // assume move
	}

	for (auto macro : preprocessor.getDefinedMacros()) {
		process_define(macro);
	}
}

void prunefl::SourceNode::process_define(
	const syntax::DefineDirectiveSyntax *define
) {
	std::string name{define->name.rawText()};
	auto location = define->getFirstToken().location();
	if (location.buffer() == buffer.id) {
		exported_macro_locations[std::move(name)] = std::move(location);
	}
}

void prunefl::SourceNode::process_usage(const parsing::Token &token) {
	auto raw = token.rawText();
	std::string name{raw.begin() + 1, raw.end()};
	auto same_file_exported_loc = exported_macro_locations.find(name);
	if (same_file_exported_loc == exported_macro_locations.end() ||
		same_file_exported_loc->second > token.location()) {
		unresolved_macro_locations[std::move(name)] = token.location();
	}
}

void prunefl::SourceNode::process_usages() {
	Diagnostics diagnostics;
	BumpAllocator alloc;
	parsing::Preprocessor preprocessor(
		driver->sourceManager, alloc, diagnostics, {}, {}
	);
	preprocessor.pushSource(buffer);

	parsing::Token token = access_private::accessor<"nextRaw">(preprocessor);
	while (token.kind != parsing::TokenKind::EndOfFile) {
		if (token.kind == parsing::TokenKind::Directive) {
			auto directive_kind = token.directiveKind();
			if (directive_kind == syntax::SyntaxKind::MacroUsage) {
				process_usage(token);
			}
		}
		token = access_private::accessor<"nextRaw">(preprocessor);
	}
}

std::filesystem::path prunefl::SourceNode::get_path() const {
	return driver->sourceManager.getFullPath(buffer.id);
}
