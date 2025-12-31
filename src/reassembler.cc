#include "reassembler.hh"
#include <cstdint>
#include <sys/types.h>
// #include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );
  // 获取接受下一字节期望首索引
  const uint64_t first_unassembled_index = output_.writer().bytes_pushed();
  // 获取字节流固定总容量
  const uint64_t capacity = output_.reader().bytes_buffered() + output_.writer().available_capacity();
  // 获取允许的最大索引
  const uint64_t first_unacceptable_index = output_.reader().bytes_popped() + capacity;

  // 处理EOF标记
  if ( is_last_substring ) {
    receive_last_ = true;
    last_index_ = first_index + data.size();
  }

  // 裁剪左侧已写入字节
  if ( first_index < first_unassembled_index ) {
    const uint64_t offset = first_unassembled_index - first_index;
    if ( offset >= data.size() ) {
      goto try_close;
    }
    data = data.substr( offset );
    first_index = first_unassembled_index;
  }

  // 裁剪右侧溢出字节
  if ( first_index >= first_unacceptable_index ) {
    goto try_close;
  }
  if ( data.size() > first_unacceptable_index - first_index ) {
    const uint64_t limit_len = first_unacceptable_index - first_index;
    data.resize( limit_len );
  }

  {
    // 待插入片段
    uint64_t new_start = first_index;
    uint64_t new_end = first_index + data.size();
    string new_data = std::move( data );

    // 寻找最后一个exist_start < new_start的数据包
    auto it = pending_.upper_bound( new_start );
    if ( it != pending_.begin() ) {
      it--;
    }

    // 遍历可能重叠的片段
    while ( it != pending_.end() ) {
      const uint64_t exist_start = it->first;
      const uint64_t exist_end = exist_start + it->second.size();
      if ( exist_start > new_end ) { // 无重叠, 已存片段全部在新片段之后
        break;
      }
      if ( exist_end < new_start ) {
        it++;
        continue;
      }

      // 合并
      // 已存片段左边超出
      if ( exist_start < new_start ) {
        new_data.insert( 0, it->second.substr( 0, new_start - exist_start ) );
        new_start = exist_start;
      }
      // 已存片段右边超出
      if ( exist_end > new_end ) {
        new_data += it->second.substr( new_end - exist_start );
        new_end = exist_end;
      }
      // exist_data被合并进new_data, 删除
      it = pending_.erase( it );
    }
    // 存入最终合并好的片段
    pending_[new_start] = std::move( new_data );
  }

  // 数据写入stream
  while ( !pending_.empty() ) {
    auto it = pending_.begin();
    const uint64_t start = it->first;
    const string& block_data = it->second;
    if ( start == output_.writer().bytes_pushed() ) {
      output_.writer().push( block_data );
      pending_.erase( it );
    } else {
      break;
    }
  }

try_close:
  if ( receive_last_ && output_.writer().bytes_pushed() == last_index_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  // debug( "unimplemented count_bytes_pending() called" );
  uint64_t pending_bytes = 0;
  for ( const auto& [idx, data] : pending_ ) {
    pending_bytes += data.size();
  }
  return pending_bytes;
}
