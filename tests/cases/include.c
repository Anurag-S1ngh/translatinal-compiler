/* Directives are skipped by trco (no preprocessor), but the file must
 * still compile in the plain C front-end path.
 */
#include <stdio.h>

int main() {
    int a = 5;
    a = 10;
    printf("value = %d\n", a);
    return 0;
}
