#include<iostream>
#include<fstream>
#include<vector>
#include<map>
#include<set>
#include<stdexcept>
#include<string>
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

struct Token{
    string type;
    string value;

    //os 의 오버로딩 -> friend 로 static 선언
    friend ostream& operator<<(ostream& os,const Token& t){os << t.type << "('" << t.value << "')"; return os;}
};

struct ParseError{
    Token t;
    int line;
    ParseError(const Token& t, const int& line): t(t), line(line){};
    friend ostream& operator<<(ostream& os,const ParseError& e){os << "ERROR at " << e.line << ": " << e.t << '\n'; return os;}
};
struct SemanticError{
    int line;
    string code;
    friend ostream& operator<<(ostream& os,const SemanticError& e){
        if(e.line > 0) os << "ERROR at " << e.line << ": ";
        else os << "ERROR: ";
        os << e.code << '\n';
        return os;
    }
};

struct CST{
    string name;
    Token token;

    vector<CST*> childs;
    CST* parent;

    int depth;

    // CST(){name = ""; token = {}; childs = vector<CST*>(); parent = nullptr;}
    ~CST(){for(auto c : childs) delete c;} // nullptr 초기화?

    friend ostream& operator<<(ostream& os, const CST& n){
        if(n.parent != nullptr){
            for(int i = 0; i < n.depth; i++) os << "  ";
            if(n.name == "") os << n.token << '\n';
            else os << n.name << '\n';
        }

        for(auto c : n.childs){
            os << *c;
        }
        return os;
    }

    CST* root(){
        CST* n = this;
        while(n->parent != nullptr) n = n->parent;
        return n;
    }
};


// Token Reader
class Reader{
    ifstream stream;
    streampos pos;
    int line;
    string tknstr;
    
public:
    Reader(const string& path) : stream(path),line(0){};

    //may occur error when read after EOF
    Token peek(){
        pos = stream.tellg();
        getline(stream,tknstr); 
        stream.seekg(pos);

        Token t;
        size_t i = tknstr.find('(');
        // size_t j = tknstr.find(')'); -> rparen 을 먼저 찾음;
        size_t j = tknstr.length() - 1;
        t.type = tknstr.substr(0,i);
        t.value = tknstr.substr(i+2,j-i-3);

        return t;
    };
    void advance(){getline(stream,tknstr); line++;};
    int getnpos(){return line+1;}
};

class RDP{
    Reader r;
    Token cur; 

    //initially root -> program
    CST* cst;
    json ast;

    //non-terminal
    void record(string name){
        CST* c = new CST();
        c->name = name;
        c->parent = cst;
        c->depth = cst->depth + 1;

        cst->childs.push_back(c);
        cst = c;        
    }

    //terminal
    void record(Token t){
        CST* c = new CST();
        c->token = t;
        c->parent = cst;
        c->depth = cst->depth + 1;
        
        cst->childs.push_back(c);
    }
    void rewind(){
        cst = cst->parent;
    }

public:
    RDP(const string& path) : r(path),cst(new CST){cst->depth = -1;};
    ~RDP(){delete cst;}


    void parse(){
        try {
            ast = program();
            
        }catch(...){cst = cst->root(); throw;}
    }

    // copy constructor 를 지정하지않아 & 를 써야함 : vector 때문에 값 복사가 막힘;
    CST& getcst(){
        return *cst;
    }
    json& getast(){
        return ast;
    }

    const json& getast() const{
        return ast;
    }

    string term(string name){
        cur = r.peek();
        
        if(cur.type == name) {
            record(cur);
            r.advance();
            return cur.value;
        }
        else throw ParseError(cur, r.getnpos());
    }
    // non-terminal

