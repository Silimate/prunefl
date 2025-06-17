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
} // namespace access_private

SourceNode::SourceNode(
	slang::driver::Driver &driver, slang::SourceBuffer &buffer
)
	: driver(driver), buffer(buffer) {
	process_defines();
	process_usages();
}

// SourceNode::SourceNode(
// 	SourceNode&& src
// ): manager(manager), alloc(alloc) {
// 	buffer = std::move(src.buffer);
// 	buffer_id = src.buffer_id;
// 	exported_macros = std::move(src.exported_macros);
// 	unresolved_macros = std::move(src.unresolved_macros);
// 	dependencies = std::move(src.dependencies);
// }

void SourceNode::output(FILE *f) const {
	fmt::println(f, "{}:", driver.sourceManager.getRawFileName(buffer.id));
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

void SourceNode::process_define(
	const slang::syntax::DefineDirectiveSyntax *define
) {
	std::string name{define->name.rawText()};
	auto location = define->getFirstToken().location();
	if (location.buffer() == buffer.id) {
		exported_macros.insert(name);
		exported_macro_locations[std::move(name)] = std::move(location);
	}
}

void SourceNode::process_usage(const slang::parsing::Token &token) {
	auto raw = token.rawText();
	std::string name{raw.begin() + 1, raw.end()};
	auto sameFileExportedLoc = exported_macro_locations.find(name);
	if (sameFileExportedLoc == exported_macro_locations.end() ||
		sameFileExportedLoc->second > token.location()) {
		unresolved_macros.insert(std::move(name));
	}
}

void SourceNode::process_defines() {
	slang::Diagnostics diagnostics;
	slang::BumpAllocator alloc;
	slang::parsing::Preprocessor preprocessor(
		driver.sourceManager, alloc, diagnostics, {}, {}
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

void SourceNode::process_usages() {
	slang::Diagnostics diagnostics;
	slang::BumpAllocator alloc;
	slang::parsing::Preprocessor preprocessor(
		driver.sourceManager, alloc, diagnostics, {}, {}
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

void SourceNode::add_dependency(std::string_view file) {
	dependencies.insert(file);
}

const std::set<std::string_view> &SourceNode::get_dependencies() const {
	return dependencies;
}

std::string_view SourceNode::getFileName() const {
	return driver.sourceManager.getRawFileName(buffer.id);
}
