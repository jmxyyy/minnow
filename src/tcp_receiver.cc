#include "tcp_receiver.hh"
// #include "debug.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // debug( "unimplemented receive() called" );
  // (void)message;
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( message.SYN ) {
    initial_seqno_ = message.seqno;
    syn_ = true;
  }
  if ( !syn_ ) {
    return;
  }

  // 计算段绝对序列号
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t abs_seqno = message.seqno.unwrap( initial_seqno_, checkpoint );

  // 转流索引
  uint64_t stream_index = abs_seqno + message.SYN - 1;

  bool is_last = message.FIN;
  // 插入重组器
  reassembler_.insert( stream_index, message.payload, is_last );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  // debug( "unimplemented send() called" );
  // return {};
  TCPReceiverMessage msg;
  msg.RST = reassembler_.reader().has_error();
  uint64_t window_size = reassembler_.writer().available_capacity();
  msg.window_size = window_size > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>( window_size );

  if ( syn_ ) {
    uint64_t bytes_pushed = reassembler_.writer().bytes_pushed();
    uint64_t abs_ackno = bytes_pushed + 1;
    if ( reassembler_.writer().is_closed() ) {
      abs_ackno++;
    }
    msg.ackno = Wrap32::wrap( abs_ackno, initial_seqno_ );
  }
  return msg;
}