    json program(){
        cur = r.peek();
    
        json node = {
            {"node","Program"},
            {"functions", json::array()},
        };
        if(cur.type == "FN"){
            record("program");
        
            while(r.peek().type == "FN") node["functions"].push_back(decl());
            term("EOF");

            rewind();
            return node;
        }
        else if(cur.type == "EOF"){
            record("program");

            term("EOF");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
        
    };
    json decl(){
        cur = r.peek();

        if(cur.type == "FN"){
            record("decl"); 

            json node = fnDecl();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    };
    json fnDecl(){
        cur = r.peek();

        json node = {
            {"node", "FnDecl"},
            {"name",nullptr},
            {"params",json::array()},
            {"returnType", nullptr},
            {"body",nullptr}
        };

        if(cur.type == "FN"){
            record("fnDecl"); 

            term("FN");
            node["name"] = term("ID");
            term("LPAREN");
            if(r.peek().type == "ID") node["params"] = params();
            term("RPAREN");
            if(r.peek().type == "ARROW"){term("ARROW"); node["returnType"] = type();}
            node["body"] = block();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    };
    json params(){
        cur = r.peek();

        json nodes = json::array();
        if(cur.type == "ID"){
            record("params");

            nodes.push_back(param());
            while(r.peek().type == "COMMA"){term("COMMA"); nodes.push_back(param());}
            
            rewind();
            return nodes;
        }   
        else throw ParseError(cur, r.getnpos());
    }

    json param(){
        cur = r.peek();

        json node = {
            {"node", "Param"},
            {"name", nullptr},
            {"type", nullptr}
        };
        if(cur.type == "ID"){
            record("param");

            node["name"] = term("ID");
            term("COLON");
            node["type"] = type();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }

    json type(){
        cur = r.peek();

        json d;
        if(cur.type == "I32"){
            record("type");

            d = term("I32");

            rewind();
            return d;
        }
        else if(cur.type == "BOOL"){
            record("type");

            d = term("BOOL");  

            rewind();
            return d;
        }
        else throw ParseError(cur, r.getnpos());
    }

    json block(){
        cur = r.peek();

        json node = {
            {"node", "Block"},
            {"stmts", json::array()},
        };
        if(cur.type == "LBRACE"){
            record("block");

            term("LBRACE");
            cur = r.peek();
            //lookahead of stmt
            while(cur.type == "LET" ||
                    cur.type == "IF" ||
                    cur.type == "WHILE" ||
                    cur.type == "RETURN" ||
                    cur.type == "PRINT" ||

                    //look ahead of exprStmt
                    cur.type == "MINUS" || 
                    cur.type == "NOT" ||
                    cur.type == "ID" ||
                    cur.type == "INT" ||
                    cur.type == "TRUE" ||
                    cur.type == "FALSE" ||
                    cur.type == "LPAREN") 
            {node["stmts"].push_back(stmt()); cur = r.peek();}
            term("RBRACE");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }

    json stmt(){
        cur = r.peek();

        json node;
        if(cur.type == "LET"){
            record("stmt");

            node = letStmt();

            rewind();
            return  node;

        }else if(cur.type == "IF"){
            record("stmt");

            node = ifStmt();

            rewind();
            return  node;

        }else if(cur.type == "WHILE"){
            record("stmt");

            node = whileStmt();

            rewind();
            return  node;

        }else if(cur.type == "RETURN"){
            record("stmt");

            node = returnStmt();

            rewind();
            return  node;

        }else if(cur.type == "PRINT"){
            record("stmt");

            node = printStmt();

            rewind();
            return  node;
        }else if(cur.type == "MINUS" || 
                    cur.type == "NOT" ||
                    cur.type == "ID" ||
                    cur.type == "INT" ||
                    cur.type == "TRUE" ||
                    cur.type == "FALSE" ||
                    cur.type == "LPAREN")
        {
            record("stmt"); 
            node = exprStmt(); 
            rewind(); 
            return node;
        }

        else throw ParseError(cur, r.getnpos());
    }
    json letStmt(){
        cur = r.peek();

        json node = {
            {"node", "VarDecl"},
            {"name", nullptr},
            {"mutable", false},
            {"type", nullptr},
            {"init", nullptr}
        };
        if(cur.type == "LET"){
            record("letStmt");

            term("LET");
            if(r.peek().type == "MUT") {term("MUT"); node["mutable"] = true;}
            node["name"] = term("ID");
            if(r.peek().type == "COLON"){term("COLON"); node["type"] = type();}
            term("ASSIGN");
            node["init"] = expr();
            term("SEMI");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json ifStmt(){
        cur = r.peek();

        if(cur.type == "IF"){
            record("ifStmt");

            json node = {
                {"node", "IfStmt"},
                {"cond", nullptr},
                {"then", nullptr},
                {"else", nullptr}
            };
            term("IF");
            node["cond"] = expr();
            node["then"] = block();
            if(r.peek().type == "ELSE"){
                term("ELSE");
                cur = r.peek();
                if(cur.type == "LBRACE") node["else"] = block();
                else if(cur.type == "IF") node["else"] = ifStmt();
                else throw ParseError(cur, r.getnpos());
            }

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json whileStmt(){
        cur = r.peek();

        if(cur.type == "WHILE"){
            record("whileStmt");

            json node = {
                {"node", "WhileStmt"},
                {"cond", nullptr},
                {"body", nullptr}
            };
            term("WHILE");
            node["cond"] = expr();
            node["body"] = block();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json returnStmt(){
        cur = r.peek();

        if(cur.type == "RETURN"){
            record("returnStmt");

            json node = {
                {"node", "ReturnStmt"},
                {"value", nullptr}
            };
            term("RETURN");
            cur = r.peek();
            if(cur.type == "MINUS" || 
                    cur.type == "NOT" ||
                    cur.type == "ID" ||
                    cur.type == "INT" ||
                    cur.type == "TRUE" ||
                    cur.type == "FALSE" ||
                    cur.type == "LPAREN")
            {node["value"] = expr();}
            term("SEMI");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json printStmt(){
        cur = r.peek();

        if(cur.type == "PRINT"){
            record("printStmt");

            json node = {
                {"node", "PrintStmt"},
                {"value", nullptr}
            };
            term("PRINT");
            term("LPAREN");
            node["value"] = expr();
            term("RPAREN");
            term("SEMI");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json exprStmt(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {   
            record("exprStmt");

            json node = {
                {"node", "ExprStmt"},
                {"expr", expr()}
            };
            term("SEMI");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());

    }
    
    // 연산자 우선순위 고려
    json expr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("expr");

            json node = assignExpr();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }

    // 우측 결합
    json assignExpr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("assignExpr");

            json target = logicalOrExpr();
            if(r.peek().type == "ASSIGN") {
                term("ASSIGN");
                json node = {
                    {"node", "AssignExpr"},
                    {"target", target},
                    {"value", assignExpr()}
                };
                rewind();
                return node;
            }

            rewind();
            return target;
        }
        else throw ParseError(cur, r.getnpos());
    }

    // 아래부터 동일한 우선순위라면, 순서 상관없어서 한번에 들어감.

    json logicalOrExpr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("logicalOrExpr");

            json node = logicalAndExpr();
            while(r.peek().type == "OR") {
                string op = term("OR");
                json right = logicalAndExpr();
                node = {
                    {"node", "BinaryExpr"},
                    {"op", op},
                    {"left", node},
                    {"right", right}
                };
            }

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }

    json logicalAndExpr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("logicalAndExpr");

            json node = equalityExpr();
            while(r.peek().type == "AND") {
                string op = term("AND");
                json right = equalityExpr();
                node = {
                    {"node", "BinaryExpr"},
                    {"op", op},
                    {"left", node},
                    {"right", right}
                };
            }

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json equalityExpr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("equalityExpr");

            json node = additiveExpr();
            cur = r.peek();
            while(cur.type == "EQ" || 
                cur.type == "NEQ" ||
                cur.type == "LT" ||
                cur.type == "LE" ||
                cur.type == "GT" ||
                cur.type == "GE")
            {
                string op = term(cur.type);
                json right = additiveExpr();
                node = {
                    {"node", "BinaryExpr"},
                    {"op", op},
                    {"left", node},
                    {"right", right}
                };
                cur = r.peek();
            }

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json additiveExpr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("additiveExpr");

            json node = multiplicativeExpr();
            cur = r.peek();
            while(cur.type == "PLUS" || cur.type == "MINUS")
            {
                string op = term(cur.type);
                json right = multiplicativeExpr();
                node = {
                    {"node", "BinaryExpr"},
                    {"op", op},
                    {"left", node},
                    {"right", right}
                };
                cur = r.peek();
            }

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json multiplicativeExpr(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("multiplicativeExpr");

            json node = unaryExpr();
            cur = r.peek();
            while(cur.type == "STAR" || cur.type == "SLASH")
            {
                string op = term(cur.type);
                json right = unaryExpr();
                node = {
                    {"node", "BinaryExpr"},
                    {"op", op},
                    {"left", node},
                    {"right", right}
                };
                cur = r.peek();
            }

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }

    json unaryExpr(){
        cur = r.peek();

        if(cur.type == "MINUS"){
            record("unaryExpr");

            json node = {
                {"node", "UnaryExpr"},
                {"op", term("MINUS")},
                {"operand", unaryExpr()}
            };

            rewind();
            return node;
        }else if(cur.type == "NOT"){
            record("unaryExpr");

            json node = {
                {"node", "UnaryExpr"},
                {"op", term("NOT")},
                {"operand", unaryExpr()}
            };

            rewind();
            return node;
        }
        else if(cur.type == "ID" ||
                cur.type == "INT" ||
                cur.type == "TRUE" ||
                cur.type == "FALSE" ||
                cur.type == "LPAREN")
        {
            record("unaryExpr");

            json node = primaryExpr();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());

    }
    
    json primaryExpr(){
        cur = r.peek();

        if(cur.type == "ID"){
            record("primaryExpr");

            string name = term("ID");
            if(r.peek().type == "LPAREN"){
                json node = {
                    {"node", "CallExpr"},
                    {"name", name},
                    {"args", json::array()}
                };
                term("LPAREN");
                cur = r.peek();
                if(cur.type == "MINUS" || 
                    cur.type == "NOT" ||
                    cur.type == "ID" ||
                    cur.type == "INT" ||
                    cur.type == "TRUE" ||
                    cur.type == "FALSE" ||
                    cur.type == "LPAREN")
                {
                    node["args"] = args();
                }
                term("RPAREN");

                rewind();
                return node;
            }

            json node = {
                {"node", "Identifier"},
                {"name", name}
            };
            rewind();
            return node;
        }else if(cur.type == "INT"){
            record("primaryExpr");

            json node = {
                {"node", "IntLiteral"},
                {"value", atoi(term("INT").c_str())}
            };

            rewind();
            return node;
        }else if(cur.type == "TRUE"){
            record("primaryExpr");

            term("TRUE");
            json node = {
                {"node", "BoolLiteral"},
                {"value", true}
            };

            rewind();
            return node;
        }else if(cur.type == "FALSE"){
            record("primaryExpr");

            term("FALSE");
            json node = {
                {"node", "BoolLiteral"},
                {"value", false}
            };

            rewind();
            return node;
        }else if(cur.type == "LPAREN"){
            record("primaryExpr");

            term("LPAREN");
            json node = {
                {"node", "ParenExpr"},
                {"expr", expr()}
            };
            term("RPAREN");

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }
    json args(){
        cur = r.peek();

        if(cur.type == "MINUS" || 
            cur.type == "NOT" ||
            cur.type == "ID" ||
            cur.type == "INT" ||
            cur.type == "TRUE" ||
            cur.type == "FALSE" ||
            cur.type == "LPAREN")
        {
            record("args");

            json nodes = json::array();
            nodes.push_back(expr());
            while(r.peek().type == "COMMA"){
                term("COMMA");
                nodes.push_back(expr());
            }

            rewind();
            return nodes;
        }
        else throw ParseError(cur, r.getnpos());

    }

};

[[noreturn]] void fail(const string& code){
    throw SemanticError{0, code};
}

class SymbolTable{
public:
    int nextID;
    struct Variable{
        string type;
        bool isMutable;
        int id;
    };

    struct Function{
        vector<string> parameterTypes;
        string returnType;
    };
    struct Scope {
        map<string, Variable> variables;
        int idMark;  
    };

private:
    vector<Scope> scopes;

public:
    map<string, Function> functions;

    void pushScope(){
        scopes.push_back(Scope{{},0});
    }

    void popScope(){
        if(scopes.empty()) throw logic_error("cannot pop an empty symbol scope");
        nextID = scopes.back().idMark;
        scopes.pop_back();
    }

    //Variable, Argument
    void insertVar(const string& name, const string& type, bool isMutable){
        if(!scopes.back().variables.emplace(name, Variable{type, isMutable, nextID++}).second) 
            fail("DUPLICATE " + name);
    }

    //Global Function 
    void insertFunc(const string& name, const vector<string>& paramtypes,const string& rettype){
        if(!functions.emplace(name,Function{paramtypes, rettype}).second) 
            fail("DUPLICATE " + name);
    }

    const Variable* findVariable(const string& name) const{
        for(auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope){
            auto symbol = scope->variables.find(name);
            if(symbol != scope->variables.end()) return &symbol->second;
        }
        return nullptr;
    }
};

class semanticAnalyzer{
    json& ast;
    SymbolTable st;
    string log;
    int i = 0;

    //record_block / use & declare
    void record(){
        i++;
    }
    void rewind(){
        i--;
    }

    //insertVar
    void declareVar(json& j){
        const string& t = j["type"].is_null() ? "" : j["type"];
        bool isMut = j["mutable"].is_null() ? false : j["mutable"].get<bool>();
        j["id"] = st.nextID;
        st.insertVar(j["name"],t,isMut);
    }

    const SymbolTable::Variable& lookupVariable(const string& name) const{
        const SymbolTable::Variable* variable = st.findVariable(name);
        if(variable == nullptr) fail("UNDECLARED_VAR " + name);
        return *variable; 
    }

    void registerFunctions(){
        log = "functions: ";
        for(json& function : ast["functions"]){
            vector<string> paramtypes;
            for(const json& param : function["params"])
                paramtypes.push_back(param["type"]);
            
            string rettype = function["returnType"].is_null() ? "" : function["returnType"];
            st.insertFunc(function["name"],paramtypes,rettype);

            log += function["name"] + "(";
            for(auto pat : paramtypes) log += pat + ","; log.pop_back(); 
            log += ") -> " + rettype;
        }
        log += '\n';
    }

    void analyzeFunction(json& function){
        string& expectedReturnType = st.functions[function["name"]].returnType;
        log += "fn " + function["name"] + '\n';
        st.pushScope();
        for(json& param : function["params"]){
            
            declareVar(param);
        }

        string returnType = analyzeBlock(function["body"],false);
        if(expectedReturnType == ""){
            expectedReturnType = returnType;
        }
        else if(returnType == "") fail("MISSING_RETURN");
        else if(returnType != expectedReturnType) fail("TYPE_MISMATCH");        
        st.popScope();
    }

    string analyzeBlock(json& block, bool createScope){
        if(createScope) st.pushScope();
        string statementReturns;
        for(json& statement : block["stmts"]){
            statementReturns = analyzeStatement(statement);
            if(statementReturns != "") break;
        }
        if(createScope) st.popScope();
        return statementReturns;
    }

    string analyzeStatement(json& statement){
        string kind = statement["node"];
        if(kind == "VarDecl"){
            const string& initType = analyzeExpression(statement["init"]);
            if (statement["type"].is_null()) statement["type"] = initType;
            else if(statement["type"] != initType) fail("TYPE_MISMATCH");
            declareVar(statement);
            return "";
        }
        if(kind == "IfStmt"){
            // IF ELSE 모두 존재하고 타입이 같다고 가정
            if(analyzeExpression(statement["cond"]) != "bool") fail("TYPE_MISMATCCH");
            string thenReturns = analyzeBlock(statement["then"], true);
            if(statement["else"].is_null()) return ""; // thentype return or null -> 모름
            string elseReturns = analyzeStatement(statement["else"]);
            return thenReturns != "" ? thenReturns : elseReturns;
        }
        if(kind == "WhileStmt"){
            if(analyzeExpression(statement["cond"]) != "bool") fail("TYPE_MISMATCCH");
            string bodyReturns = analyzeBlock(statement["body"], true);
            return bodyReturns;
        }
        if(kind == "ReturnStmt"){
            json& value = statement["value"];
            if(!value.is_null()) return analyzeExpression(value);
            return "";
        }
        if(kind == "PrintStmt"){
            analyzeExpression(statement.at("value"));
            return "";
        }
        if(kind == "ExprStmt"){
            analyzeExpression(statement.at("expr"));
            return "";
        }
        fail("ERROR");
    }

    string analyzeExpression(json& expression){
        string kind = expression["node"];
        if(kind == "IntLiteral") {return expression["type"] = "i32";}
        if(kind == "BoolLiteral") {return expression["type"] = "bool";}
        if(kind == "Identifier"){
            auto var = lookupVariable(expression["name"]);
            expression["decl"] = var.id;
            return expression["type"] = var.type;
        }
        if(kind == "ParenExpr") {return expression["type"] = analyzeExpression(expression.at("expr"));}
        if(kind == "CallExpr"){
            const string name = expression["name"];
            auto function = st.functions.find(name);
            if(function == st.functions.end()){
                if(st.findVariable(name) == nullptr) fail("UNDECLARED_FUNC");
                else fail("NOT_A_FUNCTION");
            } 

            json& arguments = expression["args"];
            int cnt = arguments.size();
            int expectedcnt = function->second.parameterTypes.size();

            if(cnt > expectedcnt) fail("too many args");
            else if(cnt < expectedcnt) fail("few args");
            
            for (size_t i = 0; i < cnt; ++i){
                if(function->second.parameterTypes[i] != analyzeExpression(arguments[i])) 
                    fail("TYPE_MISMATCH");
            }
            return expression["type"] = function->second.returnType;
        }
        if(kind == "UnaryExpr"){
            const string op = expression["op"];
            string operandType = analyzeExpression(expression["operand"]);
            if(op == "-"){
                if(operandType != "i32") fail("TYPE_MISMATCH");
                
                return expression["type"] = "i32";
            }
            if(op == "!"){
                if(operandType != "bool") fail("TYPE_MISMATCH");
                return expression["type"] = "bool";
            }
        }
        if(kind == "BinaryExpr"){
            const string op = expression["op"];
            string leftType = analyzeExpression(expression["left"]);
            string rightType = analyzeExpression(expression["right"]);
            
            //for check chained comparison
            const json& leftExpr = expression["left"];
            
            auto iscompare = [&](){
                return leftExpr["node"] == "BinaryExpr" && 
                    ((leftExpr["op"] == "&&") || (leftExpr["op"] == "||") || 
                    (leftExpr["op"] == "==") || (leftExpr["op"] == "!=") ||
                    (leftExpr["op"] == "<") || (leftExpr["op"] == "<=") ||
                    (leftExpr["op"] == ">") || (leftExpr["op"] == ">="));
            };

            if(leftType == "" || rightType == ""){
                fail("TYPE_MISMATCH");
            }
            if(op == "+" || op == "-" || op == "*" || op == "/"){
                if(leftType != "i32") fail("TYPE_MISMATCH");
                if(rightType != "i32") fail("TYPE_MISMATCH");
                return expression["type"] = "i32";
            }
            if(op == "&&" || op == "||"){
                if(leftType != "bool") fail("TYPE_MISMATCH");
                if(rightType != "bool") fail("TYPE_MISMATCH");
                
                if(iscompare()) fail("CHAINED_COMPARISON");
                return expression["type"] = "bool";
            }
            if(op == "==" || op == "!="){
                if(leftType != rightType) fail("TYPE_MISMATCH");
                if(iscompare()) fail("CHAINED_COMPARISON");
                return expression["type"] = "bool";
            }
            if(op == "<" || op == "<=" || op == ">" || op == ">="){
                if(leftType != "i32") fail("TYPE_MISMATCH");
                if(rightType != "i32") fail("TYPE_MISMATCH");
                if(iscompare()) fail("CHAINED_COMPARISON");
                return expression["type"] = "bool";
            }
        }
        if(kind == "AssignExpr"){
            const json& target = expression["target"];
            if(target["node"] != "Identifier") fail("INVALID_ASSIGN");

            const SymbolTable::Variable& variable = lookupVariable(target["name"]);
            if(!variable.isMutable) fail("IMMUTABLE_ASSIGN");

            string valueType = analyzeExpression(expression.at("value"));
            fail("TYPE_MISMATCH");
            return expression["type"] = variable.type;
        }
        fail("ERROR");
    }

public:
    explicit semanticAnalyzer(json& program) : ast(program){}

    void process(){
        Program();
    }

    void Program(){
        registerFunctions();
        for(json& function : ast["functions"]){
            analyzeFunction(function);
        } 
    }

    string log(){
        st.functions    
    }
};

int main(int argc, char* argv[]){

    // // reader test
    // Reader r("./test1.txt"); 
    // Token t = r.peek();

    // // Tree test
    // CST n,n1,n2;
    // n.token = t; 
    // r.advance();
    // n1.token = r.peek();
    // r.advance();
    // n2.token = r.peek();

    // n.childs.push_back(&n1);
    // n.childs.push_back(&n2);
    // cout << n.childs[0]->token;


    // RDP Test
    if(argc != 4){cout << argv[0] << " tokenfile logfile jsonfile"; return -1;}

    RDP rdp(argv[1]);
    
    ofstream logf;
    ofstream astf;
    logf.open(argv[2]);
    astf.open(argv[3]);

    try{
        rdp.parse();
        logf << rdp.getcst();    
        logf << "ACCEPT\n";

        json& ast = rdp.getast();
        semanticAnalyzer analyzer(ast);
        analyzer.process();

        logf << 


        astf << rdp.getast().dump(2) << '\n';

    }catch(ParseError& e){
        logf << rdp.getcst();    
        logf << e;
    }catch(SemanticError& e){
        logf << e;
    }

    return 0;
}