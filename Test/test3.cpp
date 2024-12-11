#include <functional>
#include <iostream>

void f(int &n)
{
    std::cout << &n << '\n';
}

int main()
{
    int n = 10;
    std::cout << &n << std::endl;
    std::function<void()> task = std::bind(f, n);
    task();
}