#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <string>

using namespace std;

const auto CELL_SIZE{30000};
unordered_set<char> collect{'+', '-', '>', '<', '.', '[', ']'};

bool build_hash(string& code,
                unordered_map<int, int>& forward_hash,
                unordered_map<int, int>& backward_hash)
{
  stack<int> s;
  for (size_t i{0}; i < code.size(); ++i)
  {
    auto ch{code[i]};
    if (ch == '[') { s.push(i); }
    if (ch == ']')
    {
      if (s.empty()) { return false; }
      forward_hash[s.top()] = i;
      backward_hash[i] = s.top();
      s.pop();
    }
  }
  if (s.size()) { return false; }
  return true;
}

bool isWhiteSpace(char ch)
{
  if (ch == ' ' || ch == '\t' || ch == '\n') { return true; }
  return false;
}

void fun(string& code)
{
  vector<int> cell(CELL_SIZE, 0);
  auto data_ptr{0};

  unordered_map<int, int> forward_hash;
  unordered_map<int, int> backward_hash;
  if (false == build_hash(code, forward_hash, backward_hash))
  {
    cout << "ERROR: unmatched []" << endl;
    return;
  }

  size_t i{0};
  while (i < code.size())
  {
    char ch{code[i]};
    if (isWhiteSpace(ch)) { ++i; continue; }
    if (0 == collect.count(ch))
    {
      cout << "ERROR: invalid char" << endl;
      return;
    }
    switch (ch)
    {
      case '+':
        ++cell[data_ptr];
        break;
      case '-':
        --cell[data_ptr];
        break;
      case '<':
        --data_ptr;
        if (data_ptr < 0)
        {
          cout << "ERROR: underflow" << endl;
          return;
        }
        break;
      case '>':
        ++data_ptr;
        if (data_ptr >= CELL_SIZE)
        {
          cout << "ERROR: overflow" << endl;
          return;
        }
        break;
      case '.':
        cout << cell[data_ptr];
        break;
      case '[':
        if (cell[data_ptr] == 0) { i = forward_hash[i]; }
        break;
      case ']':
        if (cell[data_ptr]) { i = backward_hash[i]; }
        break;
      default:
        break;
    }
    ++i;
  }
  cout << endl;
}

int main() {
  string program{"+++++   >+++ [<+.>-]"}; // output 678
  fun(program);
  return 0;
}
