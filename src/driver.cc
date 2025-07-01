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

extern const char VERSION[];

prunefl::Driver::Driver() : driver::Driver::Driver() {
	addStandardArgs();
	cmdLine.add("-h,--help", show_help, "Display available options");
	cmdLine.add(
		"--version", show_version, "Display version information and exit"
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
	parseAllSources();
	compilation = createCompilation();
	reportCompilation(*compilation, true);
	reportDiagnostics(true);
}

bool prunefl::Driver::topological_sort_recursive(
	tsl::ordered_set<BufferID> &result,
	std::unordered_map<BufferID, prunefl::Driver::NodeState> &node_states,
	BufferID target
) {
	auto state = node_states.find(target)->second;
	if (state.visited == NodeVisitStatus::done) {
		return true; // already visited
	}
	if (state.visited == NodeVisitStatus::in_progress) {
		return false; // cycle
	}
	node_states[target].visited = NodeVisitStatus::in_progress;
	for (auto dependency : sourceManager.getDependencies(target)) {
		if (!topological_sort_recursive(result, node_states, dependency)) {
			return false; // propagate detected cycle
		}
	}
	result.insert(target);
	node_states[target].visited = NodeVisitStatus::done;
	return true;
}

void prunefl::Driver::topological_sort(
	tsl::ordered_set<std::filesystem::path> &result
) {
	result.clear();

	auto &root = compilation->getRoot();
	auto instances = root.topInstances;
	if (instances.size() != 1) {
		throw std::runtime_error("Less or more than one top module has been "
								 "found. Cannot prune file list.");
	}
	auto instance = instances[0];
	auto top_node = instance->getDefinition().location.buffer();
	std::deque<BufferID> q;
	q.push_back(top_node);

	tsl::ordered_set<BufferID> id_result;
	std::unordered_map<BufferID, NodeState> node_states;
	for (auto id : sourceManager.getAllBuffers()) {
		auto path = sourceManager.getFullPath(id);
		if (path.empty()) {
			continue;
		}
		node_states[id] = {};
	}
	auto peer_dependency_enqueuing_idx = 0;
	while (!q.empty()) {
		auto current_node = q.front();
		q.pop_front();
		if (node_states[current_node].peer_dependencies_enqueued) {
			continue;
		}
		auto success =
			topological_sort_recursive(id_result, node_states, current_node);
		if (!success) {
			// realistically this shouldn't happen if we got to this point
			throw std::runtime_error(
				"Cycle detected during final topological sort "
				"of files. Cannot output final file list."
			);
		}
		while (id_result.nth(peer_dependency_enqueuing_idx) != id_result.end()
		) {
			auto target = *id_result.nth(peer_dependency_enqueuing_idx);
			assert(node_states[target].visited == NodeVisitStatus::done);
			auto peer_deps = sourceManager.getPeerDependencies(target);
			for (auto peer_id : peer_deps) {
				q.push_back(peer_id);
			}
			peer_dependency_enqueuing_idx++;
		}
		node_states[current_node].peer_dependencies_enqueued = true;
	}

	for (auto id : id_result) {
		result.insert(sourceManager.getFullPath(id));
	}
}
