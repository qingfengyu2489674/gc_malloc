#ifndef GC_MALLOC_BITMAP_H
#define GC_MALLOC_BITMAP_H

#include <cstddef>
#include <climits>
#include <vector>


class Bitmap {
public:
    explicit Bitmap(size_t num_bits);

    void Set(size_t bit_index);
    void Clear(size_t bit_index);
    bool IsSet(size_t bit_index) const;
    size_t FindFirstSet(size_t start_bit) const;

private:
    size_t size_;
    std::vector<unsigned char> map_;
};



#endif // GC_MALLOC_BITMAP_H