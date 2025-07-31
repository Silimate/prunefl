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

#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <fstream>
#include <set>

using namespace slang;

static std::optional<std::string> hash_file(std::filesystem::path path) {
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		return std::nullopt;
	}
	std::string current_hash;
	picosha2::hash256_hex_string(
		std::istreambuf_iterator<char>(f),
		std::istreambuf_iterator<char>(),
		current_hash
	);
	f.close();
	return current_hash;
}

extern const char VERSION[];

prunefl::Driver::Driver() : driver::Driver::Driver() {
	addStandardArgs();
	cmdLine.add("-h,--help", show_help, "Display available options");
	cmdLine.add(
		"--version", show_version, "Display version information and exit"
	);
	cmdLine.add(
		"--cache-to",
		cache_file,
		"Optional- if specified, the file in question is used to store caching "
		"information. The directory it lies in must exist."
	);
	cmdLine.add(
		"--include-dirs",
		include_dirs,
		"Instead of explicitly listing included files,"
		"'+incdir+'. Needed for some less-flexible parsers."
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

bool prunefl::Driver::load_cache() {
	std::filesystem::path cache_path(*cache_file);
	std::ifstream cache_reader;
	cache_reader.open(cache_path);
	// If we fail to open the file, that's a cache miss:
	if (!cache_reader) {
		fmt::println(stderr, "Cache file not found. Pruning…");
		return false;
	}

	nlohmann::json j;
	cache_reader >> j;
	cache_reader.close();

	std::set<std::filesystem::path> cache_loaded_files;
	auto cache_input_file_list = j["input_file_list"];
	for (auto it = cache_input_file_list.begin();
		 it != cache_input_file_list.end();
		 it++) {
		cache_loaded_files.insert(std::filesystem::path(*it));
	}

	// If the file list does not match, that is also a cache miss:
	if (input_file_list != cache_loaded_files) {
		fmt::println(
			stderr,
			"File list is different between current invocation and "
			"cache. Pruning new file list…"
		);
		return false;
	}

	// Time to compare the hashes.
	bool cache_hit = true;
	auto file_hashes = j["file_hashes"];
	for (auto it = file_hashes.begin(); it != file_hashes.end(); it++) {
		auto path = std::filesystem::path(it.key());
		auto au_hash = it.value().get<std::string>();
		auto current_hash = hash_file(path);
		if (current_hash != au_hash) {
			fmt::println(
				stderr, "File {} changed, re-running prune…", path.c_str()
			);
			cache_hit = false;
			break;
		}
		result.insert(path);
	}
	if (cache_hit) {
		fmt::println(
			stderr, "Input files have not changed, loading from cache…"
		);
		auto included_files = j["included_files"];
		for (auto it = included_files.begin(); it != included_files.end();
			 it++) {
			auto file = std::filesystem::path(it.value().get<std::string>());
			result_includes.insert(file);
		}
		return true;
	}
	result.clear();
	return false;
}

void prunefl::Driver::prepare() {
	auto loaded_files = sourceLoader.loadSources();
	for (auto &buffer : loaded_files) {
		auto full_path = sourceManager.getFullPath(buffer.id);
		input_file_list.insert(full_path);
	}
	if (cache_file.has_value() && load_cache()) {
		return;
	}

	// Parse sources and report compilation issues to stderr
	if (!parseAllSources()) {
		throw std::runtime_error("Could not parse sources!");
	}
	compilation = createCompilation();
	reportCompilation(*compilation, true);
	if (!reportDiagnostics(true)) {
		throw std::runtime_error("Could not report diagnostics!");
	}
	root = &compilation->getRoot();
	if (root->topInstances.size() != 1) {
		throw std::runtime_error("Less or more than one top module has been "
								 "found. Cannot prune file list.");
	}

	auto included_files = getDepfiles(true);
	for (auto &file : included_files) {
		result_includes.insert(file);
	}
}

void prunefl::Driver::topological_sort_recursive(
	tsl::ordered_set<BufferID> &result,
	std::unordered_map<BufferID, prunefl::Driver::NodeState> &node_states,
	BufferID target
) {
	auto state = node_states.find(target)->second;
	if (state.visited == NodeVisitStatus::done) {
		return; // already visited
	}
	if (state.visited == NodeVisitStatus::in_progress) {
		throw std::runtime_error("Cycle detected during final topological sort "
								 "of files: Cannot output final file list.");
	}
	node_states[target].visited = NodeVisitStatus::in_progress;
	auto &dependencies = sourceManager.getDependencies(target);
	for (const auto &dependency : dependencies) {
		topological_sort_recursive(result, node_states, dependency);
	}
	result.insert(target);
	node_states[target].visited = NodeVisitStatus::done;
}

void prunefl::Driver::try_write_cache() const {
	if (!cache_file.has_value()) {
		return;
	}
	std::filesystem::path cache_path{*cache_file};
	nlohmann::json file_hashes;
	for (auto &path : result) {
		auto current_hash = hash_file(path);
		// file must have been processed, unless the user is deleting
		// things this should not be encountered
		assert(current_hash.has_value());
		file_hashes[path.string()] = *current_hash;
	}

	nlohmann::json meta;
	meta["prunefl_cache_version"] = 1;

	nlohmann::json prunefl_cache_info{
		{"meta", meta},
		{"file_hashes", file_hashes},
		{"input_file_list", input_file_list},
		{"included_files", result_includes}
	};

	std::ofstream writer(cache_path);
	writer << prunefl_cache_info;
	writer.close();
}

const tsl::ordered_set<std::filesystem::path> &
prunefl::Driver::get_sorted_set() {
	if (!result.empty()) {
		return result;
	}

	// prepare checks there is exactly one top instance
	auto instance = root->topInstances[0];
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
		topological_sort_recursive(id_result, node_states, current_node);
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
		auto file = sourceManager.getFullPath(id);
		if (print_include_dirs() && result_includes.count(file) &&
			!input_file_list.count(file)) {
			// Do not add included files to list unless they are explicitly
			// listed as an input.
			continue;
		}
		result.insert(file);
	}
	return result;
}

const tsl::ordered_set<std::filesystem::path>
prunefl::Driver::get_include_directories() const {
	tsl::ordered_set<std::filesystem::path> include_dirs;
	for (auto &file : result_includes) {
		include_dirs.insert(file.parent_path());
	}
	return include_dirs;
}
