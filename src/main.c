#include "lab.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

static void print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]\n"
            "                  [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
            "       myapp recv -s <session> [-p port] <relay> <file>\n"
            "\n"
            "  -s <session>     session name shared by the sender and the receiver\n"
            "  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)\n"
            "  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)\n"
            "  -l <loss>        probability the relay drops a packet (default: 0)\n"
            "  -c <corrupt>     probability the relay flips a bit (default: 0)\n"
            "  -d <dup>         probability the relay duplicates a packet (default: 0)\n"
            "  -p <port>        relay port (default: 4250)\n"
            "  <relay>          host name or address of the relay\n"
            "  <file>           file to send, or file to write what is received\n");
}

static int parse_long_value(const char *text, long min, long max, long *value)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < min ||
        parsed > max) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static int parse_probability(const char *text, double *value)
{
    char *end = NULL;
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || parsed < 0.0 ||
        parsed > 1.0) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static int64_t monotonic_milliseconds(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (int64_t)now.tv_sec * INT64_C(1000) +
           now.tv_nsec / INT64_C(1000000);
}

static int wait_for_socket(int socket_fd, int timeout_ms)
{
    struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN};
    int result;

    do {
        result = poll(&descriptor, 1, timeout_ms);
    } while (result < 0 && errno == EINTR);
    return result;
}

static int register_receiver(int socket_fd, const char *session)
{
    char hello[64];
    char response[128];
    int attempt;

    if (snprintf(hello, sizeof(hello), "HELLO %s recv", session) < 0) {
        return -1;
    }
    for (attempt = 0; attempt < 5; ++attempt) {
        size_t hello_length = strlen(hello);
        ssize_t sent = send(socket_fd, hello, hello_length, 0);
        if (sent < 0 || (size_t)sent != hello_length) {
            return -1;
        }
        if (wait_for_socket(socket_fd, 1000) > 0) {
            ssize_t received = recv(socket_fd, response, sizeof(response) - 1, 0);
            if (received < 0) {
                return -1;
            }
            response[received] = '\0';
            if (strcmp(response, "OK") == 0) {
                return 0;
            }
            fprintf(stderr, "relay refused receiver: %s\n", response);
            return -1;
        }
    }
    fprintf(stderr, "relay did not answer receiver registration\n");
    return -1;
}

static int register_sender(int socket_fd, const char *session, double loss,
                           double corrupt, double duplicate)
{
    char hello[128];
    char response[128];
    int attempt;

    if (snprintf(hello, sizeof(hello), "HELLO %s send %g %g %g", session,
                 loss, corrupt, duplicate) < 0) {
        return -1;
    }
    for (attempt = 0; attempt < 5; ++attempt) {
        size_t hello_length = strlen(hello);
        ssize_t sent = send(socket_fd, hello, hello_length, 0);
        if (sent < 0 || (size_t)sent != hello_length) {
            return -1;
        }
        if (wait_for_socket(socket_fd, 1000) > 0) {
            ssize_t received = recv(socket_fd, response, sizeof(response) - 1, 0);
            if (received < 0) {
                return -1;
            }
            response[received] = '\0';
            if (strcmp(response, "OK") == 0) {
                return 0;
            }
            fprintf(stderr, "relay refused sender: %s\n", response);
            return -1;
        }
    }
    fprintf(stderr, "relay did not answer sender registration\n");
    return -1;
}

static int send_packet(int socket_fd, const struct packet *packet)
{
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    size_t wire_length;

    if (packet_encode(packet, wire, sizeof(wire), &wire_length) != 0 ||
        send(socket_fd, wire, wire_length, 0) != (ssize_t)wire_length) {
        return -1;
    }
    return 0;
}

