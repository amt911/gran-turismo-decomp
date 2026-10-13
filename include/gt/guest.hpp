// Typed views of game structs that live in guest (PSP) memory
// (docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md).
//
// Game objects stay in guest memory — 32-bit PSP addresses, shared with the recompiled code — so
// decompiled code can't use host pointers to them. Instead a struct's layout is declared once:
//
//     struct RaceParameter : gt::guest::Layout {
//         GT_FIELD(std::uint8_t, use_launch_data, 0x60);
//         GT_ARRAY(std::uint8_t, launch_speed, 0x61, 1, 2);   // element i at 0x61 + i * 1, 2 of them
//     };
//
// and used through a view: `gt::guest::View<RaceParameter> race(rt.memory(), address);`
// `race.launch_speed(i) = 3;` stores a byte at address + 0x61 + i; `int s = race.launch_speed(0);`
// loads it. Every access goes through GuestMemory, like the recompiled code's, so it behaves the same on
// every host (endianness, bounds). Fields GT stores big-endian are declared `be16` / `be32`; a field of
// a Layout type is a nested struct (its accessor returns a View); `Pointer<T>` is a field holding the
// guest address of a T. Only fields with evidence get a name (see each layout's comments).
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <bit>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace gt::guest {

// Base of every layout: the view's memory and address (set by View).
class Layout {
public:
    [[nodiscard]] std::uint32_t address() const noexcept { return address_; }
    [[nodiscard]] psprecomp::GuestMemory &memory() const noexcept { return *memory_; }

protected:
    psprecomp::GuestMemory *memory_ = nullptr;
    std::uint32_t address_ = 0u;
};

// A layout bound to an object in guest memory.
template <typename T>
class View : public T {
    static_assert(std::is_base_of_v<Layout, T>, "View<T> needs a layout deriving from gt::guest::Layout");

public:
    View(psprecomp::GuestMemory &memory, std::uint32_t address) {
        this->memory_ = &memory;
        this->address_ = address;
    }
};

// Field types GT stores big-endian (Adhoc data, some tables): the value is the host-order number.
struct be16 {};
struct be32 {};
// A field holding the guest address of a T (0 = null).
template <typename T>
struct Pointer {};

namespace detail {

template <typename T>
struct Scalar;  // how a scalar field type is loaded and stored
template <> struct Scalar<std::uint8_t> {
    using value_type = std::uint8_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return m.load8(a); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store8(a, v); }
};
template <> struct Scalar<std::int8_t> {
    using value_type = std::int8_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return static_cast<std::int8_t>(m.load8(a)); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store8(a, static_cast<std::uint8_t>(v)); }
};
template <> struct Scalar<std::uint16_t> {
    using value_type = std::uint16_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return m.load16(a); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store16(a, v); }
};
template <> struct Scalar<std::int16_t> {
    using value_type = std::int16_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return static_cast<std::int16_t>(m.load16(a)); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store16(a, static_cast<std::uint16_t>(v)); }
};
template <> struct Scalar<std::uint32_t> {
    using value_type = std::uint32_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return m.load32(a); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store32(a, v); }
};
template <> struct Scalar<std::int32_t> {
    using value_type = std::int32_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return static_cast<std::int32_t>(m.load32(a)); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store32(a, static_cast<std::uint32_t>(v)); }
};
template <> struct Scalar<float> {
    using value_type = float;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) { return std::bit_cast<float>(m.load32(a)); }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) { m.store32(a, std::bit_cast<std::uint32_t>(v)); }
};
template <> struct Scalar<be16> {
    using value_type = std::uint16_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) {
        const std::uint16_t v = m.load16(a);
        return static_cast<std::uint16_t>((v << 8) | (v >> 8));
    }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) {
        m.store16(a, static_cast<std::uint16_t>((v << 8) | (v >> 8)));
    }
};
template <> struct Scalar<be32> {
    using value_type = std::uint32_t;
    static value_type load(const psprecomp::GuestMemory &m, std::uint32_t a) {
        const std::uint32_t v = m.load32(a);
        return (v << 24) | ((v << 8) & 0x00FF0000u) | ((v >> 8) & 0x0000FF00u) | (v >> 24);
    }
    static void store(psprecomp::GuestMemory &m, std::uint32_t a, value_type v) {
        m.store32(a, (v << 24) | ((v << 8) & 0x00FF0000u) | ((v >> 8) & 0x0000FF00u) | (v >> 24));
    }
};

