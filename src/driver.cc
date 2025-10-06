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
namespace fs = std::filesystem;

static std::optional<std::string> hash_file(fs::path path) {
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
		"--output-flags-to",
		output_flags_to,
		"Instead of explicitly listing included files and modules, "
		"output '+incdir+/-y/-Y/+define+' to this file. "
		"--output must be specified, in which case it will be passed "
	);
	cmdLine.add(
		"--verific-compat",
		verific_compat,
		"For --output-flags-to, only output flags compatible with the Verific "
		"frontend."
	);
	cmdLine.add("--output", output, "The file to output file paths to");

	options.compat = slang::driver::CompatMode::All;
	options.timeScale = "1ns/1ns";
	options.singleUnit = true;
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
	if (!output.has_value()) {
		if (output_flags_to.has_value()) {
			throw std::runtime_error(
				"--output must be specified if --output-flags-to is specified."
			);
		}
	}

	if (options.topModules.size() != 1) {
		throw std::runtime_error(
			"Exactly one top module should be provided. (--top …)"
		);
	}
}

bool prunefl::Driver::load_cache() {
	fs::path cache_path(*cache_file);
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

	if (j["meta"]["prunefl_cache_version"] != 2) {
		fmt::println(
			stderr,
			"Cache is incompatible with current version of prunefl. Pruning "
			"new file list…"
		);
		return false;
	}

	std::set<fs::path> cache_loaded_files;
	auto cache_input_file_list = j["input_file_list"];
	for (auto it = cache_input_file_list.begin();
		 it != cache_input_file_list.end();
		 it++) {
		cache_loaded_files.insert(fs::path(*it));
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
		auto path = fs::path(it.key());
		auto au_hash = it.value().get<std::string>();
		auto current_hash = hash_file(path);
		if (current_hash != au_hash) {
			fmt::println(
				stderr, "File {} changed, re-running prune…", path.c_str()
			);
			cache_hit = false;
			break;
		}
	}
	if (cache_hit) {
		fmt::println(
			stderr, "Input files have not changed, loading from cache…"
		);
		for (const auto &file : j["result"]) {
			result.insert(fs::path(file.get<std::string>()));
		}
		for (const auto &file : j["result_includes"]) {
			result_includes.insert(fs::path(file.get<std::string>()));
		}
		for (const auto &file : j["result_library_files"]) {
			result_library_files.insert(fs::path(file.get<std::string>()));
		}
		return true;
	}
	result.clear();
	return false;
}

std::vector<fs::path> prunefl::Driver::getIncludes() const {
	flat_hash_set<fs::path> includeSet;

	for (auto &tree : syntaxTrees) {
		for (auto &inc : tree->getIncludeDirectives()) {
			if (inc.isSystem)
				continue;

			includeSet.insert(sourceManager.getFullPath(inc.buffer.id));
		}
	}

	return std::vector<fs::path>(includeSet.begin(), includeSet.end());
}

bool prunefl::Driver::gather_input_files() {
	auto loaded_files = sourceLoader.loadSources();

	// source files
	for (auto &buffer : loaded_files) {
		auto full_path = sourceManager.getFullPath(buffer.id);
		input_file_list.insert(full_path);
	}

	// library files
	for (auto &[library, path] : sourceLoader.getLibraryFiles()) {
		input_file_list.insert(path);
	}

	// library map files
	for (auto &path : sourceLoader.getLibraryMapFiles()) {
		input_file_list.insert(path);
	}

	for (auto &path : getProcessedCommandFiles()) {
		input_file_list.insert(fs::absolute(path));
	}

	return cache_file.has_value() && load_cache();
}

