// From prunefl

// MIT License

// Copyright (c) 2025 Silimate Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <slang/driver/Driver.h>
#include <slang/parsing/Preprocessor.h>
#include <slang/text/SourceLocation.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

namespace prunefl {
	typedef std::function<void(slang::SourceBuffer &)> SourceBufferCallback;
	struct SourceNode {
		slang::driver::Driver *driver;
		slang::SourceBuffer buffer;
		size_t load_order;

		SourceNode(
			slang::driver::Driver *driver,
			slang::SourceBuffer &buffer,
			size_t load_order
		);
		void output(FILE *f = stderr) const;
		std::filesystem::path get_path() const;

		std::unordered_map<std::filesystem::path, slang::SourceLocation>
			includes;
		std::set<std::filesystem::path> dependencies;
		std::unordered_map<std::string, slang::SourceLocation>
			exported_macro_locations;
		std::unordered_map<std::string, slang::SourceLocation>
			unresolved_macro_locations;

		void process(SourceBufferCallback source_buffer_cb) {
			process_directives(source_buffer_cb);
			process_usages();
		}

	private:
		void process_directives(SourceBufferCallback source_buffer_cb);
		void process_define(const slang::syntax::DefineDirectiveSyntax *define);
		void process_usages();
		void process_usage(const slang::parsing::Token &token);
	};
} // namespace prunefl
