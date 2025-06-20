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

#include <cstdio>
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
	std::unordered_map<std::filesystem::path, std::shared_ptr<SourceNode>>
		&source_nodes,
	std::unordered_map<std::filesystem::path, ResultCache> &node_states,
	const ast::InstanceSymbol *current_instance,
	int depth,
	FILE *out_stream = nullptr
) {
	auto &definition = current_instance->getDefinition();
	auto buffer_id = definition.location.buffer();
	auto path = manager.getFullPath(buffer_id);
	auto &cache_result = node_states.find(path)->second;
	auto file = source_nodes[path];
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
			file->get_path().c_str()
		);
	}
	std::set<std::string_view> resolved_macros_so_far;
	auto parentScope = current_instance->getDefinition().getSyntax()->parent;
	for (auto &member : current_instance->body.members()) {
		if (member.kind == ast::SymbolKind::Instance) {
			auto &instanceSymbol = member.as<ast::InstanceSymbol>();
			auto &definition = instanceSymbol.getDefinition();
			auto definition_buffer = definition.location.buffer();
			auto definition_filename = manager.getFullPath(definition_buffer);
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
	std::unordered_map<std::filesystem::path, std::shared_ptr<SourceNode>>
		&source_nodes,
	const ast::InstanceSymbol *top_instance,
	FILE *out_stream = nullptr
) {
	std::unordered_map<std::filesystem::path, ResultCache> node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = ResultCache();
	}

	return process_dependencies_recursive(
		manager, source_nodes, node_states, top_instance, 0, out_stream
	);
}

bool topological_sort_recursive(
	std::vector<std::filesystem::path> &result,
	std::unordered_map<std::filesystem::path, NodeState> &node_states,
	std::unordered_map<std::filesystem::path, std::shared_ptr<SourceNode>>
		&source_nodes,
	std::shared_ptr<SourceNode> target
) {
	auto path = target->get_path();
	auto state = node_states.find(path)->second;
	if (state == NodeState::visited) {
		return true; // already visited
	}
	if (state == NodeState::visiting) {
		return false; // cycle
	}
	node_states[path] = NodeState::visiting;
	for (auto dependency : target->get_dependencies()) {
		if (!topological_sort_recursive(
				result, node_states, source_nodes, source_nodes[dependency]
			)) {
			return false; // propagate detected cycle
		}
	}
	result.push_back(path);
	node_states[path] = NodeState::visited;
	return true;
}

bool topological_sort(
	std::vector<std::filesystem::path> &result,
	std::unordered_map<std::filesystem::path, std::shared_ptr<SourceNode>>
		&source_nodes,
	std::shared_ptr<SourceNode> top_node
) {
	result.clear();

	std::unordered_map<std::filesystem::path, NodeState> node_states;
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
	std::optional<bool> show_help;
	std::optional<bool> show_version;
	std::optional<std::string> debug_output_prefix_string;
	driver.cmdLine.add("-h,--help", show_help, "Display available options");
	driver.cmdLine.add(
		"--version", show_version, "Display version information and exit"
	);
	driver.cmdLine.add("--debug-out-pfx", debug_output_prefix_string, "");
	
	
	if (!driver.parseCommandLine(argc, argv)) {
		return -1;
	}
	if (show_help == true) {
		OS::print(fmt::format("{}", driver.cmdLine.getHelpText("nodo")));
		return 0;
	}
	if (show_version == true) {
		OS::print(fmt::format("nodo {}", (const char *)VERSION));
		return 0;
	}
	if (!driver.processOptions()) {
		return -1;
	}

	auto top_modules = driver.options.topModules;
	if (top_modules.size() != 1) {
		OS::printE("Exactly one top module should be provided. (--top …)");
		return 0;
	}

	// Elaboration
	bool ok = driver.parseAllSources();
	std::unique_ptr<ast::Compilation> compilation;
	compilation = driver.createCompilation();
	driver.reportCompilation(*compilation, true);
	bool build_succeeded = driver.reportDiagnostics(true);

	auto buffers_tmp = driver.sourceLoader.loadSources();
	std::queue<SourceBuffer> q;
	std::unordered_map<std::filesystem::path, size_t> file_order;
	size_t file_index = 0;
	for (auto &buffer : buffers_tmp) {
		file_order[driver.sourceManager.getFullPath(buffer.id)] = file_index++;
		q.emplace(std::move(buffer));
	}

	std::unordered_map<std::filesystem::path, std::shared_ptr<SourceNode>>
		source_nodes;
	while (!q.empty()) {
		auto buffer = q.front();
		q.pop();
		auto path = driver.sourceManager.getFullPath(buffer.id);
		if (source_nodes.find(path) != source_nodes.end()) {
			continue;
		}
		auto node = std::make_shared<SourceNode>(driver, buffer);
		node->process(q);
		source_nodes[path] = node;
	}
	
	if (debug_output_prefix_string.has_value()) {
		auto preprocessed = *debug_output_prefix_string + "_preprocessed.log";
		FILE *f = fopen(preprocessed.c_str(), "w");
		for (auto &tuple: source_nodes) {
			tuple.second->output(f);
		}
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
	if (debug_output_prefix_string.has_value()) {
		auto modules_resolved = *debug_output_prefix_string + "_modules_resolved.log";
		FILE *f = fopen(modules_resolved.c_str(), "w");
		for (auto &tuple: source_nodes) {
			tuple.second->output(f);
		}
	}

	std::vector<std::filesystem::path> result; // toposort result
	if (!topological_sort(result, source_nodes, resolution.file)) {
		fmt::println(stderr, "[FATAL] Cycle detected.");
		return 1;
	}
	for (auto &file : result) {
		fmt::println("{}", file.c_str());
	}
}
