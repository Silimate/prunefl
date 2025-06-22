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

namespace nodo {
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
} // namespace nodo
