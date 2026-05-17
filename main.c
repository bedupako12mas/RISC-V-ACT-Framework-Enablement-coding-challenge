/* UART interface test using Linux termios */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

static int            serial_fd     = -1;
static struct termios saved_termios;

/* Restores terminal settings on SIGINT/SIGTERM so the shell isn't left in raw mode */
static void restore_and_exit(int sig)
{
    if (serial_fd >= 0) {
        tcsetattr(serial_fd, TCSANOW, &saved_termios);
        close(serial_fd);
        serial_fd = -1;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage:   %s <device>\n", argv[0]);
        fprintf(stderr, "Example: %s /dev/ttyUSB0\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *device = argv[1];

    /* O_NOCTTY: don't make this our controlling terminal
     * O_NDELAY: don't block waiting on carrier detect (USB-UART adapters often don't assert it) */
    serial_fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd < 0) {
        fprintf(stderr, "Error: cannot open '%s': ", device);
        perror(NULL);
        return EXIT_FAILURE;
    }

    /* Switch back to blocking I/O — select() manages the timeout */
    if (fcntl(serial_fd, F_SETFL, 0) < 0) {
        perror("fcntl");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    if (tcgetattr(serial_fd, &saved_termios) < 0) {
        perror("tcgetattr");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    /* Register before applying raw mode so Ctrl+C always cleans up */
    signal(SIGINT,  restore_and_exit);
    signal(SIGTERM, restore_and_exit);

    /* 115200 8N1 — standard default for OpenSBI and U-Boot on RISC-V boards */
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;        /* 8 data bits */
    tty.c_cflag &= ~PARENB;    /* no parity   */
    tty.c_cflag &= ~CSTOPB;    /* 1 stop bit  */
    tty.c_cflag &= ~CRTSCTS;   /* no hardware flow control */
    tty.c_cflag |= CREAD | CLOCAL;

    /* Raw mode: pass bytes through as-is, no line editing or echo */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~(OPOST | ONLCR);

    tty.c_cc[VMIN]  = 0; /* non-blocking reads — select() handles the wait */
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(serial_fd, TCSANOW, &tty) < 0) {
        perror("tcsetattr");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    printf("Opened %s at 115200 baud (8N1)\n", device);

    const char *message       = "Hello from UART\r\n";
    ssize_t     bytes_written = write(serial_fd, message, strlen(message));

    if (bytes_written < 0) {
        perror("write");
        tcsetattr(serial_fd, TCSANOW, &saved_termios);
        close(serial_fd);
        serial_fd = -1;
        return EXIT_FAILURE;
    }

    printf("Transmitted %zd bytes: %s", bytes_written, message);
    printf("Waiting for incoming data (3s timeout)...\n");

    char           rx_buf[256];
    fd_set         read_fds;
    struct timeval timeout;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(serial_fd, &read_fds);

        timeout.tv_sec  = 3; /* reset each iteration — Linux select() modifies the timeval */
        timeout.tv_usec = 0;

        int ret = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret < 0)  { perror("select"); break; }
        if (ret == 0) { printf("No data received within timeout.\n"); break; }

        ssize_t bytes_read = read(serial_fd, rx_buf, sizeof(rx_buf) - 1);

        if (bytes_read < 0) { perror("read"); break; }
        if (bytes_read == 0) break;

        rx_buf[bytes_read] = '\0';
        printf("Received %zd bytes: %s", bytes_read, rx_buf);
    }

    tcsetattr(serial_fd, TCSANOW, &saved_termios);
    close(serial_fd);
    serial_fd = -1;

    return EXIT_SUCCESS;
}
