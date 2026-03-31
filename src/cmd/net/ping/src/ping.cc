#include <sys/types.h>
#include <sys/param.h>  /* Often defines BYTE_ORDER */
#include <sys/time.h>

/* Force endianness detection for netinet headers if host fails */
#ifndef BYTE_ORDER
# ifdef _LITTLE_ENDIAN
#  define BYTE_ORDER 1234
#  define LITTLE_ENDIAN 1234
# elif defined(_BIG_ENDIAN)
#  define BYTE_ORDER 4321
#  define BIG_ENDIAN 4321
# endif
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

/* Standard C++20 continues below */
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <csignal>
#include <cstring>

#include <kayoko/net/icmp.hh>

using namespace std::chrono;
namespace kn = kayoko::net;

/* Global state for the summary (Standard BSD practice) */
struct ping_state {
    std::string target_name;
    std::string target_ip;
    size_t tx{0};
    size_t rx{0};
    std::vector<double> rtts;
} state;

/* Standard Internet Checksum - POSIX/BSD compliant */
uint16_t calculate_checksum(const uint16_t* buf, size_t len) {
    uint32_t sum = 0;
    const uint16_t* w = buf;
    size_t nleft = len;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        uint16_t left = 0;
        std::memcpy(&left, w, 1);
        sum += left;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}


void process_packet(uint8_t* buffer, ssize_t total_len, uint16_t expected_id) {
    // 1. Minimum safety check: Must contain at least a basic IP header (20 bytes)
    if (total_len < static_cast<ssize_t>(sizeof(struct ip))) {
        return;
    }

    // 2. Map the IP header
    struct ip* ip_h = reinterpret_cast<struct ip*>(buffer);

    // 3. Extract Header Length (IHL)
    // ip_hl is the number of 32-bit words. Multiply by 4 (shift left 2) to get bytes.
    size_t ip_hlen = ip_h->ip_hl << 2;

    // 4. Safety check: Ensure the buffer actually contains the full IP header 
    // and at least the ICMP header (8 bytes)
    if (total_len < static_cast<ssize_t>(ip_hlen + sizeof(kn::icmp_hdr))) {
        return;
    }

    // 5. Offset the buffer to find the ICMP header
    kn::icmp_hdr* reply = reinterpret_cast<kn::icmp_hdr*>(buffer + ip_hlen);

    // 6. Validate the ICMP packet
    // Note: We use ntohs/htons because 'seq' and 'id' are multi-byte fields
    if (reply->type == kn::echo_reply && ntohs(reply->id) == expected_id) {
        state.rx++;
        
        // Calculate ICMP payload size for standard output (usually 64 bytes)
        // total_len (84) - ip_hlen (20) = 64 bytes
        size_t icmp_len = total_len - ip_hlen;

        std::cout << icmp_len << " bytes from " << inet_ntoa(ip_h->ip_src)
                  << ": icmp_seq=" << ntohs(reply->seq)
                  << " ttl=" << static_cast<int>(ip_h->ip_ttl)
                  << " time=..." << std::endl;
    }
}


