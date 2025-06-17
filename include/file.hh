#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <set>
#include <algorithm>

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
} // namespace slang

struct SourceFile {
	slang::SourceManager &manager;
	slang::BumpAllocator &alloc;
	slang::BufferID buffer_id;
	slang::SourceBuffer buffer;

	SourceFile(
		std::string_view path,
		slang::SourceManager &manager,
		slang::BumpAllocator &alloc
	);

	// SourceFile(SourceFile &&src);

	const std::set<slang::BufferID> &get_dependencies() const;
	void add_dependency(slang::BufferID id);
	void output(FILE *f = stderr) const;
	std::string_view getFileName() const;

	std::set<std::string> exported_macros;
	std::set<std::string> unresolved_macros;
private:
	void process_define(const slang::syntax::DefineDirectiveSyntax *define);
	void process_usage(const slang::parsing::Token &token);
	void process_defines();
	void process_usages();

	std::unordered_map<std::string, slang::SourceLocation> exported_macro_locations;
	std::set<slang::BufferID> dependencies;
};
