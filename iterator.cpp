#include <vector>
#include <iostream>
#include <istream>
#include <sstream>

#include <stdexcept>
#include <optional>

std::optional<int> convert(const std::string &str)
{
  if (0 == str.size()) { return std::nullopt; }

  size_t startIdx{0};
  while (startIdx < str.size() && ' ' == str[startIdx]) { ++startIdx; }
  if (startIdx == str.size()) { return std::nullopt; }

  auto positive{true};
  if ('-' == str[startIdx])
  {
    if (startIdx + 1 == str.size()) { return std::nullopt; }
    positive = false;
    ++startIdx;
  }

  if ('+' == str[startIdx])
  {
    if (startIdx + 1 == str.size()) { return std::nullopt; }
    ++startIdx;
  }

  auto ret{0};
  for (size_t i = startIdx; i < str.size(); ++i)
  {
    if (str[i] < '0' || str[i] > '9') { return std::nullopt; }
    if (i == startIdx && '0' == str[i] && i != str.size() - 1) { return std::nullopt; }

    ret = ret * 10 + str[i] - '0';

    if ((ret == 1000000000 && i != str.size() - 1) || ret > 1000000000) { return std::nullopt; }
  }

  if (!positive) { ret = -ret; }
  return ret;
}


class Solution
{
public:
  class iterator
  {
  public:
    iterator(int* ptr) { m_ptr = ptr; }

    int operator*() const
    {
      if (nullptr == m_ptr) { throw std::runtime_error("error"); }
      return *m_ptr;
    }

    iterator& operator++()
    {
      ++m_ptr;
      return *this;
    }

    iterator operator++(int)
    {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    friend bool operator != (const iterator& a, const iterator& b) { return a.m_ptr != b.m_ptr; }

  private:
    int* m_ptr;
  };

  Solution(std::istream &stream)
  {
    std::string str;
    while(std::getline(stream, str))
    {
      auto ret = convert(str);
      if (std::nullopt != ret) { m_data.push_back(*ret); }
    }
  }

  iterator begin() { return 0 == m_data.size() ? nullptr : iterator(&m_data[0]); }
  iterator end() { return 0 == m_data.size() ? nullptr : iterator(&m_data[m_data.size() - 1] + 1); }

private:
  std::vector<int> m_data;
};



int main()
{
    int val;
    auto ret = convert("0");
    std::cout << "ret " << *ret << " val " << val << std::endl;
    ret = convert("98");
    std::cout << "ret " << *ret << " val " << val << std::endl;
    ret = convert("098");
    std::cout << "ret " << *ret << " val " << val << std::endl;
    ret = convert("-398");
    std::cout << "ret " << *ret << " val " << val << std::endl;
    ret = convert("-0398");
    std::cout << "ret " << *ret << " val " << val << std::endl;
    ret = convert("1 098");
    std::cout << "ret " << *ret << " val " << val << std::endl;

    //std::vector<std::string> v1 { "98", "098", "-398", "-0398", "3213", "1 098", "1000000000", "1000000001", "999999999", "+1", " -1" };

    std::stringstream s1("98\n098\n-398\n-0398\n3123\n1 098\n1000000000\n1000000001\n999999999");
    std::istream s2(s1.rdbuf()); //just creates the istream to start with

    Solution s(s2);
    Solution::iterator it = s.begin();
    std::cout << "it result:" << std::endl;
    while (it != s.end()) {
        std::cout << *it << std::endl;
        ++it;
    }

    return 0;
}

