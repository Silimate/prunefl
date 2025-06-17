#pragma once

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

#include <slang/text/SourceLocation.h>

namespace slang {
	class SourceManager;
	class BumpAllocator;
	struct SourceBuffer;
	struct SourceLocation;

	namespace syntax {
		class DefineDirectiveSyntax;
	}

	namespace parsing {
		class Token;
	}

	namespace driver {
		class Driver;
	}
} // namespace slang

struct SourceNode {
	slang::driver::Driver &driver;
	slang::SourceBuffer &buffer;

	SourceNode(slang::driver::Driver &driver, slang::SourceBuffer &buffer);

	const std::set<std::string_view> &get_dependencies() const;
	void add_dependency(std::string_view file);
	void output(FILE *f = stderr) const;
	std::string_view getFileName() const;

	std::set<std::string> exported_macros;
	std::set<std::string> unresolved_macros;

private:
	void process_define(const slang::syntax::DefineDirectiveSyntax *define);
	void process_usage(const slang::parsing::Token &token);
	void process_defines();
	void process_usages();

	std::unordered_map<std::string, slang::SourceLocation>
		exported_macro_locations;
	std::set<std::string_view> dependencies;
};
