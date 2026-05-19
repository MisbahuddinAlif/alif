int sum(int a, int b) {
    return a + b;
}

int factorial(int n) {
    int result;
    result = 1;
    while (n > 0) {
        result = result * n;
        n = n - 1;
    }
    return result;
}

int main() {
    int x;
    int y;
    int z;
    int arr[5];
    int i;

    x = 10;
    y = 20;
    z = sum(x, y);
    printf(z);

    i = 0;
    while (i < 5) {
        arr[i] = i * 2;
        i++;
    }

    if (x > y) {
        z = x;
    } else {
        z = y;
    }

    for (i = 0; i < 5; i++) {
        arr[i] = arr[i] + 1;
    }

    z = factorial(5);
    printf(z);

    return 0;
}
