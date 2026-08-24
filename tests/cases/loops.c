int main() {
    int sum = 0;
    int i = 0;
    while (i < 10) {
        i = i + 1;
        if (i == 5) {
            continue;
        }
        if (i == 8) {
            break;
        }
        sum = sum + i;
    }

    int product = 1;
    for (int j = 1; j <= 5; j++) {
        product = product * j;
    }

    printf("sum = %d, product = %d\n", sum, product);
    return 0;
}
