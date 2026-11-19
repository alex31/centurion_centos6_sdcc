int sum4(int a, int b, int c, int d)
{
    return a + b + c + d;
}

int factorial(int n)
{
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

int main(void)
{
    int values[4];
    int *p = values;

    values[0] = 1000;
    values[1] = -200;
    values[2] = 300;
    values[3] = 40;
    if (sum4(p[0], p[1], p[2], p[3]) != 1140)
        return 1;
    if (factorial(5) != 120)
        return 2;
    if ((unsigned int)65530 + 10 != 4)
        return 3;
    return 7;
}
