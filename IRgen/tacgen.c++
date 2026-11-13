#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>

#include "json.hpp"

using json = nlohmann::json;
using namespace std;

class TacGenerationError : public runtime_error {
public:
    int tokenIndex;

    TacGenerationError(int token, const string& message)
        : runtime_error(message), tokenIndex(token) {}
};

class TacGenerator {
    ostream& out;
    int tempIndex = 0;
    int labelIndex = 0;
    map<string, string> functionNames;

    static bool isTacReserved(const string& name) {
        static const set<string> reserved = {
            "func", "endfunc", "goto", "if", "ifFalse", "param", "call",
            "return", "print_i32", "print_bool"
        };
        return reserved.count(name) != 0;
    }

    [[noreturn]] static void fail(const json& node, const string& message) {
        throw TacGenerationError(node.value("_tokenIndex", -1), message);
    }

    string newTemp() {
        return "t" + to_string(++tempIndex);
    }

    string newLabel() {
        return "L" + to_string(++labelIndex);
    }

    void emit(const string& code) {
        out << '\t' << code << '\n';
    }

    void emitLabel(const string& label) {
        out << label << ":\n";
    }

    static string variableName(const json& variable) {
        const string kind = variable.at("node").get<string>();
        if (kind == "Identifier") {
            return variable.at("name").get<string>() + "_" +
                   to_string(variable.at("decl").get<int>());
        }
        return variable.at("name").get<string>() + "_" +
               to_string(variable.at("id").get<int>());
    }

    string functionName(const json& call) const {
        const string name = call.at("name").get<string>();
        const auto function = functionNames.find(name);
        if (function == functionNames.end()) {
            fail(call, "function is not defined: " + name);
        }
        return function->second;
    }

    string translateExpression(const json& expression) {
        const string kind = expression.at("node").get<string>();

        if (kind == "IntLiteral") {
            return to_string(expression.at("value").get<int>());
        }
        if (kind == "BoolLiteral") {
            return expression.at("value").get<bool>() ? "1" : "0";
        }
        if (kind == "Identifier") {
            return variableName(expression);
        }
        if (kind == "ParenExpr") {
            return translateExpression(expression.at("expr"));
        }
        if (kind == "UnaryExpr") {
            const string operand = translateExpression(expression.at("operand"));
            const string result = newTemp();
            emit(result + " = " + expression.at("op").get<string>() + " " + operand);
            return result;
        }
        if (kind == "BinaryExpr") {
            const string op = expression.at("op").get<string>();
            if (op == "&&" || op == "||") {
                const string left = translateExpression(expression.at("left"));
                const string result = newTemp();
                const string shortCircuit = newLabel();
                const string done = newLabel();

                if (op == "&&") {
                    emit("ifFalse " + left + " goto " + shortCircuit);
                } else {
                    emit("if " + left + " goto " + shortCircuit);
                }

                const string right = translateExpression(expression.at("right"));
                emit(result + " = " + right);
                emit("goto " + done);
                emitLabel(shortCircuit);
                emit(result + (op == "&&" ? " = 0" : " = 1"));
                emitLabel(done);
                return result;
            }

            const string left = translateExpression(expression.at("left"));
            const string right = translateExpression(expression.at("right"));
            const string result = newTemp();
            emit(result + " = " + left + " " + op + " " + right);
            return result;
        }
        if (kind == "AssignExpr") {
            const json* target = &expression.at("target");
            while (target->at("node") == "ParenExpr") {
                target = &target->at("expr");
            }
            if (target->at("node") != "Identifier") {
                fail(*target, "assignment target is not an identifier");
            }
            const string value = translateExpression(expression.at("value"));
            const string destination = variableName(*target);
            emit(destination + " = " + value);
            return destination;
        }
        if (kind == "CallExpr") {
            const json& arguments = expression.at("args");
            for (const json& argument : arguments) {
                const string value = translateExpression(argument);
                emit("param " + value);
            }
            const string name = functionName(expression);
            const string count = to_string(arguments.size());
            if (expression.at("type").is_null()) {
                emit("call " + name + ", " + count);
                return "";
            }
            const string result = newTemp();
            emit(result + " = call " + name + ", " + count);
            return result;
        }

        fail(expression, "unsupported expression node: " + kind);
    }

    void translateBlock(const json& block) {
        for (const json& statement : block.at("stmts")) {
            translateStatement(statement);
        }
    }

