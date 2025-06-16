#include "file.hh"

#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>
#include <slang/util/BumpAllocator.h>

#include <argparse/argparse.hpp>

int main(int argc, char *argv[]) {
	argparse::ArgumentParser nodo_cmd(argc > 0 ? argv[0] : "nodo", "0.1.0");
	nodo_cmd.add_argument("files").remaining();
	try {
		nodo_cmd.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << err.what() << std::endl;
		std::cerr << nodo_cmd;
		return -1;
	}
	try {
		slang::SourceManager manager;
		slang::BumpAllocator alloc;
		auto files = nodo_cmd.get<std::vector<std::string>>("files");
		for (auto &file : files) {
			auto info = SVFile(file, manager, alloc);
			info.output();
		}
	} catch (std::logic_error &e) {
		// NO files
	}
	return 0;
}
