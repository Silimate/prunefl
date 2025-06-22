#include "driver.hh"
#include "file.hh"

#include <cstdio>
#include <stack>

int main(int argc, char *argv[]) {
	nodo::Driver driver;

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
