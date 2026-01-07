#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

void handle_winch(int sig) {
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
    } else {
        printf("SIGWINCH received! Rows: %d, Cols: %d\n", ws.ws_row, ws.ws_col);
    }
}

int main() {
    struct winsize ws;
    
    signal(SIGWINCH, handle_winch);
    
    printf("SIGWINCH test program running. PID: %d\n", getpid());
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
    } else {
        printf("Initial size: Rows: %d, Cols: %d\n", ws.ws_row, ws.ws_col);
    }
    
    while(1) {
        sleep(2);
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
            perror("poll ioctl");
        } else {
            printf("Polled size: Rows: %d, Cols: %d\n", ws.ws_row, ws.ws_col);
        }
    }
    return 0;
}
