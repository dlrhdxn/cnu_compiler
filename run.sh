cd "$(dirname "$0")"

g++ -o parser ./RDP.c++
g++ -o tacgen ./tacgen.c++

./parser $1 $2 temp.json && ./tacgen temp.json $2 $3