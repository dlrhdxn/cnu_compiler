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
    int tokenIndex;
    string code;
    friend ostream& operator<<(ostream& os,const SemanticError& e){
        os << "ERROR at " << e.tokenIndex << ": " << e.code;
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
string typeFromJson(const json& Jtype) {
    if (Jtype.is_null()) return "";
    return Jtype.get<string>();
}

json typeToJson(const string& Stype) {
    return Stype == "" ? json(nullptr) : json(Stype);
}

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
    RDP(const string& path) : r(path),cst(new CST){cst->depth = -1; cst->parent=nullptr;};
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

    int term(string name){
        cur = r.peek();
        
        if(cur.type == name) {
            record(cur);
            r.advance();
            return r.getnpos()-1;
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
            node["name"] = r.peek().value;
            node["_tokenIndex"] = term("ID");
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

    //param must have type
    json param(){
        cur = r.peek();

        json node = {
            {"node", "Param"},
            {"name", nullptr},
            {"type", nullptr}
        };
        if(cur.type == "ID"){
            record("param");

            node["name"] = r.peek().value;
            node["_tokenIndex"] = term("ID");
            term("COLON");
            node["type"] = type();

            rewind();
            return node;
        }
        else throw ParseError(cur, r.getnpos());
    }

    string type(){
        cur = r.peek();

        if(cur.type == "I32"){
            record("type");

            term("I32");

            rewind();
            return "i32";
        }
        else if(cur.type == "BOOL"){
            record("type");

            term("BOOL");  

            rewind();
            return "bool";
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
            
            node["_tokenIndex"] = term("LET");
            if(r.peek().type == "MUT") {term("MUT"); node["mutable"] = true;}
            node["name"] = r.peek().value;
            node["_nameTokenIndex"] = term("ID");
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
            node["_tokenIndex"] = term("IF");
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
            node["_tokenIndex"] = term("WHILE");

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
            node["_tokenIndex"] = term("RETURN");

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
            node["_tokenIndex"] = term("PRINT");

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
                json node = {
                    {"node", "AssignExpr"},
                    {"_tokenIndex", term("ASSIGN")},
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
    // 관례상 좌측 유도로 정함
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
                node = {
                    {"node", "BinaryExpr"},
                    {"op", "||"},
                    {"_tokenIndex", term("OR")},
                    {"left", node},
                    {"right", logicalAndExpr()}
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
                node = {
                    {"node", "BinaryExpr"},
                    {"op", "&&"},
                    {"_tokenIndex", term("AND")},
                    {"left", node},
                    {"right", equalityExpr()}
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
                node = {
                    {"node", "BinaryExpr"},
                    {"op", cur.value},
                    {"_tokenIndex", term(cur.type)},
                    {"left", node},
                    {"right", additiveExpr()}
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
                node = {
                    {"node", "BinaryExpr"},
                    {"op", cur.value},
                    {"_tokenIndex", term(cur.type)},
                    {"left", node},
                    {"right", multiplicativeExpr()}
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
                node = {
                    {"node", "BinaryExpr"},
                    {"op", cur.value},
                    {"_tokenIndex", term(cur.type)},
                    {"left", node},
                    {"right", unaryExpr()}
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
                {"op", "-"},
                {"_tokenIndex", term("MINUS")},
                {"operand", unaryExpr()}
            };

            rewind();
            return node;
        }else if(cur.type == "NOT"){
            record("unaryExpr");

            json node = {
                {"node", "UnaryExpr"},
                {"op", "!"},
                {"_tokenIndex", term("NOT")},
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

            string name = cur.value;
            int tokenIndex = term("ID");
            if(r.peek().type == "LPAREN"){
                json node = {
                    {"node", "CallExpr"},
                    {"name", name},
                    {"_tokenIndex", tokenIndex},
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
                {"name", name},
                {"_tokenIndex", tokenIndex}
            };
            rewind();
            return node;
        }else if(cur.type == "INT"){
            record("primaryExpr");

            json node = {
                {"node", "IntLiteral"},
                {"value", atoi(cur.value.c_str())},
                {"_tokenIndex", term("INT")}
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


class SymbolTable{
public:
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

    SymbolTable(){scopes.push_back(Scope{{},0});}
    void pushScope(){
        scopes.push_back(Scope{{},scopes.back().idMark});
    }
    void popScope(){
        if(scopes.empty()) throw logic_error("cannot pop an empty symbol scope");
        scopes.pop_back();
    }

    //Variable, Argument
    pair<int,bool> insertVar(const string& name, const string& type, bool isMutable){
        int &id = scopes.back().idMark;
        bool res = scopes.back().variables.emplace(name, Variable{type, isMutable, id}).second;
        if(res) return {id++, res};
        else return {id,res};
    }
    //Global Function 

    bool insertFunc(const string& name, const vector<string>& paramtypes,const string& rettype){
        bool res = functions.emplace(name,Function{paramtypes, rettype}).second;
        return res;
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
    stringstream log; // only record declare & use
    int d = 0;


    [[noreturn]] void fail(int tokenIndex, const string& code) const{
        throw SemanticError{tokenIndex, code};
    }

    [[noreturn]] void fail(const json& node, const string& code) const{
        fail(node.value("_tokenIndex", -1), code);
    }

    void recdepth(){
        for(int i = 0; i < d; i++) log << "  ";
    }
    //insertVar and record
    int registerVariable(
        const string& name,
        const string& type,
        bool isMutable,
        int nameTokenIndex,
        const string& kind
    ){
        auto res = st.insertVar(name,type,isMutable);
        if(!res.second) fail(nameTokenIndex,"DUPLICATE var " + name);
        
        recdepth();
        log << "declare ";
        if(isMutable && kind != "(param)") log << "mut ";
        log << name << " #" << res.first << " : " << type << ' ' << kind << '\n';
        return res.first;
    }

    void declareLocal(json& decl){
        const string& t = typeFromJson(decl["type"]);
        decl["id"] = registerVariable(decl["name"],t,decl["mutable"],decl["_nameTokenIndex"],"(let)");
    }
    void declareParameter(json& param){
        param["id"] = registerVariable(param["name"],param["type"],true,param["_tokenIndex"],"(param)");
    }

    //Node : FnDecl
    string declareFunction(json& function){
        string name = function["name"];
        vector<string> paramtypes;
        string rettype = typeFromJson(function["returnType"]);
        string l = name;
        l += '(';
        for(const json& param : function["params"]) paramtypes.push_back(param["type"]);
        for(auto pat : paramtypes) l += pat + ", ";
        if(l.back() == ' ') {
            l.pop_back();
            l.pop_back();
        }
        l += ')';
        if(rettype != "") l += " -> " + rettype;
        bool res = st.insertFunc(name,paramtypes,rettype);
        if(!res) fail(function,"DUPLICATE fn " + name);
        return l;
    }


    const SymbolTable::Variable& lookupVariable(const string& name, int tokenIndex) const{
        const SymbolTable::Variable* variable = st.findVariable(name);
        if(variable == nullptr) fail(tokenIndex, "UNDECLARED_VAR " + name);
        return *variable; 
    }
    // const SymbolTable::Function& lookupFunction(const string& name, int tokenIndex) const{
    //     auto iter = st.functions.find(name);
    //     if(iter == st.functions.end()) fail(tokenIndex, "UNDECLARED_FUNC " + name);
    //     return (*iter).second; 
    // }

    void registerFunctions(){
        string l = "functions: ";
        bool noMain = true;
        for(json& function : ast["functions"]){
            const string& name = function["name"];
            vector<string> paramtypes;
            if(name == "main") noMain = false;
            l += declareFunction(function) + ", ";
        }
        if(noMain) fail(1, "NO_MAIN");
        
        if(l.back() == ' ') {
            l.pop_back();
            l.pop_back();
        }
        log  << l << '\n';
    }

    void analyzeFunction(json& function){
        const string& name =  function["name"];
        log << "fn " << name << '\n';
        string& expectedReturnType = st.functions[name].returnType;
        st.pushScope();
        ++d;
        for(json& param : function["params"]){
            declareParameter(param);
        }
        st.pushScope();
        string returnType = analyzeBlock(function["body"],false);
        st.popScope();
        st.popScope();

        if(expectedReturnType == ""){
            expectedReturnType = returnType;
        }
        else if(returnType == "") fail(function, "MISSING_RETURN");
        else if(returnType != expectedReturnType) fail(function, "TYPE_MISMATCH");        
        --d;

    }

    string analyzeBlock(json& block, bool createScope){
        
        if(createScope) {
            st.pushScope();
            ++d;
        }

        string statementReturns;
        string t;
        for(json& statement : block["stmts"]){
            t = analyzeStatement(statement);
            if(statementReturns == "") statementReturns = t;
        }
        if(createScope) {
            st.popScope();
            --d;
        }
        return statementReturns;
    }

    string analyzeStatement(json& statement){
        string kind = statement["node"];
        if(kind == "VarDecl"){
            const string& initType = analyzeExpression(statement["init"]);
            if (initType == "") fail(statement, "TYPE_MISMATCH");
            else if (statement["type"].is_null()) statement["type"] = initType;
            else if(statement["type"] != initType) fail(statement, "TYPE_MISMATCH");
            declareLocal(statement);
            return "";
        }
        if(kind == "IfStmt"){
            if(analyzeExpression(statement["cond"]) != "bool") fail(statement, "TYPE_MISMATCH condition");
            string thenReturns = analyzeBlock(statement["then"], true);
            if(statement["else"].is_null()) return ""; // it may no return

            json& el = statement["else"];
            string elseReturns;
            string kind = el["node"];
            if(kind == "Block"){
                elseReturns = analyzeBlock(el, true);
            }
            else if(kind == "IfStmt"){
                elseReturns = analyzeStatement(el);
            }
            if(thenReturns == "" || elseReturns == "") return ""; //it may no return
            if(thenReturns != elseReturns) fail(statement, "TYPE_MISMATCH if/else then expect:" + thenReturns + " else expect: " + elseReturns);
            return thenReturns;
        }
            
        if(kind == "WhileStmt"){
            if(analyzeExpression(statement["cond"]) != "bool") fail(statement, "TYPE_MISMATCH");
            string bodyReturns = analyzeBlock(statement["body"], true);
            return ""; // it may no return
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
        fail(statement, "[UNREACHABLE] Not stmt kind of "+kind);
    }

    string analyzeExpression(json& expression){
        string kind = expression["node"];
        if(kind == "IntLiteral") {return expression["type"] = "i32";}
        if(kind == "BoolLiteral") {return expression["type"] = "bool";}
        if(kind == "Identifier"){
            const string& name = expression["name"];
            auto var = lookupVariable(name, expression["_tokenIndex"]);
            expression["decl"] = var.id;
            recdepth();
            log << "use " << name << " (token " << expression["_tokenIndex"] << ") -> #" << var.id << '\n';
            return expression["type"] = var.type; // Var 은 선언=초기화 -> type 이 항상 존재
        }
        if(kind == "ParenExpr") {return expression["type"] = analyzeExpression(expression["expr"]);}
        if(kind == "CallExpr"){
            const string name = expression["name"];
            auto function = st.functions.find(name);
            if(function == st.functions.end()){
                if(st.findVariable(name) == nullptr) fail(expression, "UNDECLARED_FUNC");
                else fail(expression, "NOT_A_FUNCTION");
            } 

            json& arguments = expression["args"];
            int cnt = arguments.size();
            int expectedcnt = function->second.parameterTypes.size();

            if(cnt > expectedcnt) fail(expression, "too many args");
            else if(cnt < expectedcnt) fail(expression, "few args");
            
            for (size_t i = 0; i < cnt; ++i){
                if(function->second.parameterTypes[i] != analyzeExpression(arguments[i])) 
                    fail(expression, "TYPE_MISMATCH");
            }
            const string& rettyp = function->second.returnType;
            expression["type"] = typeToJson(rettyp); // may no return
            return rettyp;
        }
        if(kind == "UnaryExpr"){
            const string op = expression["op"];
            string operandType = analyzeExpression(expression["operand"]);
            if(op == "-"){
                if(operandType != "i32") fail(expression, "TYPE_MISMATCH");
                return expression["type"] = "i32";
            }
            if(op == "!"){
                if(operandType != "bool") fail(expression, "TYPE_MISMATCH");
                return expression["type"] = "bool";
            }
        }
        if(kind == "BinaryExpr"){
            const string op = expression["op"];
            string leftType = analyzeExpression(expression["left"]);
            string rightType = analyzeExpression(expression["right"]);
            
            //for checking chained comparison
            auto iscompare = [&](const json& expr){
                return expr["node"] == "BinaryExpr" && 
                    ((expr["op"] == "==") || (expr["op"] == "!=") ||
                    (expr["op"] == "<") || (expr["op"] == "<=") ||
                    (expr["op"] == ">") || (expr["op"] == ">="));
            };

            if(iscompare(expression) && iscompare(expression["left"])) fail(expression["left"],"CHAINED_COMPARISON");

            if(leftType == "" || rightType == ""){
                fail(expression, "TYPE_MISMATCH");
            }
            if(op == "+" || op ==  "-" || op == "*" || op == "/"){
                if(leftType != "i32" || rightType != "i32") fail(expression, "TYPE_MISMATCH");
                return expression["type"] = "i32";
            }
            if(op == "&&" || op == "||"){
                if(leftType != "bool" || rightType != "bool") fail(expression, "TYPE_MISMATCH");                
                return expression["type"] = "bool";
            }
            if(op == "==" || op == "!="){
                if(leftType != rightType) fail(expression, "TYPE_MISMATCH");
                return expression["type"] = "bool";
            }
            if(op == "<" || op == "<=" || op == ">" || op == ">="){
                if(leftType != "i32" || rightType != "i32") fail(expression, "TYPE_MISMATCH");
                return expression["type"] = "bool";
            }
        }
        if(kind == "AssignExpr"){
            analyzeExpression(expression["target"]);
            json* tar = &expression["target"];
            while(tar->at("node") == "ParenExpr") tar = &tar->at("expr");
            if(tar->at("node") != "Identifier") fail(*tar, "INVALID_ASSIGN");

            const json& target = *tar;
            const SymbolTable::Variable& variable = lookupVariable(target["name"], target["_tokenIndex"]);
            if(!variable.isMutable) fail(target, "IMMUTABLE_ASSIGN " + target["name"].get<string>());

            string valueType = analyzeExpression(expression.at("value"));
            if(valueType == "" || valueType != variable.type) fail(expression, "TYPE_MISMATCH");
            return expression["type"] = variable.type;
        }
        fail(expression, "[UNREACHABLE] Not expr kind of " + kind);
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
    string getlog(){
        return log.str();
    }

};


void removeTokenInfo(json& value) {
    if (value.is_object()) {
        value.erase("_tokenIndex");
        value.erase("_nameTokenIndex");
        for (auto& [key, child] : value.items()) {
            removeTokenInfo(child);
        }
    } else if (value.is_array()) {
        for (auto& child : value) {
            removeTokenInfo(child);
        }
    }
}

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
    semanticAnalyzer* analyzer;
    ofstream logf;
    ofstream astf;
    logf.open(argv[2]);
    

    try{
        rdp.parse();
        logf << rdp.getcst();    
        logf << "ACCEPT\n";

        json& ast = rdp.getast();
        analyzer = new semanticAnalyzer(ast);
        analyzer->process();

        logf << analyzer->getlog();
        logf << "OK\n";

        removeTokenInfo(ast);
        astf.open(argv[3]);
        astf << ast.dump(2) << '\n';

    }catch(ParseError& e){
        logf << rdp.getcst();    
        logf << e << '\n';
        cout << "[Parse] " << e << '\n';
    }catch(SemanticError& e){
        logf << analyzer->getlog();
        logf << e << '\n';
        cout << "[Semantic Analyze] " << e << '\n';

    }catch(exception& e){
        logf << analyzer->getlog();
        astf << rdp.getast().dump(2);
        logf << e.what();
        throw;
    }
    return 0;
}