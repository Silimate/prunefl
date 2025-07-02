// From prunefl

// MIT License

// Copyright (c) 2025 Silimate Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "driver.hh"

#include <slang/ast/ASTVisitor.h>
#include <slang/ast/Symbol.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/util/OS.h>

#include <fmt/ostream.h>

using namespace slang;

extern const char *VERSION;

prunefl::Driver::Driver() : driver::Driver::Driver() {
	addStandardArgs();
	cmdLine.add("-h,--help", show_help, "Display available options");
	cmdLine.add(
		"--version", show_version, "Display version information and exit"
	);
	cmdLine.add(
		"--debug-out-pfx",
		debug_output_prefix_string,
		"If specified, a number of log files showing the current state of the "
		"node tree will be created as the process runs. Any requisite "
		"directories will be created."
	);
}

void prunefl::Driver::parse_cli(int argc, char *argv[]) {
	if (!cmdLine.parse(argc, argv)) {
		std::string what = "";
		for (auto &err : cmdLine.getErrors()) {
			what = what + err + "\n";
		}
		throw std::runtime_error(what);
	}
	if (show_help == true) {
		OS::print(fmt::format("{}", cmdLine.getHelpText("prunefl")));
		exit(0);
	}
	if (show_version == true) {
		OS::print(fmt::format("prunefl {}\n", (const char *)VERSION));
		exit(0);
	}
	if (!processOptions()) {
		throw std::runtime_error("Failed to process slang command-line options"
		);
	}

	if (options.topModules.size() != 1) {
		throw std::runtime_error(
			"Exactly one top module should be provided. (--top …)"
		);
	}
}

void prunefl::Driver::prepare() {
	// Parse sources and report compilation issues to stderr
	if (!parseAllSources()) {
		throw std::runtime_error("Could not parse sources!");
	}
	compilation = createCompilation();
	reportCompilation(*compilation, true);
	if (!reportDiagnostics(true)) {
		throw std::runtime_error("Could not report diagnostics!");
	}

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

prunefl::Driver::Resolution prunefl::Driver::process_included_macros_recursive(
	std::unordered_map<std::filesystem::path, ResolutionCacheEntry> &cache,
	std::shared_ptr<prunefl::SourceNode> current_node
) {
	auto path = current_node->get_path();

	// Recursion: Check if result if cached, or if a cycle happened
	auto cache_entry_it = cache.find(path);
	auto &cache_entry = cache_entry_it->second;
	if (cache_entry.state == NodeState::visited) {
		return {current_node, cache_entry.resolved_macros}; // cached
	}
	if (cache_entry.state == NodeState::visiting) {
		return {nullptr, {}}; // cycle
	}
	cache_entry.state = NodeState::visiting;

	// Includes
	std::set<std::string_view> resolved_macros_so_far;
	for (auto &include : current_node->includes) {
		auto &[inc_path, inc_loc] = include;
		auto resolution =
			process_included_macros_recursive(cache, source_nodes[inc_path]);
		if (resolution.file == nullptr) {
			return resolution; // propagate detected cycle
		}
		for (auto &macro : resolution.resolved_macros) {
			current_node->exported_macro_locations[std::string{macro}] =
				inc_loc;
			resolved_macros_so_far.insert(macro);
			auto potential_usage =
				current_node->unresolved_macro_locations.find(std::string{macro}
				);
			if (potential_usage !=
					current_node->unresolved_macro_locations.end() &&
				potential_usage->second > inc_loc) {
				current_node->unresolved_macro_locations.erase(std::string(macro
				));
			}
		}
	}
	for (auto &macro_export : current_node->exported_macro_locations) {
		resolved_macros_so_far.insert(macro_export.first);
		// no need to resolve in-file, already done
	}
	cache_entry.state = NodeState::visited;
	cache_entry.resolved_macros = resolved_macros_so_far;
	return {current_node, resolved_macros_so_far};
}

void prunefl::Driver::preprocess() {
	while (!buffer_preprocessing_queue.empty()) {
		auto tuple =
			std::move(buffer_preprocessing_queue.front()); // move buffer
		buffer_preprocessing_queue.pop(); // pop leftover structure
		auto &[buffer, order] = tuple;	  // reference buffer
		auto path = sourceManager.getFullPath(buffer.id);
		if (source_nodes.find(path) != source_nodes.end()) {
			continue;
		}
		auto node = std::make_shared<prunefl::SourceNode>(
			this, buffer, order
		); // move buffer
		node->process([&](SourceBuffer &buffer) {
			buffer_preprocessing_queue.push({std::move(buffer), -1}
			); // move buffer
		});
		source_nodes[path] = node;
		// local var buffer deallocated
	}

	std::unordered_map<
		std::filesystem::path,
		prunefl::Driver::ResolutionCacheEntry>
		node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = prunefl::Driver::ResolutionCacheEntry();
	}

	bool errors_found = false;
	for (auto &node : source_nodes) {
		if (node_states[node.first].state !=
			prunefl::Driver::NodeState::visited) {
			auto resolution =
				process_included_macros_recursive(node_states, node.second);
			if (resolution.file == nullptr) {
				errors_found = true;
			}
		}
	}

	write_debug("_preprocessed.log");

	if (errors_found) {
		throw std::runtime_error(
			"Cycle detected while processing includes. Cannot prune file list."
		);
	}
}

