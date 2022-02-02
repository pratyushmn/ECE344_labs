#include "common.h"
#include <stdlib.h>

int recursiveFactorial(int n);

int main(int argc, char **argv) {
    if (argc == 2) {
        int i = 0;
        while (argv[1][i] != '\0') {
            if (argv[1][i] < '0' || argv[1][i] > '9') {
                printf("Huh?\n");
                return 0;
            }
            i++;
        }

        int n = atoi(argv[1]);

        if (n <= 0) printf("Huh?\n");
        else if (n > 12) printf("Overflow\n");
        else printf("%d\n", recursiveFactorial(n));
    } else {
        printf("Huh?\n");
    }
    
    return 0;
}

int recursiveFactorial(int n) {
    if (n == 0) return 1;
    return n * recursiveFactorial(n - 1);
}