#include "driver.hh"

#include <slang/ast/Symbol.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/util/OS.h>

using namespace slang;

extern unsigned char VERSION[];

Driver::Driver() : driver::Driver() {
	addStandardArgs();
	cmdLine.add("-h,--help", show_help, "Display available options");
	cmdLine.add(
		"--version", show_version, "Display version information and exit"
	);
	cmdLine.add("--debug-out-pfx", debug_output_prefix_string, "");
}

void Driver::parse_cli(int argc, char *argv[]) {
	if (!cmdLine.parse(argc, argv)) {
		std::string what = "";
		for (auto &err : cmdLine.getErrors()) {
			what = what + err + "\n";
		}
		throw std::runtime_error(what);
	}
	if (show_help == true) {
		OS::print(fmt::format("{}", cmdLine.getHelpText("nodo")));
		exit(0);
	}
	if (show_version == true) {
		OS::print(fmt::format("nodo {}", (const char *)VERSION));
		exit(0);
	}
	if (!processOptions()) {
		throw std::runtime_error(
			"Failed to process slang command-line options\n"
		);
	}

	if (options.topModules.size() != 1) {
		throw std::runtime_error(
			"Exactly one top module should be provided. (--top …)"
		);
	}
}

void Driver::prepare() {
	// Parse sources and report compilation issues
	parseAllSources();
	compilation = createCompilation();
	reportCompilation(*compilation, true);
	reportDiagnostics(true);

	// Recreate file list (with fully resolved paths)
	auto buffers_tmp = std::move(sourceLoader.loadSources()); // move buffers
	size_t file_index = 0;
	for (auto &buffer : buffers_tmp) { // reference
		buffer_preprocessing_queue.push(
			std::make_pair(std::move(buffer), file_index++)
		); // move buffer
	}
	// buffers_tmp leftover structures deallocated
}

void Driver::preprocess() {
	while (!buffer_preprocessing_queue.empty()) {
		auto tuple =
			std::move(buffer_preprocessing_queue.front()); // move buffer
		buffer_preprocessing_queue.pop(); // pop leftover structure
		auto &[buffer, order] = tuple;	  // reference buffer
		auto path = sourceManager.getFullPath(buffer.id);
		if (source_nodes.find(path) != source_nodes.end()) {
			continue;
		}
		auto node =
			std::make_shared<SourceNode>(this, buffer, order); // move buffer
		node->process([&](SourceBuffer &buffer) {
			buffer_preprocessing_queue.push({std::move(buffer), -1}
			); // move buffer
		});
		source_nodes[path] = node;
		// local var buffer deallocated
	}

	if (debug_output_prefix_string.has_value()) {
		auto preprocessed = *debug_output_prefix_string + "_preprocessed.log";
		FILE *f = fopen(preprocessed.c_str(), "w");
		for (auto &tuple : source_nodes) {
			tuple.second->output(f);
		}
	}
}

