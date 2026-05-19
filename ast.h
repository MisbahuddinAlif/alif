#ifndef AST_H
#define AST_H

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>

using namespace std;

// ============================================================
// BASE CLASS
// ============================================================

class ASTNode {
public:
    virtual ~ASTNode() {}
    virtual string generate_code(ofstream& outcode,
                                 map<string, string>& symbol_to_temp,
                                 int& temp_count,
                                 int& label_count) const = 0;
};

// ============================================================
// EXPRESSION NODES
// ============================================================

class ExprNode : public ASTNode {
protected:
    string node_type;
public:
    ExprNode(string type) : node_type(type) {}
    virtual string get_type() const { return node_type; }
};

// ------------------------------------------------------------------
// VarNode  –  variable reference  (e.g. x  or  arr[i])
// ------------------------------------------------------------------
class VarNode : public ExprNode {
private:
    string name;
    ExprNode* index; // non-null for array access

public:
    VarNode(string name, string type, ExprNode* idx = nullptr)
        : ExprNode(type), name(name), index(idx) {}

    ~VarNode() { if (index) delete index; }

    bool has_index() const { return index != nullptr; }

    string generate_index_code(ofstream& outcode,
                               map<string, string>& symbol_to_temp,
                               int& temp_count,
                               int& label_count) const {
        if (index) {
            return index->generate_code(outcode, symbol_to_temp,
                                        temp_count, label_count);
        }
        return "0";
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        if (!has_index()) {
            return name;
        } else {
            string idx_val = generate_index_code(outcode, symbol_to_temp,
                                                 temp_count, label_count);
            string temp = "t" + to_string(temp_count++);
            outcode << temp << " = " << name << "[" << idx_val << "]\n";
            return temp;
        }
    }

    string get_name() const { return name; }
};

// ------------------------------------------------------------------
// ConstNode  –  numeric constant  (e.g. 42, 3.14)
// ------------------------------------------------------------------
class ConstNode : public ExprNode {
private:
    string value;

public:
    ConstNode(string val, string type) : ExprNode(type), value(val) {}

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        return value;
    }
};

// ------------------------------------------------------------------
// BinaryOpNode  –  left op right  (+, -, *, /, %, &&, ||, <, >, etc.)
// ------------------------------------------------------------------
class BinaryOpNode : public ExprNode {
private:
    string op;
    ExprNode* left;
    ExprNode* right;

public:
    BinaryOpNode(string op, ExprNode* left, ExprNode* right, string result_type)
        : ExprNode(result_type), op(op), left(left), right(right) {}

    ~BinaryOpNode() {
        delete left;
        delete right;
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        string left_val  = left->generate_code(outcode, symbol_to_temp,
                                               temp_count, label_count);
        string right_val = right->generate_code(outcode, symbol_to_temp,
                                                temp_count, label_count);
        string temp = "t" + to_string(temp_count++);
        outcode << temp << " = " << left_val << " " << op
                << " " << right_val << "\n";
        return temp;
    }
};


// UnaryOpNode  –  op expr  (e.g. -x, !flag)
class UnaryOpNode : public ExprNode {
private:
    string op;
    ExprNode* expr;

public:
    UnaryOpNode(string op, ExprNode* expr, string result_type)
        : ExprNode(result_type), op(op), expr(expr) {}

    ~UnaryOpNode() { delete expr; }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        string expr_val = expr->generate_code(outcode, symbol_to_temp,
                                              temp_count, label_count);
        string temp = "t" + to_string(temp_count++);
        outcode << temp << " = " << op << expr_val << "\n";
        return temp;
    }
};

// ------------------------------------------------------------------
// AssignNode  –  lhs = rhs
// ------------------------------------------------------------------
class AssignNode : public ExprNode {
private:
    VarNode* lhs;
    ExprNode* rhs;

public:
    AssignNode(VarNode* lhs, ExprNode* rhs, string result_type)
        : ExprNode(result_type), lhs(lhs), rhs(rhs) {}

