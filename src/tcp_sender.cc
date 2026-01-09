#include <algorithm>

#include "tcp_sender.hh"
// #include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // debug( "unimplemented sequence_numbers_in_flight() called" );
  return outstanding_bytes_;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  // debug( "unimplemented consecutive_retransmissions() called" );
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // debug( "unimplemented push() called" );
  // (void)transmit;
  // 1. 如果还没发送SYN，先发送SYN
  if ( !syn_sent_ ) {
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    msg.SYN = true;
    msg.RST = reader().has_error();

    // 尝试携带数据（如果有且窗口允许）
    uint64_t payload_size = std::min( {static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ),
                                  reader().bytes_buffered(),
                                      static_cast<uint64_t>( window_size_ ) - 1 } // -1 for SYN
    );

    if ( payload_size > 0 ) {
      read( reader(), payload_size, msg.payload );
    }

    // 如果流已关闭且所有数据已发送，尝试发送FIN
    if ( reader().is_finished() && !fin_sent_ && ( 1 + msg.payload.size() < window_size_ ) ) {
      msg.FIN = true;
      fin_sent_ = true;
    }

    syn_sent_ = true;
    next_seqno_ += msg.sequence_length();
    outstanding_bytes_ += msg.sequence_length();
    outstanding_segments_.push( msg );
    transmit( msg );

    // 启动定时器
    if ( !timer_running_ ) {
      timer_running_ = true;
      timer_ = 0;
    }

    return;
  }

  // 2. SYN已发送，发送数据段
  // 计算可用窗口空间
  uint64_t window_available = 0;
  if ( window_size_ == 0 ) {
    // 窗口为0时，假设窗口为1（用于发送零窗口探测）
    window_available = ( next_seqno_ - acked_seqno_ < 1 ) ? 1 : 0;
  } else {
    window_available = window_size_ - ( next_seqno_ - acked_seqno_ );
  }

  // 持续发送segment直到窗口满或没有数据
  while ( window_available > 0 && ( reader().bytes_buffered() > 0 || ( reader().is_finished() && !fin_sent_ ) ) ) {
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    msg.SYN = false;
    msg.RST = reader().has_error();

    // 确定payload大小
    uint64_t payload_size = min(
      { static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ), reader().bytes_buffered(), window_available } );

    if ( payload_size > 0 ) {
      read( reader(), payload_size, msg.payload );
    }

    // 如果流已关闭且所有数据已读取，发送FIN
    if ( reader().is_finished() && !fin_sent_ && msg.payload.size() < window_available ) {
      msg.FIN = true;
      fin_sent_ = true;
    }

    // 如果这个segment没有内容（没有数据也没有FIN），停止发送
    if ( msg.sequence_length() == 0 ) {
      break;
    }
    next_seqno_ += msg.sequence_length();
    outstanding_bytes_ += msg.sequence_length();
    window_available -= msg.sequence_length();
    outstanding_segments_.push( msg );
    transmit( msg );

    // 启动定时器
    if ( !timer_running_ ) {
      timer_running_ = true;
      timer_ = 0;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // debug( "unimplemented make_empty_message() called" );
  // return {};
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.SYN = false;
  msg.FIN = false;
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // debug( "unimplemented receive() called" );
  // (void)msg;
  // 处理RST
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  // 更新窗口大小
  window_size_ = msg.window_size;

  // 处理ackno
  if ( msg.ackno.has_value() ) {
    uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, next_seqno_ );

    // 忽略不可能的ackno（确认了未发送的数据）
    if ( abs_ackno > next_seqno_ ) {
      return;
    }

    // 忽略旧的ackno（没有确认新数据）
    if ( abs_ackno <= acked_seqno_ ) {
      return;
    }

    // 更新已确认序列号
    acked_seqno_ = abs_ackno;

    // 移除已确认的segments
    while ( !outstanding_segments_.empty() ) {
      const auto& seg = outstanding_segments_.front();
      uint64_t seg_start = seg.seqno.unwrap( isn_, next_seqno_ );
      uint64_t seg_end = seg_start + seg.sequence_length();

      if ( seg_end <= acked_seqno_ ) {
        outstanding_bytes_ -= seg.sequence_length();
        outstanding_segments_.pop();
      } else {
        break;
      }
    }
    // 收到新的确认，重置RTO和定时器
    current_RTO_ms_ = initial_RTO_ms_;
    timer_ = 0;
    consecutive_retransmissions_ = 0;

    // 如果还有未确认的数据，继续运行定时器
    timer_running_ = outstanding_bytes_ > 0;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  // (void)transmit;
  // 更新定时器
  if ( !timer_running_ ) {
    return;
  }

  timer_ += ms_since_last_tick;

  // 检查是否超时
  if ( timer_ >= current_RTO_ms_ ) {
    // 重传最早的未确认segment
    if ( !outstanding_segments_.empty() ) {
      transmit( outstanding_segments_.front() );

      // 如果窗口大小不为0，执行指数退避
      if ( window_size_ > 0 ) {
        current_RTO_ms_ *= 2;
      }

      // 增加连续重传计数
      consecutive_retransmissions_++;

      // 重置定时器
      timer_ = 0;
    }
  }
}