static int send_file(int socket_fd, const char *file_name, long window_size,
                     long timeout_ms)
{
    FILE *input = fopen(file_name, "rb");
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    struct packet incoming;
    struct sender_state sender;
    struct protocol_actions actions;
    uint8_t chunk[PACKET_MAX_PAYLOAD];
    int result = 2;

    if (input == NULL) {
        fprintf(stderr, "could not open input file %s: %s\n", file_name,
                strerror(errno));
        return 2;
    }
    sender_init(&sender, (uint32_t)window_size, timeout_ms);
    for (;;) {
        int64_t now = monotonic_milliseconds();
        if (now < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            goto done;
        }
        while (!sender.input_finished && sender_capacity(&sender) > 0) {
            size_t bytes_read = fread(chunk, 1, sizeof(chunk), input);
            if (bytes_read == 0) {
                if (ferror(input) != 0) {
                    fprintf(stderr, "could not read input file %s: %s\n",
                            file_name, strerror(errno));
                    goto done;
                }
                if (sender_finish(&sender, now, &actions) != 0) {
                    goto done;
                }
                break;
            }
            if (sender_push_data(&sender, chunk, (uint16_t)bytes_read, now,
                                 &actions) != 0) {
                goto done;
            }
            for (size_t i = 0; i < actions.outgoing_count; ++i) {
                if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                    fprintf(stderr, "sender failed to send packet: %s\n",
                            strerror(errno));
                    goto done;
                }
            }
        }
        if (sender.input_finished && sender.base == sender.next &&
            !sender.fin_sent) {
            if (sender_finish(&sender, now, &actions) != 0) {
                goto done;
            }
        }
        if (sender_is_complete(&sender)) {
            result = 0;
            break;
        }
        if (sender.timer_due < 0) {
            continue;
        }
        now = monotonic_milliseconds();
        if (now < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            goto done;
        }
        if (sender.timer_due <= now) {
            int timeout_result = sender_on_timeout(&sender, now, &actions);
            if (timeout_result != 0) {
                fprintf(stderr, "sender gave up after 10 timeouts\n");
                goto done;
            }
            for (size_t i = 0; i < actions.outgoing_count; ++i) {
                if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                    fprintf(stderr, "sender failed to retransmit packet: %s\n",
                            strerror(errno));
                    goto done;
                }
            }
            continue;
        }
        {
            int64_t remaining = sender.timer_due - now;
            int ready = wait_for_socket(
                socket_fd, remaining > INT_MAX ? INT_MAX : (int)remaining);
            if (ready < 0) {
                fprintf(stderr, "sender failed while waiting: %s\n",
                        strerror(errno));
                goto done;
            }
            if (ready == 0) {
                continue;
            }
        }
        {
            ssize_t received = recv(socket_fd, wire, sizeof(wire), 0);
            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                fprintf(stderr, "sender failed to receive ACK: %s\n",
                        strerror(errno));
                goto done;
            }
            if (packet_decode(wire, (size_t)received, &incoming) != 0 ||
                incoming.type != PACKET_ACK || incoming.length != 0) {
                continue;
            }
            now = monotonic_milliseconds();
            if (now < 0) {
                fprintf(stderr, "could not read monotonic clock\n");
                goto done;
            }
            sender_on_ack(&sender, incoming.seq, now, &actions);
            for (size_t i = 0; i < actions.outgoing_count; ++i) {
                if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                    fprintf(stderr, "sender failed to send packet: %s\n",
                            strerror(errno));
                    goto done;
                }
            }
        }
    }

done:
    if (fclose(input) != 0 && result == 0) {
        fprintf(stderr, "could not close input file %s: %s\n", file_name,
                strerror(errno));
        result = 2;
    }
    return result;
}

static int receive_file(int socket_fd, const char *file_name)
{
    FILE *output = fopen(file_name, "wb");
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    struct packet packet;
    struct receiver_state receiver;
    struct protocol_actions actions;
    int64_t last_valid;
    int result = 2;

    if (output == NULL) {
        fprintf(stderr, "could not open output file %s: %s\n", file_name,
                strerror(errno));
        return 2;
    }
    receiver_init(&receiver);
    last_valid = monotonic_milliseconds();
    if (last_valid < 0) {
        fprintf(stderr, "could not read monotonic clock\n");
        (void)fclose(output);
        return 2;
    }
    for (;;) {
        int64_t now = monotonic_milliseconds();
        int timeout_ms;
        ssize_t received;
        int ready;

        if (now < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            break;
        }
        if (receiver_linger_expired(&receiver, now)) {
            result = 0;
            break;
        }
        if (receiver_is_finished(&receiver)) {
            timeout_ms = (int)(receiver.linger_until - now);
        } else {
            timeout_ms = (int)(30000 - (now - last_valid));
        }
        if (timeout_ms <= 0) {
            if (receiver_is_finished(&receiver)) {
                result = 0;
            } else {
                fprintf(stderr, "receiver timed out waiting for a valid packet\n");
            }
            break;
        }
        ready = wait_for_socket(socket_fd, timeout_ms);
        if (ready < 0) {
            fprintf(stderr, "receiver failed while waiting: %s\n",
                    strerror(errno));
            break;
        }
        if (ready == 0) {
            if (receiver_is_finished(&receiver)) {
                result = 0;
            } else {
                fprintf(stderr, "receiver timed out waiting for a valid packet\n");
            }
            break;
        }
        received = recv(socket_fd, wire, sizeof(wire), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "receiver failed to read packet: %s\n",
                    strerror(errno));
            break;
        }
        if (packet_decode(wire, (size_t)received, &packet) != 0) {
            continue;
        }
        last_valid = monotonic_milliseconds();
        if (last_valid < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            break;
        }
        receiver_on_packet(&receiver, &packet, last_valid, &actions);
        if (actions.delivered_packet &&
            fwrite(actions.delivered, 1, actions.delivered_length, output) !=
                actions.delivered_length) {
            fprintf(stderr, "could not write output file %s: %s\n",
                    file_name, strerror(errno));
            break;
        }
        for (size_t i = 0; i < actions.outgoing_count; ++i) {
            if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                fprintf(stderr, "receiver failed to send ACK: %s\n",
                        strerror(errno));
                break;
            }
        }
        if (receiver_is_finished(&receiver)) {
            if (fclose(output) != 0) {
                fprintf(stderr, "could not close output file %s: %s\n",
                        file_name, strerror(errno));
                return 2;
            }
            output = NULL;
        }
    }
    if (output != NULL) {
        (void)fclose(output);
    }
    return result;
}

