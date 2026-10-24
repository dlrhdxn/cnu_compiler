#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;

enum class TokenType {
    FN,
    LET,
    MUT,
    IF,
    ELSE,
    WHILE,
    RETURN,
    PRINT,

    I32,
    BOOL,
    TRUE,
    FALSE,

    IDENTIFIER,
    NUMBER,

    ARROW,          // ->
    EQ,             // ==
    NEQ,            // !=
    LE,             // <=
    GE,             // >=
    LT,             // <
    GT,             // >
    AND,            // &&
    OR,             // ||

    ASSIGN,         // =
    PLUS,
    MINUS,
    STAR,
    SLASH,
    NOT,

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    COMMA,
    COLON,
    SEMICOLON,

    WHITESPACE,
    COMMENT,

    END,
    MISMATCH,


    PREFIX_AND,
    PREFIX_OR
};

struct Token {
    TokenType type;
    string value;
};

// x 전체가 하나의 token으로 인정되는지 검사

/* REGEX -> NFA -> DFA 를 구현해야하지만
    if 문들은 final state 를 구현;
    모든 종류의 토큰들이 accept 만 된다면 우선순위 및 Longest length match 는 구현됐으므로 올바른 토큰으로 매칭된다.
*/
// dead configuration -> mismatch; prefix 검사 해야함

Token match(const string& x) {

    // keyword
    if (x == "fn")     return {TokenType::FN, x};
    if (x == "let")    return {TokenType::LET, x};
    if (x == "mut")    return {TokenType::MUT, x};
    if (x == "if")     return {TokenType::IF, x};
    if (x == "else")   return {TokenType::ELSE, x};
    if (x == "while")  return {TokenType::WHILE, x};
    if (x == "return") return {TokenType::RETURN, x};
    if (x == "print")  return {TokenType::PRINT, x};

    if (x == "i32")    return {TokenType::I32, x};
    if (x == "bool")   return {TokenType::BOOL, x};
    if (x == "true")   return {TokenType::TRUE, x};
    if (x == "false")  return {TokenType::FALSE, x};


    // identifier
    if (!x.empty()) {

        if (isalpha(x[0]) || x[0] == '_') {

            for (size_t i = 1; i < x.size(); i++) {
                if (!isalnum(x[i]) && x[i] != '_')
                    return {TokenType::MISMATCH, x};
            }

            return {TokenType::IDENTIFIER, x};
        }
    }


    // number
    if (!x.empty()) {

        bool valid = true;

        for (char c : x) {
            if (!isdigit(c)) {
                valid = false;
                break;
            }
        }

        if (valid)
            return {TokenType::NUMBER, x};
    }


    // operators

    if (x == "->") return {TokenType::ARROW, x};

    if (x == "==") return {TokenType::EQ, x};
    if (x == "!=") return {TokenType::NEQ, x};
    if (x == "<=") return {TokenType::LE, x};
    if (x == ">=") return {TokenType::GE, x};

    if (x == "<")  return {TokenType::LT, x};
    if (x == ">")  return {TokenType::GT, x};

    if (x == "&") return {TokenType::PREFIX_AND, x};
    if (x == "|") return {TokenType::PREFIX_OR, x};
    if (x == "&&") return {TokenType::AND, x};
    if (x == "||") return {TokenType::OR, x};

    if (x == "=")  return {TokenType::ASSIGN, x};
    if (x == "+")  return {TokenType::PLUS, x};
    if (x == "-")  return {TokenType::MINUS, x};
    if (x == "*")  return {TokenType::STAR, x};
    if (x == "/")  return {TokenType::SLASH, x};
    if (x == "!")  return {TokenType::NOT, x};


    // punctuation

    if (x == "(") return {TokenType::LPAREN, x};
    if (x == ")") return {TokenType::RPAREN, x};
    if (x == "{") return {TokenType::LBRACE, x};
    if (x == "}") return {TokenType::RBRACE, x};

    if (x == ",") return {TokenType::COMMA, x};
    if (x == ":") return {TokenType::COLON, x};
    if (x == ";") return {TokenType::SEMICOLON, x};


    // whitespace

    if (!x.empty()) {

        bool valid = true;

        for (char c : x) {
            if (!isspace(c)) {
                valid = false;
                break;
            }
        }

        if (valid)
            return {TokenType::WHITESPACE, x};
    }
    if(x.size() >= 2 && x[0] == '/' && x[1] == '/' && x.back() != '\n') return { TokenType::COMMENT, x};

    return {TokenType::MISMATCH, x};
}

