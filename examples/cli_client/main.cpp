#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <mini_muduo/event_loop.h>
#include <mini_muduo/inet_address.h>
#include <mini_muduo/tcp_client.h>

namespace mini_muduo {

class CliClient {
public:
    CliClient(EventLoop *pLoop, const InetAddress &serverAddr)
        : pOwnerLoop(pLoop)
        , client_(pLoop, serverAddr, "cli_client")
        , inputThread_([this] {
            this->inputThreadFunc();
        }) {
        client_.enableRetry();

        client_.setConnectionCallback([this](const TcpConnectionPtr &conn) {
            this->onConnection(conn);
        });

        client_.setMessageCallback([this](const mini_muduo::TcpConnectionPtr &conn,
                                          mini_muduo::Buffer &buf,
                                          mini_muduo::Timestamp receiveTime) {
            this->onMessage(conn, buf, receiveTime);
        });
    }

    void connect() {
        client_.connect();
    }

    ~CliClient() {
        inputThread_.join();
    }

private:
    void onConnection(const TcpConnectionPtr &conn) {
        if (conn->connected()) {
            std::unique_lock lg{mu_};
            conn_ = conn;
            cond_.notify_one();
        } else {
            pOwnerLoop->quit();
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer &buf, Timestamp receiveTime) const {
        (void)conn;
        (void)receiveTime;
        std::cout << buf.retrieveAllAsString();
        std::cout.flush();
    }

    static std::string trim(const std::string &str) {
        // Find first non-whitespace character
        auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) {
            return std::isspace(c);
        });

        // Find last non-whitespace character
        auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
                       return std::isspace(c);
                   }).base();

        // Handle all-whitespace or empty cases
        return (start >= end) ? std::string() : std::string(start, end);
    }

    void inputThreadFunc() {
        {
            std::unique_lock lg{mu_};
            cond_.wait(lg, [this] {
                return this->conn_ != nullptr;
            });
        }

        while (true) {
            std::string input_line;

            std::getline(std::cin, input_line);

            if (trim(input_line) == "quit") {
                client_.disconnect();
                conn_.reset();
                return;
            }

            input_line += "\r\n";

            conn_->send(input_line);
        }
    }

    EventLoop *pOwnerLoop;
    TcpClient client_;
    std::thread inputThread_;

    std::mutex mu_;
    std::condition_variable cond_;
    TcpConnectionPtr conn_;
};

}  // namespace mini_muduo

using namespace mini_muduo;

int main() {
    EventLoop loop;

    const InetAddress serverAddr{"127.0.0.1", 2007};

    CliClient client{&loop, serverAddr};

    client.connect();
    loop.loop();

    return 0;
}