std::shared_ptr<prunefl::SourceNode>
prunefl::Driver::process_module_dependencies_recursive(
	std::unordered_map<std::filesystem::path, prunefl::Driver::NodeState>
		&cache,
	const ast::InstanceSymbol *current_instance
) {
	// Get filepath and node pointer
	auto [loc, path] = get_definition_syntax_location(*current_instance);
	auto current_node = source_nodes[path];

	// Recursion: Check if result if cached, or if a cycle happened
	auto cache_entry_it = cache.find(path);
	auto cache_entry = cache_entry_it->second;
	if (cache_entry == NodeState::visited) {
		return current_node;
	}
	if (cache_entry == NodeState::visiting) {
		return nullptr; // cycle
	}
	cache_entry_it->second = NodeState::visiting;

	// Resolve the modules
	std::set<std::string_view> resolved_macros_so_far;
	for (auto &member : current_instance->body.members()) {
		if (member.kind == ast::SymbolKind::Instance) {
			auto &instance_symbol = member.as<ast::InstanceSymbol>();
			auto [def_loc, def_path] =
				get_definition_syntax_location(instance_symbol);
			current_node->dependencies.insert(def_path);
			auto resolution =
				process_module_dependencies_recursive(cache, &instance_symbol);
			if (resolution == nullptr) {
				return resolution; // propagate detected cycle
			}
		}
	}
	cache_entry_it->second = NodeState::visited;
	return current_node;
}

std::shared_ptr<prunefl::SourceNode> prunefl::Driver::module_resolution() {
	auto &root = compilation->getRoot();
	auto instances = root.topInstances;
	if (instances.size() != 1) {
		throw std::runtime_error("Less or more than one top module has been "
								 "found. Cannot prune file list.");
	}
	auto instance = instances[0];

	std::unordered_map<std::filesystem::path, prunefl::Driver::NodeState>
		node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = prunefl::Driver::NodeState();
	}

	auto resolution =
		process_module_dependencies_recursive(node_states, instance);

	write_debug("_modules_resolved.log");

	if (resolution == nullptr) {
		throw std::runtime_error("Cycle detected while resolving module "
								 "hierarchy. Cannot prune file list.");
	}
	return resolution;
}

bool prunefl::Driver::topological_sort_recursive(
	std::vector<std::filesystem::path> &result,
	std::unordered_map<std::filesystem::path, prunefl::Driver::NodeState>
		&node_states,
	std::shared_ptr<prunefl::SourceNode> target
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
	for (auto &dependency : target->dependencies) {
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

void prunefl::Driver::topological_sort(
	std::vector<std::filesystem::path> &result,
	std::shared_ptr<prunefl::SourceNode> top_node
) {
	result.clear();

	std::unordered_map<std::filesystem::path, NodeState> node_states;
	for (auto &[name, _] : source_nodes) {
		node_states[name] = NodeState::unvisited;
	}

	auto success = topological_sort_recursive(result, node_states, top_node);
	if (!success) {
		// realistically this shouldn't happen if we got to this point
		throw std::runtime_error("Cycle detected during final topological sort "
								 "of files. Cannot output final file list.");
	}
}

struct SourceNodeOrderComparator {
	bool operator()(
		const std::shared_ptr<prunefl::SourceNode> &a,
		const std::shared_ptr<prunefl::SourceNode> &b
	) const {
		return a->load_order < b->load_order;
	}
};

void prunefl::Driver::implicit_macro_resolution() {
	// Maps each macro name to the set of source nodes that export it,
	// ordered by SourceNodeOrderComparator (likely by load_order).
	std::map<
		std::string_view,
		std::set<
			std::shared_ptr<prunefl::SourceNode>,
			SourceNodeOrderComparator>>
		macro_to_exporters;

	// First pass: collect all macro exporters
	for (const auto &[path, node] : source_nodes) {
		if (node->load_order == -1) {
			// Not in file list, skip
			continue;
		}
		for (const auto &macro : node->exported_macro_locations) {
			macro_to_exporters[macro.first].insert(node);
		}
	}

	// Second pass: resolve macros for each source node
	for (const auto &[path, node] : source_nodes) {
		if (node->load_order == -1) {
			// Not in file list, skip
			continue;
		}

		// Work with a queue of unresolved macros to process them one by one
		std::deque<std::string_view> macros_to_resolve;

		for (auto it = node->unresolved_macro_locations.begin();
			 it != node->unresolved_macro_locations.end();
			 it++) {
			macros_to_resolve.push_back(it->first);
		}

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
				node->unresolved_macro_locations.erase(std::string{macro});
				node->dependencies.insert(exporter_node->get_path());
				break;
			}
		}
	}

	write_debug("_macros_resolved.log");
}
