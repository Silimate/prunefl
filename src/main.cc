// From prunefl

// MIT License

// Copyright (c) 2025 Silimate Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "driver.hh"
#include "file.hh"

#include <cstdio>
#include <stack>

int main(int argc, char *argv[]) {
	prunefl::Driver driver;

	try {
		driver.parse_cli(argc, argv);
		driver.prepare();
		driver.preprocess();
		driver.implicit_macro_resolution();
		auto top_node = driver.module_resolution();

		std::vector<std::filesystem::path> result;
		driver.topological_sort(result, top_node);
		for (auto &file : result) {
			fmt::println("{}", file.c_str());
		}

	} catch (std::runtime_error &e) {
		slang::OS::printE(fmt::format("[FATAL] {}\n", e.what()));
		return -1;
	}
}