    ~AssignNode() {
        delete lhs;
        delete rhs;
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        if (lhs->has_index()) {
            string idx_val = lhs->generate_index_code(outcode, symbol_to_temp,
                                                      temp_count, label_count);
            string rhs_val = rhs->generate_code(outcode, symbol_to_temp,
                                                temp_count, label_count);
            outcode << lhs->get_name() << "[" << idx_val << "] = "
                    << rhs_val << "\n";
            return lhs->get_name() + "[" + idx_val + "]";
        } else {
            string rhs_val = rhs->generate_code(outcode, symbol_to_temp,
                                                temp_count, label_count);
            outcode << lhs->get_name() << " = " << rhs_val << "\n";
            return lhs->get_name();
        }
    }
};

// ------------------------------------------------------------------
// IncDecNode  –  var++ or var-- 
// Safe post-increment node to avoid double memory freeing
// ------------------------------------------------------------------
class IncDecNode : public ExprNode {
private:
    VarNode* var;
    string op; 
public:
    IncDecNode(VarNode* v, string op, string result_type)
        : ExprNode(result_type), var(v), op(op) {}

    ~IncDecNode() { delete var; }

    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp, int& temp_count, int& label_count) const override {
        string var_val = var->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        
        string temp1 = "t" + to_string(temp_count++);
        outcode << temp1 << " = " << var_val << "\n";

        string temp2 = "t" + to_string(temp_count++);
        string math_op = (op == "++") ? "+" : "-";
        outcode << temp2 << " = " << temp1 << " " << math_op << " 1\n";

        if (var->has_index()) {
            string idx_val = var->generate_index_code(outcode, symbol_to_temp, temp_count, label_count);
            outcode << var->get_name() << "[" << idx_val << "] = " << temp2 << "\n";
        } else {
            outcode << var->get_name() << " = " << temp2 << "\n";
        }
        return temp1; 
    }
};

// ============================================================
// STATEMENT NODES
// ============================================================

class StmtNode : public ASTNode {
public:
    virtual string generate_code(ofstream& outcode,
                                 map<string, string>& symbol_to_temp,
                                 int& temp_count,
                                 int& label_count) const = 0;
};

// ------------------------------------------------------------------
// ExprStmtNode  –  expression used as a statement  (e.g. i = i+1;)
// ------------------------------------------------------------------
class ExprStmtNode : public StmtNode {
private:
    ExprNode* expr;

public:
    ExprStmtNode(ExprNode* e) : expr(e) {}
    ~ExprStmtNode() { if (expr) delete expr; }

    ExprNode* get_expr() const { return expr; }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        if (expr) {
            return expr->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }
        return "";
    }
};

// ------------------------------------------------------------------
// BlockNode  –  sequence of statements between { }
// ------------------------------------------------------------------
class BlockNode : public StmtNode {
private:
    vector<StmtNode*> statements;

public:
    ~BlockNode() {
        for (auto stmt : statements) delete stmt;
    }

    void add_statement(StmtNode* stmt) {
        if (stmt) statements.push_back(stmt);
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        for (auto stmt : statements) {
            stmt->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }
        return "";
    }
};

// ------------------------------------------------------------------
// IfNode  –  if (cond) then_block [ else else_block ]
// ------------------------------------------------------------------
class IfNode : public StmtNode {
private:
    ExprNode* condition;
    StmtNode* then_block;
    StmtNode* else_block;

public:
    IfNode(ExprNode* cond, StmtNode* then_stmt, StmtNode* else_stmt = nullptr)
        : condition(cond), then_block(then_stmt), else_block(else_stmt) {}

    ~IfNode() {
        delete condition;
        delete then_block;
        if (else_block) delete else_block;
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        string cond_val = condition->generate_code(outcode, symbol_to_temp,
                                                   temp_count, label_count);

        string true_label  = "L" + to_string(label_count++);
        string false_label = "L" + to_string(label_count++);

        outcode << "if " << cond_val << " goto " << true_label << "\n";
        outcode << "goto " << false_label << "\n";

        outcode << true_label << ":\n";
        then_block->generate_code(outcode, symbol_to_temp, temp_count, label_count);

        if (else_block) {
            string end_label = "L" + to_string(label_count++);
            outcode << "goto " << end_label << "\n";

            outcode << false_label << ":\n";
            else_block->generate_code(outcode, symbol_to_temp, temp_count, label_count);

            outcode << end_label << ":\n";
        } else {
            outcode << false_label << ":\n";
        }

        return "";
    }
};

