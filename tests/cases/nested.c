int main() {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (j == 2) { continue; }
            if (i == 3) { break; }
            count = count + 1;
        }
    }

    int total = 0;
    int k = 0;
    while (k < 5) {
        int m = 0;
        while (m < 3) {
            total = total + 1;
            m = m + 1;
        }
        k = k + 1;
    }

    printf("count = %d, total = %d\n", count, total);
    return 0;
}
