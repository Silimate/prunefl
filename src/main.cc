#include "file.hh"

#include <argparse/argparse.hpp>
#include <fmt/format.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/Symbol.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/driver/Driver.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>
#include <slang/util/BumpAllocator.h>
#include <slang/util/OS.h>

#include <stack>

using namespace slang;

extern unsigned char VERSION[];

enum class NodeState { unvisited = 0, visiting, visited };
struct ResultCache {
	NodeState state = NodeState::unvisited;
	std::set<std::string_view> resolved_macros{};
};

struct Resolution {
	std::shared_ptr<SourceNode> file;
	std::set<std::string_view> resolved_macros;
};

Resolution process_dependencies_recursive(
	SourceManager &manager,
	std::unordered_map<std::string_view, std::shared_ptr<SourceNode>>
		&source_nodes,
	std::unordered_map<std::string_view, ResultCache> &node_states,
	const ast::InstanceSymbol *current_instance,
	int depth,
	FILE *out_stream = nullptr
) {
	auto &definition = current_instance->getDefinition();
	auto buffer_id = definition.location.buffer();
	auto name = manager.getRawFileName(buffer_id);
	auto &cache_result = node_states.find(name)->second;
	auto file = source_nodes[name];
	if (cache_result.state == NodeState::visited) {
		return {file, cache_result.resolved_macros};
	}
	if (cache_result.state == NodeState::visiting) {
		return {nullptr, {}}; // cycle
	}
	cache_result.state = NodeState::visiting;
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
		if (member.kind == ast::SymbolKind::Instance) {
			auto &instanceSymbol = member.as<ast::InstanceSymbol>();
			auto &definition = instanceSymbol.getDefinition();
			auto definition_buffer = definition.location.buffer();
			auto definition_filename =
				manager.getRawFileName(definition_buffer);
			file->add_dependency(definition_filename);
			auto resolution = process_dependencies_recursive(
				manager,
				source_nodes,
				node_states,
				&instanceSymbol,
				depth + 1,
				out_stream
			);
			if (resolution.file == nullptr) {
				return resolution;
			}
			for (auto definition : resolution.resolved_macros) {
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
	cache_result.state = NodeState::visited;
	cache_result.resolved_macros = resolved_macros_so_far;
	return {file, resolved_macros_so_far};
}

Resolution process_dependencies(
	SourceManager &manager,
	std::unordered_map<std::string_view, std::shared_ptr<SourceNode>>
		&source_nodes,
	const ast::InstanceSymbol *top_instance,
	FILE *out_stream = nullptr
) {
	std::unordered_map<std::string_view, ResultCache> node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = ResultCache();
	}

	return process_dependencies_recursive(
		manager, source_nodes, node_states, top_instance, 0, out_stream
	);
}

bool topological_sort_recursive(
	std::vector<std::string_view> &result,
	std::unordered_map<std::string_view, NodeState> &node_states,
	std::unordered_map<std::string_view, std::shared_ptr<SourceNode>>
		&source_nodes,
	std::shared_ptr<SourceNode> target
) {
	auto name = target->getFileName();
	auto state = node_states.find(name)->second;
	if (state == NodeState::visited) {
		return true; // already visited
	}
	if (state == NodeState::visiting) {
		return false; // cycle
	}
	node_states[name] = NodeState::visiting;
	for (auto dependency : target->get_dependencies()) {
		if (!topological_sort_recursive(
				result, node_states, source_nodes, source_nodes[dependency]
			)) {
			return false; // propagate detected cycle
		}
	}
	result.push_back(name);
	node_states[name] = NodeState::visited;
	return true;
}

bool topological_sort(
	std::vector<std::string_view> &result,
	std::unordered_map<std::string_view, std::shared_ptr<SourceNode>>
		&source_nodes,
	std::shared_ptr<SourceNode> top_node
) {
	result.clear();

	std::unordered_map<std::string_view, NodeState> node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = NodeState::unvisited;
	}

	return topological_sort_recursive(
		result, node_states, source_nodes, top_node
	);
}

int main(int argc, char *argv[]) {
	driver::Driver driver;

	// CLI parsing
	driver.addStandardArgs();
	std::optional<bool> showHelp;
	std::optional<bool> showVersion;
	driver.cmdLine.add("-h,--help", showHelp, "Display available options");
	driver.cmdLine.add(
		"--version", showVersion, "Display version information and exit"
	);
	if (!driver.parseCommandLine(argc, argv)) {
		return -1;
	}
	if (showHelp == true) {
		OS::print(fmt::format("{}", driver.cmdLine.getHelpText("nodo")));
		return 0;
	}
	if (showVersion == true) {
		OS::print(fmt::format("nodo {}", (const char *)VERSION));
		return 0;
	}
	if (!driver.processOptions()) {
		return -1;
	}

	auto top_modules = driver.options.topModules;
	if (top_modules.size() != 1) {
		fmt::println(
			stderr,
			"[ERROR] Exactly one top module should be provided. (--top …)"
		);
		return 0;
	}

	// Elaboration
	bool ok = driver.parseAllSources();

	std::unique_ptr<ast::Compilation> compilation;
	compilation = driver.createCompilation();
	driver.reportCompilation(*compilation, true);
	driver.reportDiagnostics(true);

	auto macro_buffers = driver.sourceLoader.loadSources();
	std::unordered_map<std::string_view, std::shared_ptr<SourceNode>>
		source_nodes;
	for (auto &file : macro_buffers) {
		auto info = std::make_shared<SourceNode>(driver, file);
		source_nodes[info->getFileName()] = std::move(info);
	}

	auto &root = compilation->getRoot();
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
	auto resolution =
		process_dependencies(driver.sourceManager, source_nodes, instance);
	if (resolution.file == nullptr) {
		fmt::println(stderr, "[FATAL] Cycle detected.");
		return 1;
	}

	std::vector<std::string_view> result; // toposort result
	if (!topological_sort(result, source_nodes, resolution.file)) {
		fmt::println(stderr, "[FATAL] Cycle detected.");
		return 1;
	}
	for (auto &file : result) {
		fmt::println("{}", file);
	}
}
