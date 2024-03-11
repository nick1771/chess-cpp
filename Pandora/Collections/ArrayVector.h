#pragma once

#include "Pandora/Pandora.h"

#include <stdexcept>
#include <array>

namespace Pandora {

    template<class T, usize N>
    class ArrayVector {
    public:
        void push(const T& value) {
            if (_size == N) {
                throw std::out_of_range("Index out of range");
            }
            _data[_size++] = value;
        }

        const T* data() const {
            return _data.data();
        }

        const usize size() const {
            return _size;
        }

        template<class Self>
        auto&& operator[](this Self&& self, usize index) {
            if (self._size <= index) {
                throw std::out_of_range("Index out of range");
            } else {
                return self._data[index];
            }
        }

        auto begin() const {
            return _data.begin();
        }

        auto end() const {
            return _data.end();
        }
    private:
        std::array<T, N> _data{};
        usize _size{};
    };
}