void prunefl::Driver::prepare() {
	if (gather_input_files()) {
		return; // cached
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

	auto included_files = getIncludes();
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

template <typename T>
void add_file_hashes(nlohmann::json &object, const T &files) {
	for (auto &path : files) {
		auto current_hash = hash_file(path);
		// file must have been processed, unless the user is deleting
		// things this should not be encountered
		assert(current_hash.has_value());
		object[path.string()] = *current_hash;
	}
}

void prunefl::Driver::try_write_cache() const {
	if (!cache_file.has_value()) {
		return;
	}
	fs::path cache_path{*cache_file};
	nlohmann::json file_hashes;

	add_file_hashes(file_hashes, result);
	add_file_hashes(file_hashes, result_library_files);
	add_file_hashes(file_hashes, result_includes);
	add_file_hashes(file_hashes, input_file_list);
	nlohmann::json meta;
	meta["prunefl_cache_version"] = 2;

	nlohmann::json prunefl_cache_info{
		{"meta", meta},
		{"input_file_list", input_file_list},
		{"file_hashes", file_hashes},
		{"result", result},
		{"result_includes", result_includes},
		{"result_library_files", result_library_files},
	};

	std::ofstream writer(cache_path);
	writer << prunefl_cache_info;
	writer.close();
}

const tsl::ordered_set<fs::path> &prunefl::Driver::get_sorted_set() {
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

	for (auto &[library, path] : sourceLoader.getLibraryFiles()) {
		result_library_files.insert(path);
	}

	for (auto id : id_result) {
		auto file = sourceManager.getFullPath(id);
		if (result_includes.count(file) && !input_file_list.count(file)) {
			// Do not add included files to list unless they are explicitly
			// listed as an input.
			continue;
		}
		if (result_library_files.count(file)) {
			// Do not add library files to list.
			continue;
		}
		if (file.string().starts_with("<unnamed_buffer")) {
			continue;
		}
		result.insert(file);
	}
	return result;
}

void prunefl::Driver::write_pruned_file_list() {
	auto sorted_set = get_sorted_set();

	std::string out_string = "";

	for (const auto &file : sorted_set) {
		out_string += fmt::format("{}\n", file.c_str());
	}

	if (!output_flags_to.has_value()) {
		for (const auto &file : result_includes) {
			out_string += fmt::format("{}\n", file.c_str());
		}
		for (const auto &file : result_library_files) {
			out_string += fmt::format("{}\n", file.c_str());
		}
	}

	if (output.has_value()) {
		OS::writeFile(*output, out_string);
	} else {
		OS::print(out_string);
	}
}

void prunefl::Driver::write_output_flags() const {
	if (!output_flags_to.has_value()) {
		return;
	}

	bool verific_compat_mode = verific_compat.has_value() && *verific_compat;
	tsl::ordered_set<std::string> output_flags;

	// defines
	for (auto &define : options.defines) {
		output_flags.insert(fmt::format("+define+{}", define));
	}

	// include search directories
	for (auto &file : result_includes) {
		output_flags.insert(
			fmt::format("+incdir+{}", file.parent_path().string())
		);
	}

	// module search directories
	for (auto &dir : sourceLoader.getSearchDirectories()) {
		output_flags.insert(fmt::format("-y {}", dir.string()));
	}

	// module search extensions
	std::string_view libext_pfx = verific_compat_mode ? "+libext+" : "-Y ";
	for (auto &ext : sourceLoader.getSearchExtensions()) {
		output_flags.insert(fmt::format("{}{}", libext_pfx, ext.string()));
	}

	// library files
	for (auto &[library, path] : sourceLoader.getLibraryFiles()) {
		if (library == nullptr) {
			output_flags.insert(fmt::format("-v {}", path.c_str()));
		} else {
			output_flags.insert(
				fmt::format("-v {}={}", library->name, path.c_str())
			);
		}
	}

	// input command file
	std::string_view cmdfile_pfx =
		verific_compat_mode
			? "-f " // verific is single-unit by default, so -C doesn't matter
			: "-C ";

	output_flags.insert(
		fmt::format("{}{}", cmdfile_pfx, fs::absolute(*output).c_str())
	);

	std::string out_string;
	for (auto &flag : output_flags) {
		out_string += fmt::format("{}\n", flag);
	}

	OS::writeFile(*output_flags_to, out_string);
}