Driver::Resolution Driver::process_module_dependencies_recursive(
	std::unordered_map<std::filesystem::path, Driver::ResolutionCacheEntry>
		&cache,
	const ast::InstanceSymbol *current_instance,
	int depth
) {
	auto &definition = current_instance->getDefinition();
	auto buffer_id = definition.location.buffer();
	auto path = sourceManager.getFullPath(buffer_id);
	auto &cache_entry = cache.find(path)->second;
	auto file = source_nodes[path];
	if (cache_entry.state == NodeState::visited) {
		return {file, cache_entry.resolved_macros}; // cached
	}
	if (cache_entry.state == NodeState::visiting) {
		return {nullptr, {}}; // cycle
	}
	cache_entry.state = NodeState::visiting;
	std::set<std::string_view> resolved_macros_so_far;
	auto parentScope = current_instance->getDefinition().getSyntax()->parent;
	for (auto &member : current_instance->body.members()) {
		if (member.kind == ast::SymbolKind::Instance) {
			auto &instanceSymbol = member.as<ast::InstanceSymbol>();
			auto &definition = instanceSymbol.getDefinition();
			auto definition_buffer = definition.location.buffer();
			auto definition_filepath =
				sourceManager.getFullPath(definition_buffer);
			file->dependencies.insert(definition_filepath);
			auto resolution = process_module_dependencies_recursive(
				cache, &instanceSymbol, depth + 1
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
	cache_entry.state = NodeState::visited;
	cache_entry.resolved_macros = resolved_macros_so_far;
	return {file, resolved_macros_so_far};
}

Driver::Resolution Driver::process_module_dependencies(
	const slang::ast::InstanceSymbol *top_instance
) {
	std::unordered_map<std::filesystem::path, Driver::ResolutionCacheEntry>
		node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = Driver::ResolutionCacheEntry();
	}

	return process_module_dependencies_recursive(node_states, top_instance, 0);
}

std::shared_ptr<SourceNode> Driver::module_resolution() {
	auto &root = compilation->getRoot();
	auto instances = root.topInstances;
	if (instances.size() != 1) {
		throw std::runtime_error("Less or more than one top module has been "
								 "found. Cannot prune file list.");
	}
	auto instance = instances[0];
	auto resolution = process_module_dependencies(instance);

	if (debug_output_prefix_string.has_value()) {
		auto modules_resolved =
			*debug_output_prefix_string + "_modules_resolved.log";
		FILE *f = fopen(modules_resolved.c_str(), "w");
		for (auto &tuple : source_nodes) {
			tuple.second->output(f);
		}
	}
	if (resolution.file == nullptr) {
		throw std::runtime_error("Cycle detected while resolving module "
								 "hierarchy. Cannot prune file list.");
	}
	return resolution.file;
}

bool Driver::topological_sort_recursive(
	std::vector<std::filesystem::path> &result,
	std::unordered_map<std::filesystem::path, Driver::NodeState> &node_states,
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
	for (auto dependency : target->dependencies) {
		if (!topological_sort_recursive(
				result, node_states, source_nodes[dependency]
			)) {
			return false; // propagate detected cycle
		}
	}
	result.push_back(path);
	node_states[path] = NodeState::visited;
	return true;
}

void Driver::topological_sort(
	std::vector<std::filesystem::path> &result,
	std::shared_ptr<SourceNode> top_node
) {
	result.clear();

	std::unordered_map<std::filesystem::path, NodeState> node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = NodeState::unvisited;
	}

	auto success = topological_sort_recursive(result, node_states, top_node);
	if (!success) {
		throw std::runtime_error(
			"Cycle detected during final topological sort of files."
		);
	}
}

struct SourceNodeOrderComparator {
	bool operator()(
		const std::shared_ptr<SourceNode> &a,
		const std::shared_ptr<SourceNode> &b
	) const {
		return a->load_order < b->load_order;
	}
};

void Driver::implicit_macro_resolution() {
	// Maps each macro name to the set of source nodes that export it,
	// ordered by SourceNodeOrderComparator (likely by load_order).
	std::map<
		std::string_view,
		std::set<std::shared_ptr<SourceNode>, SourceNodeOrderComparator>>
		macro_to_exporters;

	// First pass: collect all macro exporters
	for (const auto &[path, node] : source_nodes) {
		if (node->load_order == -1) {
			// Not in file list, skip
			continue;
		}
		for (const auto &macro : node->exported_macros) {
			macro_to_exporters[macro].insert(node);
		}
	}

	// Second pass: resolve macros for each source node
	for (const auto &[path, node] : source_nodes) {
		if (node->load_order == -1) {
			// Not in file list, skip
			continue;
		}

		// Work with a queue of unresolved macros to process them one by one
		std::deque<std::string_view> macros_to_resolve(
			node->unresolved_macros.begin(), node->unresolved_macros.end()
		);

		while (!macros_to_resolve.empty()) {
			std::string_view macro = macros_to_resolve.front();
			macros_to_resolve.pop_front();

			const auto &exporters = macro_to_exporters[macro];

			// Try to find a valid exporter with a lower load_order
			for (auto it = exporters.rbegin(); it != exporters.rend(); ++it) {
				const auto &exporter_node = *it;

				if (exporter_node->load_order == -1 ||
					exporter_node->load_order >= node->load_order) {
					// Either not in file list OR later in file list -
					// not a valid candidate
					continue;
				}

				// Found a valid macro exporter
				node->unresolved_macros.erase(std::string{macro});
				node->dependencies.insert(exporter_node->get_path());
				break;
			}
		}
	}
}
