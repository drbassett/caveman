#pragma once

#include <cstdint>

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

typedef float f32;
static_assert(sizeof(f32) == 4, "float type expected to occupy 4 bytes");

typedef double f64;
static_assert(sizeof(f64) == 8, "double type expected to occupy 8 bytes");

struct FilePath
{
	const char *_0;
};

struct MemStack
{
	u8 *floor, *top, *ceiling;
	MemStack *next;
};

struct MemMarker
{
	u8 *_0;
};

#define unreachable() assert(false)

#define stackAllocArray(mem, type, count) (type*) allocate(mem, sizeof(type) * count)

MemStack newMemStack(size_t capacity);
u8* allocate(MemStack& m, size_t size);
MemMarker mark(MemStack m);
void release(MemStack& m, MemMarker marker);

