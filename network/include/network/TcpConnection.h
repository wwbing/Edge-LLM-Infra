#pragma once

#include <memory>

#include <boost/any.hpp>

#include "network/Buffer.h"
#include "network/Callbacks.h"
#include "network/InetAddress.h"

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace network {

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    /// Constructs a TcpConnection with a connected sockfd
    ///
    /// User should not create this object.
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const
    {
        return loop_;
    }
    const std::string &name() const
    {
        return name_;
    }
    const InetAddress &localAddress() const
    {
        return localAddr_;
    }
    const InetAddress &peerAddress() const
    {
        return peerAddr_;
    }
    bool connected() const
    {
        return state_ == kConnected;
    }
    bool disconnected() const
    {
        return state_ == kDisconnected;
    }
    // return true if success.
    bool getTcpInfo(struct tcp_info *) const;
    std::string getTcpInfoString() const;

    // void send(string&& message); // C++11
    // void send(const void *message, int len);
    // void send(const std::string &message);
    // void send(Buffer&& message); // C++11
    void send(Buffer *message);  // this one will swap data
    void shutdown();             // NOT thread safe, no simultaneous calling
    // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no
    // simultaneous calling
    void forceClose();
    void forceCloseWithDelay(double seconds);
    void setTcpNoDelay(bool on);
    // reading or not
    void startRead();
    void stopRead();
    bool isReading() const
    {
        return reading_;
    };  // NOT thread safe, may race with start/stopReadInLoop

    void setContext(const boost::any &context)
    {
        context_ = context;
    }

    const boost::any &getContext() const
    {
        return context_;
    }

    boost::any *getMutableContext()
    {
        return &context_;
    }

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

    // void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
    //                               size_t highWaterMark) {
    //   highWaterMarkCallback_ = cb;
    //   highWaterMark_ = highWaterMark;
    // }

    /// Advanced interface
    Buffer *inputBuffer()
    {
        return &inputBuffer_;
    }

    Buffer *outputBuffer()
    {
        return &outputBuffer_;
    }

    /// Internal use only.
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    // called when TcpServer accepts a new connection
    void connectEstablished();  // should be called only once
    // called when TcpServer has removed me from its map
    void connectDestroyed();  // should be called only once

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    // void sendInLoop(string&& message);
    void sendInLoop(const std::string &message);
    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();
    // void shutdownAndForceCloseInLoop(double seconds);
    void forceCloseInLoop();
    void setState(StateE s)
    {
        state_ = s;
    }
    const char *stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop *loop_;
    const std::string name_;
    StateE state_;  // FIXME: use atomic variable
    bool reading_;
    // we don't expose those classes to client.
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    // HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    // size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;  // FIXME: use list<Buffer> as output buffer.
    boost::any context_;
    // FIXME: creationTime_, lastReceiveTime_
    //        bytesReceived_, bytesSent_
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

}  // namespace network
