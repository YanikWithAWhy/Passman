#pragma once

#ifndef PASSMAN_SECURE_MEMORY_H
#define PASSMAN_SECURE_MEMORY_H

#include <memory>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

extern "C" {
#include <sodium.h>
}

template<typename T>
struct SodiumAllocator {
    using value_type             = T;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal                        = std::true_type;

    SodiumAllocator() noexcept = default;

    template<typename U>
    SodiumAllocator(const SodiumAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        T* p = static_cast<T*>(sodium_allocarray(n == 0 ? 1 : n, sizeof(T)));
        if (!p) throw std::bad_alloc();
        return p;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (p) {
            sodium_memzero(p, (n == 0 ? 1 : n) * sizeof(T));
            sodium_free(p);
        }
    }

    template<typename U>
    struct rebind { using other = SodiumAllocator<U>; };
};

template<typename T, typename U>
bool operator==(const SodiumAllocator<T>&, const SodiumAllocator<U>&) noexcept { return true; }
template<typename T, typename U>
bool operator!=(const SodiumAllocator<T>&, const SodiumAllocator<U>&) noexcept { return false; }

using SecureBytes  = std::vector<unsigned char, SodiumAllocator<unsigned char>>;

using SecureString = std::basic_string<char, std::char_traits<char>, SodiumAllocator<char>>;

inline void wipeStdString(std::string& s) {
    if (!s.empty())
        sodium_memzero(const_cast<char*>(s.data()), s.size());
    s.clear();
}

#endif // PASSMAN_SECURE_MEMORY_H
