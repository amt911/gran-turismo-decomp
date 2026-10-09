// scePsmf HLE: the PSMF header queries GT makes before playing a movie (intro, replays). Return
// values and error codes follow PPSSPP's scePsmf, checked against its log of the original game.
// Decoding itself is sceMpeg (host/gt_kernel.cpp).

#include "psmf.hpp"
#include "psmf_hle.hpp"

#include <unordered_map>
#include <vector>

namespace gt::psmf {
namespace {

constexpr std::uint32_t kErrorNotFound = 0x80615025u;
constexpr std::uint32_t kErrorInvalidId = 0x80615100u;
constexpr std::uint32_t kErrorInvalidTimestamp = 0x80615500u;
constexpr std::uint32_t kErrorInvalidPsmf = 0x80615501u;
constexpr std::uint32_t kErrorNotInitialized = 0x80615001u;

struct Movie {
    std::vector<std::uint8_t> bytes;  // the header, as the game handed it to scePsmfSetPsmf
    Header header;
    std::int32_t current{-1};         // stream selected by scePsmfSpecifyStream
};

std::unordered_map<std::uint32_t, Movie> g_movies;  // keyed by the guest ScePsmf struct

std::vector<std::uint8_t> read_header_bytes(psprecomp::Runtime &rt, std::uint32_t address) {
    auto &memory = rt.memory();
    if (!memory.contains(address, kHeaderSize)) return {};
    std::vector<std::uint8_t> bytes(kHeaderSize);
    memory.copy_out(address, bytes);
    // The header region runs up to the stream data; EP maps can sit past the first 2 KiB.
    const auto parsed = parse(bytes);
    if (parsed && parsed->stream_offset > kHeaderSize && parsed->stream_offset <= 0x100000u &&
        memory.contains(address, parsed->stream_offset)) {
        bytes.resize(parsed->stream_offset);
        memory.copy_out(address, bytes);
    }
    return bytes;
}

Movie *movie_at(std::uint32_t address) {
    const auto found = g_movies.find(address);
    return found != g_movies.end() ? &found->second : nullptr;
}

const Stream *current_stream(const Movie &movie) {
    if (movie.current < 0 || static_cast<std::size_t>(movie.current) >= movie.header.streams.size()) return nullptr;
    return &movie.header.streams[static_cast<std::size_t>(movie.current)];
}

void store(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    if (rt.memory().contains(address, 4u)) rt.memory().store32(address, value);
}

// Handlers that take the ScePsmf struct in a0 and fail with NOT_INITIALIZED without one.
template <typename Body>
void with_movie(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, Body body) {
    Movie *movie = movie_at(ctx.gpr[4]);
    if (movie == nullptr) {
        ctx.set_gpr(2, kErrorNotInitialized);
        return;
    }
    ctx.set_gpr(2, body(rt, ctx, *movie));
}

} // namespace

void install_hle(psprecomp::Runtime &runtime) {
    // scePsmfVerifyPsmf(header)
    runtime.register_hle("scePsmf", 0x2673646Bu, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        ctx.set_gpr(2, parse(read_header_bytes(rt, ctx.gpr[4])) ? 0u : kErrorInvalidPsmf);
    });
    // scePsmfQueryStreamOffset(header, out) / scePsmfQueryStreamSize(header, out)
    runtime.register_hle("scePsmf", 0x5B70FCC1u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const auto header = parse(read_header_bytes(rt, ctx.gpr[4]));
        if (!header) { ctx.set_gpr(2, kErrorInvalidPsmf); return; }
        store(rt, ctx.gpr[5], header->stream_offset);
        ctx.set_gpr(2, 0u);
    });
    runtime.register_hle("scePsmf", 0x9553CC91u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const auto header = parse(read_header_bytes(rt, ctx.gpr[4]));
        if (!header) { ctx.set_gpr(2, kErrorInvalidPsmf); return; }
        store(rt, ctx.gpr[5], header->stream_size);
        ctx.set_gpr(2, 0u);
    });
    // scePsmfSetPsmf(psmf, header): fills the guest struct like PPSSPP (version, header size,
    // header address, stream size, current stream).
    runtime.register_hle("scePsmf", 0xC22C8327u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const std::uint32_t psmf = ctx.gpr[4];
        const std::uint32_t data = ctx.gpr[5];
        Movie movie;
        movie.bytes = read_header_bytes(rt, data);
        auto header = parse(movie.bytes);
        if (!header || !rt.memory().contains(psmf, 32u)) {
            ctx.set_gpr(2, kErrorInvalidPsmf);
            return;
        }
        movie.header = std::move(*header);
        auto &memory = rt.memory();
        memory.store32(psmf + 0u, movie.header.raw_version);
        memory.store32(psmf + 4u, static_cast<std::uint32_t>(kHeaderSize));
        memory.store32(psmf + 8u, data);
        memory.store32(psmf + 12u, movie.header.stream_size);
        memory.store32(psmf + 16u, 0u);
        memory.store32(psmf + 20u, static_cast<std::uint32_t>(movie.current));
        memory.store32(psmf + 24u, 0u);
        memory.store32(psmf + 28u, 0u);
        g_movies[psmf] = std::move(movie);
        ctx.set_gpr(2, 0u);
    });
    // scePsmfGetPsmfVersion(psmf)
    runtime.register_hle("scePsmf", 0xE1283895u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](auto &, auto &, Movie &movie) { return movie.header.raw_version; });
    });
    // scePsmfGetPresentationStartTime / EndTime(psmf, out)
    runtime.register_hle("scePsmf", 0x76D3AEBAu, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](psprecomp::Runtime &r, psprecomp::AllegrexContext &c, Movie &movie) {
            store(r, c.gpr[5], movie.header.start_time);
            return 0u;
        });
    });
    runtime.register_hle("scePsmf", 0xBD8AE0D8u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](psprecomp::Runtime &r, psprecomp::AllegrexContext &c, Movie &movie) {
            store(r, c.gpr[5], movie.header.end_time);
            return 0u;
        });
    });
    // scePsmfGetNumberOfSpecificStreams(psmf, type)
    runtime.register_hle("scePsmf", 0x68D42328u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](auto &, psprecomp::AllegrexContext &c, Movie &movie) {
            return static_cast<std::uint32_t>(count_streams(movie.header, static_cast<std::int32_t>(c.gpr[5])));
        });
    });
    // scePsmfSpecifyStream(psmf, number)
    runtime.register_hle("scePsmf", 0x4BC9BDE0u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](auto &, psprecomp::AllegrexContext &c, Movie &movie) {
            const auto number = static_cast<std::int32_t>(c.gpr[5]);
            if (number < 0 || static_cast<std::size_t>(number) >= movie.header.streams.size()) return kErrorInvalidId;
            movie.current = number;
            return 0u;
        });
    });
    // scePsmfGetCurrentStreamType(psmf, type_out, channel_out)
    runtime.register_hle("scePsmf", 0xC7DB3A5Bu, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](psprecomp::Runtime &r, psprecomp::AllegrexContext &c, Movie &movie) {
            const Stream *stream = current_stream(movie);
            if (stream == nullptr) return kErrorNotFound;
            store(r, c.gpr[5], static_cast<std::uint32_t>(stream->type));
            store(r, c.gpr[6], stream->channel);
            return 0u;
        });
    });
    // scePsmfGetVideoInfo(psmf, info_out): width, height of the current (video) stream
    runtime.register_hle("scePsmf", 0x0BA514E5u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](psprecomp::Runtime &r, psprecomp::AllegrexContext &c, Movie &movie) {
            const Stream *stream = current_stream(movie);
            if (stream == nullptr || stream->type != kAvc) return kErrorInvalidId;
            store(r, c.gpr[5], stream->width);
            store(r, c.gpr[5] + 4u, stream->height);
            return 0u;
        });
    });
    // scePsmfCheckEPmap(psmf)
    runtime.register_hle("scePsmf", 0x971A3A90u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](auto &, auto &, Movie &movie) {
            const Stream *stream = current_stream(movie);
            return stream != nullptr && !ep_map(movie.bytes, *stream).empty() ? 0u : kErrorNotFound;
        });
    });
    // scePsmfGetEPWithTimestamp(psmf, pts, entry_out): pts, offset, index, picture offset
    runtime.register_hle("scePsmf", 0x7C0E7AC3u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        with_movie(rt, ctx, [](psprecomp::Runtime &r, psprecomp::AllegrexContext &c, Movie &movie) {
            const std::uint32_t pts = c.gpr[5];
            if (pts < movie.header.start_time) return kErrorInvalidTimestamp;
            const Stream *stream = current_stream(movie);
            if (stream == nullptr) return kErrorNotFound;
            const auto entry = find_ep(ep_map(movie.bytes, *stream), pts);
            if (!entry) return kErrorNotFound;
            store(r, c.gpr[6], entry->pts);
            store(r, c.gpr[6] + 4u, entry->offset);
            store(r, c.gpr[6] + 8u, entry->index);
            store(r, c.gpr[6] + 12u, entry->pic_offset);
            return 0u;
        });
    });
}

} // namespace gt::psmf
