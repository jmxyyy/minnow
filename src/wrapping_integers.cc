#include "wrapping_integers.hh"
#include <cstdint>
// #include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  /*
    64位绝对序列号转32位包装
    (n + zero_point) mod 2^32
   */
  return Wrap32 { zero_point.raw_value_ + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  // debug( "unimplemented unwrap( {}, {} ) called", zero_point.raw_value_, checkpoint );
  // return {};
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  // 获取checkpoint当前周期位置 低32位
  uint64_t checkpoint_low = checkpoint & 0xFFFFFFFFULL;
  // 基准值 高32
  uint64_t base = checkpoint - checkpoint_low;
  // 候选
  uint64_t candidate = base + offset;
  
  if ( candidate > checkpoint + ( 1ULL << 31 ) && candidate >= ( 1ULL << 32 ) ) {
    candidate -= ( 1ULL << 32 );
  } else if ( candidate + ( 1ULL << 32 ) < checkpoint + ( 1ULL << 31 ) ) {
    candidate += ( 1ULL << 32 );
  }
  return candidate;
}
