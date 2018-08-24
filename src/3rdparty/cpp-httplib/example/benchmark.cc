#include <httplib.h>
#include <chrono>
#include <iostream>

using namespace std;

struct StopWatch {
  StopWatch(const string& label) : label_(label) {
    start_ = chrono::system_clock::now();
  }
  ~StopWatch() {
    auto end = chrono::system_clock::now();
    auto diff = end - start_;
    auto count = chrono::duration_cast<chrono::milliseconds>(diff).count();
    cout << label_ << ": " << count << " millisec." << endl;
  }
  string label_;
  chrono::system_clock::time_point start_;
};

int main(int argc, char* argv[]) {
  string body(1024 * 5, 'a');

  httplib::Client cli("httpbin.org", 80);

  for (int i = 0; i < 3; i++) {
    StopWatch sw(to_string(i).c_str());
    auto res = cli.post("/post", body, "application/octet-stream");
    assert(res->status == 200);
  }

  return 0;
}