// A scalar field: loads when read, stores when assigned.
template <typename T>
class Ref {
public:
    using value_type = typename Scalar<T>::value_type;
    Ref(psprecomp::GuestMemory &memory, std::uint32_t address) : memory_(&memory), address_(address) {}
    operator value_type() const { return Scalar<T>::load(*memory_, address_); }  // NOLINT: a field reads as its value
    const Ref &operator=(value_type value) const {
        Scalar<T>::store(*memory_, address_, value);
        return *this;
    }
    const Ref &operator=(const Ref &other) const { return *this = static_cast<value_type>(other); }
    const Ref &operator+=(value_type v) const { return *this = static_cast<value_type>(value() + v); }
    const Ref &operator-=(value_type v) const { return *this = static_cast<value_type>(value() - v); }
    const Ref &operator|=(value_type v) const { return *this = static_cast<value_type>(value() | v); }
    const Ref &operator&=(value_type v) const { return *this = static_cast<value_type>(value() & v); }
    const Ref &operator^=(value_type v) const { return *this = static_cast<value_type>(value() ^ v); }
    [[nodiscard]] value_type value() const { return *this; }
    [[nodiscard]] std::uint32_t address() const noexcept { return address_; }

private:
    psprecomp::GuestMemory *memory_;
    std::uint32_t address_;
};

// A pointer field: assign a guest address, test for null, follow it with ->.
template <typename T>
class PointerRef {
public:
    PointerRef(psprecomp::GuestMemory &memory, std::uint32_t address) : memory_(&memory), address_(address) {}
    [[nodiscard]] std::uint32_t target() const { return memory_->load32(address_); }
    operator std::uint32_t() const { return target(); }  // NOLINT: the raw guest address
    const PointerRef &operator=(std::uint32_t address) const {
        memory_->store32(address_, address);
        return *this;
    }
    explicit operator bool() const { return target() != 0u; }
    [[nodiscard]] View<T> operator*() const { return View<T>(*memory_, target()); }
    const View<T> *operator->() const {
        view_.emplace(*memory_, target());
        return &*view_;
    }

private:
    psprecomp::GuestMemory *memory_;
    std::uint32_t address_;
    mutable std::optional<View<T>> view_;
};

template <typename T>
struct is_pointer_field : std::false_type {};
template <typename T>
struct is_pointer_field<Pointer<T>> : std::true_type {
    using target = T;
};

template <typename T>
auto field(psprecomp::GuestMemory &memory, std::uint32_t address) {
    if constexpr (std::is_base_of_v<Layout, T>) {
        return View<T>(memory, address);
    } else if constexpr (is_pointer_field<T>::value) {
        return PointerRef<typename is_pointer_field<T>::target>(memory, address);
    } else {
        return Ref<T>(memory, address);
    }
}

} // namespace detail
} // namespace gt::guest

// `type name` at `offset` of the layout.
#define GT_FIELD(type, name, offset) \
    auto name() const { return ::gt::guest::detail::field<type>(*this->memory_, this->address_ + (offset)); }

// `count` elements of `type`, element i at `offset + i * stride`; name##_count is the count.
#define GT_ARRAY(type, name, offset, stride, count) \
    auto name(std::uint32_t index) const { \
        return ::gt::guest::detail::field<type>(*this->memory_, this->address_ + (offset) + index * (stride)); \
    } \
    static constexpr std::uint32_t name##_count = (count);
