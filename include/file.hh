#pragma once

#include <slang/driver/Driver.h>
#include <slang/parsing/Preprocessor.h>
#include <slang/text/SourceLocation.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

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

	std::set<std::filesystem::path> dependencies;
	std::set<std::string> exported_macros;
	std::set<std::string> unresolved_macros;

	void process(SourceBufferCallback source_buffer_cb) {
		process_usages();
		process_directives(source_buffer_cb);
	}

private:
	void process_directives(SourceBufferCallback source_buffer_cb);
	void process_define(const slang::syntax::DefineDirectiveSyntax *define);
	void process_usages();
	void process_usage(const slang::parsing::Token &token);

	std::unordered_map<std::string, slang::SourceLocation>
		exported_macro_locations;
};
