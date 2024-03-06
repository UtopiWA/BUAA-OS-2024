#include <stdio.h>
int check(int n)
{
    int a[10], len = 0;
    while (n)
    {
        a[len++] = n % 10;
        n /= 10;
    }
    for (int i = 0; i < len; i++)
        if (a[i] != a[len - i - 1])
            return 0;
    return 1;
}
int main() {
	int n;
	scanf("%d", &n);

	if (check(n)) {
		printf("Y\n");
	} else {
		printf("N\n");
	}
	return 0;
}
