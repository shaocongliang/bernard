#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <Scanner.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

class ExprNode {
public:
    ExprNode() = default;

    virtual llvm::Value *CodeGen() = 0;

    virtual ~ExprNode() = default;
};

class NumberNode : public ExprNode {
public:
    explicit NumberNode(const double &num) : m_number(num) {}

    llvm::Value *CodeGen();

    double m_number;
};

class VariableNode : public ExprNode {
public:
    explicit VariableNode(const std::string &name) : m_name(name) {}

    llvm::Value *CodeGen() override;

private:
    std::string m_name;
};

class BinaryOpNode : public ExprNode {
public:
    BinaryOpNode(char op, std::unique_ptr<ExprNode> &lhs, std::unique_ptr<ExprNode> &rhs)
            : m_op(op), mp_lhs(std::move(lhs)), mp_rhs(std::move(rhs)) {}

    virtual llvm::Value *CodeGen() override;

    char m_op;
    std::unique_ptr<ExprNode> mp_lhs;
    std::unique_ptr<ExprNode> mp_rhs;
};

class ConditionNode : public ExprNode {
public:
    ConditionNode(std::unique_ptr<ExprNode> &cond, std::unique_ptr<ExprNode> &then, std::unique_ptr<ExprNode> &el) :
            m_cond(std::move(cond)), m_then(std::move(then)), m_else(std::move(el)) {}

    llvm::Value *CodeGen() override;

private:
    std::unique_ptr<ExprNode> m_cond;
    std::unique_ptr<ExprNode> m_then;
    std::unique_ptr<ExprNode> m_else;
};

class ForLoopNode : public ExprNode {
public:
    ForLoopNode(const std::string &varName, std::unique_ptr<ExprNode> start, std::unique_ptr<ExprNode> end,
                std::unique_ptr<ExprNode> step,
                std::unique_ptr<ExprNode> body) : m_valName(varName), mp_start(std::move(start)),
                                                   mp_end(std::move(end)),
                                                   mp_step(std::move(step)), mp_body(std::move(body)) {}

    llvm::Value *CodeGen() override;

private:
    std::string m_valName;
    std::unique_ptr<ExprNode> mp_start;
    std::unique_ptr<ExprNode> mp_end;
    std::unique_ptr<ExprNode> mp_step;
    std::unique_ptr<ExprNode> mp_body;
};

class FunctionDeclAst {
public:
    FunctionDeclAst(const std::string &name, const std::vector<std::string> &args) : m_name(name), m_args(args) {}

    llvm::Function *CodeGen();
    std::string Name() const { return m_name; }
private:
    std::string m_name;
    std::vector<std::string> m_args;
};

class FunctionDefAst {
public:
    FunctionDefAst(std::unique_ptr<FunctionDeclAst> decl, std::unique_ptr<ExprNode> &body) : m_decl(std::move(decl)),
                                                                                             m_body(std::move(body)) {
    }

    llvm::Function *CodeGen();

private:
    std::unique_ptr<FunctionDeclAst> m_decl;
    std::unique_ptr<ExprNode> m_body;
};

class FunctionCallNode : public ExprNode {
public:
    FunctionCallNode(const std::string &callee, std::vector<std::unique_ptr<ExprNode>> args) : m_callee(callee),
                                                                                               m_args(std::move(
                                                                                                       args)) {}

    llvm::Value *CodeGen() override;

private:
    std::string m_callee;
    std::vector<std::unique_ptr<ExprNode>> m_args;
};

struct ExprTree {
    ExprTree(ExprTree *lhs, ExprTree *rhs, const std::string &val) :
            mp_left(lhs), mp_right(rhs), m_val(val) {}

    ExprTree(const std::string &val) : mp_left(nullptr), mp_right(nullptr), m_val(val) {}

    ExprTree *mp_left;
    ExprTree *mp_right;
    std::string m_val;
};

ExprTree *BuildExprTree(Scanner &scan);

void PrintTreeInOrder(ExprTree *root);

int Calc(ExprTree *root);

double Calc(ExprNode *binaryOp);

std::unique_ptr<ExprNode> ParseExpression(const Scanner &scan);

void MainLoop(const Scanner &scanner);

