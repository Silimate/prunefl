#pragma once

#include <slang/driver/Driver.h>
#include <slang/text/SourceLocation.h>
#include <slang/parsing/Preprocessor.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

template <typename C>
concept SourceBufferContainer = requires(C c, slang::SourceBuffer node) {
	typename C::value_type;
	{ std::is_same_v<typename C::value_type, slang::SourceBuffer> };
	{ c.push(node) } -> std::same_as<void>;
};

struct SourceNode {
	slang::driver::Driver &driver;
	slang::SourceBuffer buffer;

	SourceNode(slang::driver::Driver &driver, slang::SourceBuffer &buffer);

	const std::set<std::filesystem::path> &get_dependencies() const;
	void add_dependency(std::filesystem::path file);
	void output(FILE *f = stderr) const;
	std::filesystem::path get_path() const;

	std::set<std::string> exported_macros;
	std::set<std::string> unresolved_macros;

	template <SourceBufferContainer ContainerType>
	void process(ContainerType &container) {
		process_usages();
		process_directives(container);
	}
private:

	template <SourceBufferContainer ContainerType>
	void process_directives(ContainerType &container) {
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

		auto include_directives = preprocessor.getIncludeDirectives();
		for (auto &include : include_directives) {
			add_dependency(driver.sourceManager.getFullPath(include.buffer.id));
			container.push(std::move(include.buffer));
		}

		for (auto macro : preprocessor.getDefinedMacros()) {
			process_define(macro);
		}
	}

	void process_define(const slang::syntax::DefineDirectiveSyntax *define);
	void process_usages();
	void process_usage(const slang::parsing::Token &token);

	std::unordered_map<std::string, slang::SourceLocation>
		exported_macro_locations;
	std::set<std::filesystem::path> dependencies;
};
