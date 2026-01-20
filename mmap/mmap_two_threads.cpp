#include <atomic>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr const char* kShmName = "/mmap-two-thread-example";
constexpr std::size_t kPayloadSize = 1024;

struct SharedRegion {
  std::atomic<uint32_t> length{0};  // number of bytes currently valid in data
  std::atomic<bool> done{false};    // set by writer when it is time to stop
  char data[kPayloadSize];
};

SharedRegion* map_region(bool create_writer_view) {
  const int flags = create_writer_view ? (O_CREAT | O_RDWR) : O_RDWR;
  const int fd = shm_open(kShmName, flags, 0600);
  if (fd == -1) {
    perror("shm_open");
    std::exit(1);
  }

  if (create_writer_view) {
    if (ftruncate(fd, sizeof(SharedRegion)) == -1) {
      perror("ftruncate");
      std::exit(1);
    }
  }

  const int prot = create_writer_view ? (PROT_READ | PROT_WRITE) : PROT_READ;
  void* addr = mmap(nullptr, sizeof(SharedRegion), prot, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    std::exit(1);
  }

  close(fd);
  return static_cast<SharedRegion*>(addr);
}

void writer() {
  SharedRegion* region = map_region(true);
  std::memset(region, 0, sizeof(SharedRegion));

  std::string line;
  std::cout << "Type lines for thread A to write (quit to stop):" << std::endl;
  while (std::getline(std::cin, line)) {
    if (line == "quit") {
      break;
    }
    const std::size_t len = std::min(line.size(), kPayloadSize - 1);
    std::memcpy(region->data, line.data(), len);
    region->data[len] = '\0';
    region->length.store(static_cast<uint32_t>(len), std::memory_order_release);
  }

  region->done.store(true, std::memory_order_release);
  munmap(region, sizeof(SharedRegion));
}

void reader() {
  SharedRegion* region = map_region(false);
  uint32_t last_len = std::numeric_limits<uint32_t>::max();

  while (true) {
    const uint32_t len = region->length.load(std::memory_order_acquire);
    if (len > 0 && len != last_len) {
      std::string text(region->data, region->data + len);
      std::cout << "[thread B] read: " << text << std::endl;
      last_len = len;
    }

    if (region->done.load(std::memory_order_acquire)) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  munmap(region, sizeof(SharedRegion));
}

}  // namespace

int main() {
  std::thread t_writer(writer);

  // Give the writer a short head start to create the shared memory segment.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::thread t_reader(reader);

  t_writer.join();
  t_reader.join();

  // Remove the shared memory object so repeated runs work cleanly.
  shm_unlink(kShmName);
  return 0;
}