// ------------------------------------------------------------------
// WhileNode  –  while (cond) body
// ------------------------------------------------------------------
class WhileNode : public StmtNode {
private:
    ExprNode* condition;
    StmtNode* body;

public:
    WhileNode(ExprNode* cond, StmtNode* body_stmt)
        : condition(cond), body(body_stmt) {}

    ~WhileNode() {
        delete condition;
        delete body;
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        string start_label = "L" + to_string(label_count++);
        string body_label  = "L" + to_string(label_count++);
        string end_label   = "L" + to_string(label_count++);

        outcode << start_label << ":\n";
        string cond_val = condition->generate_code(outcode, symbol_to_temp,
                                                   temp_count, label_count);
        outcode << "if " << cond_val << " goto " << body_label << "\n";
        outcode << "goto " << end_label << "\n";

        outcode << body_label << ":\n";
        body->generate_code(outcode, symbol_to_temp, temp_count, label_count);

        outcode << "goto " << start_label << "\n";
        outcode << end_label << ":\n";

        return "";
    }
};

// ------------------------------------------------------------------
// ForNode  –  for (init; cond; update) body
// ------------------------------------------------------------------
class ForNode : public StmtNode {
private:
    ASTNode* init;       
    ASTNode* condition;  
    ExprNode* update;    
    StmtNode* body;

public:
    ForNode(ASTNode* init_expr, ASTNode* cond_expr,
            ExprNode* update_expr, StmtNode* body_stmt)
        : init(init_expr), condition(cond_expr),
          update(update_expr), body(body_stmt) {}

    ~ForNode() {
        if (init)      delete init;
        if (condition) delete condition;
        if (update)    delete update;
        delete body;
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        if (init) {
            init->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }

        string start_label = "L" + to_string(label_count++);
        string body_label  = "L" + to_string(label_count++);
        string end_label   = "L" + to_string(label_count++);

        outcode << start_label << ":\n";
        if (condition) {
            string cond_val = condition->generate_code(outcode, symbol_to_temp,
                                                       temp_count, label_count);
            if (cond_val == "") cond_val = "1"; // Fallback if cond_val is empty
            outcode << "if " << cond_val << " goto " << body_label << "\n";
            outcode << "goto " << end_label << "\n";
        } else {
             outcode << "goto " << body_label << "\n";
        }

        outcode << body_label << ":\n";
        body->generate_code(outcode, symbol_to_temp, temp_count, label_count);

        if (update) {
            update->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }

        outcode << "goto " << start_label << "\n";
        outcode << end_label << ":\n";

        return "";
    }
};

// ------------------------------------------------------------------
// ReturnNode  –  return [expr];
// ------------------------------------------------------------------
class ReturnNode : public StmtNode {
private:
    ExprNode* expr;

public:
    ReturnNode(ExprNode* e) : expr(e) {}
    ~ReturnNode() { if (expr) delete expr; }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        if (expr) {
            string val = expr->generate_code(outcode, symbol_to_temp,
                                             temp_count, label_count);
            outcode << "return " << val << "\n";
            return val;
        } else {
            outcode << "return\n";
            return "";
        }
    }
};

// ------------------------------------------------------------------
// DeclNode  –  variable declaration  (e.g. int a, b[10];)
// ------------------------------------------------------------------
class DeclNode : public StmtNode {
private:
    string type;
    vector<pair<string, int>> vars; // (name, array_size) – 0 = not an array

public:
    DeclNode(string t) : type(t) {}