void handle_sigint(int sig) {
    (void)sig;
    std::cout << "\n----" << state.target_name << " PING Statistics----" << std::endl;
    
    double loss = state.tx ? (100.0 * (state.tx - state.rx) / state.tx) : 0.0;
    std::cout << state.tx << " packets transmitted, " << state.rx << " packets received, " 
              << std::fixed << std::setprecision(1) << loss << "% packet loss" << std::endl;

    if (!state.rtts.empty()) {
        auto [mi, ma] = std::minmax_element(state.rtts.begin(), state.rtts.end());
        double sum = std::accumulate(state.rtts.begin(), state.rtts.end(), 0.0);
        double avg = sum / state.rtts.size();
        
        double variance = 0;
        for(double r : state.rtts) {
            variance += (r - avg) * (r - avg);
        }
        double stddev = std::sqrt(variance / state.rtts.size());

        std::cout << "round-trip min/avg/max/stddev = " 
                  << std::fixed << std::setprecision(6) 
                  << *mi << "/" << avg << "/" << *ma << "/" << stddev << " ms" << std::endl;
    }
    std::exit(0);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: ping host" << std::endl;
        return 1;
    }

    /* Resolve host */
    struct hostent *he = gethostbyname(argv[1]);
    if (!he) {
        std::cerr << "ping: unknown host " << argv[1] << std::endl;
        return 1;
    }
    
    struct in_addr addr;
    std::memcpy(&addr, he->h_addr, sizeof(addr));
    state.target_ip = inet_ntoa(addr);

    /* Reverse lookup for the BSD-style header */
    struct hostent *rev = gethostbyaddr(reinterpret_cast<const char*>(&addr), sizeof(addr), AF_INET);
    state.target_name = rev ? rev->h_name : state.target_ip;

    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        perror("ping: socket");
        return 1;
    }

    std::signal(SIGINT, handle_sigint);

    std::cout << "PING " << state.target_name << " (" << state.target_ip << "): 56 data bytes" << std::endl;

    uint16_t seq = 0;
    uint16_t id = static_cast<uint16_t>(getpid() & 0xFFFF);

    while (true) {
        /* 1. Prepare and Send Packet */
        std::vector<uint8_t> pkt(64, 0); // 8-byte ICMP header + 56-byte payload
        kn::icmp_hdr hdr;
        hdr.type = kn::echo_request;
        hdr.code = 0;
        hdr.checksum = 0;
        hdr.id = htons(id);           // Use network byte order
        hdr.seq = htons(seq++);

        std::memcpy(pkt.data(), &hdr, sizeof(hdr));
        uint16_t cksum = calculate_checksum(reinterpret_cast<uint16_t*>(pkt.data()), pkt.size());
        std::memcpy(pkt.data() + 2, &cksum, sizeof(cksum));

        auto start = high_resolution_clock::now();
        struct sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_addr = addr;

        if (sendto(fd, pkt.data(), pkt.size(), 0, reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst)) > 0) {
            state.tx++;
        }

        /* 2. Receive and Parse Response */
        std::vector<uint8_t> rb(128);
        struct timeval tv{1, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in from{};
        socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, rb.data(), rb.size(), 0, reinterpret_cast<struct sockaddr*>(&from), &flen);

        if (n > 0) {
            auto end = high_resolution_clock::now();

            // Extract IP Header Length
            struct ip* ip_h = reinterpret_cast<struct ip*>(rb.data());
            size_t hlen = ip_h->ip_hl << 2; // IHL * 4 = bytes

            // Check if we received enough data for IP + ICMP headers
            if (n >= static_cast<ssize_t>(hlen + sizeof(kn::icmp_hdr))) {
                kn::icmp_hdr reply;
                // Move pointer past the IP header to find ICMP
                std::memcpy(&reply, rb.data() + hlen, sizeof(reply));

                // Verify this is an Echo Reply for our specific PID
                if (reply.type == kn::echo_reply && ntohs(reply.id) == id) {
                    state.rx++;
                    duration<double, std::milli> rtt = end - start;
                    state.rtts.push_back(rtt.count());

                    // Report ICMP size (Total Received - IP Header Length)
                    // This will now show "64 bytes" instead of "84 bytes"
                    size_t icmp_len = n - hlen;

                    std::cout << icmp_len << " bytes from " << inet_ntoa(from.sin_addr)
                              << ": icmp_seq=" << ntohs(reply.seq)
                              << " ttl=" << static_cast<int>(ip_h->ip_ttl)
                              << " time=" << std::fixed << std::setprecision(6) << rtt.count() << " ms" << std::endl;
                }
            }
        }
        std::this_thread::sleep_for(1s);
    }


    return 0;
}
