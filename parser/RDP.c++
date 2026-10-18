#include<iostream>
#include<fstream>
#include<vector>
// #include "token.h++"
using namespace std;

class CompileError{
    Token t;
    int line;
public:
    CompileError(const Token& t, const int& line): t(t), line(line){};
};
struct Token{
    string type;
    string value;

    friend ostream& operator<<(ostream& os, const Token& t){os << t.type << "('" << t.value << "')"; return os;}
};


struct Node{
    Token token;
    vector<Node*> childs;
};

class AST : Node{

};

class Log : Node{

};

// Token Reader
class Reader{
    ifstream stream;
    streampos pos;

    string tknstr;
    
public:
    Reader(const string& path) : stream(path){};
    Token peek(){
        pos = stream.tellg();
        getline(stream,tknstr); 
        stream.seekg(pos);

        Token t;
        size_t i = tknstr.find('(');
        size_t j = tknstr.find(')');
        t.type = tknstr.substr(0,i);
        t.value = tknstr.substr(i+2,j-i-3);

        return t;
    };
    void advance(){getline(stream,tknstr);};
    int getnpos(){return pos + 1;}
};

class RDP{
    Reader r;
    Token cur; 
    Log log;
    AST ast;

    void record(string name){
        log.push(name);
        
    }
    void record(Token t){
        log.push(throw);
    }

    void term(string name){
        cur = r.peek();
        
        if(cur.type == name) {
            record(cur);
            r.advance();
        }
        else throw CompileError(cur, r.getnpos());
    }
public:
    RDP(const string& path) : r(path){};
    

    // non-terminal

    void program(){
        cur = r.peek();
        
        if(cur.type == "FN"){
            record("program");

            while(r.peek().type == "FN") decl();
            term("EOF");
        }
        else throw CompileError(cur, r.getnpos());
        
    };

    void decl(){
        cur = r.peek();

        if(cur.type == "FN"){
            record("decl"); 

            fnDecl();
        }
        else throw CompileError(cur, r.getnpos());
    };
    void fnDecl(){
        cur = r.peek();


        if(cur.type == "FN"){
            record("fnDecl"); 

            term("FN");
            term("ID");
            term("LPAREN");
            if(r.peek().type == "ID") params();
            term("RPAREN");
            if(r.peek().type == "ARROW"){term("ARROW"); type();}
            block();
        }
        else throw CompileError(cur, r.getnpos());
    };

    void params(){
        cur = r.peek();

        if(cur.type == "ID"){
            record("params");

            param();
            while(r.peek().type == "COMMA"){term("COMMA"); param();}
            
        }   
        else throw CompileError(cur, r.getnpos());
    }

    void param(){
        cur = r.peek();

        if(cur.type == "ID"){
            record("param");

            term("ID");
            term("CONLON");
            type();
        }
        else throw CompileError(cur, r.getnpos());
    }

    void type(){
        cur = r.peek();

        if(cur.type == "I32"){
            record("type");

            term("I32");
        }
        else if(cur.type == "BOOL"){
            record("type");

            term("BOOl");  
        }
        else throw CompileError(cur, r.getnpos());
    }

