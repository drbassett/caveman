#include <cmath>

struct Vec2
{
	f32 x, y;
};

inline static bool isNanOrInf(f32 a)
{
	return std::isnan(a) || std::isinf(a);
}

inline Vec2 operator+=(Vec2& a, Vec2 b)
{
	a.x += b.x;
	a.y += b.y;
	return a;
}

inline Vec2 operator+(Vec2 a, Vec2 b)
{
	a += b;
	return a;
}

inline Vec2 operator-=(Vec2& a, Vec2 b)
{
	a.x -= b.x;
	a.y -= b.y;
	return a;
}

inline Vec2 operator-(Vec2 a, Vec2 b)
{
	a -= b;
	return a;
}

inline Vec2 operator*=(Vec2& v, f32 s)
{
	v.x *= s;
	v.y *= s;
	return v;
}

inline Vec2 operator*(Vec2 v, f32 s)
{
	v *= s;
	return v;
}

inline Vec2 operator*(f32 s, Vec2 v)
{
	v *= s;
	return v;
}

inline f32 dot(Vec2 a, Vec2 b)
{
	return a.x * b.x + a.y * b.y;
}

inline f32 normSq(Vec2 v)
{
	return dot(v, v);
}

inline f32 norm(Vec2 v)
{
	return std::sqrt(normSq(v));
}

inline Vec2 normalize(Vec2 v)
{
	f32 n = norm(v);
	v.x /= n;
	v.y /= n;
	assert(!isNanOrInf(v.x));
	assert(!isNanOrInf(v.y));
	return v;
}

inline static bool isNormalized(Vec2 v)
{
	return std::abs(norm(v) - 1.0f) < 1.0e-8;
}

inline static f32 angleVZeroCheck(f32 angle, bool zeroTest)
{
	bool vectorZero = isNanOrInf(angle);
	if (zeroTest)
	{
		if (vectorZero)
		{
			return 0.0f;
		}
	} else
	{
		assert(!vectorZero);
	}
	return angle;
}

// cosine of angle with the positive x axis
inline f32 cosAngleV(Vec2 v, bool normalized, bool zeroTest)
{
	// The boolean parameters should be constants at the
	// call site. Thus, the compiler should be able to
	// optimize away most of the `if`s in here.

	if (normalized)
	{
		// check that the vector is close to normalized
		assert(isNormalized(v));
		return v.y;
	}

	f32 angle = v.y / norm(v);
	return angleVZeroCheck(angle, zeroTest);
}

// angle with the positive x axis
inline f32 angleV(Vec2 v, bool normalized, bool zeroTest)
{
	return std::cos(cosAngleV(v, normalized, zeroTest));
}

// cosine of angle between two vectors
inline f32 cosAngleV(Vec2 a, Vec2 b, bool normalized, bool zeroTest)
{
	f32 d = dot(a, b);
	if (normalized)
	{
		assert(isNormalized(a));
		assert(isNormalized(b));
		return d;
	}

	f32 denom = std::sqrt(normSq(a) * normSq(b));
	f32 angle = d / denom;
	return angleVZeroCheck(angle, zeroTest);
}

// angle between two vectors
//
// normalized: set to true if both incoming vectors are normalized
// zeroTest: set to true if one of the incoming vectors could have zero length
inline f32 angleV(Vec2 a, Vec2 b, bool normalized, bool zeroTest)
{
	return std::cos(cosAngleV(a, b, normalized, zeroTest));
}