    void translateStatement(const json& statement) {
        const string kind = statement.at("node").get<string>();

        if (kind == "VarDecl") {
            const string value = translateExpression(statement.at("init"));
            emit(variableName(statement) + " = " + value);
            return;
        }
        if (kind == "ExprStmt") {
            translateExpression(statement.at("expr"));
            return;
        }
        if (kind == "PrintStmt") {
            const json& value = statement.at("value");
            const string operand = translateExpression(value);
            const string type = value.at("type").get<string>();
            if (type != "i32" && type != "bool") {
                fail(value, "print expression must have type i32 or bool");
            }
            emit(string(type == "i32" ? "print_i32 " : "print_bool ") + operand);
            return;
        }
        if (kind == "ReturnStmt") {
            if (statement.at("value").is_null()) {
                emit("return");
            } else {
                emit("return " + translateExpression(statement.at("value")));
            }
            return;
        }
        if (kind == "IfStmt") {
            const string condition = translateExpression(statement.at("cond"));
            const string otherwise = newLabel();
            const json& alternative = statement.at("else");
            if (alternative.is_null()) {
                emit("ifFalse " + condition + " goto " + otherwise);
                translateBlock(statement.at("then"));
                emitLabel(otherwise);
                return;
            }

            const string done = newLabel();
            emit("ifFalse " + condition + " goto " + otherwise);
            translateBlock(statement.at("then"));
            emit("goto " + done);
            emitLabel(otherwise);
            if (alternative.at("node") == "Block") {
                translateBlock(alternative);
            } else {
                translateStatement(alternative);
            }
            emitLabel(done);
            return;
        }
        if (kind == "WhileStmt") {
            const string start = newLabel();
            const string done = newLabel();
            emitLabel(start);
            const string condition = translateExpression(statement.at("cond"));
            emit("ifFalse " + condition + " goto " + done);
            translateBlock(statement.at("body"));
            emit("goto " + start);
            emitLabel(done);
            return;
        }

        fail(statement, "unsupported statement node: " + kind);
    }

    void translateFunction(const json& function) {
        tempIndex = 0;
        labelIndex = 0;
        
        out << "func " << functionNames.at(function.at("name").get<string>()) << '(';
        bool first = true;
        for (const json& parameter : function.at("params")) {
            if (!first) {
                out << ", ";
            }
            first = false;
            out << variableName(parameter);
        }
        out << ")\n";
        translateBlock(function.at("body"));
        out << "endfunc\n";
    }

    void trannslateProgram(const json& program){
        if (program.at("node") != "Program") {
            throw runtime_error("input JSON root must be a Program node");
        }

        set<string> usedNames;
        for (const json& function : program.at("functions")) {
            usedNames.insert(function.at("name").get<string>());
        }
        for (const json& function : program.at("functions")) {
            const string name = function.at("name").get<string>();
            string tacName = name;
            if (isTacReserved(tacName)) {
                tacName = "fn_" + name;
                while (usedNames.count(tacName) != 0 || isTacReserved(tacName)) {
                    tacName.insert(tacName.begin(), '_');
                }
                usedNames.insert(tacName);
            }
            functionNames.emplace(name, tacName);
        }

        for (const json& function : program.at("functions")) {
            translateFunction(function);
        }
    }

public:
    explicit TacGenerator(ostream& output) : out(output) {}

    void process(const json& program) {
        trannslateProgram(program);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <ast.json> <log.log> <output.tac>\n";
        return 1;
    }

    ifstream input(argv[1]);
    if (!input) {
        const string message = "cannot open input file: " + string(argv[1]);
        ofstream logf(argv[2]);
        cerr << "TAC generator: ERROR at -1: " << message << '\n';
        return 1;
    }

    try {
        const json ast = json::parse(input);
        ostringstream generated;
        TacGenerator generator(generated);
        generator.process(ast);
        ofstream outputFile(argv[3]);
        if (!outputFile) {
            throw TacGenerationError(-1, "cannot open output file: " + string(argv[3]));
        }
        outputFile << generated.str();
        if (!outputFile) {
            throw TacGenerationError(-1, "failed to write output file: " + string(argv[3]));
        }
    } catch (const TacGenerationError& error) {
        ofstream logf(argv[2]);
        const string message = "ERROR at " + to_string(error.tokenIndex) +
                               ": " + error.what();
        if (logf) {
            logf << message << '\n';
        }
        cerr << "TAC generator: " << message << '\n';
        return 1;
    } catch (const exception& error) {
        cerr << "TAC generator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