    void block(){
        cur = r.peek();

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
            {stmt(); cur = r.peek();}
            term("RBRACE");
        }
        else throw CompileError(cur, r.getnpos());
    }

    void stmt(){
        cur = r.peek();

        if(cur.type == "LET"){
            record("stmt");

            letStmt();
        }else if(cur.type == "IF"){
            record("stmt");

            ifStmt();
        }else if(cur.type == "WHILE"){
            record("stmt");

            whileStmt();
        }else if(cur.type == "RETURN"){
            record("stmt");

            returnStmt();
        }else if(cur.type == "PRINT"){
            record("stmt");

            printStmt();
        }else if(cur.type == "MINUS" || 
                    cur.type == "NOT" ||
                    cur.type == "ID" ||
                    cur.type == "INT" ||
                    cur.type == "TRUE" ||
                    cur.type == "FALSE" ||
                    cur.type == "LPAREN")
        {record("stmt"); exprStmt();}

        else throw CompileError(cur, r.getnpos());
    }
    void letStmt(){
        cur = r.peek();

        if(cur.type == "LET"){
            record("letStmt");

            term("LET");
            if(r.peek().type == "MUT") term("MUT");
            term("ID");
            if(r.peek().type == "COLON"){term("COLON"); type();}
            term("ASSIGN");
            expr();
            term("SEMI");
        }
        else throw CompileError(cur, r.getnpos());
    }
    void ifStmt(){
        cur = r.peek();

        if(cur.type == "IF"){
            record("ifStmt");

            term("IF");
            expr();
            block();
            if(r.peek().type == "ELSE"){
                term("ELSE");
                cur = r.peek();
                if(cur.type == "LBRACE") block();
                else if(cur.type == "IF") ifStmt();
                else throw CompileError(cur, r.getnpos());
            }
        }
        else throw CompileError(cur, r.getnpos());
    }
    void whileStmt(){
        cur = r.peek();

        if(cur.type == "WHILE"){
            record("whileStmt");

            term("WHILE");
            expr();
            block();
        }
        else throw CompileError(cur, r.getnpos());
    }
    void returnStmt(){
        cur = r.peek();

        if(cur.type == "RETURN"){
            record("returnStmt");

            term("RETURN");
            cur = r.peek();
            if(cur.type == "MINUS" || 
                    cur.type == "NOT" ||
                    cur.type == "ID" ||
                    cur.type == "INT" ||
                    cur.type == "TRUE" ||
                    cur.type == "FALSE" ||
                    cur.type == "LPAREN")
            {expr();}
            term("SEMI");
        }
        else throw CompileError(cur, r.getnpos());
    }
    void printStmt(){
        cur = r.peek();

        if(cur.type == "PRINT"){
            record("printStmt");

            term("PRINT");
            term("LPAREN");
            expr();
            term("RPAREN");
            term("SEMI");
        }
        else throw CompileError(cur, r.getnpos());
    }
    void exprStmt(){
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

            expr();
            term("SEMI");
        }
        else throw CompileError(cur, r.getnpos());

    }
    
    // 연산자 우선순위 고려
    void expr(){
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

            assignExpr();
        }
        else throw CompileError(cur, r.getnpos());
    }

    // 우측 결합
    void assignExpr(){
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

            logicalOrExpr();
            if(r.peek().type == "ASSIGN") {term("ASSIGN"); assignExpr();}
        }
        else throw CompileError(cur, r.getnpos());
    }

    // 아래부터 동일한 우선순위라면, 순서 상관없어서 한번에 들어감.

    void logicalOrExpr(){
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

            logicalAndExpr();
            while(r.peek().type == "OR")
            {
                term("OR");
                logicalAndExpr();
            }
            while(r.peek().type == "OR") {term("OR"); logicalAndExpr();}
        }
        else throw CompileError(cur, r.getnpos());
    }

    void logicalAndExpr(){
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

            equalityExpr();
            while(r.peek().type == "AND") {term("AND"); equalityExpr();}

        }
        else throw CompileError(cur, r.getnpos());
    }
    void equalityExpr(){
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

            additiveExpr();
            cur = r.peek();
            while(cur.type == "EQ" || 
                cur.type == "NEQ" ||
                cur.type == "LT" ||
                cur.type == "LE" ||
                cur.type == "GT" ||
                cur.type == "GE")
            {
                term(cur.type);
                additiveExpr();
                cur = r.peek();
            }
        }
        else throw CompileError(cur, r.getnpos());
    }
    void additiveExpr(){
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

            multiplicativeExpr();
            cur = r.peek();
            while(cur.type == "PLUS" || cur.type == "MINUS")
            {
                term(cur.type);
                multiplicativeExpr();
                cur = r.peek();
            }
        }
        else throw CompileError(cur, r.getnpos());
    }
    void multiplicativeExpr(){
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

            unaryExpr();
            cur = r.peek();
            while(cur.type == "STAR" || cur.type == "SLASH")
            {
                term(cur.type);
                unaryExpr();
                cur = r.peek();
            }
        }
        else throw CompileError(cur, r.getnpos());
    }

    void unaryExpr(){
        cur = r.peek();

        if(cur.type == "MINUS"){
            record("unaryExpr");

            term("MINUS");
            unaryExpr();
        }else if(cur.type == "NOT"){
            record("unaryExpr");

            term("NOT");
            unaryExpr();
        }
        else if(cur.type == "ID" ||
                cur.type == "INT" ||
                cur.type == "TRUE" ||
                cur.type == "FALSE" ||
                cur.type == "LPAREN")
        {
            record("unaryExpr");

            primaryExpr();
        }
        else throw CompileError(cur, r.getnpos());

    }
    
    void primaryExpr(){
        cur = r.peek();

        if(cur.type == "ID"){
            record("primaryExpr");

            term("ID");
            if(r.peek().type == "LPAREN"){
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
                    args();
                }
                term("RPAREN");
            }
        }else if(cur.type == "INT"){
            record("primaryExpr");

            term("INT");
        }else if(cur.type == "TRUE"){
            record("primaryExpr");

            term("TRUE");
        }else if(cur.type == "FALSE"){
            record("primaryExpr");

            term("FALSE");
        }else if(cur.type == "LPAREN"){
            record("primaryExpr");

            term("LPAREN");
            expr();
            term("RPAREN");
        }
        else throw CompileError(cur, r.getnpos());
    }
    void args(){
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

            expr();
            while(r.peek().type == "COMMA"){
                term("COMMA");
                expr();
            }
        }
        else throw CompileError(cur, r.getnpos());

    }
    void parse(){
        program();
    }
};


int main(){
/*
    // reader test
    Reader r("./test1.txt"); 
    Token t = r.peek();

    // Tree test
    Node n,n1,n2;
    n.token = t; 
    r.advance();
    n1.token = r.peek();
    r.advance();
    n2.token = r.peek();

    n.childs.push_back(&n1);
    n.childs.push_back(&n2);
    cout << n.childs[0]->token;
*/
    // RDP Test
    return 0;
}