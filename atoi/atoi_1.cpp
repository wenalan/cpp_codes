// g++ -std=gnu++17 -O2 -Wall -Wextra atoi_sv.cpp && ./a.out
#include <string_view>
#include <climits>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------
// 面试版实现：不使用 long long，计算前做溢出预判；签名用 string_view
// ---------------------------------------------------------
int myAtoi(std::string_view s) {
    int i=0;
    int n=s.size();

    //space
    while (i<n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

    //sign
    int sign = 1;
    if (i<n && (s[i]=='+' || s[i]=='-')) {
        sign = (s[i]=='-') ? -1 : 1;
        ++i; //
    }

    //digit
    int res=0;
    while (i<n && std::isdigit(static_cast<unsigned char>(s[i]))) {
        int d = s[i] - '0';

        if (res > INT_MAX/10 || (res==INT_MAX/10 && d>7)) {
            return sign==-1 ? INT_MIN : INT_MAX;
        }
        
        res = res*10 + d;
        ++i;
    }
    return sign * res; //
}

// -------------------------
// 轻量测试工具
// -------------------------
struct Case {
    std::string input;
    int expected;
};

void run_case(const Case& c, int& passed, int& failed) {
    int got = myAtoi(std::string_view{c.input});
    bool ok = (got == c.expected);
    if (ok) {
        ++passed;
    } else {
        ++failed;
        std::cout << "[FAIL] input: \"" << c.input << "\"  expected: " << c.expected
                  << "  got: " << got << "\n";
    }
}

int main() {
    std::vector<Case> cases = {
        // 基本
        {"42", 42},
        {"0", 0},
        {"-0", 0},
        {"-1", -1},

        // 前导空格 + 符号
        {"   -42", -42},
        {"   +42", 42},
        {"   +00000123", 123},
        {"   -00000123", -123},

        // 混合文本
        {"4193 with words", 4193},
        {"words and 987", 0},
        {"+", 0},
        {"-", 0},
        {" + 1", 0},              // 符号后不是数字
        {"   0032abc45", 32},

        // 溢出（正/负）
        {"91283472332", INT_MAX},
        {"2147483647", INT_MAX},  // 边界：INT_MAX
        {"2147483648", INT_MAX},  // 超过上界
        {"-91283472332", INT_MIN},
        {"-2147483648", INT_MIN}, // 边界：INT_MIN
        {"-2147483649", INT_MIN}, // 超过下界

        // 前缀空白 + 尾随空白
        {"   123   ", 123},

        // 非ASCII数字前缀（应停止于第一非digit）
        {"\t\n  77xyz", 77},

        // 大量前导零与边界相邻
        {"00000000002147483647", INT_MAX}, // 2147483647
        {"00000000002147483648", INT_MAX},
        {"-00000000002147483648", INT_MIN},
        {"-00000000002147483649", INT_MIN},

        // 空串
        {"", 0},
        {"    ", 0},
    };

    int passed = 0, failed = 0;
    for (const auto& c : cases) run_case(c, passed, failed);

    std::cout << "Tests passed: " << passed << " / " << (passed + failed) << "\n";
    if (failed) return 1;
    return 0;
}
