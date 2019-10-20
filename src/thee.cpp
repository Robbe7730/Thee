#include <ctype.h>
#include <iostream>
#include <vector>

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Verifier.h"
#include <llvm/IR/Constants.h>

#ifdef DEBUG
# define DEBUG_PRINT(x) puts(x)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

// Error "handling"
void crash() {
    std::cerr << "Error\n";
    exit(EXIT_FAILURE);
}

// Global IR constants

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;
static std::map<int, llvm::Value *> variables;

// Stolen smart pointer class
template <class T> class Smartpointer {
    T *p;
    public:
        Smartpointer(T *ptr = NULL) {
            p = ptr;
        }

        ~Smartpointer() {
            // Who *actually* needs RAM?
            //delete(p);
        }

        T & operator * () {
            return *p;
        }

        T * operator -> () {
            return p;
        }
};

// AST Classes

class ASTInstruction {
  public:
    virtual llvm::Value* generate_IR() = 0;
};

class ASTExpression : public ASTInstruction {
  public:
    virtual llvm::Value* generate_IR() = 0;
};

class ASTFuncDef : public ASTInstruction {
    int name;
    std::vector<Smartpointer<ASTInstruction>> body;
  public:
    ASTFuncDef(int name, std::vector<Smartpointer<ASTInstruction>> body) : name(name), body(body) {};
    llvm::Function* generate_IR() override;
};

class ASTVariable : public ASTExpression {
    int name;
  public:
    ASTVariable(int name): name(name) {};
    llvm::Value* generate_IR() override;
};

class ASTConstant : public ASTExpression {
    int value;
  public:
    ASTConstant(int value) : value(value) {};
    llvm::Value* generate_IR() override;
};

class ASTFuncCall : public ASTExpression {
    int name;
    std::vector<Smartpointer<ASTInstruction>> arguments;
  public:
    ASTFuncCall(int name, std::vector<Smartpointer<ASTInstruction>> arguments) : name(name), arguments(arguments) {};
    llvm::Value* generate_IR() override;
};

class ASTBinaryOperator : public ASTExpression {
    int op;
    Smartpointer<ASTInstruction> left;
    Smartpointer<ASTInstruction> right;
  public:
    ASTBinaryOperator(int op, Smartpointer<ASTInstruction> left, Smartpointer<ASTInstruction> right) : op(op), left(left), right(right) {};
    llvm::Value* generate_IR() override;
};

// AST Class functions

llvm::Value* ASTConstant::generate_IR() {
    DEBUG_PRINT("ASTConstant::generate_IR()");
    return llvm::ConstantInt::get(context, llvm::APInt(16, value, true));
};

llvm::Value* ASTVariable::generate_IR() {
    DEBUG_PRINT("ASTVariable::generate_IR()");
    llvm::Value* val = variables[name];
    if (!val) {
        DEBUG_PRINT("That aint no variable!");
        crash();
    }
    return val;
}

llvm::Value* ASTBinaryOperator::generate_IR() {
    DEBUG_PRINT("ASTBinaryOperator::generate_IR()");
    llvm::Value* left_value = left->generate_IR();
    llvm::Value* right_value = right->generate_IR();

    if (!left_value) {
        DEBUG_PRINT("Left part of binop doesn't exist");
        crash();
    }

    if (!right_value) {
        DEBUG_PRINT("Right part of binop doesn't exist");
        crash();
    }

    switch (op) {
        case '+':
            return builder.CreateAdd(left_value, right_value, "addtemp");
        case '-':
            return builder.CreateSub(left_value, right_value, "mintemp");
        default:
            DEBUG_PRINT("Unknown operator");
            crash();
            return nullptr;
    }
}

llvm::Value* ASTFuncCall::generate_IR() {
    DEBUG_PRINT("ASTFuncCall::generate_IR()");
    llvm::Function* called_function = module->getFunction("f" + std::to_string(name));
    if (!called_function) {
        DEBUG_PRINT("That aint no function!");
        crash();
    }

    std::vector<llvm::Value*> args_values;

    for (size_t i = 0; i <= 9; i++) {
        if (i < arguments.size()) {
            args_values.push_back(arguments[i]->generate_IR());
        } else {
            args_values.push_back(llvm::ConstantInt::get(context, llvm::APInt(16, 0, true)));
        }
    }

    return builder.CreateCall(called_function, args_values, "calltemp");
}

