#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <string>

using namespace std;

const auto MEM_SIZE{30000};
unordered_set<char> keywords{'+', '-', '>', '<', '.', '[', ']'};

bool prepareJumpTable(string_view program,
                      unordered_map<int, int>& forward,
                      unordered_map<int, int>& backward)
{
  stack<int> s;
  for (size_t i{0}; i < program.size(); ++i)
  {
    auto ch{program[i]};
    if (ch == '[') { s.push(i); }
    if (ch == ']')
    {
      if (s.empty()) { return false; }
      forward[s.top()] = i;
      backward[i] = s.top();
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

void interpreter(string_view program)
{
  vector<char> memory(MEM_SIZE, 0);
  auto dp{0};

  unordered_map<int, int> forward;
  unordered_map<int, int> backward;
  if (false == prepareJumpTable(program, forward, backward))
  {
    cout << "ERROR: unmatched []" << endl;
    return;
  }

  size_t ip{0};
  while (ip < program.size())
  {
    char ch{program[ip]};
    if (isWhiteSpace(ch)) { ++ip; continue; }
    if (0 == keywords.count(ch))
    {
      cout << "ERROR: invalid program" << endl;
      return;
    }
    switch (ch)
    {
      case '+':
        ++memory[dp];
        break;
      case '-':
        --memory[dp];
        break;
      case '<':
        --dp;
        if (dp < 0)
        {
          cout << "ERROR: underflow" << endl;
          return;
        }
        break;
      case '>':
        ++dp;
        if (dp >= MEM_SIZE)
        {
          cout << "ERROR: overflow" << endl;
          return;
        }
        break;
      case '.':
        cout << memory[dp];
        break;
      case '[':
        if (memory[dp] == 0) { ip = forward[ip]; }
        break;
      case ']':
        if (memory[dp]) { ip = backward[ip]; }
        break;
      case ',':
        // not support input file
        break;
      default:
        break;
    }
    ++ip;
  }
  cout << endl;
}

int main() {
  string_view helloWorld{"++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++."}; // output Hello World
  interpreter(helloWorld);

  string_view triangle{
    "++++++++[>+>++++<<-]>++>>+<[-[>>+<<-]+>>]>+["
      "-<<<["
        "->[+[-]+>++>>>-<<]<[<]>>++++++[<<+++++>>-]+<<++.[-]<<"
      "]>.>+[>>]>+"
    "]"};  // output Sierpinski triangle
  interpreter(triangle);

  return 0;
}
