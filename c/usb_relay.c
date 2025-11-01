#include <stdio.h>
#include <stdlib.h>
#include <termios.h> // For termios structure and functions
#include <unistd.h>
#include <string.h>
#include <fcntl.h> // For O_RDWR

// Serial port exchange protocol with ch340:
//
// Request: 4 bytes length.
// Reply: 4 bytes.
// Message format: start_symbol, port_number, command, check_sum
// Start symbol = 0xa0
// Port number starts since 1
// Commands: 0 = close (quiet),
//           1 = open (quiet),
//           2 = close (with previous port status reply),
//           3 = open (same as above),
//           4 = switch (same as above),
//           5 = request port status (reply current port status)
// Checksum: 0xa0 + port_number + command.
//

struct relay_message {
    char sts;
#define START_SYMBOL            0xa0
    char port;
    char cmd;
#define CMD_CLOSE_QUIET         0x00
#define CMD_OPEN_QUIET          0x01
#define CMD_CLOSE               0x02
#define CMD_OPEN                0x03
#define CMD_SWITCH              0x04
#define CMD_STATUS              0x05
#define STATUS_CLOSED           0x00
#define STATUS_OPENED           0x01
    char csum;
#define CMD_INIT(p, c) \
    { .sts = START_SYMBOL, .port = p, .cmd = c, .csum = (unsigned char)(START_SYMBOL + p + c) }
#define CMD_UPDATE(m, p, c) \
    do { (m).sts = START_SYMBOL; (m).port = p; (m).cmd = c; (m).csum = (unsigned char)((m).sts+(m).port+(m).cmd); } while(0);
};


// Usage help information
void usage(char *progname)
{
    printf("Usage: %s -f <tty_file> -p <port> -c <command>\n\n"
           "    -f      path to serial file (/dev/ttyUSB0 for example)\n"
           "    -p      port number (>= 1)\n"
           "    -c      command ( open, close, switch, status )\n"
           "    -q      be quite (do not print previous port status)\n\n",
            progname);
}


// Setup tty device
int setup_tty(int tty_fd)
{
    struct termios tty_settings;
    if (tcgetattr(tty_fd, &tty_settings) != 0) {
        perror("Error getting TTY attributes");
        return -1;
    }

    // Example settings:
    cfsetispeed(&tty_settings, B9600); // Input baud rate
    cfsetospeed(&tty_settings, B9600); // Output baud rate
    tty_settings.c_cflag &= ~PARENB;   // No parity
    tty_settings.c_cflag &= ~CSTOPB;   // 1 stop bit
    tty_settings.c_cflag &= ~CSIZE;    // Clear data size bits
    tty_settings.c_cflag |= CS8;       // 8 data bits
    tty_settings.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines
    // Enable Raw Input
    tty_settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    // Disable Software Flow control
    tty_settings.c_iflag &= ~(IXON | IXOFF | IXANY);
    // Chose raw (not processed) output
    tty_settings.c_oflag &= ~OPOST;

    // Blocking read for 4 bytes:
    tty_settings.c_cc[VMIN] = 4; // Read returns after 4 bytes are received
    tty_settings.c_cc[VTIME] = 0; // No inter-character timeout

    if (tcsetattr(tty_fd, TCSANOW, &tty_settings) != 0) {
        perror("Error setting TTY attributes");
        return -1;
    }

    fcntl(tty_fd, F_SETFL, FNDELAY);

    return 0;
}


int main(int argc, char *argv[])
{
    struct relay_message reply, msg = CMD_INIT(-1, -1);
    char *tty_file = NULL;
    int tty_fd;
    int quiet = 0;
    int ret = 0;
    char c;

    // parse command line arguments
    while ((c = getopt(argc, argv, "f:p:c:q")) != -1) {
        switch (c) {
            case 'f':
                tty_file = optarg;
                break;
            case 'p':
                msg.port = atoi(optarg);
                break;
            case 'q':
                quiet = 1;
                break;
            case 'c':
                if (strncmp(optarg, "open_q", 6) == 0)
                    msg.cmd = CMD_OPEN_QUIET;
                else if (strncmp(optarg, "close_q", 7) == 0)
                    msg.cmd = CMD_CLOSE_QUIET;
                else if (strncmp(optarg, "open", 4) == 0)
                    msg.cmd = CMD_OPEN;
                else if (strncmp(optarg, "close", 5) == 0)
                    msg.cmd = CMD_CLOSE;
                else if (strncmp(optarg, "switch", 6) == 0)
                    msg.cmd = CMD_SWITCH;
                else if (strncmp(optarg, "status", 6) == 0)
                    msg.cmd = CMD_STATUS;
                else
                    printf("unknown command: %s\n", optarg);
                break;
        }
    }

    if(!tty_file || msg.cmd == -1 || msg.port == -1) {
        usage(argv[0]);
        exit(-1);
    }

    tty_fd = open(tty_file, O_RDWR | O_NOCTTY | O_SYNC);
    if (tty_fd < 0) {
        perror("can not open tty file");
        exit(-1);
    }

    if (setup_tty(tty_fd) < 0) {
        ret = -1;
        goto quit;
    }

    CMD_UPDATE(msg, msg.port, msg.cmd);

    // send command
    write(tty_fd, (char *)&msg, sizeof(msg));
    if (msg.cmd == CMD_CLOSE_QUIET || msg.cmd == CMD_OPEN_QUIET || quiet)
        goto quit;

    // wait for reply
    usleep(20000);

    // read reply
    ssize_t bytes_read = read(tty_fd, (char *)&reply, sizeof(reply));
    if (bytes_read != 4 || ((reply.sts + reply.port + reply.cmd) != reply.csum)) {
        perror("incorrect reply is gotten\n");
        ret = -1;
        goto quit;
    }

    // print previous port status
    printf("%s\n", reply.cmd?"opened":"closed");

quit:
    close(tty_fd);
    return ret;
}
