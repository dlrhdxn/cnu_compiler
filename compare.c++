#include <iostream>
#include <fstream>
using namespace std;
int main(int _,char* argv[]){
    ifstream c1(argv[1]);
    ifstream c2(argv[2]);

    string l1;
    string l2;
    int line = 0;
    while(getline(c1,l1)){
        line++;
        if(getline(c2,l2)) {}
        else{cout << "lack at: " << line <<'\n' ; break;}
        if (l1 == l2) continue;
        cout << "Not EQ at: "<< line <<'\n'; 
    }
    cout << "FIN\n";
    return 0;
}

