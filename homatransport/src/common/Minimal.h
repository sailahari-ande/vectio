/* Copyright (c) 2009-2015 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __HOMATRANSPORT_MINIMAL_H_
#define __HOMATRANSPORT_MINIMAL_H_

#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// *********This file must be included in all files header files********//
// Unfortunately, unit tests based on gtest can't access private members
// of classes.  If the following uppercase versions of "private" and
// "protected" are used instead, it works around the problem:  when
// compiling unit test files (anything that includes TestUtil.h)
// everything becomes public.
#ifdef EXPOSE_PRIVATES
#define PRIVATE public
#define protected public
#define public public
#else
#define PRIVATE private
#define protected protected
#define public public
#endif

/**
 * Cast one size of int down to another one.
 * Asserts that no precision is lost at runtime.
 */
template<typename Small, typename Large>
Small
downCast(const Large& large)
{
    Small small = static_cast<Small>(large);
    // The following comparison (rather than "large==small") allows
    // this method to convert between signed and unsigned values.
    assert(large-small == 0);
    return small;
}

std::string format(const char* format, ...)
    __attribute__((format(printf, 1, 2)));
std::string vformat(const char* format, va_list ap)
    __attribute__((format(printf, 1, 0)));

// A macro to disallow the copy constructor and operator= functions
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;             \
    TypeName& operator=(const TypeName&) = delete;
#endif

/*
 * The following two macros are used in highly optimized code paths to hint
 * to the compiler what the expected truth value of a given expression is.
 * For instance, an 'if (expr) { ... }' statement in a hot code path might
 * benefit from being coded 'if (expect_true(expr)) { ... }' if we know that
 * 'expr' is usually true. If, instead, 'expr' is almost always false, one may
 * use the expect_false macro instead.
 */
#define expect_true(expr)   __builtin_expect((expr), true)
#define expect_false(expr)   __builtin_expect((expr), false)

/**
 * Return the size of the given type as a uint32_t. This convenience macro
 * tavoids having downcasts everywhere we take sizeof, which returns size_t,
 * but want a uint32_t instead. Stay tuned for a fancier templated version
 * by syang0 and ongaro...
 */
#define sizeof32(type) downCast<uint32_t>(sizeof(type))

#endif // RAMCLOUD_MINIMAL_H
