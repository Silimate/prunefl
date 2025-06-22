#include "driver.hh"
#include "file.hh"

#include <cstdio>
#include <stack>

using namespace slang;

int main(int argc, char *argv[]) {
	Driver driver;

	try {
		driver.parse_cli(argc, argv);
		driver.prepare();
		driver.preprocess();
		auto top_node = driver.module_resolution();
		driver.implicit_macro_resolution();

		std::vector<std::filesystem::path> result;
		driver.topological_sort(result, top_node);
		for (auto &file : result) {
			fmt::println("{}", file.c_str());
		}

	} catch (std::runtime_error &e) {
		OS::printE(fmt::format("[FATAL] {}\n", e.what()));
		return -1;
	}
}
