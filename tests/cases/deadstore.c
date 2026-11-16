/* Dead-store elimination: the fold must collapse "int a = 5; a = 10;"
 * into "int a = 10;" while keeping "b = b + 1" intact. */
int main() {
    int a = 5;
    a = 10;

    int b = 1;
    b = b + 1;

    printf("a=%d b=%d\n", a, b);
    return 0;
}
