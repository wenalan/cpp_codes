#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void test_et_overflow() {
    printf("=== 测试ET模式事件溢出 ===\n\n");
    
    int epfd = epoll_create1(0);
    const int TOTAL = 15;
    const int MAX = 10;
    int sockets[TOTAL];
    
    // 创建15个socket，都写入数据
    for (int i = 0; i < TOTAL; i++) {
        int sv[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
        sockets[i] = sv[0];
        
        fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL, 0) | O_NONBLOCK);
        
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  // ET模式
        ev.data.u32 = i;
        epoll_ctl(epfd, EPOLL_CTL_ADD, sv[0], &ev);
        
        write(sv[1], "data", 4);
        close(sv[1]);
    }
    
    struct epoll_event events[MAX];
    int processed = 0;
    int calls = 0;
    
    printf("有 %d 个socket就绪，maxevents=%d\n", TOTAL, MAX);
    
    // 循环处理
    while (processed < TOTAL) {
        calls++;
        int n = epoll_wait(epfd, events, MAX, 1000);  // 1秒超时
        
        printf("\n第 %d 次 epoll_wait:\n", calls);
        
        if (n < 0) {
            perror("epoll_wait");
            break;
        } else if (n == 0) {
            printf("  超时\n");
            break;
        }
        
        printf("  返回 %d 个事件\n", n);
        
        for (int i = 0; i < n; i++) {
            int idx = events[i].data.u32;
            printf("  socket[%d] 就绪\n", idx);
            
            // 读取数据
            char buf[10];
            ssize_t bytes = recv(sockets[idx], buf, sizeof(buf), 0);
            printf("    读取 %zd 字节\n", bytes);
            
            processed++;
        }
    }
    
    printf("\n总计：%d 次调用处理了 %d 个socket\n", calls, processed);
    
    // 结果会是：
    // 第1次：返回10个
    // 第2次：立即返回5个（不需要新事件！）
    // 总计：2次调用
}

int main() {
	test_et_overflow();
}
