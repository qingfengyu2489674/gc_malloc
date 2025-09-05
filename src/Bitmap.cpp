#include "gc_malloc/Bitmap.hpp"
#include <cassert>

Bitmap::Bitmap(size_t num_bits)
    :size_(num_bits),
    map_((num_bits + CHAR_BIT -1) / CHAR_BIT) {

}

void Bitmap::Set(size_t bit_index) {

    if(bit_index >= size_) {
        return;
    }

    size_t byte_index = bit_index / CHAR_BIT;
    size_t bit_in_byte = bit_index % CHAR_BIT;

    map_[byte_index] |= (1U << bit_in_byte);
}

void Bitmap::Clear(size_t bit_index) {

    if(bit_index >= size_) {
        return;
    }

    size_t byte_index = bit_index / CHAR_BIT;
    size_t bit_in_byte = bit_index % CHAR_BIT;

    map_[byte_index] &= ~(1U << bit_in_byte);
}



bool Bitmap::IsSet(size_t bit_index) const {

    if(bit_index >= size_) {
        return false;
    }

    size_t byte_index = bit_index / CHAR_BIT;
    size_t bit_in_byte = bit_index % CHAR_BIT;

    return (map_[byte_index] >> bit_in_byte) & 1U;
}



size_t Bitmap::FindFirstSet(size_t start_bit) const {
    for (size_t i = start_bit; i < size_; i++) {
        if(IsSet(i)) {
            return i;
        }
    }
    return size_;
}