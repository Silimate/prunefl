#include "file.hh"

#include <access_private.hpp>
#include <fmt/format.h>

#include <slang/parsing/Preprocessor.h>
#include <slang/parsing/TokenKind.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxKind.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>

template struct access_private::access<&slang::parsing::Preprocessor::nextRaw>;

SVFile::SVFile(
	std::string_view path,
	slang::SourceManager &manager,
	slang::BumpAllocator &alloc
)
	: manager(manager), alloc(alloc) {
	auto buffer_opt = manager.readSource(path, nullptr);
	if (!buffer_opt) {
		throw std::runtime_error("Failed to read file");
	}
	buffer = std::move(*buffer_opt);
	process_defines();
	process_usages();
}

void SVFile::output(FILE *f) {
	fmt::println(f, "{}:", manager.getRawFileName(buffer.id));
	fmt::println(f, "  exported:");
	for (auto &macro : exported_macros) {
		fmt::println(f, "    - {}", macro.first);
	}
	fmt::println(f, "  used:");
	for (auto &macro : unresolved_macros) {
		fmt::println(f, "    - {}", macro);
	}
	fflush(f);
}

void SVFile::process_define(const slang::syntax::DefineDirectiveSyntax *define
) {
	std::string name{define->name.rawText()};
	auto location = define->getFirstToken().location();
	if (location.buffer() == buffer.id) {
		exported_macros[std::move(name)] = std::move(location);
	}
}

void SVFile::process_usage(const slang::parsing::Token &token) {
	auto raw = token.rawText();
	std::string name{raw.begin() + 1, raw.end()};
	auto sameFileExportedLoc = exported_macros.find(name);
	if (sameFileExportedLoc == exported_macros.end() ||
		sameFileExportedLoc->second > token.location()) {
		unresolved_macros.insert(std::move(name));
	}
}

void SVFile::process_defines() {
	slang::Diagnostics diagnostics;
	slang::parsing::Preprocessor preprocessor(
		manager, alloc, diagnostics, {}, {}
	);
	preprocessor.pushSource(buffer);

	auto token = preprocessor.next();
	while (token.kind != slang::parsing::TokenKind::EndOfFile) {
		token = preprocessor.next();
	}

	for (auto macro : preprocessor.getDefinedMacros()) {
		process_define(macro);
	}
}

void SVFile::process_usages() {
	slang::Diagnostics diagnostics;
	slang::parsing::Preprocessor preprocessor(
		manager, alloc, diagnostics, {}, {}
	);
	preprocessor.pushSource(buffer);

	auto token = access_private::accessor<"nextRaw">(preprocessor);
	while (token.kind != slang::parsing::TokenKind::EndOfFile) {
		if (token.kind == slang::parsing::TokenKind::Directive) {
			if (token.directiveKind() ==
				slang::syntax::SyntaxKind::MacroUsage) {
				process_usage(token);
			}
		}
		token = access_private::accessor<"nextRaw">(preprocessor);
	}
}
