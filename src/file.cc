#include "file.hh"

#include <access_private.hpp>
#include <fmt/format.h>

#include <iostream>
#include <slang/parsing/Preprocessor.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxKind.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>

namespace access_private {
	// Expose private members
	template struct access<
		&slang::parsing::Preprocessor::handleIncludeDirective>;
	template struct access<&slang::parsing::Preprocessor::nextRaw>;
	// Allow l-value for handleIncludeDirective argument
	constexpr decltype(auto) call(
		accessor_t<"handleIncludeDirective">,
		slang::parsing::Preprocessor &,
		slang::parsing::Token
	);
}

SourceFile::SourceFile(
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
	buffer_id = buffer.id;
	process_defines();
	process_usages();
}

// SourceFile::SourceFile(
// 	SourceFile&& src
// ): manager(manager), alloc(alloc) {
// 	buffer = std::move(src.buffer);
// 	buffer_id = src.buffer_id;
// 	exported_macros = std::move(src.exported_macros);
// 	unresolved_macros = std::move(src.unresolved_macros);
// 	dependencies = std::move(src.dependencies);
// }

void SourceFile::output(FILE *f) const {
	fmt::println(f, "{}:", manager.getRawFileName(buffer.id));
	fmt::println(f, "  exported:");
	for (auto &macro : exported_macros) {
		fmt::println(f, "    - {}", macro);
	}
	fmt::println(f, "  used:");
	for (auto &macro : unresolved_macros) {
		fmt::println(f, "    - {}", macro);
	}
	fflush(f);
}

void SourceFile::process_define(
	const slang::syntax::DefineDirectiveSyntax *define
) {
	std::string name{define->name.rawText()};
	auto location = define->getFirstToken().location();
	if (location.buffer() == buffer.id) {
		exported_macros.insert(name);
		exported_macro_locations[std::move(name)] = std::move(location);
	}
}

void SourceFile::process_usage(const slang::parsing::Token &token) {
	auto raw = token.rawText();
	std::string name{raw.begin() + 1, raw.end()};
	auto sameFileExportedLoc = exported_macro_locations.find(name);
	if (sameFileExportedLoc == exported_macro_locations.end() ||
		sameFileExportedLoc->second > token.location()) {
		unresolved_macros.insert(std::move(name));
	}
}

void SourceFile::process_defines() {
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

void SourceFile::process_usages() {
	slang::Diagnostics diagnostics;
	slang::parsing::Preprocessor preprocessor(
		manager, alloc, diagnostics, {}, {}
	);
	preprocessor.pushSource(buffer);

	slang::parsing::Token token =
		access_private::accessor<"nextRaw">(preprocessor);
	while (token.kind != slang::parsing::TokenKind::EndOfFile) {
		if (token.kind == slang::parsing::TokenKind::Directive) {
			auto directive_kind = token.directiveKind();
			if (directive_kind == slang::syntax::SyntaxKind::MacroUsage) {
				process_usage(token);
			} else if (directive_kind ==
					   slang::syntax::SyntaxKind::IncludeDirective) {
				/*
				// TODO: Handle includes
				slang::parsing::Trivia trivia =
					access_private::accessor<"handleIncludeDirective">(
						preprocessor, token
					);
				auto includeDirectiveSyntax =
					static_cast<slang::syntax::IncludeDirectiveSyntax *>(
						trivia.syntax()
					);
				*/
			}
		}
		token = access_private::accessor<"nextRaw">(preprocessor);
	}
}

void SourceFile::add_dependency(slang::BufferID id) { dependencies.insert(id); }

const std::set<slang::BufferID> &
SourceFile::get_dependencies() const {
	return dependencies;
}

std::string_view SourceFile::getFileName() const {
	return manager.getRawFileName(buffer_id);
}