    void add_var(string name, int array_size = 0) {
        vars.push_back(make_pair(name, array_size));
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        for (auto& var : vars) {
            if (var.second > 0) {
                outcode << "# declare " << type << " "
                        << var.first << "[" << var.second << "]\n";
            } else {
                outcode << "# declare " << type << " " << var.first << "\n";
            }
        }
        return "";
    }

    string get_type() const { return type; }
    const vector<pair<string, int>>& get_vars() const { return vars; }
};

// ------------------------------------------------------------------
// PrintNode  –  printf(var);
// ------------------------------------------------------------------
class PrintNode : public StmtNode {
private:
    string var_name;

public:
    PrintNode(string name) : var_name(name) {}

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        outcode << "print " << var_name << "\n";
        return "";
    }
};

// ============================================================
// FUNCTION-RELATED NODES
// ============================================================

// ------------------------------------------------------------------
// FuncDeclNode  –  full function definition  (header + body)
// ------------------------------------------------------------------
class FuncDeclNode : public ASTNode {
private:
    string return_type;
    string name;
    vector<pair<string, string>> params; // (type, param_name)
    BlockNode* body;

public:
    FuncDeclNode(string ret_type, string n)
        : return_type(ret_type), name(n), body(nullptr) {}

    ~FuncDeclNode() { if (body) delete body; }

    void add_param(string type, string param_name) {
        params.push_back(make_pair(type, param_name));
    }

    void set_body(BlockNode* b) { body = b; }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        outcode << "\n# Function: " << name
                << " (returns " << return_type << ")\n";
        outcode << "func " << name << ":\n";

        for (auto& param : params) {
            outcode << "# param: " << param.first << " " << param.second << "\n";
        }

        if (body) {
            body->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }

        outcode << "endfunc\n";
        return "";
    }
};

// ------------------------------------------------------------------
// ArgumentsNode  –  helper to collect function-call arguments
// ------------------------------------------------------------------
class ArgumentsNode : public ASTNode {
private:
    vector<ExprNode*> args;

public:
    ~ArgumentsNode() {}

    void add_argument(ExprNode* arg) {
        if (arg) args.push_back(arg);
    }

    ExprNode* get_argument(int index) const {
        if (index >= 0 && index < (int)args.size())
            return args[index];
        return nullptr;
    }

    size_t size() const { return args.size(); }

    const vector<ExprNode*>& get_arguments() const { return args; }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        return "";
    }
};

// ------------------------------------------------------------------
// FuncCallNode  –  function call expression  (e.g. foo(a, b))
// ------------------------------------------------------------------
class FuncCallNode : public ExprNode {
private:
    string func_name;
    vector<ExprNode*> arguments;

public:
    FuncCallNode(string name, string result_type)
        : ExprNode(result_type), func_name(name) {}

    ~FuncCallNode() {
        for (auto arg : arguments) delete arg;
    }

    void add_argument(ExprNode* arg) {
        if (arg) arguments.push_back(arg);
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        vector<string> arg_vals;
        for (auto arg : arguments) {
            string val = arg->generate_code(outcode, symbol_to_temp,
                                            temp_count, label_count);
            arg_vals.push_back(val);
        }

        for (auto& val : arg_vals) {
            outcode << "param " << val << "\n";
        }

        string temp = "t" + to_string(temp_count++);
        outcode << temp << " = call " << func_name << ", "
                << arguments.size() << "\n";
        return temp;
    }
};

// ============================================================
// PROGRAM ROOT
// ============================================================

// ------------------------------------------------------------------
// ProgramNode  –  root of the entire AST
// ------------------------------------------------------------------
class ProgramNode : public ASTNode {
private:
    vector<ASTNode*> units;

public:
    ~ProgramNode() {
        for (auto unit : units) delete unit;
    }

    void add_unit(ASTNode* unit) {
        if (unit) units.push_back(unit);
    }

    string generate_code(ofstream& outcode,
                         map<string, string>& symbol_to_temp,
                         int& temp_count,
                         int& label_count) const override {
        for (auto unit : units) {
            unit->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }
        return "";
    }
};

#endif