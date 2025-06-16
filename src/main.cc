#include "slang/ast/ASTVisitor.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/parsing/TokenKind.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxKind.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/text/SourceLocation.h"

#include <fmt/format.h>
#include <argparse.hpp>
#include <access_private.hpp>

#include <unordered_map>
#include <unordered_set>

// HACK: Need raw preprocessor tokens to identify macro usages…
ACCESS_PRIVATE_FUN(slang::parsing::Preprocessor, slang::parsing::Token(), nextRaw);

struct SVFile {
    slang::SourceManager *manager;
    slang::BufferID id;
    
    SVFile(slang::SourceManager *manager, slang::BufferID id): manager(manager), id(id) {}
    
    void process_define(const slang::syntax::DefineDirectiveSyntax *define) {
        std::string name { define->name.rawText() } ;
        auto location = define->getFirstToken().location();
        if (location.buffer() == id) {
            exported_macros[std::move(name)] = std::move(location);
        }
    }
    
    void process_usage(const slang::parsing::Token &token) {
        auto raw = token.rawText();
        std::string name { raw.begin() + 1, raw.end() };
        auto sameFileExportedLoc = exported_macros.find(name);
        if (sameFileExportedLoc == exported_macros.end()
            || sameFileExportedLoc->second > token.location()) {
            unresolved_macros.insert(std::move(name));
        }
    }
    
    void output(FILE *f = stderr) {
        fmt::println(f, "{}:", manager->getRawFileName(id));
        fmt::println(f, "  exported:");
        for (auto &macro: exported_macros) {
            fmt::println(f, "    - {}", macro.first);
        }
        fmt::println(f, "  used:");
        for (auto &macro: unresolved_macros) {
            fmt::println(f, "    - {}", macro);
        }
        fflush(f);
    }
    
private:
    std::unordered_map<std::string, slang::SourceLocation> exported_macros = {};
    std::unordered_set<std::string> unresolved_macros = {};
};

void add_exports(SVFile &f, slang::SourceBuffer &buffer, slang::SourceManager &manager, slang::BumpAllocator &alloc, slang::Diagnostics &diagnostics) {
    slang::parsing::Preprocessor preprocessor(manager, alloc, diagnostics, {}, {});
    preprocessor.pushSource(buffer);
    
    auto token = preprocessor.next();
    while (token.kind != slang::parsing::TokenKind::EndOfFile) {
        token = preprocessor.next();
    }
    
    for (auto macro: preprocessor.getDefinedMacros()) {
        f.process_define(macro);
    }
}

void add_unresolved_imports(SVFile &f, slang::SourceBuffer &buffer, slang::SourceManager &manager, slang::BumpAllocator &alloc, slang::Diagnostics &diagnostics) {
    slang::parsing::Preprocessor preprocessor(manager, alloc, diagnostics, {}, {});
    preprocessor.pushSource(buffer);
    
    auto token = call_private::nextRaw(preprocessor);
    while (token.kind != slang::parsing::TokenKind::EndOfFile) {
        if (token.kind == slang::parsing::TokenKind::Directive) {
            auto directiveKind = token.directiveKind();
            if (token.directiveKind() == slang::syntax::SyntaxKind::MacroUsage) {
                f.process_usage(token);
            }
        }
        token = call_private::nextRaw(preprocessor);
    }
}

SVFile get_macro_info(slang::SourceManager &manager, std::string_view file_path) {
    auto buffer = manager.readSource(file_path, nullptr);
    if (!buffer) {
        throw std::runtime_error("Failed to read");
    }
    slang::BumpAllocator alloc;
    slang::Diagnostics diagnostics;

    SVFile f { &manager, buffer->id };

    const slang::SourceLibrary* library = buffer->library;
    
    // Pass 1: Get produced
    add_exports(
        f,
        *buffer,
        manager,
        alloc,
        diagnostics
    );
    
    // Pass 2: Get consumed
    add_unresolved_imports(
        f,
        *buffer,
        manager,
        alloc,
        diagnostics
    );
    
    return f;
}

int main(int argc, char *argv[]) {
    argparse::ArgumentParser nodo_cmd(argc > 0 ? argv[0] : "nodo", "0.1.0");
    nodo_cmd.add_argument("files").remaining();
    try {
        nodo_cmd.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << nodo_cmd;
        return -1;
    }
    try {
        slang::SourceManager manager;
        auto files = nodo_cmd.get<std::vector<std::string>>("files");
        std::cout << files.size() << " files provided" << std::endl;
        for (auto& file : files) {
            auto info = get_macro_info(manager, file);
            info.output();
        }
    } catch (std::logic_error& e) {
        // NO files
    }
    return 0;
}
