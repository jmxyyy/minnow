#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <queue>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), current_RTO_ms_( initial_RTO_ms)
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_ ;
  // 发送状态
  uint64_t next_seqno_ { 0 }; // 下一个要发送的绝对序列号
  bool syn_sent_ { false };   // 是否已发送SYN
  bool fin_sent_ { false };   // 是否已发送FIN

  // 接收方窗口
  uint16_t window_size_ { 1 }; // 接收方窗口大小（初始为1以便发送SYN）
  uint64_t acked_seqno_ { 0 }; // 已确认的绝对序列号

  // 未确认的segments队列
  std::queue<TCPSenderMessage> outstanding_segments_ {};
  uint64_t outstanding_bytes_ { 0 }; // 未确认的字节数（序列号数）

  // 重传定时器
  uint64_t timer_ { 0 };                       // 当前计时器值
  uint64_t current_RTO_ms_;                    // 当前RTO
  bool timer_running_ { false };               // 定时器是否运行
  uint64_t consecutive_retransmissions_ { 0 }; // 连续重传次数
};
