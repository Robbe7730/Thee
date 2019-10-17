#include <iostream>
#include <ctype.h>
#include <vector>


#ifdef DEBUG
# define DEBUG_PRINT(x) puts(x)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

// AST Classes

class Instruction {};

class Expression : public Instruction {};

class FuncDef : public Instruction {
    int name;
    std::vector<Expression> body;
  public:
    FuncDef(int name, std::vector<Expression> body) : name(name), body(body) {};
};

class Variable : public Expression {
    int name;
  public:
    Variable(int name): name(name) {};
};

class Constant : public Expression {
    int value;
  public:
    Constant(int value) : value(value) {};
};

class FuncCall : public Expression {
    int name;
    std::vector<Expression> arguments;
  public:
    FuncCall(int name, std::vector<Expression> arguments) : name(name), arguments(arguments) {};
};

class BinaryOperator : public Expression {
    int op;
    Expression left;
    Expression right;
  public:
    BinaryOperator(int op, Expression left, Expression right) : op(op), left(left), right(right) {};
};

// Parser

int func_idx = 0;

int get_next_char() {
    int curr_char;

    do {
        curr_char = getchar();
    } while (isspace(curr_char));

    return curr_char;
}

// This should be in a header but who uses headers anyway?
Expression parse_expression();

std::vector<Expression> parse_args() {
    DEBUG_PRINT("parse_args");
    int curr_char;

    std::vector<Expression> args;

    do {
        args.push_back(parse_expression());
        curr_char = get_next_char();
    } while (curr_char == 'i');

    return args;
}

BinaryOperator parse_binop(int op) {
    DEBUG_PRINT("parse_binop");
    Expression left = parse_expression();
    Expression right = parse_expression();
    return BinaryOperator(op, left, right);
}

Expression parse_expression() {
    int curr_char = get_next_char();
    DEBUG_PRINT("parse_expression");
    if (curr_char == 'a') {
        DEBUG_PRINT("'t is a variable");
        curr_char = get_next_char();
        int name = curr_char - '0';
        return Variable(name);
    } else if (curr_char == 'f') {
        DEBUG_PRINT("'t is a function");
        curr_char = get_next_char();
        int name = curr_char - '0';
        std::vector<Expression> args = parse_args();
        return FuncCall(name, args);
    } else if(isdigit(curr_char)) {
        DEBUG_PRINT("'t is a number");
        return Constant(curr_char - '0');
    } else {
        DEBUG_PRINT("'t is something else, probably binop");
        return parse_binop(curr_char);
    }
}

FuncDef parse_funcdef() {
    DEBUG_PRINT("parse_funcdef");
    int curr_char;

    std::vector<Expression> body;

    do {
        body.push_back(parse_expression());
        curr_char = get_next_char();
    } while (curr_char == ';');
    
    return FuncDef(func_idx++, body);
}

Instruction parse_instruction() {
    DEBUG_PRINT("parse_instruction");
    int curr_char = get_next_char();

    if (curr_char == EOF) {
        DEBUG_PRINT("EOF, Imma head out");
        exit(EXIT_SUCCESS);
    } else if (curr_char == 'd') {
        return parse_funcdef();
    } else {
        ungetc(curr_char, stdin);
        return parse_expression();
    }
}

void parse_main() {
    while (true) {
        DEBUG_PRINT("parse_main loop");
        Instruction inst = parse_instruction();
    }
}

int main() {
    parse_main();
    return 0;
}