vector<Token> lex(const string& Program) {

    vector<Token> tokens;

    size_t i = 0;
    size_t j = 0;

    while (i < Program.size()) {

        j = i + 1;

        Token best = {
            TokenType::MISMATCH,
            ""
        };

        // i부터 j까지 계속 늘려본다
        while (j <= Program.size()) {

            string cur = Program.substr(i, j - i);

            Token token = match(cur);

            if (token.type == TokenType::MISMATCH)
                break;

            // 지금까지 유효한 가장 긴 token
            best = token;

            j++;
        }

        // 하나도 매칭되지 않았으면 lexical error
        if (best.type == TokenType::MISMATCH) {

            cerr << "Lexer error at position "
                 << i << ": "
                 << Program[i] << '\n';

            break;
        }

   
        if(best.type != TokenType::WHITESPACE && best.type != TokenType::COMMENT)
            tokens.push_back(best);
        // tokens.push_back(best);

        // best가 끝난 위치
        i += best.value.size();
    }

    tokens.push_back({TokenType::END, ""});

    return tokens;
}

constexpr const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::FN:         return "FN";
        case TokenType::LET:        return "LET";
        case TokenType::MUT:        return "MUT";
        case TokenType::IF:         return "IF";
        case TokenType::ELSE:       return "ELSE";
        case TokenType::WHILE:      return "WHILE";
        case TokenType::RETURN:     return "RETURN";
        case TokenType::PRINT:      return "PRINT";

        case TokenType::I32:        return "I32";
        case TokenType::BOOL:       return "BOOL";
        case TokenType::TRUE:       return "TRUE";
        case TokenType::FALSE:      return "FALSE";

        case TokenType::IDENTIFIER: return "ID";
        case TokenType::NUMBER:     return "INT";

        case TokenType::ARROW:      return "ARROW";
        case TokenType::EQ:         return "EQ";
        case TokenType::NEQ:        return "NEQ";
        case TokenType::LE:         return "LE";
        case TokenType::GE:         return "GE";
        case TokenType::LT:         return "LT";
        case TokenType::GT:         return "GT";
        case TokenType::AND:        return "AND";
        case TokenType::OR:         return "OR";

        case TokenType::ASSIGN:     return "ASSIGN";
        case TokenType::PLUS:       return "PLUS";
        case TokenType::MINUS:      return "MINUS";
        case TokenType::STAR:       return "STAR";
        case TokenType::SLASH:      return "SLASH";
        case TokenType::NOT:        return "NOT";

        case TokenType::LPAREN:     return "LPAREN";
        case TokenType::RPAREN:     return "RPAREN";
        case TokenType::LBRACE:     return "LBRACE";
        case TokenType::RBRACE:     return "RBRACE";
        case TokenType::COMMA:      return "COMMA";
        case TokenType::COLON:      return "COLON";
        case TokenType::SEMICOLON:  return "SEMI";

        // // SKIP
        // case TokenType::WHITESPACE: return "WHITESPACE"; 
        // case TokenType::COMMENT:    return "COMMENT";
        
        case TokenType::END:        return "EOF";
        case TokenType::MISMATCH:   return "MISMATCH";
        // PREFIXES
        case TokenType::PREFIX_AND:   return "MISMATCH";
        case TokenType::PREFIX_OR:   return "MISMATCH";

    }

    return "UNKNOWN";
}

int main(int argc, char* argv[]){
    ifstream fin(argv[1]);
    ofstream fout(argv[2]);
    string Program;

    stringstream ss;
    ss << fin.rdbuf();
    Program = ss.str();

    auto tokens = lex(Program);
    
    for (const auto& t : tokens) {
        fout << tokenTypeName(t.type)
            << "('"
            << t.value
            << "')\n";
    }
    return 0;
}