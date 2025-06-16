#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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

struct SVFile {
	slang::SourceManager &manager;
	slang::BumpAllocator &alloc;
	slang::SourceBuffer buffer;

	SVFile(
		std::string_view path,
		slang::SourceManager &manager,
		slang::BumpAllocator &alloc
	);

	void output(FILE *f = stderr);

	private:
	void process_define(const slang::syntax::DefineDirectiveSyntax *define);
	void process_usage(const slang::parsing::Token &token);
	void process_defines();
	void process_usages();

	std::unordered_map<std::string, slang::SourceLocation> exported_macros;
	std::unordered_set<std::string> unresolved_macros;
};
