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

#pragma once

#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/driver/Driver.h>

#include <tsl/ordered_set.h>

namespace prunefl {
	class Driver : public slang::driver::Driver {
	public:
		Driver();

		void parse_cli(int argc, char *argv[]);

		/**
		 * @brief Prepares the file list for preprocessing and performs a
		 * preliminary compilation check.
		 *
		 * The preparation phase of the prunefl driver prepares the file list
		 * for the preprocessor passes. It also runs a preliminary compilation
		 * to verify that the pruned file list compiles properly.
		 *
		 * Processing is terminated if the file list is invalid.
		 *
		 * If a valid cache file is found, the data is re-populated and
		 * compilation is skipped to save time.
		 *
		 * @exception std::runtime_error if any fatal compilation issues occur.
		 */
		void prepare();

		/**
		 * @brief Must be run after prepare().
		 *
		 * Produces the subset of files required to successfully compile the
		 * requested top module in reverse-topological order.
		 *
		 * @returns A reference to a container inside which the result is
		 *  emplaced.
		 *
		 * @exception std::runtime_error if a cycle is detected during the
		 * topological sort.
		 */
		const tsl::ordered_set<std::filesystem::path> &get_sorted_set();

		/**
		 * @brief To be optionally run after prepare() (at any point).
		 *
		 * @returns A container inside which include search paths
		 * 	are emplaced.
		 */
		const tsl::ordered_set<std::filesystem::path>
		get_include_directories() const;

		/**
		 * @returns Whether the user specified --include-dirs or not.
		 */
		bool print_include_dirs() const { return include_dirs.has_value(); }

		/**
		 * @brief Writes the final results to the file specified by --cache-to.
		 */
		void try_write_cache() const;

	private:
		// types
		enum class NodeVisitStatus {
			unvisited = 0,
			in_progress,
			done,
		};

		struct NodeState {
			NodeVisitStatus visited = NodeVisitStatus::unvisited;
			bool peer_dependencies_enqueued = false;
		};

		// methods
		bool load_cache();
		void topological_sort_recursive(
			tsl::ordered_set<slang::BufferID> &result,
			std::unordered_map<slang::BufferID, prunefl::Driver::NodeState>
				&node_states,
			slang::BufferID target
		);

		// members
		std::unique_ptr<slang::ast::Compilation> compilation;
		std::set<std::filesystem::path> input_file_list;
		tsl::ordered_set<std::filesystem::path> result_includes;
		tsl::ordered_set<std::filesystem::path> result;
		const slang::ast::RootSymbol *root = nullptr;

		// cli
		std::optional<bool> show_help;
		std::optional<bool> show_version;
		std::optional<bool> include_dirs;
		std::optional<std::string> cache_file;
	};
} // namespace prunefl
