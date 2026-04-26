#include <unistd.h>
int main() { fork(); fork(); while(1) sleep(1); return 0; }