llvm::Function* ASTFuncDef::generate_IR() {
    DEBUG_PRINT("ASTFuncDef::generate_IR()");
    std::vector<llvm::Type*> ints(10, llvm::Type::getInt16Ty(context));

    llvm::FunctionType* type;

    std::string name_str;
    if (name == 11) {
        name_str = "main";
        type = llvm::FunctionType::get(llvm::Type::getInt16Ty(context), false);
    } else {
        name_str = "f" + std::to_string(name);
        type = llvm::FunctionType::get(llvm::Type::getInt16Ty(context), ints, false);
    }
    llvm::Function* prototype = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name_str, module.get());

    char i = '0';

    for (llvm::Value &arg : prototype->args()) {
        std::string name = "a";
        name += i;
        i++;
        arg.setName(name);
    }

    llvm::BasicBlock* basic_block = llvm::BasicBlock::Create(context, "entry", prototype);

    builder.SetInsertPoint(basic_block);

    int idx = 0;
    for (llvm::Value& arg : prototype->args()) {
        variables[idx++] = &arg;
    }

    // Keep this so we can use this later, making the code more usable :O
    llvm::Value* last_value;
    for (Smartpointer<ASTInstruction> expr : body) {
        last_value = expr->generate_IR();
    }

    builder.CreateRet(last_value);

    return prototype;
}

// Parser

int func_idx = 0;

int get_next_char() {
    int curr_char;

    do {
        curr_char = getchar();
    } while (isspace(curr_char));

    char curr_char_str[2] = {0};
    curr_char_str[0] = curr_char;
    DEBUG_PRINT(curr_char_str);

    return curr_char;
}

// This should be in a header but who uses headers anyway?
Smartpointer<ASTInstruction> parse_expression();

std::vector<Smartpointer<ASTInstruction>> parse_args() {
    DEBUG_PRINT("parse_args");
    int curr_char;

    std::vector<Smartpointer<ASTInstruction>> args;

    do {
        args.push_back(parse_expression());
        curr_char = get_next_char();
    } while (curr_char == 'i');

    return args;
}

Smartpointer<ASTInstruction> parse_binop(int op) {
    DEBUG_PRINT("parse_binop");
    Smartpointer<ASTInstruction> left = parse_expression();
    Smartpointer<ASTInstruction> right = parse_expression();
    return Smartpointer<ASTInstruction>(new ASTBinaryOperator(op, left, right));
}

Smartpointer<ASTInstruction> parse_expression() {
    DEBUG_PRINT("parse_expression");
    int curr_char = get_next_char();
    if (curr_char == 'a') {
        DEBUG_PRINT("'t is a variable");
        curr_char = get_next_char();
        int name = curr_char - '0';
        return Smartpointer<ASTInstruction>(new ASTVariable(name));
    } else if (curr_char == 'f') {
        DEBUG_PRINT("'t is a function");
        curr_char = get_next_char();
        int name = curr_char - '0';
        std::vector<Smartpointer<ASTInstruction>> args = parse_args();
        return Smartpointer<ASTInstruction>(new ASTFuncCall(name, args));
    } else if(isdigit(curr_char)) {
        DEBUG_PRINT("'t is a number");
        return Smartpointer<ASTInstruction>(new ASTConstant(curr_char - '0'));
    } else {
        DEBUG_PRINT("'t is something else, probably binop");
        return parse_binop(curr_char);
    }
}

Smartpointer<ASTInstruction> parse_funcdef() {
    DEBUG_PRINT("parse_funcdef");
    int curr_char;

    std::vector<Smartpointer<ASTInstruction>> body;

    do {
        body.push_back(parse_expression());
        curr_char = get_next_char();
    } while (curr_char == ';');
    
    return Smartpointer<ASTInstruction>(new ASTFuncDef(func_idx++, body));
}

Smartpointer<ASTInstruction> parse_instruction() {
    DEBUG_PRINT("parse_instruction");
    int curr_char = get_next_char();

    if (curr_char == EOF) {
        DEBUG_PRINT("EOF, Imma head out");

        llvm::raw_ostream* out = &llvm::outs();

        std::error_code error_code;
        out = new llvm::raw_fd_ostream("tas.bc", error_code);

        module->print(*out, nullptr);

        delete(out);

        std::cout << "Ok\n";
        exit(EXIT_SUCCESS);
    } else if (curr_char == 'd') {
        return parse_funcdef();
    } else {
        DEBUG_PRINT("top level expression");
        ungetc(curr_char, stdin);
        Smartpointer<ASTInstruction> expr = parse_expression();
        std::vector<Smartpointer<ASTInstruction>> body;
        body.push_back(expr);
        return Smartpointer<ASTInstruction>(new ASTFuncDef(11, body));
    }
}

void parse_main() {
    module = llvm::make_unique<llvm::Module>("Waterkoker", context);
    while (true) {
        DEBUG_PRINT("parse_main loop");
        Smartpointer<ASTInstruction> inst = parse_instruction();
        (*inst).generate_IR();
    }
}

int main() {
    parse_main();
    return 0;
}