int main(int argc, char **argv)
{
    const char *mode;
    const char *session = NULL;
    const char *relay;
    const char *file;
    long window = 8;
    long timeout_ms = 250;
    long port = 4250;
    double loss = 0.0;
    double corrupt = 0.0;
    double duplicate = 0.0;
    int option;
    int sender_option_seen = 0;
    struct addrinfo hints = {0};
    struct addrinfo *relay_address = NULL;
    struct addrinfo *address;
    char port_text[6];
    int socket_fd = -1;
    int result;

    if (argc == 1) {
        print_usage(stdout);
        return 0;
    }
    if (argc < 2 || (strcmp(argv[1], "send") != 0 &&
                     strcmp(argv[1], "recv") != 0)) {
        print_usage(stderr);
        return 1;
    }
    mode = argv[1];

    optind = 2;
    opterr = 0;
    while ((option = getopt(argc, argv, "s:w:T:l:c:d:p:")) != -1) {
        switch (option) {
        case 's':
            session = optarg;
            break;
        case 'w':
            if (parse_long_value(optarg, 1, 64, &window) != 0) {
                fprintf(stderr, "invalid window: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'T':
            if (parse_long_value(optarg, 1, INT32_MAX, &timeout_ms) != 0) {
                fprintf(stderr, "invalid timeout: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'l':
            if (parse_probability(optarg, &loss) != 0) {
                fprintf(stderr, "invalid loss probability: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'c':
            if (parse_probability(optarg, &corrupt) != 0) {
                fprintf(stderr, "invalid corruption probability: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'd':
            if (parse_probability(optarg, &duplicate) != 0) {
                fprintf(stderr, "invalid duplicate probability: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'p':
            if (parse_long_value(optarg, 1, 65535, &port) != 0) {
                fprintf(stderr, "invalid port: %s\n", optarg);
                return 1;
            }
            break;
        case '?':
        default:
            print_usage(stderr);
            return 1;
        }
    }

    if (session == NULL || session[0] == '\0' ||
        (strcmp(mode, "recv") == 0 && sender_option_seen) ||
        argc - optind != 2) {
        print_usage(stderr);
        return 1;
    }

    relay = argv[optind];
    file = argv[optind + 1];
    (void)file;
    (void)window;
    (void)timeout_ms;
    (void)loss;
    (void)corrupt;
    (void)duplicate;

    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;
    (void)snprintf(port_text, sizeof(port_text), "%ld", port);
    if (getaddrinfo(relay, port_text, &hints, &relay_address) != 0) {
        fprintf(stderr, "could not resolve relay: %s\n", relay);
        return 2;
    }
    for (address = relay_address; address != NULL; address = address->ai_next) {
        socket_fd = socket(address->ai_family, address->ai_socktype,
                           address->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }
        if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        close(socket_fd);
        socket_fd = -1;
    }
    if (socket_fd < 0) {
        fprintf(stderr, "could not connect to relay: %s\n", strerror(errno));
        freeaddrinfo(relay_address);
        return 2;
    }
    if (strcmp(mode, "send") == 0) {
        result = register_sender(socket_fd, session, loss, corrupt, duplicate);
        if (result == 0) {
            result = send_file(socket_fd, file, window, timeout_ms);
        }
    } else {
        result = register_receiver(socket_fd, session);
        if (result == 0) {
            result = receive_file(socket_fd, file);
        }
    }
    close(socket_fd);
    freeaddrinfo(relay_address);
    return result == 0 ? 0 : 2;
}
