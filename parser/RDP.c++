#include<iostream>
#include<fstream>
#include<vector>
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

struct Token{
    string type;
    string value;

    //os 의 오버로딩 -> friend 로 static 선언
    friend ostream& operator<<(ostream& os,const Token& t){os << t.type << "('" << t.value << "')"; return os;}
};

struct CompileError{
    Token t;
    int line;
    CompileError(const Token& t, const int& line): t(t), line(line){};
    friend ostream& operator<<(ostream& os,const CompileError& e){os << "ERROR at " << e.line << ": " << e.t << '\n'; return os;}
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
        
    };
    json decl(){
        cur = r.peek();

        if(cur.type == "FN"){
            record("decl"); 

            json node = fnDecl();

            rewind();
            return node;
        }
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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

        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
                else throw CompileError(cur, r.getnpos());
            }

            rewind();
            return node;
        }
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());

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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());

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
        else throw CompileError(cur, r.getnpos());
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
        else throw CompileError(cur, r.getnpos());

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
    ofstream cstf;
    ofstream astf;
    cstf.open(argv[2]);
    astf.open(argv[3]);

    try{
        rdp.parse();
        cstf << rdp.getcst();    
        cstf << "ACCEPT\n";

        astf << rdp.getast().dump(2) << '\n';

    }catch(CompileError& e){
        cstf << rdp.getcst();    
        cstf << e;
    }

    return 0;
}