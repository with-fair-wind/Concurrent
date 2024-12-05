#include <iostream>
#include <utility>
#include <vector>

class CopyOnly
{
public:
    CopyOnly(int value) : value_(value) {}
    CopyOnly(const CopyOnly &other) : value_(other.value_) { std::cout << "Copy constructor\n"; }
    CopyOnly &operator=(const CopyOnly &other)
    {
        std::cout << "Copy assignment\n";
        value_ = other.value_;
        return *this;
    }
#ifdef MOVE
#if 1
    CopyOnly(CopyOnly &&other) : value_(other.value_) { std::cout << "move constructor\n"; }
    CopyOnly &operator=(CopyOnly &&other)
    {
        std::cout << "move assignment\n";
        value_ = other.value_;
        return *this;
    }
#else
    CopyOnly(CopyOnly &&other) = default;
    CopyOnly &operator=(CopyOnly &&other) = default;
#endif
#endif
    void display() const { std::cout << value_ << std::endl; }

private:
    int value_;
};

// std::move 本质上只是一个类型转换，将传入的对象转换为一个右值引用，但它本身不决定移动的行为。移动语义的真正实现取决于对象的移动构造函数和移动赋值运算符。

// 如果对象类型未定义移动操作，编译器将尝试使用拷贝构造和拷贝赋值。 const 左值引用可以接受右值
// 因此，即使类型 T 不支持移动，std::move 仍然可以使用（会触发拷贝操作）

// 有些容器（如std::vector）通过提供特化版本来提高swap的效率。例如，std::vector 的 swap 仅交换内部指针，而不实际移动元素

void stringSwap()
{
    std::string str1 = "Hello, World!";
    std::string str2 = "C++ is great!";

    // 获取交换前的底层数据地址
    const char *ptr1 = str1.data();
    const char *ptr2 = str2.data();

    std::cout << "Before swap:" << std::endl;
    std::cout << "str1: " << str1 << " (address: " << static_cast<const void *>(ptr1) << ")" << std::endl;
    std::cout << "str2: " << str2 << " (address: " << static_cast<const void *>(ptr2) << ")" << std::endl;

    // 交换
    std::swap(str1, str2);

    // 获取交换后的底层数据地址
    const char *ptr1_after = str1.data();
    const char *ptr2_after = str2.data();

    std::cout << "\nAfter swap:" << std::endl;
    std::cout << "str1: " << str1 << " (address: " << static_cast<const void *>(ptr1_after) << ")" << std::endl;
    std::cout << "str2: " << str2 << " (address: " << static_cast<const void *>(ptr2_after) << ")" << std::endl;

    // 检查地址是否被交换
    if (ptr1 == ptr2_after && ptr2 == ptr1_after)
    {
        std::cout << "\nSwap only exchanged pointers (efficient)." << std::endl;
    }
    else
    {
        std::cout << "\nSwap performed a deep copy (inefficient)." << std::endl;
    }
}

void vectorSwap()
{
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    std::vector<int> vec2 = {10, 20, 30, 40, 50};

    // 获取交换前的底层数据地址
    const int *ptr1 = vec1.data();
    const int *ptr2 = vec2.data();

    std::cout << "Before swap:" << std::endl;
    std::cout << "vec1: ";
    for (int v : vec1)
        std::cout << v << " ";
    std::cout << " (address: " << static_cast<const void *>(ptr1) << ")" << std::endl;

    std::cout << "vec2: ";
    for (int v : vec2)
        std::cout << v << " ";
    std::cout << " (address: " << static_cast<const void *>(ptr2) << ")" << std::endl;

    // 交换
    std::swap(vec1, vec2);

    // 获取交换后的底层数据地址
    const int *ptr1_after = vec1.data();
    const int *ptr2_after = vec2.data();

    std::cout << "\nAfter swap:" << std::endl;
    std::cout << "vec1: ";
    for (int v : vec1)
        std::cout << v << " ";
    std::cout << " (address: " << static_cast<const void *>(ptr1_after) << ")" << std::endl;

    std::cout << "vec2: ";
    for (int v : vec2)
        std::cout << v << " ";
    std::cout << " (address: " << static_cast<const void *>(ptr2_after) << ")" << std::endl;

    // 检查地址是否被交换
    if (ptr1 == ptr2_after && ptr2 == ptr1_after)
    {
        std::cout << "\nSwap only exchanged pointers (efficient)." << std::endl;
    }
    else
    {
        std::cout << "\nSwap performed a deep copy (inefficient)." << std::endl;
    }
}

int main()
{
    CopyOnly a(10), b(20);
    std::swap(a, b); // 触发拷贝构造和拷贝赋值
    a.display();     // 输出：20
    b.display();     // 输出：10

    stringSwap();
    std::cout << std::endl;
    vectorSwap();
    return 0;
}
