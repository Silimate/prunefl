#include <iostream>
#include "access_private.hpp"

#include "slang/ast/ASTVisitor.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/text/SourceLocation.h"

using slang::syntax::SyntaxNode;
using slang::syntax::SyntaxTree;
    
void traverseSyntaxTree(const SyntaxNode* node, int indent = 0) {
    if (!node) return;
    
    for (int i = 0; i < indent; ++i)
        std::cout << "  ";
        
    std::cout << "Node kind: " << static_cast<int>(node->kind) << "\n";

    size_t childCount = node->getChildCount();
    for (size_t i = 0; i < childCount; ++i) {
        const SyntaxNode* child = node->childNode(i);
        if (child) {
            traverseSyntaxTree(child, indent + 1);
        }
        else {
            const auto token = node->childToken(i);
            if (token) {
                for (int j = 0; j < indent + 1; ++j)
                    std::cout << "  ";
                std::cout << "Token: " << token.rawText() << std::endl;
            }
        }
    }
}

// HACK: Make private static function available -- the reason is we want to get
// access to the buffer, which we can't otherwise
ACCESS_PRIVATE_STATIC_FUN(slang::syntax::SyntaxTree, std::shared_ptr<slang::syntax::SyntaxTree>(
    slang::SourceManager &,
    std::span<const slang::SourceBuffer>,
    const slang::Bag &,
    slang::syntax::SyntaxTree::MacroList,
    bool
), create);

struct SVFile {
    slang::BufferID id;
    std::vector<std::string> exported_macros = {};
    std::vector<std::string> unresolved_macros = {};
};

int main(int argc, char *argv[]) {
    // preprocessor
    slang::SourceManager manager;
    
    auto buffer = manager.readSource(argv[1], nullptr);
    if (!buffer)
        throw "wtf";
    slang::Bag empty;
    SyntaxTree::MacroList ml;
    std::shared_ptr<SyntaxTree> ast = call_private_static::slang::syntax::SyntaxTree::create(manager, std::span(&buffer.value(), 1), empty, ml, false);

    SVFile f { buffer->id };
    
    // 0. Exported macros
    auto macros = ast->getDefinedMacros();
    for (auto macro: macros) {
        auto loc = macro->sourceRange();
        if (loc.start().buffer() != f.id) {
            continue;
        }
        f.exported_macros.push_back(std::string(macro->name.rawText()));
    }
    
    // 1. Unresolved macros
    traverseSyntaxTree(&ast->root());
}
