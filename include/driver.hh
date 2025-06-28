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
		 * Processing is not terminated if the file list is invalid, but this
		 * may affect the final results if one or more files being pruned are
		 * part of the compilation hierarchy.
		 */
		void prepare();

		/**
		 * @brief To be run last.
		 *
		 * Produces the subset of files required to successfully compile the
		 * requested top module in reverse-topological order.
		 *
		 * @param result A reference to a container inside which the result is
		 *  emplaced.
		 *
		 * @exception std::runtime_error if a cycle is detected during the
		 * topological sort.
		 */
		void topological_sort(std::vector<std::filesystem::path> &result);

	private:
		// types
		enum class NodeState {
			unvisited = 0,
			visiting,
			visited,
		};

		bool topological_sort_recursive(
			std::vector<std::filesystem::path> &result,
			std::unordered_map<std::filesystem::path, NodeState> &node_states,
			slang::BufferID target
		);

		// members
		std::unique_ptr<slang::ast::Compilation> compilation;

		// cli
		std::optional<bool> show_help;
		std::optional<bool> show_version;
	};
} // namespace prunefl
