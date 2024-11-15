#include <iostream>
class MyClass;

using fptr = void (*)();
using funcptr = void (MyClass::*)(int);

class MyClass
{
public:
    // 静态成员函数
    static void sayHello()
    {
        std::cout << "Hello from MyClass!" << std::endl;
    }

    void func(int n)
    {
        std::cout << "MyClass func: " << n << std::endl;
    }

    // 转换运算符：返回一个指向静态成员函数的指针
    operator fptr()
    {
        return &MyClass::sayHello; // 返回一个指向静态成员函数的指针
    }

    operator funcptr()
    {
        return &func;
    }
};

int main()
{
    MyClass obj;

    // obj(5);
    funcptr ptr = obj; // 转换为成员函数指针
    (obj.*ptr)(5);

    // 将 obj 转换为函数指针
    void (*funcPtr)() = obj;

    // 调用静态成员函数
    funcPtr(); // 输出 "Hello from MyClass!"
}
