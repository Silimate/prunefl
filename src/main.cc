#include "file.hh"

#include <argparse/argparse.hpp>
#include <fmt/format.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/Symbol.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>
#include <slang/util/BumpAllocator.h>

struct Resolution {
	std::shared_ptr<SourceFile> file;
	std::set<std::string_view> resolved_macros;
};

Resolution process_dependencies(
	std::unordered_map<slang::BufferID, std::shared_ptr<SourceFile>>
		&source_nodes,
	const slang::ast::InstanceSymbol *current_instance,
	FILE* out_stream = nullptr,
	int depth = 0
) {
	auto &definition = current_instance->getDefinition();
	auto file = source_nodes[definition.location.buffer()];
	if (out_stream) {
		fmt::println(
			out_stream,
			"{}{}{} @ \"{}\"",
			std::string(depth * 2, ' '),
			depth > 0 ? "↳ " : "",
			current_instance->name,
			file->getFileName()
		);
	}
	std::set<std::string_view> resolved_macros_so_far;
	for (auto &member : current_instance->body.members()) {
		if (member.kind == slang::ast::SymbolKind::Instance) {
			auto &instanceSymbol = member.as<slang::ast::InstanceSymbol>();
			auto &definition = instanceSymbol.getDefinition();
			file->add_dependency(definition.location.buffer());
			auto resolution = process_dependencies(source_nodes, &instanceSymbol, out_stream, depth + 1);
			for (auto definition: resolution.resolved_macros) {
				resolved_macros_so_far.insert(definition);
			}
		}
	}
	for (auto resolved_macro : resolved_macros_so_far) {
		file->unresolved_macros.erase(std::string(resolved_macro));
	}
	for (std::string_view current_file_macro : file->exported_macros) {
		resolved_macros_so_far.insert(current_file_macro);
	}
	return { file, std::move(resolved_macros_so_far) };
}

int main(int argc, char *argv[]) {
	argparse::ArgumentParser nodo_cmd(argc > 0 ? argv[0] : "nodo", "0.1.0");
	nodo_cmd.add_argument("-t", "--top-module")
		.help("The top module to use to prune")
		.required();
	nodo_cmd.add_argument("files").remaining();
	try {
		nodo_cmd.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << err.what() << std::endl;
		std::cerr << nodo_cmd;
		return -1;
	}
	std::vector<std::string> files;
	try {
		files = nodo_cmd.get<std::vector<std::string>>("files");
	} catch (std::logic_error &e) {
		fmt::println(stderr, "[WARNING] No files given. Exiting.");
		return 0;
	}

	slang::SourceManager manager;
	slang::BumpAllocator alloc;
	std::vector<std::shared_ptr<slang::syntax::SyntaxTree>> trees;
	slang::ast::CompilationOptions options;
	auto top_module = nodo_cmd.get<std::string>("--top-module");
	options.topModules.emplace(top_module);
	std::unordered_map<slang::BufferID, std::shared_ptr<SourceFile>>
		source_nodes;

	slang::ast::Compilation compilation(options);
	for (auto &file : files) {
		auto info = std::make_shared<SourceFile>(file, manager, alloc);
		auto tree = slang::syntax::SyntaxTree::fromBuffer(
			info->buffer, manager, {}, {}
		);
		trees.push_back(tree);
		compilation.addSyntaxTree(tree);
		source_nodes[info->buffer_id] = std::move(info);
	}
	auto &root = compilation.getRoot();

	slang::DiagnosticEngine de(manager);
	auto diagnostics = compilation.getAllDiagnostics();
	fmt::println(stderr, "{}", de.reportAll(manager, diagnostics));

	auto instances = root.topInstances;
	if (instances.size() != 1) {
		fmt::println(
			stderr,
			"[ERROR] Less or more than one top module has been found. Cannot "
			"prune "
			"file list."
		);
		return 0;
	}
	auto instance = instances[0];
	process_dependencies(source_nodes, instance, stderr);
}
